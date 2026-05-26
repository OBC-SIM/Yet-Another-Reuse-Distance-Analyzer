#pragma once

#include "Statement.hpp"
#include "llvm/Support/JSON.h"

namespace lat {

/**
 * @brief Loop Annotated Tree를 llvm::json::Value로 직렬화하는 Visitor.
 *
 * 사용 예:
 *   JsonExportVisitor v;
 *   root.accept(v);
 *   llvm::json::Value result = v.getResult();
 */
class JsonExportVisitor : public Visitor {
public:
    const llvm::json::Value& getResult() const { return Result_; }

    void visit(ScalarAccess& node) override {
        Result_ = llvm::json::Object{
            {"type", "Scalar"},
            {"name", node.getName()}
        };
    }

    void visit(ArrayAccess& node) override {
        llvm::json::Array indices;
        for (const auto& idx : node.getIndexVars())
            indices.push_back(idx);
        llvm::json::Object obj{
            {"type",    "Array"},
            {"name",    node.getArrayName()},
            {"indices", std::move(indices)}
        };
        const auto& metadata = node.getMetadata();
        if (!metadata.shape.empty()) {
            llvm::json::Array shape;
            for (int64_t dim : metadata.shape)
                shape.push_back(dim);
            obj["shape"] = std::move(shape);
        }
        if (metadata.elem_size > 0)
            obj["elem_size"] = metadata.elem_size;
        Result_ = std::move(obj);
    }

    void visit(CallStmt& node) override {
        llvm::json::Array args;
        for (const auto& arg : node.getArgs())
            args.push_back(arg);
        Result_ = llvm::json::Object{
            {"type",   "Call"},
            {"callee", node.getCallee()},
            {"args",   std::move(args)}
        };
    }

    void visit(LoopNest& node) override {
        llvm::json::Array body;
        for (const auto& child : node.getBody()) {
            child->accept(*this);
            body.push_back(getResult());
        }
        Result_ = llvm::json::Object{
            {"type",  "Loop"},
            {"var",   node.getInductionVar()},
            {"start", node.getStart()},
            {"bound", node.getBound()},
            {"depth", static_cast<int64_t>(node.getDepth())},
            {"body",  std::move(body)}
        };
    }

private:
    llvm::json::Value Result_ = nullptr;
};

}  // namespace lat
