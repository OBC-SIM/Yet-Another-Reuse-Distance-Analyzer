#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ArrayMetadata.hpp"

namespace lat {

class ScalarAccess;
class ArrayAccess;
class CallStmt;
class LoopNest;

// ── Visitor 추상 클래스 ─────────────────────────────────────
class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(ScalarAccess& node) = 0;
    virtual void visit(ArrayAccess&  node) = 0;
    virtual void visit(CallStmt&     node) = 0;
    virtual void visit(LoopNest&     node) = 0;
};

// ── Statement: 추상 기반 클래스 ────────────────────────────
class Statement {
public:
    virtual ~Statement() = default;
    virtual void accept(Visitor& v) = 0;
};

// ── ScalarAccess: 단말 노드 ────────────────────────────────
/**
 * @brief 루프 인덱스와 무관한 스칼라 메모리 접근을 나타낸다.
 *
 * @param name 접근 대상 변수 이름 (LLVM Value 이름에서 추출)
 */
class ScalarAccess : public Statement {
public:
    explicit ScalarAccess(std::string name) : name_(std::move(name)) {}
    void accept(Visitor& v) override { v.visit(*this); }
    const std::string& getName() const { return name_; }
private:
    std::string name_;
};

// ── ArrayAccess: 단말 노드 ─────────────────────────────────
/**
 * @brief GEP 명령어에서 추출된 배열 접근을 나타낸다.
 *
 * @param array_name  배열 베이스 포인터 이름
 * @param index_vars  각 차원의 인덱스 변수 이름 목록 (루프 IV 기반)
 */
class ArrayAccess : public Statement {
public:
    ArrayAccess(std::string array_name, std::vector<std::string> index_vars)
        : array_name_(std::move(array_name)), index_vars_(std::move(index_vars)) {}
    ArrayAccess(std::string array_name, std::vector<std::string> index_vars,
                ArrayMetadata metadata)
        : array_name_(std::move(array_name)), index_vars_(std::move(index_vars)),
          metadata_(std::move(metadata)) {}
    void accept(Visitor& v) override { v.visit(*this); }
    const std::string& getArrayName()               const { return array_name_; }
    const std::vector<std::string>& getIndexVars()  const { return index_vars_; }
    const ArrayMetadata& getMetadata()              const { return metadata_; }
private:
    std::string              array_name_;
    std::vector<std::string> index_vars_;
    ArrayMetadata            metadata_;
};

// ── CallStmt: 단말 노드 ───────────────────────────────────
/**
 * @brief 직접 함수 호출 위치를 나타낸다.
 *
 * @param callee 호출 대상 함수 이름
 * @param args   호출 인자 이름 목록
 */
class CallStmt : public Statement {
public:
    CallStmt(std::string callee, std::vector<std::string> args)
        : callee_(std::move(callee)), args_(std::move(args)) {}
    void accept(Visitor& v) override { v.visit(*this); }
    const std::string& getCallee() const { return callee_; }
    const std::vector<std::string>& getArgs() const { return args_; }
private:
    std::string callee_;
    std::vector<std::string> args_;
};

// ── LoopNest: 내부 노드 ────────────────────────────────────
/**
 * @brief LoopInfo + SCEV에서 추출한 루프 하나를 나타내는 내부 노드.
 *
 * @param induction_var  유도 변수 이름
 * @param start          루프 시작 값 (컴파일 타임 상수)
 * @param bound          루프 종료 값 (exclusive, 컴파일 타임 상수)
 * @param depth          루프 중첩 깊이 (1-based)
 */
class LoopNest : public Statement {
public:
    LoopNest(std::string induction_var, int64_t start, int64_t bound, unsigned depth)
        : induction_var_(std::move(induction_var)), start_(start), bound_(bound), depth_(depth) {}

    void accept(Visitor& v) override { v.visit(*this); }

    /**
     * @brief 자식 Statement를 body에 추가한다.
     * @param child  이동할 Statement (소유권 이전)
     */
    void addChild(std::unique_ptr<Statement> child) {
        body_.push_back(std::move(child));
    }

    const std::string&                             getInductionVar() const { return induction_var_; }
    int64_t                                        getStart()        const { return start_; }
    int64_t                                        getBound()        const { return bound_; }
    unsigned                                       getDepth()        const { return depth_; }
    const std::vector<std::unique_ptr<Statement>>& getBody()         const { return body_; }

private:
    std::string                            induction_var_;
    int64_t                                start_;
    int64_t                                bound_;
    unsigned                               depth_;
    std::vector<std::unique_ptr<Statement>> body_;
};

}  // namespace lat
