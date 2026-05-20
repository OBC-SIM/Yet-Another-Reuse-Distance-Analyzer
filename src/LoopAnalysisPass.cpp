#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/JsonExportVisitor.hpp"
#include "../include/Statement.hpp"

using namespace llvm;
using namespace lat;

namespace {

using NameMap = std::unordered_map<const Value*, std::string>;

static NameMap buildDebugNameMap(Function& F) {
    NameMap names;
    for (BasicBlock& BB : F)
        for (Instruction& I : BB)
            if (auto* DVI = dyn_cast<DbgValueInst>(&I))
                if (Value* V = DVI->getValue())
                    if (DILocalVariable* Var = DVI->getVariable())
                        if (!Var->getName().empty())
                            names.emplace(V, Var->getName().str());
    return names;
}

// ── 헬퍼 함수 ─────────────────────────────────────────────

static std::string getInductionVarName(Loop* L, ScalarEvolution& SE,
                                        const NameMap& names) {
    auto lookup = [&](Value* V) -> std::string {
        auto it = names.find(V);
        if (it != names.end()) return it->second;
        return V->hasName() ? V->getName().str() : "";
    };

    if (PHINode* IV = L->getInductionVariable(SE)) {
        std::string n = lookup(IV);
        return n.empty() ? "iv" : n;
    }
    for (PHINode& PN : L->getHeader()->phis()) {
        if (SE.isSCEVable(PN.getType()) && isa<SCEVAddRecExpr>(SE.getSCEV(&PN))) {
            std::string n = lookup(&PN);
            return n.empty() ? "iv" : n;
        }
    }
    return "iv";
}

static int64_t getTripCount(Loop* L, ScalarEvolution& SE) {
    const SCEV* BTC = SE.getBackedgeTakenCount(L);
    if (auto* C = dyn_cast<SCEVConstant>(BTC))
        return C->getValue()->getSExtValue() + 1;
    return 0;
}

// 복합 SCEV에서 AddRecExpr 루프를 수집한다 (SCEVNAryExpr 계층 재귀)
static void collectAddRecLoops(const SCEV* S, std::vector<const Loop*>& out) {
    if (auto* AR = dyn_cast<SCEVAddRecExpr>(S)) {
        out.push_back(AR->getLoop());
        collectAddRecLoops(AR->getStart(), out);
        return;
    }
    // SCEVAddExpr, SCEVMulExpr, SCEVAddRecExpr 모두 SCEVNAryExpr를 상속
    if (auto* NAry = dyn_cast<SCEVNAryExpr>(S)) {
        for (const SCEV* Op : NAry->operands())
            collectAddRecLoops(Op, out);
    }
}

/**
 * @brief GEP 인덱스 하나를 루프 유도 변수 이름으로 변환한다.
 *
 * AddRecExpr이면 해당 루프의 IV 이름, 상수면 숫자 문자열(0은 생략),
 * 복합 SCEV면 포함된 모든 AddRec 루프의 IV 이름 목록을 반환한다.
 *
 * @param Idx  GEP 인덱스 피연산자
 * @param SE   ScalarEvolution 분석 결과
 * @return     식별된 인덱스 변수 이름 목록 (없으면 빈 벡터)
 */
static std::vector<std::string> resolveIndex(Value* Idx, ScalarEvolution& SE,
                                              const NameMap& names) {
    if (!SE.isSCEVable(Idx->getType())) return {"?"};

    const SCEV* S = SE.getSCEV(Idx);

    if (auto* C = dyn_cast<SCEVConstant>(S)) {
        int64_t val = C->getValue()->getSExtValue();
        if (val == 0) return {};  // 구조체/배열 첫 번째 오프셋 — 생략
        return {std::to_string(val)};
    }

    auto ivName = [&](const Loop* L) -> std::string {
        if (PHINode* IV = L->getInductionVariable(SE)) {
            auto it = names.find(IV);
            if (it != names.end()) return it->second;
            if (IV->hasName()) return IV->getName().str();
        }
        return "iv";
    };

    if (auto* AR = dyn_cast<SCEVAddRecExpr>(S))
        return {ivName(AR->getLoop())};

    // 복합 SCEV (e.g. i*N + j): AddRec 루프를 깊이 순으로 정렬
    std::vector<const Loop*> loops;
    collectAddRecLoops(S, loops);
    if (loops.empty()) return {"?"};

    std::sort(loops.begin(), loops.end(), [](const Loop* a, const Loop* b) {
        return a->getLoopDepth() < b->getLoopDepth();
    });
    loops.erase(std::unique(loops.begin(), loops.end()), loops.end());

    std::vector<std::string> result;
    for (const Loop* L : loops)
        result.push_back(ivName(L));
    return result;
}

static std::vector<std::string> getIndexVars(GetElementPtrInst* GEP,
                                              ScalarEvolution& SE,
                                              const NameMap& names) {
    std::vector<std::string> result;
    for (auto it = GEP->idx_begin(); it != GEP->idx_end(); ++it)
        for (auto& name : resolveIndex(*it, SE, names))
            result.push_back(std::move(name));
    return result;
}

static std::string getBaseName(Value* Ptr, const NameMap& names) {
    Value* Base = Ptr->stripPointerCasts();
    auto it = names.find(Base);
    if (it != names.end()) return it->second;
    if (Base->hasName()) return Base->getName().str();
    if (auto* Arg = dyn_cast<Argument>(Base))
        return "arg" + std::to_string(Arg->getArgNo());
    return "arr";
}

// ── 트리 빌더 ─────────────────────────────────────────────

static std::unique_ptr<lat::LoopNest> buildLoopNest(Loop* L, ScalarEvolution& SE,
                                                     unsigned depth,
                                                     const NameMap& names);

/**
 * @brief 루프 바디의 직접 Basic Block에서 메모리 접근 Statement를 수집한다.
 *
 * 서브루프에 속하는 BB는 건너뛰며, Load/Store에 연결된 GEP만 처리한다.
 * 서브루프 헤더 BB를 만나면 해당 서브루프를 자식 LoopNest로 삽입한다.
 *
 * @param L     현재 루프
 * @param SE    ScalarEvolution 분석 결과
 * @param depth 현재 루프 깊이
 * @param nest  Statement를 추가할 대상 LoopNest
 * @param names llvm.dbg.value에서 구축한 Value → 변수명 맵
 */
static void populateBody(Loop* L, ScalarEvolution& SE, unsigned depth,
                         lat::LoopNest& nest, const NameMap& names) {
    std::set<BasicBlock*> subLoopBlocks;
    std::map<BasicBlock*, Loop*> subLoopHeaders;
    for (Loop* Sub : L->getSubLoops()) {
        subLoopHeaders[Sub->getHeader()] = Sub;
        for (BasicBlock* BB : Sub->blocks())
            subLoopBlocks.insert(BB);
    }

    std::set<Loop*> processed;
    for (BasicBlock* BB : L->blocks()) {
        // 서브루프 헤더: 해당 서브루프를 자식으로 삽입
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
                                                     unsigned depth,
                                                     const NameMap& names) {
    std::string iv    = getInductionVarName(L, SE, names);
    int64_t     bound = getTripCount(L, SE);
    auto nest = std::make_unique<lat::LoopNest>(iv, bound, depth);
    populateBody(L, SE, depth, *nest, names);
    return nest;
}

// ── Pass ──────────────────────────────────────────────────

struct LoopAnnotatedTracePass : public PassInfoMixin<LoopAnnotatedTracePass> {
    PreservedAnalyses run(Function& F, FunctionAnalysisManager& AM) {
        auto& LI = AM.getResult<LoopAnalysis>(F);
        auto& SE = AM.getResult<ScalarEvolutionAnalysis>(F);

        NameMap names = buildDebugNameMap(F);

        llvm::json::Array loops;
        for (Loop* L : LI) {
            if (getTripCount(L, SE) == 0) continue;  // 상수 바운드가 아닌 루프 생략
            auto nest = buildLoopNest(L, SE, 1, names);
            JsonExportVisitor vis;
            nest->accept(vis);
            loops.push_back(vis.getResult());
        }

        std::string filename = F.getName().str() + "_loop_annotated_trace.json";
        std::error_code EC;
        raw_fd_ostream OS(filename, EC, sys::fs::OF_Text);
        if (EC) {
            errs() << "[LoopAnnotatedTrace] cannot open " << filename << ": "
                   << EC.message() << "\n";
            return PreservedAnalyses::all();
        }
        OS << llvm::json::Value(std::move(loops));
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
