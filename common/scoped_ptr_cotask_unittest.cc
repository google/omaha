// Copyright 2007-2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================

#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/scoped_ptr_cotask.h"
#include "omaha/testing/unit_test.h"

// TestMallocSpy monitors CoTaskMemAlloc/Free, and records statistics about
// them.

class TestMallocSpy : public IMallocSpy {
 public:
  struct Alloc {
    size_t size;
    void* ptr;
    bool freed;
  };

  TestMallocSpy() : ref_(1) {}

  virtual ~TestMallocSpy() {}

  size_t NumAllocs() const { return allocs_.size(); }

  const Alloc* GetAlloc(size_t i) const {
    if (i >= allocs_.size())
      return NULL;
    return &allocs_[i];
  }

  size_t NumFrees() const { return frees_.size(); }

  const Alloc* GetFree(size_t i) const {
    if (i >= frees_.size())
      return NULL;
    ASSERT1(frees_[i] < allocs_.size());
    return &allocs_[frees_[i]];
  }

  // IUnknown methods
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) {
    if (NULL == ppv) {
      return E_POINTER;
    }
    if (::IsEqualIID(__uuidof(IUnknown), riid) ||
        ::IsEqualIID(__uuidof(IMallocSpy), riid)) {
      AddRef();
      *ppv = static_cast<IUnknown*>(this);
      return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
  }
  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return ++ref_;
  }
  virtual ULONG STDMETHODCALLTYPE Release() {
    ULONG r = --ref_;
    if (0 == r) {
      delete this;
    }
    return r;
  }

  // IMallocSpy methods
  virtual SIZE_T STDMETHODCALLTYPE PreAlloc(SIZE_T request_size) {
    Alloc a = { request_size, NULL, false };
    allocs_.push_back(a);
    return request_size;
  }
  virtual void* STDMETHODCALLTYPE PostAlloc(void* actual) {
    ASSERT1(!allocs_.empty());
    ASSERT1(NULL == allocs_.back().ptr);
    allocs_.back().ptr = actual;
    return actual;
  }
  virtual void* STDMETHODCALLTYPE PreFree(void* request, BOOL spyed) {
    if (spyed) {
      bool found = false;
      for (size_t i = 0; i < allocs_.size(); ++i) {
        if ((allocs_[i].ptr == request) && !allocs_[i].freed) {
          allocs_[i].freed = true;
          frees_.push_back(i);
          found = true;
          break;
        }
      }
      ASSERT1(found);
    }
    return request;
  }
  virtual void STDMETHODCALLTYPE PostFree(BOOL) {}
  virtual SIZE_T STDMETHODCALLTYPE PreRealloc(void*,
                                              SIZE_T request_size,
                                              void**,
                                              BOOL) {
    return request_size;
  }
  virtual void* STDMETHODCALLTYPE PostRealloc(void* actual, BOOL) {
    return actual;
  }
  virtual void* STDMETHODCALLTYPE PreGetSize(void* request, BOOL) {
    return request;
  }
  virtual SIZE_T STDMETHODCALLTYPE PostGetSize(SIZE_T actual_size, BOOL) {
    return actual_size;
  }
  virtual void* STDMETHODCALLTYPE PreDidAlloc(void* request, BOOL) {
    return request;
  }
  virtual int STDMETHODCALLTYPE PostDidAlloc(void*, BOOL, int fActual) {
    return fActual;
  }
  virtual void STDMETHODCALLTYPE PreHeapMinimize() {}
  virtual void STDMETHODCALLTYPE PostHeapMinimize() {}

 private:
  ULONG ref_;
  std::vector<Alloc> allocs_;
  std::vector<size_t> frees_;
};

// MallocTest runs tests with a TestMallocSpy installed.

class MallocTest : public testing::Test {
 public:
  virtual void SetUp() {
    spy_.Attach(new TestMallocSpy);
    ASSERT_SUCCEEDED(::CoRegisterMallocSpy(spy_.p));
    EXPECT_EQ(0, spy()->NumAllocs());
  }
  virtual void TearDown() {
    EXPECT_EQ(spy()->NumAllocs(), spy()->NumFrees());
    ASSERT_SUCCEEDED(::CoRevokeMallocSpy());
  }
  TestMallocSpy* spy() { return spy_.p; }

 private:
  CComPtr<TestMallocSpy> spy_;
};

TEST_F(MallocTest, StrDupCoTask) {
  const char kNarrowString[] = "Hello";
  const size_t kNarrowLen = strlen(kNarrowString);
  const wchar_t kWideString[] = L"World";
  const size_t kWideLen = wcslen(kWideString);

  // Test StrDupCoTask with narrow strings.
  char* narrow_copy = StrDupCoTask(kNarrowString, kNarrowLen);

  ASSERT_EQ(1, spy()->NumAllocs());
  EXPECT_EQ(0, spy()->NumFrees());
  EXPECT_EQ((kNarrowLen + 1) * sizeof(char), spy()->GetAlloc(0)->size);
  EXPECT_EQ(narrow_copy, spy()->GetAlloc(0)->ptr);
  EXPECT_FALSE(spy()->GetAlloc(0)->freed);

  ::CoTaskMemFree(narrow_copy);

  ASSERT_EQ(1, spy()->NumFrees());
  EXPECT_EQ(spy()->GetAlloc(0), spy()->GetFree(0));
  EXPECT_EQ(narrow_copy, spy()->GetFree(0)->ptr);
  EXPECT_TRUE(spy()->GetFree(0)->freed);

  // Test StrDupCoTask with wide strings.
  wchar_t* wide_copy = StrDupCoTask(kWideString, kWideLen);

  ASSERT_EQ(2, spy()->NumAllocs());
  EXPECT_EQ(1, spy()->NumFrees());
  EXPECT_EQ((kWideLen + 1) * sizeof(wchar_t), spy()->GetAlloc(1)->size);
  EXPECT_EQ(wide_copy, spy()->GetAlloc(1)->ptr);
  EXPECT_FALSE(spy()->GetAlloc(1)->freed);

  ::CoTaskMemFree(wide_copy);

  ASSERT_EQ(2, spy()->NumFrees());
  EXPECT_EQ(spy()->GetAlloc(1), spy()->GetFree(1));
  EXPECT_EQ(wide_copy, spy()->GetFree(1)->ptr);
  EXPECT_TRUE(spy()->GetFree(1)->freed);
}

TEST_F(MallocTest, scoped_ptr_cotask) {
  scoped_ptr_cotask<wchar_t>* string_ptr;

  // Creating an empty ptr does no additional allocations.
  string_ptr = new scoped_ptr_cotask<wchar_t>;
  ASSERT_EQ(0, spy()->NumAllocs());
  EXPECT_EQ(0, spy()->NumFrees());

  // Assigning a string does not additional allocations.
  string_ptr->reset(StrDupCoTask(L"hi", 2));
  ASSERT_EQ(1, spy()->NumAllocs());
  EXPECT_EQ(0, spy()->NumFrees());
  EXPECT_EQ(3 * sizeof(wchar_t), spy()->GetAlloc(0)->size);
  EXPECT_FALSE(spy()->GetAlloc(0)->freed);

  EXPECT_EQ(0, memcmp(string_ptr->get(), L"hi", 3 * sizeof(wchar_t)));

  // Replacing the string frees the old memory.
  string_ptr->reset(StrDupCoTask(L"there", 5));
  ASSERT_EQ(2, spy()->NumAllocs());
  EXPECT_EQ(1, spy()->NumFrees());
  EXPECT_EQ(6 * sizeof(wchar_t), spy()->GetAlloc(1)->size);
  EXPECT_TRUE(spy()->GetAlloc(0)->freed);
  EXPECT_FALSE(spy()->GetAlloc(1)->freed);

  // Deleting the string frees the memory.
  delete string_ptr;
  ASSERT_EQ(2, spy()->NumAllocs());
  EXPECT_EQ(2, spy()->NumFrees());
  EXPECT_TRUE(spy()->GetAlloc(1)->freed);
}

TEST_F(MallocTest, scoped_array_cotask) {
  const size_t kSize = 5;
  scoped_array_cotask<wchar_t*>* string_array;

  // Allocate an array of 5 empty elements.
  string_array = new scoped_array_cotask<wchar_t*>(kSize);
  ASSERT_EQ(kSize, string_array->size());
  ASSERT_EQ(1, spy()->NumAllocs());
  EXPECT_EQ(0, spy()->NumFrees());
  EXPECT_EQ(kSize * sizeof(wchar_t*), spy()->GetAlloc(0)->size);

  // Populate array elements.
  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_TRUE(NULL == (*string_array)[i]);
    (*string_array)[i] = StrDupCoTask(L"hi", 2);
  }
  EXPECT_EQ(1 + kSize, spy()->NumAllocs());
  EXPECT_EQ(0, spy()->NumFrees());

  // Get is idempotent.
  wchar_t** ptr = string_array->get();
  EXPECT_EQ(ptr, string_array->get());
  EXPECT_EQ(ptr, spy()->GetAlloc(0)->ptr);
  EXPECT_EQ(0, spy()->NumFrees());

  // Release is not idempotent, but does not free memory.
  ptr = string_array->release();
  EXPECT_TRUE(NULL == string_array->release());
  EXPECT_EQ(ptr, spy()->GetAlloc(0)->ptr);
  EXPECT_EQ(0, spy()->NumFrees());

  // Deleting a released array does not free memory.
  delete string_array;
  EXPECT_EQ(0, spy()->NumFrees());

  // Constructing an array from existing memory, does not cause allocations.
  string_array = new scoped_array_cotask<wchar_t*>(kSize, ptr);
  EXPECT_EQ(1 + kSize, spy()->NumAllocs());
  EXPECT_EQ(0, spy()->NumFrees());

  // Deleting an array frees all elements and the array.
  delete string_array;
  ASSERT_EQ(1 + kSize, spy()->NumAllocs());
  EXPECT_EQ(1 + kSize, spy()->NumFrees());
  for (size_t i = 0; i < spy()->NumAllocs(); ++i) {
    EXPECT_TRUE(spy()->GetAlloc(i)->freed);
  }
}

TEST_F(MallocTest, scoped_array_cotask_reset) {
  // This test exposes a former bug, where reset did not reallocate a new
  // array after being released.

  // Allocate an empty array.
  const size_t kSize = 5;
  scoped_array_cotask<int*>* array = new scoped_array_cotask<int*>(kSize);
  ASSERT_EQ(1, spy()->NumAllocs());

  // Release the array, to verify it was allocated.
  int** first_raw_array = array->release();
  EXPECT_TRUE(NULL != first_raw_array);
  EXPECT_FALSE(spy()->GetAlloc(0)->freed);

  // Allocate another empty array.
  array->reset(kSize);
  ASSERT_EQ(2, spy()->NumAllocs());

  // Release the second array, to verify it was allocated.
  int** second_raw_array = array->release();
  EXPECT_TRUE(NULL != second_raw_array);
  EXPECT_FALSE(spy()->GetAlloc(1)->freed);

  // Use the scoped_array_cotask object to dispose of the allocated arrays.
  array->reset(kSize, first_raw_array);
  array->reset(kSize, second_raw_array);
  delete array;

  // Check the final conditions.
  ASSERT_EQ(2, spy()->NumAllocs());
  for (size_t i = 0; i < spy()->NumAllocs(); ++i) {
    EXPECT_TRUE(spy()->GetAlloc(i)->freed);
  }
}
