#include "../include/AccessBuilder.hpp"

#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Operator.h"

using namespace llvm;

namespace lat {

static std::unique_ptr<Statement> makeInlineCall(
    CallBase& Call,
    const NameMap& names,
    const std::set<const Function*>& inlineFuncs,
    const Function& current) {
    Function* Callee = Call.getCalledFunction();
    if (!Callee || Callee == &current || !inlineFuncs.count(Callee))
        return nullptr;

    std::vector<std::string> args;
    for (Value* Arg : Call.args())
        args.push_back(getValueName(Arg, names));
    return std::make_unique<CallStmt>(Callee->getName().str(), args);
}

static Value* loadStorePointer(Instruction& I, std::string& op) {
    if (auto* Load = dyn_cast<LoadInst>(&I)) {
        op = "load";
        return Load->getPointerOperand();
    }
    if (auto* Store = dyn_cast<StoreInst>(&I)) {
        op = "store";
        return Store->getPointerOperand();
    }
    return nullptr;
}

static std::unique_ptr<Statement> makeArrayOrScalar(
    Value* ptr,
    Instruction& I,
    ScalarEvolution& SE,
    const NameMap& names,
    std::string op) {
    if (auto* GEP = dyn_cast<GEPOperator>(ptr)) {
        auto indices = getIndexVars(GEP, SE, names);
        std::string base = getBaseName(GEP->getPointerOperand(), names);
        if (indices.empty())
            return std::make_unique<ScalarAccess>(base, std::move(op));
        return std::make_unique<ArrayAccess>(
            base,
            indices,
            getArrayMetadata(GEP, I.getModule()->getDataLayout()),
            std::move(op));
    }

    Value* base = ptr->stripPointerCasts();
    if (!isa<Argument>(base) && !isa<GlobalVariable>(base) && !isa<AllocaInst>(base))
        return nullptr;

    std::string name = getBaseName(ptr, names);
    for (const User* U : base->users()) {
        if (isa<GetElementPtrInst>(U))
            return std::make_unique<ArrayAccess>(
                name, std::vector<std::string>{"0"}, std::move(op));
    }
    return std::make_unique<ScalarAccess>(name, std::move(op));
}

std::unique_ptr<Statement> makeAccessFromInstr(
    Instruction& I,
    ScalarEvolution& SE,
    const NameMap& names,
    const std::set<const Function*>& inlineFuncs,
    const Function& current) {
    if (auto* Call = dyn_cast<CallBase>(&I))
        return makeInlineCall(*Call, names, inlineFuncs, current);

    std::string op;
    Value* ptr = loadStorePointer(I, op);
    if (!ptr)
        return nullptr;

    return makeArrayOrScalar(ptr, I, SE, names, std::move(op));
}

}  // namespace lat
