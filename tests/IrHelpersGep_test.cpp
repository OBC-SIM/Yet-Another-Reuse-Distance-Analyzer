#include <vector>

#include <gtest/gtest.h>

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include "../include/IrHelpers.hpp"

using namespace lat;
using namespace llvm;

struct SEContext {
    DominatorTree DT;
    LoopInfo LI;
    AssumptionCache AC;
    TargetLibraryInfoImpl TLII;
    TargetLibraryInfo TLI;
    ScalarEvolution SE;

    explicit SEContext(Function& F)
        : DT(F), LI(DT), AC(F), TLI(TLII), SE(F, TLI, AC, DT, LI) {}
};

TEST(GetIndexVars, KeepsOuterIndexFromArrayPointerParamGepChain) {
    LLVMContext Ctx;
    Module M("test", Ctx);
    IRBuilder<> Builder(Ctx);
    auto* RowTy = ArrayType::get(Type::getFloatTy(Ctx), 64);
    auto* PtrTy = PointerType::getUnqual(RowTy);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), {PtrTy}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    Builder.SetInsertPoint(BasicBlock::Create(Ctx, "entry", F));
    auto* I32 = Type::getInt32Ty(Ctx);
    auto* Zero = ConstantInt::get(I32, 0);
    auto* One = ConstantInt::get(I32, 1);
    auto* Two = ConstantInt::get(I32, 2);
    auto* RowGep = cast<GetElementPtrInst>(
        Builder.CreateInBoundsGEP(RowTy, &*F->arg_begin(), {One}));
    auto* ElemGep = cast<GetElementPtrInst>(
        Builder.CreateInBoundsGEP(RowTy, RowGep, {Zero, Two}));
    Builder.CreateRetVoid();
    NameMap names;
    SEContext SECtx(*F);

    EXPECT_EQ(getIndexVars(cast<GEPOperator>(ElemGep), SECtx.SE, names),
              (std::vector<std::string>{"1", "2"}));
}

TEST(GetIndexVars, HandlesGlobalAndParamArrayAccessesTogether) {
    LLVMContext Ctx;
    Module M("test", Ctx);
    IRBuilder<> Builder(Ctx);
    auto* RowTy = ArrayType::get(Type::getFloatTy(Ctx), 32);
    auto* MatrixTy = ArrayType::get(RowTy, 16);
    auto* ParamTy = PointerType::getUnqual(RowTy);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), {ParamTy}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    Builder.SetInsertPoint(BasicBlock::Create(Ctx, "entry", F));
    auto* G = new GlobalVariable(M, MatrixTy, false, GlobalValue::ExternalLinkage,
                                 nullptr, "G");
    auto* I32 = Type::getInt32Ty(Ctx);
    auto* Zero = ConstantInt::get(I32, 0);
    auto* One = ConstantInt::get(I32, 1);
    auto* Two = ConstantInt::get(I32, 2);
    auto* GlobalGep = cast<GEPOperator>(
        Builder.CreateInBoundsGEP(MatrixTy, G, {Zero, One, Two}));
    auto* ParamRow = cast<GetElementPtrInst>(
        Builder.CreateInBoundsGEP(RowTy, &*F->arg_begin(), {One}));
    auto* ParamGep = cast<GetElementPtrInst>(
        Builder.CreateInBoundsGEP(RowTy, ParamRow, {Zero, Two}));
    Builder.CreateRetVoid();
    NameMap names;
    SEContext SECtx(*F);

    EXPECT_EQ(getIndexVars(GlobalGep, SECtx.SE, names),
              (std::vector<std::string>{"1", "2"}));
    EXPECT_EQ(getIndexVars(cast<GEPOperator>(ParamGep), SECtx.SE, names),
              (std::vector<std::string>{"1", "2"}));
}
