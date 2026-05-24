#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
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
using lat::getLoopStart;
using lat::getIndexVars;
using lat::getBaseName;
using lat::getValueName;
using lat::hasFunctionAnnotation;

namespace {

constexpr llvm::StringLiteral AnalyzeAnnotation = "yard.analyze";

// ── 명령어 → Statement 변환 ───────────────────────────────

/**
 * @brief Load/Store 명령어 하나에서 Statement를 생성한다.
 *
 * GEP 있으면 ArrayAccess/ScalarAccess, GEP 없으면 Argument/Global/Alloca에
 * 한해 ScalarAccess로 처리한다. 해당 없으면 nullptr 반환.
 *
 * @param I      분석할 명령어
 * @param SE     ScalarEvolution 분석 결과
 * @param names  llvm.dbg.value 기반 Value → 변수명 맵
 */
static std::unique_ptr<Statement> makeAccessFromInstr(Instruction& I,
                                                       ScalarEvolution& SE,
                                                       const NameMap& names,
                                                       const std::set<const Function*>& annotated,
                                                       const Function& current) {
    if (auto* Call = dyn_cast<CallBase>(&I)) {
        if (Function* Callee = Call->getCalledFunction()) {
            if (Callee != &current && annotated.count(Callee)) {
                std::vector<std::string> args;
                for (Value* Arg : Call->args())
                    args.push_back(getValueName(Arg, names));
                return std::make_unique<lat::CallStmt>(Callee->getName().str(), args);
            }
        }
        return nullptr;
    }

    Value* ptr = nullptr;
    if (auto* Load  = dyn_cast<LoadInst>(&I))  ptr = Load->getPointerOperand();
    else if (auto* Store = dyn_cast<StoreInst>(&I)) ptr = Store->getPointerOperand();
    if (!ptr) return nullptr;

    // GetElementPtrInst(명령어)와 ConstantExpr GEP(전역 배열 상수 접근) 모두 처리
    if (auto* GEP = dyn_cast<GEPOperator>(ptr)) {
        auto indices = getIndexVars(GEP, SE, names);
        std::string base = getBaseName(GEP->getPointerOperand(), names);
        if (indices.empty()) return std::make_unique<ScalarAccess>(base);
        return std::make_unique<ArrayAccess>(base, indices);
    }

    // GEP 없는 직접 포인터 접근 (e.g. -O1에서 arr[0]이 base pointer로 최적화된 경우)
    Value* base = ptr->stripPointerCasts();
    if (!isa<Argument>(base) && !isa<GlobalVariable>(base) && !isa<AllocaInst>(base))
        return nullptr;
    std::string name = getBaseName(ptr, names);
    // base가 다른 GEP의 포인터 피연산자로도 쓰이면 배열의 index-0 접근으로 처리
    for (const User* U : base->users())
        if (isa<GetElementPtrInst>(U))
            return std::make_unique<ArrayAccess>(name, std::vector<std::string>{"0"});
    return std::make_unique<ScalarAccess>(name);
}

// ── 트리 빌더 ─────────────────────────────────────────────

static std::unique_ptr<lat::LoopNest> buildLoopNest(Loop* L, ScalarEvolution& SE,
                                                unsigned depth, const NameMap& names,
                                                const std::set<const Function*>& annotated,
                                                const Function& current);

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
                         lat::LoopNest& nest, const NameMap& names,
                         const std::set<const Function*>& annotated,
                         const Function& current) {
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
                nest.addChild(buildLoopNest(
                    hIt->second, SE, depth + 1, names, annotated, current));
            }
            continue;
        }
        if (subLoopBlocks.count(BB)) continue;

        for (Instruction& I : *BB)
            if (auto stmt = makeAccessFromInstr(I, SE, names, annotated, current))
                nest.addChild(std::move(stmt));
    }
}

static std::unique_ptr<lat::LoopNest> buildLoopNest(Loop* L, ScalarEvolution& SE,
                                                unsigned depth, const NameMap& names,
                                                const std::set<const Function*>& annotated,
                                                const Function& current) {
    auto nest = std::make_unique<lat::LoopNest>(getInductionVarName(L, SE, names),
                                           getLoopStart(L, SE), getTripCount(L, SE), depth);
    populateBody(L, SE, depth, *nest, names, annotated, current);
    return nest;
}

// ── 루트 Statement 빌더 ───────────────────────────────────

/**
 * @brief 함수 전체 BB를 RPO(Reverse Post-Order)로 순회해 최상위 Statement 목록을 구성한다.
 *
 * RPO는 CFG 흐름 순서를 따르므로 루프 전/후 코드가 올바른 순서로 출력된다.
 * 최상위 루프 헤더 BB → LoopNest, 루프 외부 BB → Scalar/Array 노드 삽입.
 * 루프 내부 BB는 buildLoopNest가 처리하므로 건너뛴다.
 *
 * @param F     분석 대상 함수
 * @param LI    LoopInfo 분석 결과
 * @param SE    ScalarEvolution 분석 결과
 * @param names llvm.dbg.value 기반 Value → 변수명 맵
 * @param root  결과를 추가할 최상위 Statement 벡터
 */
static void buildRootStatements(Function& F, LoopInfo& LI, ScalarEvolution& SE,
                                 const NameMap& names,
                                 const std::set<const Function*>& annotated,
                                 std::vector<std::unique_ptr<Statement>>& root) {
    std::map<BasicBlock*, Loop*> topLoopHeaders;
    std::set<BasicBlock*>        topLoopBlocks;
    for (Loop* L : LI) {
        if (getTripCount(L, SE) == 0) continue;
        topLoopHeaders[L->getHeader()] = L;
        for (BasicBlock* BB : L->blocks())
            topLoopBlocks.insert(BB);
    }

    std::set<Loop*> processed;
    for (BasicBlock* BB : llvm::ReversePostOrderTraversal<Function*>(&F)) {
        auto hIt = topLoopHeaders.find(BB);
        if (hIt != topLoopHeaders.end()) {
            if (!processed.count(hIt->second)) {
                processed.insert(hIt->second);
                root.push_back(buildLoopNest(hIt->second, SE, 1, names, annotated, F));
            }
            continue;
        }
        if (topLoopBlocks.count(BB)) continue;

        for (Instruction& I : *BB)
            if (auto stmt = makeAccessFromInstr(I, SE, names, annotated, F))
                root.push_back(std::move(stmt));
    }
}

// ── Pass ──────────────────────────────────────────────────

struct LoopAnnotatedTracePass : public PassInfoMixin<LoopAnnotatedTracePass> {
    PreservedAnalyses run(Module& M, ModuleAnalysisManager& MAM) {
        auto& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
        std::set<const Function*> annotated;
        for (Function& F : M)
            if (!F.isDeclaration() && hasFunctionAnnotation(F, AnalyzeAnnotation))
                annotated.insert(&F);

        llvm::json::Array moduleFuncs;
        for (Function& F : M) {
            if (F.isDeclaration()) continue;
            if (!annotated.empty() && !annotated.count(&F))
                continue;

            auto& LI  = FAM.getResult<LoopAnalysis>(F);
            auto& SE  = FAM.getResult<ScalarEvolutionAnalysis>(F);
            NameMap names = buildDebugNameMap(F);

            std::vector<std::unique_ptr<Statement>> root;
            buildRootStatements(F, LI, SE, names, annotated, root);

            lat::JsonExportVisitor vis;
            llvm::json::Array bodyJson;
            for (auto& stmt : root) {
                stmt->accept(vis);
                bodyJson.push_back(vis.getResult());
            }

            llvm::json::Object funcEntry;
            funcEntry["function"] = F.getName().str();
            funcEntry["body"]     = std::move(bodyJson);
            moduleFuncs.push_back(std::move(funcEntry));
        }

        llvm::StringRef stem = llvm::sys::path::stem(M.getModuleIdentifier());
        std::string filename = stem.str() + "_lat.json";
        std::error_code EC;
        raw_fd_ostream OS(filename, EC, sys::fs::OF_Text);
        if (EC) {
            errs() << "[LoopAnnotatedTrace] cannot open " << filename << ": "
                   << EC.message() << "\n";
            return PreservedAnalyses::all();
        }
        OS << llvm::json::Value(std::move(moduleFuncs));
        errs() << "[LoopAnnotatedTrace] wrote " << filename << "\n";
        return PreservedAnalyses::all();
    }
};

}  // namespace

llvm::PassPluginLibraryInfo getLoopAnnotatedTracePluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "LoopAnnotatedTrace", LLVM_VERSION_STRING,
            [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, ModulePassManager& MPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "loop-annotated-trace") {
                            MPM.addPass(LoopAnnotatedTracePass());
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
