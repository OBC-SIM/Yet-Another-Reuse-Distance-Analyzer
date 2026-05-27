#include "../include/IrHelpers.hpp"

#include "llvm/IR/DerivedTypes.h"

using namespace llvm;

namespace lat {

static Type* collectArrayShape(Type* Ty, std::vector<int64_t>& shape) {
    while (auto* ArrayTy = dyn_cast<ArrayType>(Ty)) {
        shape.push_back(static_cast<int64_t>(ArrayTy->getNumElements()));
        Ty = ArrayTy->getElementType();
    }
    return Ty;
}

ArrayMetadata getArrayMetadata(GEPOperator* GEP, const DataLayout& DL) {
    ArrayMetadata metadata;
    Type* ElemTy = collectArrayShape(GEP->getSourceElementType(), metadata.shape);
    if (!ElemTy || ElemTy->isVoidTy() || ElemTy->isFunctionTy())
        return metadata;
    metadata.elem_size = static_cast<int64_t>(DL.getTypeAllocSize(ElemTy).getFixedValue());
    return metadata;
}

}  // namespace lat
