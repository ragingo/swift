//===--- NonFixedTypeInfo.h - Non-fixed-layout types ------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file defines classes that are useful for implementing types
//  that do not have a fixed representation and cannot be laid out
//  statically.
//
//  These classes are useful only for creating TypeInfo
//  implementations; unlike the similiarly-named FixedTypeInfo, they
//  do not provide a supplemental API.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_NONFIXEDTYPEINFO_H
#define SWIFT_IRGEN_NONFIXEDTYPEINFO_H

#include "GenOpaque.h"
#include "TypeInfo.h"

namespace swift {
namespace irgen {

/// An abstract CRTP class designed for types whose storage size,
/// alignment, and stride need to be fetched from the value witness
/// table for the type.
///
/// The implementation class should implement:
///   llvm::Value *getMetadataRef(IRGenFunction &IGF) const;
///   llvm::Value *getValueWitnessTable(IRGenFunction &IGF) const;
template <class Impl>
class WitnessSizedTypeInfo : public TypeInfo {
protected:
  const Impl &asImpl() const { return static_cast<const Impl &>(*this); }

  WitnessSizedTypeInfo(llvm::Type *type, Alignment align, IsPOD_t pod)
    : TypeInfo(type, align, pod, IsNotFixedSize) {}

public:
  // This is useful for metaprogramming.
  static bool isFixed() { return false; }

  OwnedAddress allocateBox(IRGenFunction &IGF,
                           const llvm::Twine &name) const override {
    // Allocate a new object using the allocBox runtime call.
    llvm::Value *metadata = asImpl().getMetadataRef(IGF);
    llvm::Value *box, *address;
    IGF.emitAllocBoxCall(metadata, box, address);
    address = IGF.Builder.CreateBitCast(address, StorageType->getPointerTo());
    return OwnedAddress(getAddressForPointer(address), box);
  }

  ContainedAddress allocateStack(IRGenFunction &IGF,
                                 const llvm::Twine &name) const override {
    // Make a fixed-size buffer.
    Address buffer = IGF.createAlloca(IGF.IGM.getFixedBufferTy(),
                                      getFixedBufferAlignment(IGF.IGM),
                                      name);

    // Allocate an object of the appropriate type within it.
    llvm::Value *metadata = asImpl().getMetadataRef(IGF);
    llvm::Value *address =
      emitAllocateBufferCall(IGF, metadata, buffer);
    address = IGF.Builder.CreateBitCast(address,
                                        getStorageType()->getPointerTo());
    return { buffer, getAddressForPointer(address) };
  }

  /// Perform a "take-initialization" from the given object.  A
  /// take-initialization is like a C++ move-initialization, except that
  /// the old object is actually no longer permitted to be destroyed.
  void initializeWithTake(IRGenFunction &IGF, Address destAddr,
                          Address srcAddr) const override {
    return IGF.emitMemCpy(destAddr, srcAddr, asImpl().Impl::getSize(IGF));
  }

  std::pair<llvm::Value*,llvm::Value*>
  getSizeAndAlignmentMask(IRGenFunction &IGF) const override {
    auto wtable = asImpl().getValueWitnessTable(IGF);
    auto size = emitLoadOfSize(IGF, wtable);
    auto align = emitLoadOfAlignmentMask(IGF, wtable);
    return { size, align };
  }

  llvm::Value *getSize(IRGenFunction &IGF) const override {
    auto wtable = asImpl().getValueWitnessTable(IGF);
    return emitLoadOfSize(IGF, wtable);
  }

  llvm::Value *getAlignmentMask(IRGenFunction &IGF) const override {
    auto wtable = asImpl().getValueWitnessTable(IGF);
    return emitLoadOfAlignmentMask(IGF, wtable);
  }

  llvm::Value *getStride(IRGenFunction &IGF) const override {
    auto wtable = asImpl().getValueWitnessTable(IGF);
    return emitLoadOfStride(IGF, wtable);
  }
  
  llvm::Constant *getStaticSize(IRGenModule &IGM) const override {
    return nullptr;
  }
  llvm::Constant *getStaticAlignmentMask(IRGenModule &IGM) const override {
    return nullptr;
  }
  llvm::Constant *getStaticStride(IRGenModule &IGM) const override {
    return nullptr;
  }
};

}
}

#endif
