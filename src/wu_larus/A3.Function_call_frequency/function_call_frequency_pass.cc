/*
 A <FREQUENCIA LOCAL DE CHAMADA> de <F> chamando <G>, eh a soma da frequencia dos blocos de <F>, que chamam <G>.
 A <FREQUENCIA GLOBAL DE CHAMADA> da funcao <F> chamando <G>, eh o numero de vezes que <F> chama <G> durante
 todas as invocacoes de <F>.
 Que eh o produto da <FREQUENCIA DE CHAMADA LOCAL> vezes a <FREQUENCIA GLOBAL DE INVOCACAO> de <F>.
*/
struct FunctionCallFrequencyPass {
    typedef std::pair<const Function*, const Function*> Edge;

    FunctionCallFrequencyPass(
        map<Function *, BranchPredictionPass *> *function_branch_prediction_results,
        map<Function *, BlockEdgeFrequencyPass *> *function_block_edge_frequency_results
    ) :
        function_branch_prediction_results_(function_branch_prediction_results),
        function_block_edge_frequency_results_(function_block_edge_frequency_results)
    { }

    PreservedAnalyses run(Module &);

private:
    void propagate_call_freq(Function *f, Function *head, bool is_final);

    // Previous passes results.
    map<Function *, BranchPredictionPass *> *function_branch_prediction_results_;
    map<Function *, BlockEdgeFrequencyPass *> *function_block_edge_frequency_results_;

    map<Function *, set<Function *>> reachable_functions_;
    set<Edge> back_edges_;
    map<Function *, bool> visited_functions_;
    map<Edge, double> lfreqs_, back_edge_prob_;
    map<Function *, double> cfreqs_; // Call frequency of each function.
    map<Edge, double> gfreqs_; // Global call frequency of Fi calling Fj (Fi -> Fj).
};

/*
   Input:
   * A call graph, each node of which is a procedure and
   * each edge Fi -> Fj represents a call from function Fi to Fj.
   * Edge Fi -> Fj has local call frequency lfreq(Fi->Fj).

   Output:
   * Assignments of global function call frequency gfreq(Fi->Fj) to edge Fi -> Fj
   * and invocation frequency cfreq(F) to F.

 */
/* Algorithm steps:
   1. foreach edge do: back_edge_prob(edge) = lfreq(edge);
   2. foreach function f in reverse depth-first order do:
        if f is a loop head then
          mark all nodes reachable from f as not visited and all other as visited;
          propagate_call_freq(f, f, false);
   3. mark all nodes reachable from entry func as not visited and others as visited;
   4. propagate_call_freq(entry_func, entry_func, true);
 */
PreservedAnalyses FunctionCallFrequencyPass::run(Module &module) {
    CallGraph cg {module};
    Function *entry_func = module.getFunction("main");
    errs() << "FUNCTION_CALL_FREQUENCY_PASS:\n"
           << "*****************************\n";
//    errs() << "MODULE'S CALL GRAPH\n*******************\n";
//    cg.dump();

    {// Step.1.
        for (Function &func : module) {
            visited_functions_[&func] = false;
            set<Function *> reachable_nodes = {}; // Graph nodes (functions) reachable by func.
            for (BasicBlock &bb : func) {
                for (Instruction &instr : bb) {
                    if (auto *call = dyn_cast<CallInst>(&instr)) {// Find call instructions.
                        Edge edge = make_pair(&func, call->getCalledFunction());
                        lfreqs_[edge] = lfreqs_[edge] + // Add block's frequency to edge.
                            function_block_edge_frequency_results_[0][&func]->getBlockFrequency(&bb);
                        reachable_nodes.insert(call->getCalledFunction());
                    }
                }
            }
            reachable_functions_[&func] = reachable_nodes;
        }
        back_edge_prob_ = lfreqs_;

        errs() << "*****[ " << "Called functions" << " ]*****\n";
        for (auto it = reachable_functions_.begin(); it != reachable_functions_.end(); ++it) {
            errs() << "FUNCTION[" << it->first->getName() << "] CALLS:\n";
            for (auto jt = it->second.begin(); jt != it->second.end(); ++jt) {
                errs() << "\t[" << (*jt)->getName() << "]\n";
            }
        } errs() << "\n";

        // Print starting probs.
        errs() << "*****[ " << "Starting Back Edge Probs" << " ]*****\n";
        for (auto &freq : back_edge_prob_) {
            errs() << "lfreq(" << freq.first.first->getName() << "<" << freq.first.first << "> , "
                   << freq.first.second->getName() << ") = " << freq.second << "\n";
        } errs() << "\n";

    }
    {// Step.2.
        vector<Function *> dfs_functions = {};
        set<Function *> loop_heads = {};

        {// Build a list of functions reached from entry_func.
            vector<Function *> visited_stack = {}; // Detect recursion.
            dfs_functions.push_back(entry_func); // Start dfs from entry function.
            std::function<void(Function *)> dfs;
            dfs = [&](Function *foo) -> void {
                visited_stack.push_back(foo);
                for (Function *calledFunction : reachable_functions_[foo]) {
                    auto found = find(dfs_functions.begin(), dfs_functions.end(), calledFunction);
                    if (found == dfs_functions.end()) {
                        dfs_functions.push_back(calledFunction);
                        dfs(calledFunction);
                    } else {// Check if it is a loop.
                        for (auto f = visited_stack.rbegin(); f != visited_stack.rend(); ++f) {
                            if (*f == calledFunction) {
                                //errs() << "DETECTED LOOP: " << foo->getName() << " calls " << f[0]->getName() << "\n";
                                loop_heads.insert(*f);
                                back_edges_.insert(make_pair(foo, *f));
                            }
                        }
                    }
                }
                visited_stack.pop_back();
            };
            dfs(dfs_functions.front());
        }

        for (auto &edge : back_edges_) {// Print back edges found.
            errs() << "Back edge [" << edge.first->getName() << " -> " << edge.second->getName() << "]\n";
        }

        // Foreach function f in reverse depth-first order do.
        for (auto f = dfs_functions.rbegin(); f != dfs_functions.rend(); ++f) {
            auto it = loop_heads.find(*f);
            if (it != loop_heads.end()) {
                Function *loop = *it;
                auto &reachable = reachable_functions_[loop]; // Nodes reachable from f.
                // Mark nodes reachable from f as not visited (false), and all others as visited (true).
                for (auto jt = visited_functions_.begin(); jt != visited_functions_.end(); ++jt) {
                    jt->second = !(reachable.find(jt->first) != reachable.end());
                }
                // Mark f as not visited.
                visited_functions_[loop] = false;
                propagate_call_freq(loop, loop, false);
            }
        }
    }
    {// Step.3.
        errs() << "STEP 3\n";
        auto &reachable = reachable_functions_[entry_func];
        for (auto jt = visited_functions_.begin(); jt != visited_functions_.end(); ++jt) {
            jt->second = !(reachable.find(jt->first) != reachable.end());
            errs() << "Marking [" << jt->first->getName() << "] as "
                   << (jt->second ? "Visited" : "Not visited") << "\n";
        }
        visited_functions_[entry_func] = false;
    }
    {// Step.4.
        propagate_call_freq(entry_func, entry_func, true);
    }
    /*
// Print visited nodes.
for (auto jt = visited_functions_.begin(); jt != visited_functions_.end(); ++jt) {
errs() << "Function [" << jt->first->getName() << "] = " << jt->second << "\n";
}
    */

    // Sum incoming cfreqs for functions not propagated to.
    for (Function &func : module) {
        if (cfreqs_.find(&func) == cfreqs_.end()) {
            errs() << "NOT FOUND [" << func.getName() << "]\n";
            for (auto reachables : reachable_functions_) {
                if (&func != reachables.first && (reachables.second.find(&func) != reachables.second.end())) {
                    errs() << "\tFunction [" << reachables.first->getName() << "] reaches [" << func.getName() << "]\n";
                    Edge edge = make_pair(reachables.first, &func);
                    cfreqs_[&func] += gfreqs_[edge];
                }
            }
        }
    }

    errs() << "<<FINAL RESULTS>>\n\n";
    errs() << "*****[ " << "Back edge probs" << " ]*****\n";
    for (auto &freq : back_edge_prob_) {
        errs() << "lfreq(" << freq.first.first->getName() << "<" << freq.first.first << "> , " << freq.first.second->getName()
               << ") = " << freq.second << "\n";
    } errs() << "\n";
    errs() << "*****[ " << "GFreq" << " ]*****\n";
    for (auto &freq : gfreqs_) {
        errs() << "gfreq(" << freq.first.first->getName() << "<" << freq.first.first << "> , " << freq.first.second->getName()
               << ") = " << freq.second << "\n";
    } errs() << "\n";

    errs() << "*****[ " << "CFreq" << " ]*****\n";
    for (auto freq : cfreqs_) {
        errs() << "Function [" << freq.first->getName() << "] = " << freq.second << "\n";
    } errs() << "\n";

    // Finally, multiply block freqs by cfreqs.
    for (auto freq : cfreqs_) {
        for (BasicBlock &bb : *freq.first) {
            errs() << "Updating [" << freq.first->getName() << "/" << &bb << "] freq from "
                   << function_block_edge_frequency_results_[0][freq.first]->getBlockFrequency(&bb)
                   << " to ";
            function_block_edge_frequency_results_[0][freq.first]->updateBlockFrequency(&bb, freq.second);
            errs() << function_block_edge_frequency_results_[0][freq.first]->getBlockFrequency(&bb) << "\n";
        }
    }

    return PreservedAnalyses::all();
}

void FunctionCallFrequencyPass::propagate_call_freq(Function *f, Function *head, bool is_final) {
    const double epsilon = 0.000001;

    if (visited_functions_[f]) return;
    errs() << "\nCALLING!! propagate_call_freq(f = " << f->getName()
           << ", head = " << head->getName()
           << ", final = " << is_final << ")\n\n";

    {// 1. Find cfreq(f).
        vector<Function *> fpreds = {}; // Predecessors of f.
        for (auto it = reachable_functions_.begin(); it != reachable_functions_.end(); ++it)
            if (it->second.find(f) != it->second.end()) {// f is called by it->first.
                Function *fp = it->first;
                Edge fp_f = make_pair(fp, f);
                if (!visited_functions_[fp] && (back_edges_.find(fp_f) == back_edges_.end())) return;
                fpreds.push_back(it->first);
            }
        cfreqs_[f] = (f == head ? 1 : 0);
        double cyclic_probability = 0;
        for (auto fp : fpreds) {
            Edge fp_f = make_pair(fp, f);
            if (is_final && (back_edges_.find(fp_f) != back_edges_.end()))
                cyclic_probability += back_edge_prob_[fp_f];
            else if (back_edges_.find(fp_f) == back_edges_.end())
                cfreqs_[f] += gfreqs_[fp_f];
        }
        if (cyclic_probability > 1 - epsilon) cyclic_probability = 1 - epsilon;
        cfreqs_[f] = cfreqs_[f] / (1.0 - cyclic_probability);
        errs() << "*****[ " << "CFreq" << " ]*****\n";
        for (auto freq : cfreqs_) {
            errs() << "Function [" << freq.first->getName() << "] = " << freq.second << "\n";
        } errs() << "\n";
    }
    {// 2. Calculate global call frequencies for f's out edges.
        visited_functions_[f] = true;
        for (auto fi : reachable_functions_[f]) {
            Edge f_fi = make_pair(f, fi);
            gfreqs_[f_fi] = lfreqs_[f_fi] * cfreqs_[f];
            if (fi == head && !is_final) back_edge_prob_[f_fi] = lfreqs_[f_fi] * cfreqs_[f];
        }
    }
    {// 3. Propagate to successor nodes.
        for (auto fi : reachable_functions_[f]) {
            Edge f_fi = make_pair(f, fi);
            errs() << "Propagate to successor node from [" << f->getName() << "] to ["
                   << fi->getName() << "]\n";
            if (back_edges_.find(f_fi) == back_edges_.end())
                propagate_call_freq(fi, head, is_final);
        }
    }
}
