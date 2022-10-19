#include <llvm/ADT/SmallSet.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/PostDominators.h>
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

llvm::PreservedAnalyses PredictionPass::run(llvm::Module &module, llvm::ModuleAnalysisManager &mam) {
    map<Function *, BranchPredictionPass *> function_branch_prediction_results {};
    map<Function *, BlockEdgeFrequencyPass *> function_block_edge_frequency_results {};
    PassBuilder pb;
    FunctionAnalysisManager fam;
    pb.registerFunctionAnalyses(fam);

    errs() << "Module: " << module.getName() << "\n";
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
