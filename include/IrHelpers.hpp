#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"

namespace lat {

/// Value* → 원본 소스 변수명 맵 (llvm.dbg.value intrinsic 기반)
using NameMap = std::unordered_map<const llvm::Value*, std::string>;

/**
 * @brief 함수 내 llvm.dbg.value intrinsic을 스캔해 Value → 변수명 맵을 구성한다.
 *
 * -g 없이 컴파일된 IR에서는 빈 맵을 반환하며, 호출자는 fallback을 처리해야 한다.
 *
 * @param F  분석 대상 함수
 * @return   Value* → 변수명 맵
 */
NameMap buildDebugNameMap(llvm::Function& F);

/**
 * @brief 루프의 유도 변수 이름을 반환한다.
 *
 * NameMap에 이름이 없으면 IR 이름을 시도하고, 그것도 없으면 "iv"를 반환한다.
 *
 * @param L      대상 루프
 * @param SE     ScalarEvolution 분석 결과
 * @param names  buildDebugNameMap 결과
 */
std::string getInductionVarName(llvm::Loop* L, llvm::ScalarEvolution& SE,
                                const NameMap& names);

/**
 * @brief 루프의 상수 trip count를 반환한다.
 *
 * @return 상수 바운드이면 trip count, 동적 바운드이면 0
 */
int64_t getTripCount(llvm::Loop* L, llvm::ScalarEvolution& SE);

/// 복합 SCEV에서 SCEVAddRecExpr 루프를 재귀적으로 수집한다.
void collectAddRecLoops(const llvm::SCEV* S, std::vector<const llvm::Loop*>& out);

/**
 * @brief GEP 인덱스 하나를 루프 유도 변수 이름 목록으로 변환한다.
 *
 * 상수 0은 빈 벡터(생략), 단일 AddRec은 해당 IV 이름,
 * 복합 SCEV는 포함된 모든 AddRec 루프의 IV 이름 목록을 반환한다.
 *
 * @param Idx    GEP 인덱스 피연산자
 * @param SE     ScalarEvolution 분석 결과
 * @param names  buildDebugNameMap 결과
 */
std::vector<std::string> resolveIndex(llvm::Value* Idx, llvm::ScalarEvolution& SE,
                                      const NameMap& names);

/**
 * @brief GEP 연산의 모든 인덱스를 변수 이름 목록으로 변환한다.
 *
 * GetElementPtrInst(명령어 GEP)와 ConstantExpr GEP(전역 배열 상수 접근)를
 * 모두 처리하기 위해 GEPOperator를 인자로 받는다.
 * 소스 타입이 배열([N x T]*)이면 첫 번째 인덱스(포인터 역참조 0)를 건너뛴다.
 *
 * @param GEP    분석할 GEPOperator (GetElementPtrInst 또는 ConstantExpr GEP)
 * @param SE     ScalarEvolution 분석 결과
 * @param names  buildDebugNameMap 결과
 */
std::vector<std::string> getIndexVars(llvm::GEPOperator* GEP,
                                      llvm::ScalarEvolution& SE,
                                      const NameMap& names);

/**
 * @brief 포인터 피연산자에서 배열/변수의 기반 이름을 추출한다.
 *
 * NameMap → IR 이름 → "argN" → "arr" 순으로 fallback한다.
 *
 * @param Ptr    GEP의 포인터 피연산자
 * @param names  buildDebugNameMap 결과
 */
std::string getBaseName(llvm::Value* Ptr, const NameMap& names);

}  // namespace lat