#include <algorithm>
#include <string>
#include <vector>

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
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
    for (BasicBlock& BB : F) {
        for (Instruction& I : BB) {
            if (auto* DVI = dyn_cast<DbgValueInst>(&I))
                if (Value* V = DVI->getValue())
                    if (DILocalVariable* Var = DVI->getVariable())
                        if (!Var->getName().empty())
                            names.emplace(V, Var->getName().str());
            if (auto* DDI = dyn_cast<DbgDeclareInst>(&I))
                if (Value* V = DDI->getAddress())
                    if (DILocalVariable* Var = DDI->getVariable())
                        if (!Var->getName().empty())
                            names.emplace(V->stripPointerCasts(), Var->getName().str());
        }
    }
    return names;
}

static std::string scalarDebugName(Value* V, const NameMap& names) {
    if (auto it = names.find(V); it != names.end())
        return it->second;
    if (auto* Cast = dyn_cast<CastInst>(V))
        return scalarDebugName(Cast->getOperand(0), names);
    if (auto* Load = dyn_cast<LoadInst>(V)) {
        Value* Ptr = Load->getPointerOperand()->stripPointerCasts();
        if (auto it = names.find(Ptr); it != names.end())
            return it->second;
    }
    return "";
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

int64_t getLoopStart(Loop* L, ScalarEvolution& SE) {
    if (PHINode* IV = L->getInductionVariable(SE)) {
        if (auto* AR = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(IV))) {
            if (auto* C = dyn_cast<SCEVConstant>(AR->getStart()))
                return C->getValue()->getSExtValue();
        }
    }
    for (PHINode& PN : L->getHeader()->phis()) {
        if (!SE.isSCEVable(PN.getType())) continue;
        if (auto* AR = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(&PN))) {
            if (auto* C = dyn_cast<SCEVConstant>(AR->getStart()))
                return C->getValue()->getSExtValue();
        }
    }
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
    if (std::string name = scalarDebugName(Idx, names); !name.empty())
        return {name};

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

    auto formatAffine = [&](const Loop* L, int64_t offset) -> std::string {
        std::string name = ivName(L);
        if (offset == 0) return name;
        if (offset > 0) return name + "+" + std::to_string(offset);
        return name + std::to_string(offset);
    };

    if (auto* AR = dyn_cast<SCEVAddRecExpr>(S)) {
        int64_t offset = 0;
        if (auto* C = dyn_cast<SCEVConstant>(AR->getStart()))
            offset = C->getValue()->getSExtValue() - getLoopStart(const_cast<Loop*>(AR->getLoop()), SE);
        return {formatAffine(AR->getLoop(), offset)};
    }

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
    // multi-index GEP의 leading zero만 포인터 역참조로 보고 스킵한다.
    if (GEP->getNumIndices() > 1 && isa<ConstantInt>(*it) &&
        cast<ConstantInt>(*it)->isZero())
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

std::string getValueName(Value* V, const NameMap& names) {
    if (auto* C = dyn_cast<ConstantInt>(V))
        return std::to_string(C->getSExtValue());
    if (V->getType()->isPointerTy())
        return getBaseName(V, names);

    auto it = names.find(V);
    if (it != names.end()) return it->second;
    if (V->hasName()) return V->getName().str();
    std::string n = irOperandName(V);
    return n.empty() ? "value" : n;
}

static const GlobalVariable* globalFromStringPointer(const Value* V) {
    V = V->stripPointerCasts();
    if (auto* GV = dyn_cast<GlobalVariable>(V))
        return GV;
    if (auto* CE = dyn_cast<ConstantExpr>(V)) {
        if (CE->getOpcode() == Instruction::GetElementPtr && CE->getNumOperands() > 0)
            return dyn_cast<GlobalVariable>(CE->getOperand(0)->stripPointerCasts());
    }
    return nullptr;
}

static std::string annotationString(const Value* V) {
    const GlobalVariable* GV = globalFromStringPointer(V);
    if (!GV || !GV->hasInitializer())
        return "";
    if (auto* Data = dyn_cast<ConstantDataArray>(GV->getInitializer()))
        if (Data->isCString())
            return Data->getAsCString().str();
    return "";
}

bool hasFunctionAnnotation(Function& F, StringRef Annotation) {
    GlobalVariable* Annos = F.getParent()->getGlobalVariable("llvm.global.annotations");
    if (!Annos || !Annos->hasInitializer())
        return false;

    auto* Entries = dyn_cast<ConstantArray>(Annos->getInitializer());
    if (!Entries)
        return false;

    for (const Use& U : Entries->operands()) {
        auto* Entry = dyn_cast<ConstantStruct>(U.get());
        if (!Entry || Entry->getNumOperands() < 2)
            continue;

        Value* Annotated = Entry->getOperand(0)->stripPointerCasts();
        if (Annotated != &F)
            continue;
        if (annotationString(Entry->getOperand(1)) == Annotation)
            return true;
    }
    return false;
}

}  // namespace lat
