#include <llvm/ADT/SmallSet.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Compiler.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <set>
#include <utility>
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;

#include "wu_larus/A1.Branch_prediction/branch_prediction_pass.cc"
#include "wu_larus/A2.Block_edge_frequency/block_edge_frequency_pass.cc"
#include "wu_larus/A3.Function_call_frequency/function_call_frequency_pass.cc"

struct PredictionPass : public llvm::PassInfoMixin<PredictionPass> {
    llvm::PreservedAnalyses run(llvm::Module &, llvm::ModuleAnalysisManager &);
};

cl::opt<std::string> cost_opt(
    "prediction-cost-kind",
    cl::init("latency"),
//    cl::init("recipthroughput"),
//    cl::init("codesize"),
//    cl::init("sizeandlatency"),
    cl::desc("Specify cost kind used"),
    cl::value_desc("one of: recipthroughput, latency, codesize, sizeandlatency"));

llvm::PreservedAnalyses PredictionPass::run(llvm::Module &module, llvm::ModuleAnalysisManager &mam) {
    map<Function *, BranchPredictionPass *> function_branch_prediction_results {};
    map<Function *, BlockEdgeFrequencyPass *> function_block_edge_frequency_results {};
    PassBuilder pb;
    FunctionAnalysisManager fam;
    pb.registerFunctionAnalyses(fam);

//    errs() << "Module: " << module.getName() << "\n";
    for (Function &func : module) {// Run wu's algorithms 1 and 2 for each function.
        // Skip declared only functions (prototypes).
        if (func.empty() && !func.isMaterializable()) continue;
//        func.viewCFG();
        // Algorithm 1.
        BranchPredictionPass *branchPredictionPass = new BranchPredictionPass;
        branchPredictionPass->run(func, fam);

        // Algorithm 2.
        BlockEdgeFrequencyPass *blockEdgeFrequencyPass = new BlockEdgeFrequencyPass{branchPredictionPass};
        blockEdgeFrequencyPass->run(func, fam);

        // Preserve the analysis passes data for Algorithm 3.
        function_branch_prediction_results[&func] = branchPredictionPass;
        function_block_edge_frequency_results[&func] = blockEdgeFrequencyPass;
    }
    // Algorithm 3.
    FunctionCallFrequencyPass functionCallFrequencyPass {
        &function_branch_prediction_results,
        &function_block_edge_frequency_results
    };
    functionCallFrequencyPass.run(module);

    //*TargetTransformInfo TTI = &getAnalysis().getTTI(fn);
    //TTI->getInstructionCost(Inst, TargetTransformInfo::TargetCostKind::TCK_RecipThroughput)

    TargetTransformInfo::TargetCostKind cost_kind =
        cost_opt == "recipthroughput" ? TargetTransformInfo::TargetCostKind::TCK_RecipThroughput :
        cost_opt == "codesize" ? TargetTransformInfo::TargetCostKind::TCK_CodeSize :
        cost_opt == "sizeandlatency" ? TargetTransformInfo::TargetCostKind::TCK_SizeAndLatency :
        cost_opt == "latency" ? TargetTransformInfo::TargetCostKind::TCK_Latency :
        ((errs() << "WARNING! Invalid option --prection-cost-kind=" << cost_opt << " using 'latency' instead.\n"),
         cost_opt = "latency", TargetTransformInfo::TargetCostKind::TCK_Latency); // Default to latency.

    double total_cost = 0;
    map<llvm::StringRef, double> function_costs = {};
    for (Function &func : module) {
        TargetTransformInfo &tira = fam.getResult<TargetIRAnalysis>(func);
        function_costs[func.getName()] = 0;
        for (BasicBlock &bb : func) {
            for (Instruction &instr : bb) {
                InstructionCost cost = tira.getInstructionCost(&instr, cost_kind);
                if (cost.getValue().hasValue()) {
                    double icost = cost.getValue().getValue();
                    function_costs[func.getName()] += icost * function_block_edge_frequency_results[&func]->getBlockFrequency(&bb);
                    total_cost += icost * function_block_edge_frequency_results[&func]->getBlockFrequency(&bb);
//                    errs() << "Instruction [" << instr << "] / Cost = [" << icost << "]\n";
                }
            }
        }
    }
/*
    errs() << "Module [" << module.getName() << "] // "
           << "Cost opt [" << cost_opt << "] // "
           << "Result = [" << total_cost << "]\n";
*/
    errs() << "Cost kind: " << cost_opt << "\n";
    errs() << "Total cost: " << total_cost << "\n";
    for (const auto &fcost : function_costs) {
        errs() << fcost.first << ": " << fcost.second << "\n";
    }

    return llvm::PreservedAnalyses::all();
}

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "prediction_pass",
        LLVM_VERSION_STRING,
        [](llvm::PassBuilder &pb) {
            pb.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::ModulePassManager &fpm, llvm::ArrayRef <llvm::PassBuilder::PipelineElement>) {
                    if (name == "prediction_pass") {
                        fpm.addPass(PredictionPass());
                        return true;
                    }
                    return false;
                }
                );
        }
    };
}
