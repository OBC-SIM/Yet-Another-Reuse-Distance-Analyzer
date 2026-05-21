#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include "../include/IrHelpers.hpp"

using namespace lat;
using namespace llvm;

// ── getBaseName ───────────────────────────────────────────────

TEST(GetBaseName, UnnamedAllocasAreDistinct) {
    // irOperandName fallback 회귀 테스트:
    // 무명 alloca 두 개가 서로 다른 이름을 받아야 한다
    LLVMContext Ctx;
    Module M("test", Ctx);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    IRBuilder<> Builder(BB);

    AllocaInst* A0 = Builder.CreateAlloca(ArrayType::get(Type::getInt32Ty(Ctx), 100));
    AllocaInst* A1 = Builder.CreateAlloca(ArrayType::get(Type::getInt32Ty(Ctx), 200));
    Builder.CreateRetVoid();

    NameMap names;
    std::string n0 = getBaseName(A0, names);
    std::string n1 = getBaseName(A1, names);

    EXPECT_FALSE(n0.empty());
    EXPECT_FALSE(n1.empty());
    EXPECT_NE(n0, n1);
}

TEST(GetBaseName, ManyUnnamedAllocsAllDistinct) {
    // N개의 무명 배열이 모두 유일한 이름을 받아야 한다
    LLVMContext Ctx;
    Module M("test", Ctx);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    IRBuilder<> Builder(BB);

    std::vector<AllocaInst*> allocs;
    for (int i = 0; i < 4; ++i)
        allocs.push_back(Builder.CreateAlloca(Type::getFloatTy(Ctx)));
    Builder.CreateRetVoid();

    NameMap names;
    std::set<std::string> seen;
    for (auto* A : allocs) {
        std::string n = getBaseName(A, names);
        EXPECT_FALSE(n.empty());
        EXPECT_TRUE(seen.insert(n).second) << "Duplicate name: " << n;
    }
}

TEST(GetBaseName, NamedAllocaUsesIRName) {
    // IR 이름이 있으면 irOperandName fallback 없이 그 이름을 반환해야 한다
    LLVMContext Ctx;
    Module M("test", Ctx);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    IRBuilder<> Builder(BB);

    AllocaInst* A = Builder.CreateAlloca(Type::getInt32Ty(Ctx), nullptr, "myArr");
    Builder.CreateRetVoid();

    NameMap names;
    EXPECT_EQ(getBaseName(A, names), "myArr");
}

TEST(GetBaseName, NameMapTakesPriorityOverIRName) {
    // NameMap에 등록된 이름이 IR 이름보다 우선해야 한다
    LLVMContext Ctx;
    Module M("test", Ctx);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    IRBuilder<> Builder(BB);

    AllocaInst* A = Builder.CreateAlloca(Type::getInt32Ty(Ctx), nullptr, "irName");
    Builder.CreateRetVoid();

    NameMap names;
    names[A] = "debugName";
    EXPECT_EQ(getBaseName(A, names), "debugName");
}

TEST(GetBaseName, UnnamedArgumentsUseArgN) {
    // 무명 인자는 "argN" 형식을 유지해야 한다 (irOperandName와 충돌하지 않음)
    LLVMContext Ctx;
    Module M("test", Ctx);
    Type* I32Ptr = Type::getInt32PtrTy(Ctx);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), {I32Ptr, I32Ptr}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);

    NameMap names;
    auto it = F->arg_begin();
    EXPECT_EQ(getBaseName(&*it, names), "arg0");
    ++it;
    EXPECT_EQ(getBaseName(&*it, names), "arg1");
}

TEST(GetBaseName, UnnamedAllocAndArgDoNotCollide) {
    // 무명 alloca의 슬롯 번호("2")와 "arg0"/"arg1"은 충돌하지 않아야 한다.
    // 인자가 슬롯 0·1을 선점하므로 첫 alloca 슬롯은 2 이상이다.
    LLVMContext Ctx;
    Module M("test", Ctx);
    Type* I32Ptr = Type::getInt32PtrTy(Ctx);
    FunctionType* FT = FunctionType::get(Type::getVoidTy(Ctx), {I32Ptr, I32Ptr}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "foo", &M);
    BasicBlock* BB = BasicBlock::Create(Ctx, "entry", F);
    IRBuilder<> Builder(BB);

    AllocaInst* A = Builder.CreateAlloca(Type::getInt32Ty(Ctx));
    Builder.CreateRetVoid();

    NameMap names;
    auto it = F->arg_begin();
    std::string arg0 = getBaseName(&*it, names);  // "arg0"
    std::string alloc = getBaseName(A, names);     // IR 슬롯 번호 (≥ "2")

    EXPECT_NE(arg0, alloc);
}