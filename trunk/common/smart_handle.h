// Copyright 2004-2009 Google Inc.
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
//
// Classes for automatically closing handles.

#ifndef OMAHA_COMMON_SMART_HANDLE_H_
#define OMAHA_COMMON_SMART_HANDLE_H_

#include <wincrypt.h>

namespace omaha {

/**
* Base traits class for handles.
* This base class provides default implementation for InvalidValue and IsValid
* @param T  The handle type to be wrapped.
*/
template<class T>
class BaseHandleTraitsT {
 public:
  // Typedef that is used by this class and derived classes
  typedef T HandleType;

  // Returns the invalid handle value
  static HandleType InvalidValue() {
    return NULL;
  }

  // Returns true only if the given handle h is invalid
  static bool IsValid(const HandleType& h) {
    return h != InvalidValue();
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BaseHandleTraitsT);
};

/**
* Smart handle class.
* Offers basic HANDLE functionality such as cast, attach/detach and automatic Close().
*/
template<class T, class Traits, class AlternateType = T>
class HandleT {
 public:
  // Default constructor.
  HandleT() : h_(Traits::InvalidValue()) {
  }

  // Constructor that assumes ownership of the supplied handle
  explicit HandleT(T h) : h_(h) {
  }

  // Destructor calls @ref Close()
  ~HandleT() {
    Close();
  }

  // Assumes ownership of the supplied handle,
  // potentially closing an already held handle.
  void Attach(T h) {
    Close();
    h_ = h;
  }

  // Transfers ownership to the caller and sets the internal
  // state to InvalidValue().
  T Detach() {
    T h = h_;
    h_ = Traits::InvalidValue();
    return h;
  }

  // Handle accessor
  T handle() {
    return h_;
  }

  // An alternate cast for the handle.
  // This can be useful for GDI objects that are used
  // in functions that e.g. accept both HGDIOBJ and HBITMAP.
  AlternateType alt_type() {
    return reinterpret_cast<AlternateType>(h_);
  }

  // Accesses the contained handle
  operator T() {
    return h_;
  }

  T& receive() {
    ASSERT(!IsValid(), (L"Should only be used for out arguments"));
    return h_;
  }

  // @returns true only if the handle is valid as depicted
  //  by the traits class.
  bool IsValid() {
    return Traits::IsValid(h_);
  }

  // Closes the handle
  void Close() {
    if (Traits::IsValid(h_)) {
      Traits::Close(h_);
      h_ = Traits::InvalidValue();
    }
  }

 protected:
  T h_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleT);
};


/*
* Traits class for a regular Win32 HANDLE.
*/
class HandleTraitsWin32Handle : public BaseHandleTraitsT<HANDLE> {
 public:
  // Calls FindClose to close the handle.
  static bool Close(HandleType h) {
    return (::CloseHandle(h) != false);
  }

  // Returns the invalid handle value
  static HandleType InvalidValue() {
    return NULL;  // note that INVALID_HANDLE_VALUE is also an invalid handle
  }

  // Returns true only if the given handle h is invalid
  static bool IsValid(const HandleType& h) {
    return h != InvalidValue() && h != INVALID_HANDLE_VALUE;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleTraitsWin32Handle);
};

/*
* Traits class for FindXXXFile handles.
*/
class HandleTraitsFindHandle : public BaseHandleTraitsT<HANDLE> {
 public:
  // Calls FindClose to close the handle.
  static bool Close(HandleType h) {
    return (::FindClose(h) != false);
  }

  // Returns the invalid handle value
  static HandleType InvalidValue() {
    return INVALID_HANDLE_VALUE;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleTraitsFindHandle);
};

/*
* Traits for an HMENU.
*/
class HandleTraitsHMenu : public BaseHandleTraitsT<HMENU> {
 public:
  // Calls DestroyMenu to destroy the menu.
  static bool Close(HandleType h) {
    return (::DestroyMenu(h) != FALSE);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleTraitsHMenu);
};

/*
* Traits for an HCRYPTKEY.
*/
class HandleTraitsHCryptKey : public BaseHandleTraitsT<HCRYPTKEY> {
 public:
  static bool Close(HandleType h) {
    return (::CryptDestroyKey(h) != FALSE);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleTraitsHCryptKey);
};

/*
* Traits for an HCRYPTHASH.
*/
class HandleTraitsHCryptHash : public BaseHandleTraitsT<HCRYPTHASH> {
 public:
  static bool Close(HandleType h) {
    return (::CryptDestroyHash(h) != FALSE);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleTraitsHCryptHash);
};

/*
 * Traits for LoadLibrary/FreeLibrary.
 */
class HandleTraitsLibrary : public BaseHandleTraitsT<HMODULE> {
 public:
  static bool Close(HandleType h) {
    return (::FreeLibrary(h) != FALSE);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(HandleTraitsLibrary);
};


/*
* Win32 handle types.  Add new ones here as you need them.
* Note that GDI handle types should be kept in common/gdi_smart_ptr.h
* rather than here.
*/
typedef HandleT<HANDLE, HandleTraitsWin32Handle> AutoHandle;
typedef HandleT<HANDLE, HandleTraitsFindHandle> AutoFindHandle;
typedef HandleT<HMENU, HandleTraitsHMenu> AutoHMenu;
typedef HandleT<HCRYPTHASH, HandleTraitsHCryptHash> AutoHCryptHash;
typedef HandleT<HCRYPTKEY, HandleTraitsHCryptKey> AutoHCryptKey;
typedef HandleT<HINSTANCE, HandleTraitsLibrary> AutoLibrary;

}  // namespace omaha

#endif  // OMAHA_COMMON_SMART_HANDLE_H_
