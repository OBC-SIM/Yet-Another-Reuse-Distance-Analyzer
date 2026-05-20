#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/IrHelpers.hpp"
#include "../include/JsonExportVisitor.hpp"
#include "../include/Statement.hpp"

using namespace llvm;
// lat::LoopNest는 llvm::LoopNest와 충돌하므로 명시적으로 lat:: 사용
using lat::NameMap;
using lat::Statement;
using lat::ScalarAccess;
using lat::ArrayAccess;
using lat::buildDebugNameMap;
using lat::getInductionVarName;
using lat::getTripCount;
using lat::getIndexVars;
using lat::getBaseName;

namespace {

// ── 트리 빌더 ─────────────────────────────────────────────

static std::unique_ptr<lat::LoopNest> buildLoopNest(Loop* L, ScalarEvolution& SE,
                                                unsigned depth, const NameMap& names);

/**
 * @brief 루프 바디의 직접 BB에서 메모리 접근 Statement를 수집한다.
 *
 * 서브루프 BB는 건너뛰고, 서브루프 헤더를 만나면 자식 LoopNest로 삽입한다.
 *
 * @param L     현재 루프
 * @param SE    ScalarEvolution 분석 결과
 * @param depth 현재 루프 깊이
 * @param nest  Statement를 추가할 대상 LoopNest
 * @param names llvm.dbg.value 기반 Value → 변수명 맵
 */
static void populateBody(Loop* L, ScalarEvolution& SE, unsigned depth,
                         lat::LoopNest& nest, const NameMap& names) {
    std::set<BasicBlock*>        subLoopBlocks;
    std::map<BasicBlock*, Loop*> subLoopHeaders;
    for (Loop* Sub : L->getSubLoops()) {
        subLoopHeaders[Sub->getHeader()] = Sub;
        for (BasicBlock* BB : Sub->blocks())
            subLoopBlocks.insert(BB);
    }

    std::set<Loop*> processed;
    for (BasicBlock* BB : L->blocks()) {
        auto hIt = subLoopHeaders.find(BB);
        if (hIt != subLoopHeaders.end()) {
            if (!processed.count(hIt->second)) {
                processed.insert(hIt->second);
                nest.addChild(buildLoopNest(hIt->second, SE, depth + 1, names));
            }
            continue;
        }
        if (subLoopBlocks.count(BB)) continue;

        for (Instruction& I : *BB) {
            GetElementPtrInst* GEP = nullptr;
            if (auto* Load = dyn_cast<LoadInst>(&I))
                GEP = dyn_cast<GetElementPtrInst>(Load->getPointerOperand());
            else if (auto* Store = dyn_cast<StoreInst>(&I))
                GEP = dyn_cast<GetElementPtrInst>(Store->getPointerOperand());
            if (!GEP) continue;

            auto indices = getIndexVars(GEP, SE, names);
            std::string base = getBaseName(GEP->getPointerOperand(), names);
            if (indices.empty())
                nest.addChild(std::make_unique<ScalarAccess>(base));
            else
                nest.addChild(std::make_unique<ArrayAccess>(base, indices));
        }
    }
}

static std::unique_ptr<lat::LoopNest> buildLoopNest(Loop* L, ScalarEvolution& SE,
                                                unsigned depth, const NameMap& names) {
    auto nest = std::make_unique<lat::LoopNest>(getInductionVarName(L, SE, names),
                                           getTripCount(L, SE), depth);
    populateBody(L, SE, depth, *nest, names);
    return nest;
}

// ── Pass ──────────────────────────────────────────────────

struct LoopAnnotatedTracePass : public PassInfoMixin<LoopAnnotatedTracePass> {
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        auto& LI    = AM.getResult<LoopAnalysis>(F);
        auto& SE    = AM.getResult<ScalarEvolutionAnalysis>(F);
        NameMap names = buildDebugNameMap(F);

        lat::JsonExportVisitor vis;
        llvm::json::Array rootJson;
        for (Loop* L : LI) {
            if (getTripCount(L, SE) == 0) continue;
            auto nest = buildLoopNest(L, SE, 1, names);
            nest->accept(vis);
            rootJson.push_back(vis.getResult());
        }

        std::string filename = F.getName().str() + "_loop_annotated_trace.json";
        std::error_code EC;
        raw_fd_ostream OS(filename, EC, sys::fs::OF_Text);
        if (EC) {
            errs() << "[LoopAnnotatedTrace] cannot open " << filename << ": "
                   << EC.message() << "\n";
            return PreservedAnalyses::all();
        }
        OS << llvm::json::Value(std::move(rootJson));
        errs() << "[LoopAnnotatedTrace] wrote " << filename << "\n";
        return PreservedAnalyses::all();
    }
};

}  // namespace

llvm::PassPluginLibraryInfo getLoopAnnotatedTracePluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "LoopAnnotatedTrace", LLVM_VERSION_STRING,
            [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager& FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "loop-annotated-trace") {
                            FPM.addPass(LoopAnnotatedTracePass());
                            return true;
                        }
                        return false;
                    });
            }};
}

#ifndef LLVM_ATTRIBUTE_WEAK
#define LLVM_ATTRIBUTE_WEAK __attribute__((weak))
#endif
#ifndef LLVM_ATTRIBUTE_VISIBILITY_DEFAULT
#define LLVM_ATTRIBUTE_VISIBILITY_DEFAULT __attribute__((visibility("default")))
#endif

extern "C" LLVM_ATTRIBUTE_WEAK LLVM_ATTRIBUTE_VISIBILITY_DEFAULT
    ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getLoopAnnotatedTracePluginInfo();
}
