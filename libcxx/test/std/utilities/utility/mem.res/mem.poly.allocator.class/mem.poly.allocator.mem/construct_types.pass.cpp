//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14
// XFAIL: use_system_cxx_lib && target={{.+}}-apple-macosx10.{{9|10|11|12|13|14|15}}
// XFAIL: use_system_cxx_lib && target={{.+}}-apple-macosx{{11.0|12.0}}

// <memory_resource>

// template <class T> class polymorphic_allocator

// template <class U, class ...Args>
// void polymorphic_allocator<T>::construct(U *, Args &&...)

#include <memory_resource>
#include <type_traits>
#include <cassert>
#include <cstdlib>

#include "test_macros.h"
#include "test_std_memory_resource.h"
#include "uses_alloc_types.h"
#include "controlled_allocators.h"
#include "test_allocator.h"

template <class T>
struct PMATest {
  TestResource R;
  std::pmr::polymorphic_allocator<T> A;
  T* ptr;
  bool constructed;

  PMATest() : A(&R), ptr(A.allocate(1)), constructed(false) {}

  template <class... Args>
  void construct(Args&&... args) {
    A.construct(ptr, std::forward<Args>(args)...);
    constructed = true;
  }

  ~PMATest() {
    if (constructed)
      A.destroy(ptr);
    A.deallocate(ptr, 1);
  }
};

template <class T, class... Args>
bool doTest(UsesAllocatorType UAExpect, Args&&... args) {
  PMATest<T> TH;
  // UNDER TEST //
  TH.construct(std::forward<Args>(args)...);
  return checkConstruct<Args&&...>(*TH.ptr, UAExpect, &TH.R);
  // ------- //
}

template <class T, class... Args>
bool doTestUsesAllocV0(Args&&... args) {
  PMATest<T> TH;
  // UNDER TEST //
  TH.construct(std::forward<Args>(args)...);
  return checkConstruct<Args&&...>(*TH.ptr, UA_None);
  // -------- //
}

template <class T, class EAlloc, class... Args>
bool doTestUsesAllocV1(EAlloc const& ealloc, Args&&... args) {
  PMATest<T> TH;
  // UNDER TEST //
  TH.construct(std::allocator_arg, ealloc, std::forward<Args>(args)...);
  return checkConstruct<Args&&...>(*TH.ptr, UA_AllocArg, ealloc);
  // -------- //
}

template <class T, class EAlloc, class... Args>
bool doTestUsesAllocV2(EAlloc const& ealloc, Args&&... args) {
  PMATest<T> TH;
  // UNDER TEST //
  TH.construct(std::forward<Args>(args)..., ealloc);
  return checkConstruct<Args&&...>(*TH.ptr, UA_AllocLast, ealloc);
  // -------- //
}

template <class Alloc, class... Args>
void test_pmr_uses_alloc(Args&&... args) {
  TestResource R(12435);
  std::pmr::memory_resource* M = &R;
  {
    // NotUsesAllocator provides valid signatures for each uses-allocator
    // construction but does not supply the required allocator_type typedef.
    // Test that we can call these constructors manually without
    // polymorphic_allocator interfering.
    using T = NotUsesAllocator<Alloc, sizeof...(Args)>;
    assert(doTestUsesAllocV0<T>(std::forward<Args>(args)...));
    assert((doTestUsesAllocV1<T>(M, std::forward<Args>(args)...)));
    assert((doTestUsesAllocV2<T>(M, std::forward<Args>(args)...)));
  }
  {
    // Test T(std::allocator_arg_t, Alloc const&, Args...) construction
    using T = UsesAllocatorV1<Alloc, sizeof...(Args)>;
    assert((doTest<T>(UA_AllocArg, std::forward<Args>(args)...)));
  }
  {
    // Test T(Args..., Alloc const&) construction
    using T = UsesAllocatorV2<Alloc, sizeof...(Args)>;
    assert((doTest<T>(UA_AllocLast, std::forward<Args>(args)...)));
  }
  {
    // Test that T(std::allocator_arg_t, Alloc const&, Args...) construction
    // is preferred when T(Args..., Alloc const&) is also available.
    using T = UsesAllocatorV3<Alloc, sizeof...(Args)>;
    assert((doTest<T>(UA_AllocArg, std::forward<Args>(args)...)));
  }
}

// Test that polymorphic_allocator does not prevent us from manually
// doing non-pmr uses-allocator construction.
template <class Alloc, class AllocObj, class... Args>
void test_non_pmr_uses_alloc(AllocObj const& A, Args&&... args) {
  {
    using T = NotUsesAllocator<Alloc, sizeof...(Args)>;
    assert(doTestUsesAllocV0<T>(std::forward<Args>(args)...));
    assert((doTestUsesAllocV1<T>(A, std::forward<Args>(args)...)));
    assert((doTestUsesAllocV2<T>(A, std::forward<Args>(args)...)));
  }
  {
    using T = UsesAllocatorV1<Alloc, sizeof...(Args)>;
    assert(doTestUsesAllocV0<T>(std::forward<Args>(args)...));
    assert((doTestUsesAllocV1<T>(A, std::forward<Args>(args)...)));
  }
  {
    using T = UsesAllocatorV2<Alloc, sizeof...(Args)>;
    assert(doTestUsesAllocV0<T>(std::forward<Args>(args)...));
    assert((doTestUsesAllocV2<T>(A, std::forward<Args>(args)...)));
  }
  {
    using T = UsesAllocatorV3<Alloc, sizeof...(Args)>;
    assert(doTestUsesAllocV0<T>(std::forward<Args>(args)...));
    assert((doTestUsesAllocV1<T>(A, std::forward<Args>(args)...)));
    assert((doTestUsesAllocV2<T>(A, std::forward<Args>(args)...)));
  }
}

int main(int, char**) {
  using PMR   = std::pmr::memory_resource*;
  using PMA   = std::pmr::polymorphic_allocator<char>;
  using STDA  = std::allocator<char>;
  using TESTA = test_allocator<char>;

  int value        = 42;
  const int cvalue = 43;
  {
    test_pmr_uses_alloc<PMA>();
    test_pmr_uses_alloc<PMA>(value);
    test_pmr_uses_alloc<PMA>(cvalue);
    test_pmr_uses_alloc<PMA>(cvalue, std::move(value));
  }
  {
    STDA std_alloc;
    TESTA test_alloc(42);
    PMR mem_res = std::pmr::new_delete_resource();

    test_non_pmr_uses_alloc<PMR>(mem_res);
    test_non_pmr_uses_alloc<STDA>(std_alloc);
    test_non_pmr_uses_alloc<TESTA>(test_alloc);
    test_non_pmr_uses_alloc<PMR>(mem_res, value);
    test_non_pmr_uses_alloc<STDA>(std_alloc, value);
    test_non_pmr_uses_alloc<TESTA>(test_alloc, value);
    test_non_pmr_uses_alloc<PMR>(mem_res, cvalue);
    test_non_pmr_uses_alloc<STDA>(std_alloc, cvalue);
    test_non_pmr_uses_alloc<TESTA>(test_alloc, cvalue);
    test_non_pmr_uses_alloc<PMR>(mem_res, cvalue, std::move(cvalue));
    test_non_pmr_uses_alloc<STDA>(std_alloc, cvalue, std::move(value));
    test_non_pmr_uses_alloc<TESTA>(test_alloc, cvalue, std::move(value));
  }

  return 0;
}
