#include <algorithm>
#include <string>
#include <vector>

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/IrHelpers.hpp"

using namespace llvm;

namespace lat {

// NameMap·IR hasName 모두 실패 시 최후 식별자.
// Module 컨텍스트를 사용해 슬롯 번호를 포함한 피연산자 표현을 반환한다.
// 무명 로컬 → "3",  전역 변수 → "array",  무명 인자 → "0"
static std::string irOperandName(const Value* V) {
    const Module* M = nullptr;
    if (auto* I = dyn_cast<Instruction>(V))      M = I->getModule();
    else if (auto* A = dyn_cast<Argument>(V))    M = A->getParent()->getParent();
    else if (auto* G = dyn_cast<GlobalValue>(V)) M = G->getParent();

    std::string s;
    raw_string_ostream os(s);
    V->printAsOperand(os, /*PrintType=*/false, M);
    os.flush();
    if (!s.empty() && (s[0] == '%' || s[0] == '@'))
        s = s.substr(1);
    return s;
}

NameMap buildDebugNameMap(Function& F) {
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

std::string getInductionVarName(Loop* L, ScalarEvolution& SE, const NameMap& names) {
    auto lookup = [&](Value* V) -> std::string {
        auto it = names.find(V);
        if (it != names.end()) return it->second;
        if (V->hasName()) return V->getName().str();
        return irOperandName(V);  // IR 슬롯 번호로 구분 (e.g. "4")
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

int64_t getTripCount(Loop* L, ScalarEvolution& SE) {
    if (BasicBlock* H = L->getHeader()) {
        if (auto* Br = dyn_cast<BranchInst>(H->getTerminator())) {
            if (Br->isConditional()) {
                if (auto* Cmp = dyn_cast<ICmpInst>(Br->getCondition())) {
                    if (auto* C = dyn_cast<ConstantInt>(Cmp->getOperand(1))) {
                        int64_t bound = C->getSExtValue();
                        if (Cmp->getPredicate() == ICmpInst::ICMP_SLT ||
                            Cmp->getPredicate() == ICmpInst::ICMP_ULT)
                            return bound;
                        if (Cmp->getPredicate() == ICmpInst::ICMP_SLE ||
                            Cmp->getPredicate() == ICmpInst::ICMP_ULE)
                            return bound + 1;
                    }
                }
            }
        }
    }
    const SCEV* BTC = SE.getBackedgeTakenCount(L);
    if (auto* C = dyn_cast<SCEVConstant>(BTC))
        return C->getValue()->getSExtValue() + 1;
    return 0;
}

void collectAddRecLoops(const SCEV* S, std::vector<const Loop*>& out) {
    if (auto* AR = dyn_cast<SCEVAddRecExpr>(S)) {
        out.push_back(AR->getLoop());
        collectAddRecLoops(AR->getStart(), out);
        return;
    }
    if (auto* NAry = dyn_cast<SCEVNAryExpr>(S))
        for (const SCEV* Op : NAry->operands())
            collectAddRecLoops(Op, out);
}

std::vector<std::string> resolveIndex(Value* Idx, ScalarEvolution& SE,
                                      const NameMap& names) {
    if (!SE.isSCEVable(Idx->getType())) return {"?"};

    const SCEV* S = SE.getSCEV(Idx);

    if (auto* C = dyn_cast<SCEVConstant>(S))
        return {std::to_string(C->getValue()->getSExtValue())};

    auto ivName = [&](const Loop* L) -> std::string {
        auto tryValue = [&](Value* V) -> std::string {
            auto it = names.find(V);
            if (it != names.end()) return it->second;
            if (V->hasName()) return V->getName().str();
            return irOperandName(V);
        };
        if (PHINode* IV = L->getInductionVariable(SE)) {
            std::string n = tryValue(IV);
            if (!n.empty()) return n;
        }
        for (PHINode& PN : L->getHeader()->phis()) {
            if (SE.isSCEVable(PN.getType()) && isa<SCEVAddRecExpr>(SE.getSCEV(&PN))) {
                std::string n = tryValue(&PN);
                if (!n.empty()) return n;
            }
        }
        return "iv";
    };

    if (auto* AR = dyn_cast<SCEVAddRecExpr>(S))
        return {ivName(AR->getLoop())};

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

std::vector<std::string> getIndexVars(GEPOperator* GEP, ScalarEvolution& SE,
                                      const NameMap& names) {
    std::vector<std::string> result;
    if (auto* Parent = dyn_cast<GEPOperator>(GEP->getPointerOperand()->stripPointerCasts())) {
        auto parentIndices = getIndexVars(Parent, SE, names);
        result.insert(result.end(), parentIndices.begin(), parentIndices.end());
    }
    auto it = GEP->idx_begin();
    // [N x T]* 소스 타입이면 첫 번째 인덱스는 포인터 역참조(항상 0) — 스킵
    if (GEP->getSourceElementType()->isArrayTy())
        ++it;
    for (; it != GEP->idx_end(); ++it)
        for (auto& name : resolveIndex(*it, SE, names))
            result.push_back(std::move(name));
    return result;
}

std::string getBaseName(Value* Ptr, const NameMap& names) {
    Value* Base = Ptr->stripPointerCasts();
    while (auto* GEP = dyn_cast<GEPOperator>(Base))
        Base = GEP->getPointerOperand()->stripPointerCasts();
    auto it = names.find(Base);
    if (it != names.end()) return it->second;
    if (Base->hasName()) return Base->getName().str();
    if (auto* Arg = dyn_cast<Argument>(Base))
        return "arg" + std::to_string(Arg->getArgNo());
    std::string n = irOperandName(Base);
    return n.empty() ? "arr" : n;
}

}  // namespace lat
