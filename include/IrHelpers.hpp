#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/DataLayout.h"

#include "ArrayMetadata.hpp"

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

/**
 * @brief 루프의 상수 시작 값을 반환한다.
 *
 * @param L  대상 루프
 * @param SE ScalarEvolution 분석 결과
 * @return 상수 시작 값이면 해당 값, 동적 시작 값이면 0
 */
int64_t getLoopStart(llvm::Loop* L, llvm::ScalarEvolution& SE);

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
 * NameMap → IR 이름 → "argN" → IR 슬롯 번호 순으로 fallback한다.
 * 무명 값도 슬롯 번호(e.g. "3")로 유일하게 식별된다.
 *
 * @param Ptr    GEP의 포인터 피연산자
 * @param names  buildDebugNameMap 결과
 */
std::string getBaseName(llvm::Value* Ptr, const NameMap& names);

/**
 * @brief 일반 LLVM Value를 LAT JSON에 기록할 이름으로 변환한다.
 *
 * 포인터 값은 배열/스칼라 base 이름으로, 정수 상수는 숫자 문자열로,
 * 그 외 값은 debug name → IR name → IR 슬롯 번호 순으로 변환한다.
 *
 * @param V      변환할 LLVM 값
 * @param names  buildDebugNameMap 결과
 */
std::string getValueName(llvm::Value* V, const NameMap& names);

/**
 * @brief 함수가 clang annotate attribute로 지정된 annotation을 갖는지 확인한다.
 *
 * Clang은 `__attribute__((annotate("...")))` 정보를
 * `@llvm.global.annotations` 전역에 기록한다. 이 helper는 해당 전역을
 * 해석해 지정된 함수와 annotation 문자열의 매칭 여부를 반환한다.
 *
 * @param F          검사할 함수
 * @param Annotation 찾을 annotation 문자열
 * @return annotation이 존재하면 true
 */
bool hasFunctionAnnotation(llvm::Function& F, llvm::StringRef Annotation);

/**
 * @brief GEP source type에서 배열 shape와 element byte size를 추출한다.
 *
 * @param GEP 분석할 GEPOperator
 * @param DL  모듈 DataLayout
 * @return 추론 가능한 배열 metadata. shape를 모르면 비워두고 elem_size만 반환할 수 있다.
 */
ArrayMetadata getArrayMetadata(llvm::GEPOperator* GEP, const llvm::DataLayout& DL);

}  // namespace lat
