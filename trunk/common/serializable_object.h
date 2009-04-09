// Copyright 2005-2009 Google Inc.
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
// Declares class SerializableObject
//
// Provides the base class framework for those objects to be serialized
//
// Currently we support the serialization for the following member type
//
//   1) Value-type object
//   2) CString
//   3) Nested serializable object
//   4) Vector of the value-type objects
//   5) Vector of CString
//   6) Vector of serializable objects
//
// Usage:
//
//   1) Declare the object class, which you want to serialize, to be derived
//      from SerializableObject
//   2) In its constructor, call AddSerializableMember(...) to add those fields
//      to be included in the serialization
//   3) If you need to serialize a vector of serializable objects,
//      a) The inner object class has to implement copy constructor and
//         operator =
//      b) The outer object class has to override SerializeVectorNestedObject()
//         and DeserializeVectorNestedObject() (see sample)
//
// Versioning support:
//
//   To add new fields in the new version, call
//        AddSerializableMember(version, &member);
//   If "version" is not given, it is 0 by default.
//
//   To deserialize the data for latest version, call
//        Deserialize(data, size, kLatestSerializableVersion);
//   To deserialize the data for a particular version, call
//        Deserialize(data, size, version);
//
// Sample:
//
//   class FooObject : public SerializableObject {
//     ...
//   };
//
//   class BarObject : public SerializableObject {
//    public:
//     BarObject() {
//      AddSerializableMember(&value1_);
//      AddSerializableMember(&value2_);
//      AddSerializableMember(&value3_);
//      AddSerializableMember(&value4_);
//      AddSerializableMember(&value5_);
//      AddSerializableMember(&value6_);
//      AddSerializableMember(1, &value7_);     // New version
//     }
//
//    protected:
//     virtual bool SerializeVectorNestedObject(std::vector<byte>* data,
//                                              const byte* ptr) const {
//       ASSERT(data, (_T("")));
//       ASSERT(ptr, (_T("")));
//       if (ptr == reinterpret_cast<const byte*>(&value6_))
//         return SerializeVectorNestedObjectHelper(data, &value6_);
//       return false;
//     }
//
//     virtual bool DeserializeVectorNestedObject(byte** data,
//                                                int size,
//                                                byte* ptr,
//                                                uint32 version) {
//       ASSERT(data, (_T("")));
//       ASSERT(*data, (_T("")));
//       ASSERT(ptr, (_T("")));
//       if (ptr == reinterpret_cast<byte*>(&value6_))
//         return DeserializeVectorNestedObjectHelper(data,
//                                                    size,
//                                                    &value6_,
//                                                    version);
//       return false;
//     }
//
//    private:
//     int value1_;
//     CString value2_;
//     FooObject value3_;
//     std::vector<byte> value4_;
//     std::vector<CString> value5_;
//     std::vector<BarObject> value6_;
//     int value7_;
//   };
//
// Binary format:
//
//   1) Value type: data
//      e.g.  100           =>   64 00 00 00
//   2) CString:    size count data
//      e.g.  "ABC"         =>   02 00 00 00 03 00 00 00 41 00 42 00 43 00
//   3) Vector:     size count data
//      e.g.  vector<int> = {1, 2}
//                          =>   04 00 00 00 02 00 00 00 01 00 00 00 02 00 00 00
//
// TODO(omaha):
//   1) Define struct TypeTrait for all type-related info
//   2) Initialize TypeTrait on per-type basis instead of per-object basis

#ifndef OMAHA_COMMON_SERIALIZABLE_OBJECT_H_
#define OMAHA_COMMON_SERIALIZABLE_OBJECT_H_

#include <vector>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/type_utils.h"

namespace omaha {

// Constants
const uint32 kLatestSerializableVersion = 0xFFFFFFFF;

// Serializable object
class SerializableObject {
 private:
  // Define SerializableMemberType, for internal use
  typedef uint32 SerializableMemberType;

  #define SERIALIZABLE_VALUE_TYPE     1
  #define SERIALIZABLE_CSTRING        2
  #define SERIALIZABLE_NESTED_OBJECT  3
  #define SERIALIZABLE_VECTOR         0x8000

  // Serializable member info
  struct SerializableMemberInfo {
    byte* ptr;                      // Pointers to the serializable member
    SerializableMemberType type;    // Type of the serializable member
    int size;                       // Size of the serializable member
    uint32 version;                 // Version when the member is added

    SerializableMemberInfo()
        : ptr(NULL), type(SERIALIZABLE_VALUE_TYPE), size(0) {}
  };

 public:
  // Constructor
  SerializableObject() {}

  // Destructor
  virtual ~SerializableObject() {}

  // Serialize
  bool Serialize(std::vector<byte>* data) const;

  // Deserialize the data for the latest version
  bool Deserialize(byte* data, int size) {
    return Deserialize(data, size, kLatestSerializableVersion);
  }

  // Deserialize the data for a particular version
  bool Deserialize(byte* data, int size, uint32 version);

 protected:
  // Clear the serializable member list
  void ClearSerializableMemberList() {
    members_.clear();
  }

  // Add value-typed member to the serializable member list
  template<typename T>
  void AddSerializableMember(T* ptr) {
    return AddSerializableMember(0, ptr);
  }

  // Add value-typed member to the serializable member list
  template<typename T>
  void AddSerializableMember(uint32 version, T* ptr) {
    if (SUPERSUBCLASS(SerializableObject, T)) {
      #pragma warning(push)
      // reinterpret_cast used between related classes
      #pragma warning(disable : 4946)
      return AddSerializableMember(version,
                                   reinterpret_cast<SerializableObject*>(ptr),
                                   sizeof(T));
      #pragma warning(pop)
    }
    SerializableMemberInfo member;
    member.ptr = reinterpret_cast<byte*>(ptr);
    member.type = SERIALIZABLE_VALUE_TYPE;
    member.size = sizeof(T);
    member.version = version;
    members_.push_back(member);
  }

  // Add CString-typed member to the serializable member list
  void AddSerializableMember(CString* ptr) {
    AddSerializableMember(0, ptr);
  }

  // Add CString-typed member to the serializable member list
  void AddSerializableMember(uint32 version, CString* ptr) {
    SerializableMemberInfo member;
    member.ptr = reinterpret_cast<byte*>(ptr);
    member.type = SERIALIZABLE_CSTRING;
    member.size = sizeof(TCHAR);
    member.version = version;
    members_.push_back(member);
  }

  // Add nested serializable member to the serializable member list
  void AddSerializableMember(SerializableObject* ptr, int size) {
    AddSerializableMember(0, ptr, size);
  }

  // Add nested serializable member to the serializable member list
  void AddSerializableMember(uint32 version,
                             SerializableObject* ptr,
                             int size) {
    SerializableMemberInfo member;
    member.ptr = reinterpret_cast<byte*>(ptr);
    member.type = SERIALIZABLE_NESTED_OBJECT;
    member.size = size;
    member.version = version;
    members_.push_back(member);
  }

  // Add vector-typed member to the serializable member list
  template<typename T>
  void AddSerializableMember(std::vector<T>* ptr) {
    AddSerializableMember(0, ptr);
  }

  // Add vector-typed member to the serializable member list
  template<typename T>
  void AddSerializableMember(uint32 version, std::vector<T>* ptr) {
    SerializableMemberInfo member;
    member.ptr = reinterpret_cast<byte*>(ptr);
    member.version = version;

    if (SUPERSUBCLASS(CString, T)) {
      member.type =
          static_cast<SerializableMemberType>(SERIALIZABLE_VECTOR |
                                              SERIALIZABLE_CSTRING);
      member.size = sizeof(TCHAR);
    } else if (SUPERSUBCLASS(SerializableObject, T)) {
      member.type =
          static_cast<SerializableMemberType>(SERIALIZABLE_VECTOR |
                                              SERIALIZABLE_NESTED_OBJECT);
      member.size = sizeof(T);
    } else {
      member.type =
          static_cast<SerializableMemberType>(SERIALIZABLE_VECTOR |
                                              SERIALIZABLE_VALUE_TYPE);
      member.size = sizeof(T);
    }

    members_.push_back(member);
  }

  // If there is a vector of SerializableObject to be serialized, the derived
  // class need to provide the implementation
  virtual bool SerializeVectorNestedObject(std::vector<byte>*,
                                           const byte*) const {
    ASSERT(false, (_T("Provide the implementation in the derived class.")));
    return false;
  }

  // Helper method to serialize a vector of SerializableObject
  template<typename T>
  bool SerializeVectorNestedObjectHelper(std::vector<byte>* data,
                                         const std::vector<T>* list) const {
    ASSERT(data, (_T("")));
    ASSERT(list, (_T("")));
    ASSERT(SUPERSUBCLASS(SerializableObject, T), (_T("")));

    // Size of SerializableObject is unknown
    SerializeSizeAndCount(data, 0, list->size());
    for (size_t i = 0; i < list->size(); ++i) {
      // To work around compiler complaint while using dynamic_cast
      SerializableObject* so =
          const_cast<SerializableObject*>(
              static_cast<const SerializableObject*>(&(*list)[i]));
      bool res = so->Serialize(data);
      if (!res)
        return false;
    }
    return true;
  }

  // If there is a vector of SerializableObject to be serialized, the derived
  // class need to provide the implementation
  virtual bool DeserializeVectorNestedObject(byte**, int, byte*, uint32) {
    ASSERT(false, (_T("provide the implementation in the derived class.")));
    return false;
  }

  // Helper method to deserialize a vector of SerializableObject
  template<typename T>
  bool DeserializeVectorNestedObjectHelper(byte** data,
                                           int size,
                                           std::vector<T>* list,
                                           uint32 version) {
    ASSERT(data, (_T("")));
    ASSERT(*data, (_T("")));
    ASSERT(size, (_T("")));
    ASSERT(list, (_T("")));
    ASSERT(SUPERSUBCLASS(SerializableObject, T), (_T("")));

    byte* tail = *data + size;

    // Size of SerializableObject is unknown
    int count = 0;
    bool res = DeserializeSizeAndCount(&count, 0, data, size);
    if (!res)
      return false;

    for (int i = 0; i < count; ++i) {
      T obj;
      bool res = obj.DeserializeHelper(data, tail - *data, version);
      if (!res)
        return false;
      list->push_back(obj);
    }
    return true;
  }

 private:
  // Serialize the size and count values
  void SerializeSizeAndCount(std::vector<byte>* data,
                             int size,
                             int count) const;

  // Serialize a list of value-typed elements
  void SerializeValueList(std::vector<byte>* ser_data,
                          const byte* raw_data,
                          int size,
                          int count) const;

  // Deserialize helper
  bool DeserializeHelper(byte** data, int size, uint32 version);

  // Deserialize the size and count values
  bool DeserializeSizeAndCount(int* count,
                               int size,
                               byte** ser_data,
                               int ser_size) const;

  // Deserialize a list of value-typed elements
  bool DeserializeValueList(std::vector<byte>* raw_data,
                            int size,
                            byte** ser_data,
                            int ser_size);

  // List of serializable members
  std::vector<SerializableMemberInfo> members_;

  // We need to initialize TypeTrait on per-type basis instead of per-object
  // basis and remove the following use of macro.
  DISALLOW_EVIL_CONSTRUCTORS(SerializableObject);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SERIALIZABLE_OBJECT_H_
