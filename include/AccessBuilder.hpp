#pragma once

#include <memory>
#include <set>

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

#include "IrHelpers.hpp"
#include "Statement.hpp"

namespace lat {

/**
 * @brief LLVM instruction 하나를 LAT access/call Statement로 변환한다.
 *
 * Load/Store 접근은 op 필드에 각각 "load"/"store" 계약을 보존한다.
 * `yard.inline` 함수로의 direct call은 CallStmt로 보존하고, 그 외
 * 명령은 분석 대상이 아니면 nullptr을 반환한다.
 *
 * @param I           변환할 LLVM instruction
 * @param SE          ScalarEvolution 분석 결과
 * @param names       LLVM value를 source-level 이름으로 복원하는 맵
 * @param inlineFuncs `yard.inline` annotation이 붙은 함수 집합
 * @param current     현재 분석 중인 함수
 * @return 변환된 Statement. 분석 대상이 아니면 nullptr
 */
std::unique_ptr<Statement> makeAccessFromInstr(
    llvm::Instruction& I,
    llvm::ScalarEvolution& SE,
    const NameMap& names,
    const std::set<const llvm::Function*>& inlineFuncs,
    const llvm::Function& current);

}  // namespace lat
