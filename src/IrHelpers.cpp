#include <algorithm>
#include <string>
#include <vector>

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Operator.h"

#include "../include/IrHelpers.hpp"

using namespace llvm;

namespace lat {

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

int64_t getTripCount(Loop* L, ScalarEvolution& SE) {
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
        if (PHINode* IV = L->getInductionVariable(SE)) {
            auto it = names.find(IV);
            if (it != names.end()) return it->second;
            if (IV->hasName()) return IV->getName().str();
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
    auto it = names.find(Base);
    if (it != names.end()) return it->second;
    if (Base->hasName()) return Base->getName().str();
    if (auto* Arg = dyn_cast<Argument>(Base))
        return "arg" + std::to_string(Arg->getArgNo());
    return "arr";
}

}  // namespace lat