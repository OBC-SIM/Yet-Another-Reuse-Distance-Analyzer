#include <gtest/gtest.h>
#include "llvm/Support/JSON.h"

#include "../include/JsonExportVisitor.hpp"
#include "../include/Statement.hpp"

using namespace lat;

// ── 헬퍼 ──────────────────────────────────────────────────

// json::Value는 반드시 수명이 보장된 lvalue에서 getAsObject()를 호출해야 한다.
// toObj()는 caller가 result를 lvalue로 보관한 뒤 사용하는 패턴을 강제한다.
static const llvm::json::Object* toObj(const llvm::json::Value& V) {
    return V.getAsObject();
}

// llvm::Optional<StringRef> → std::string 변환 헬퍼
static std::string str(llvm::Optional<llvm::StringRef> opt) {
    return opt ? opt->str() : "";
}

// llvm::Optional<int64_t> → int64_t 변환 헬퍼
static int64_t i64(llvm::Optional<int64_t> opt) {
    return opt.getValueOr(0);
}

// ============================================================
// ScalarAccess
// ============================================================

TEST(ScalarAccess, GetName) {
    ScalarAccess s("x");
    EXPECT_EQ(s.getName(), "x");
}

TEST(ScalarAccess, JsonFields) {
    ScalarAccess s("x");
    JsonExportVisitor vis;
    s.accept(vis);
    auto* obj = toObj(vis.getResult());
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(str(obj->getString("type")), "Scalar");
    EXPECT_EQ(str(obj->getString("name")), "x");
}

// ============================================================
// ArrayAccess
// ============================================================

TEST(ArrayAccess, Getters) {
    ArrayAccess a("arr", {"i", "j"});
    EXPECT_EQ(a.getArrayName(), "arr");
    ASSERT_EQ(a.getIndexVars().size(), 2u);
    EXPECT_EQ(a.getIndexVars()[0], "i");
    EXPECT_EQ(a.getIndexVars()[1], "j");
}

TEST(ArrayAccess, JsonFields) {
    ArrayAccess a("arr", {"i", "j"});
    JsonExportVisitor vis;
    a.accept(vis);
    auto* obj = toObj(vis.getResult());
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(str(obj->getString("type")), "Array");
    EXPECT_EQ(str(obj->getString("name")), "arr");
    auto* indices = obj->getArray("indices");
    ASSERT_NE(indices, nullptr);
    ASSERT_EQ(indices->size(), 2u);
    EXPECT_EQ(str((*indices)[0].getAsString()), "i");
    EXPECT_EQ(str((*indices)[1].getAsString()), "j");
}

TEST(ArrayAccess, JsonOneDimensional) {
    ArrayAccess a("vec", {"k"});
    JsonExportVisitor vis;
    a.accept(vis);
    auto* obj = toObj(vis.getResult());
    ASSERT_NE(obj, nullptr);
    auto* indices = obj->getArray("indices");
    ASSERT_NE(indices, nullptr);
    EXPECT_EQ(indices->size(), 1u);
}

// ============================================================
// LoopNest
// ============================================================

TEST(LoopNest, Getters) {
    LoopNest loop("i", 100, 1);
    EXPECT_EQ(loop.getInductionVar(), "i");
    EXPECT_EQ(loop.getBound(), 100);
    EXPECT_EQ(loop.getDepth(), 1u);
    EXPECT_TRUE(loop.getBody().empty());
}

TEST(LoopNest, AddChild) {
    LoopNest loop("i", 100, 1);
    loop.addChild(std::make_unique<ScalarAccess>("s"));
    loop.addChild(std::make_unique<ArrayAccess>("arr", std::vector<std::string>{"i"}));
    EXPECT_EQ(loop.getBody().size(), 2u);
}

TEST(LoopNest, JsonFlatLoop) {
    LoopNest loop("i", 100, 1);
    loop.addChild(std::make_unique<ArrayAccess>("arr", std::vector<std::string>{"i"}));

    JsonExportVisitor vis;
    loop.accept(vis);
    auto* obj = toObj(vis.getResult());
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(str(obj->getString("type")), "Loop");
    EXPECT_EQ(str(obj->getString("var")),  "i");
    EXPECT_EQ(i64(obj->getInteger("bound")), 100);
    EXPECT_EQ(i64(obj->getInteger("depth")), 1);
    auto* body = obj->getArray("body");
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->size(), 1u);
    EXPECT_EQ(str((*body)[0].getAsObject()->getString("type")), "Array");
}

// ── 2중 중첩 루프: for i { for j { arr[i][j] } } ──────────

TEST(LoopNest, JsonNestedLoop) {
    auto inner = std::make_unique<LoopNest>("j", 200, 2);
    inner->addChild(std::make_unique<ArrayAccess>("arr", std::vector<std::string>{"i", "j"}));

    LoopNest outer("i", 100, 1);
    outer.addChild(std::move(inner));

    JsonExportVisitor vis;
    outer.accept(vis);
    auto* outerObj = toObj(vis.getResult());
    ASSERT_NE(outerObj, nullptr);
    EXPECT_EQ(str(outerObj->getString("var")),    "i");
    EXPECT_EQ(i64(outerObj->getInteger("bound")), 100);

    auto* outerBody = outerObj->getArray("body");
    ASSERT_NE(outerBody, nullptr);
    ASSERT_EQ(outerBody->size(), 1u);

    auto* innerObj = (*outerBody)[0].getAsObject();
    ASSERT_NE(innerObj, nullptr);
    EXPECT_EQ(str(innerObj->getString("type")),   "Loop");
    EXPECT_EQ(str(innerObj->getString("var")),    "j");
    EXPECT_EQ(i64(innerObj->getInteger("bound")), 200);
    EXPECT_EQ(i64(innerObj->getInteger("depth")), 2);

    auto* innerBody = innerObj->getArray("body");
    ASSERT_NE(innerBody, nullptr);
    ASSERT_EQ(innerBody->size(), 1u);
    EXPECT_EQ(str((*innerBody)[0].getAsObject()->getString("type")), "Array");
}

TEST(LoopNest, EmptyBodyJson) {
    LoopNest loop("k", 50, 3);
    JsonExportVisitor vis;
    loop.accept(vis);
    auto* obj = toObj(vis.getResult());
    ASSERT_NE(obj, nullptr);
    auto* body = obj->getArray("body");
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
