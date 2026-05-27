#include <vector>

#include <gtest/gtest.h>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include "../include/IrHelpers.hpp"

using namespace lat;
using namespace llvm;

TEST(ArrayMetadata, ExtractsNestedArrayShapeAndElementSize) {
    LLVMContext Ctx;
    Module M("test", Ctx);
    IRBuilder<> Builder(Ctx);
    Type* I32 = Type::getInt32Ty(Ctx);
    auto* RowTy = ArrayType::get(I32, 70);
    auto* MatrixTy = ArrayType::get(RowTy, 150);
    auto* PtrTy = PointerType::getUnqual(MatrixTy);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), {PtrTy}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    Builder.SetInsertPoint(BB);
    Argument* Arg = &*F->arg_begin();
    auto* Zero = ConstantInt::get(I32, 0);
    auto* One = ConstantInt::get(I32, 1);
    auto* Gep = cast<GetElementPtrInst>(
        Builder.CreateInBoundsGEP(MatrixTy, Arg, {Zero, One, Zero}));
    Builder.CreateRetVoid();

    ArrayMetadata metadata = getArrayMetadata(cast<GEPOperator>(Gep), M.getDataLayout());

    EXPECT_EQ(metadata.shape, (std::vector<int64_t>{150, 70}));
    EXPECT_EQ(metadata.elem_size, 4);
}

TEST(ArrayMetadata, ExtractsTrailingShapeForArrayPointerParam) {
    LLVMContext Ctx;
    Module M("test", Ctx);
    IRBuilder<> Builder(Ctx);
    Type* F64 = Type::getDoubleTy(Ctx);
    auto* RowTy = ArrayType::get(F64, 70);
    auto* PtrTy = PointerType::getUnqual(RowTy);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), {PtrTy}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    Builder.SetInsertPoint(BB);
    Argument* Arg = &*F->arg_begin();
    auto* I32 = Type::getInt32Ty(Ctx);
    auto* Zero = ConstantInt::get(I32, 0);
    auto* One = ConstantInt::get(I32, 1);
    auto* Gep = cast<GetElementPtrInst>(
        Builder.CreateInBoundsGEP(RowTy, Arg, {One, Zero}));
    Builder.CreateRetVoid();

    ArrayMetadata metadata = getArrayMetadata(cast<GEPOperator>(Gep), M.getDataLayout());

    EXPECT_EQ(metadata.shape, (std::vector<int64_t>{70}));
    EXPECT_EQ(metadata.elem_size, 8);
}
