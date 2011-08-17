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

#include "omaha/base/serializable_object.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Template test function to round-trip an object through its serialized form.
// Requires the object to implement SetTestValues() and VerifyTestValues().
template <class T>
void TestSerializeRoundTrip() {
  T in;
  T out;
  std::vector<byte> data;

  in.SetTestValues();
  in.VerifyTestValues();  // sanity check to catch broken tests
  EXPECT_TRUE(in.Serialize(&data));
  EXPECT_TRUE(out.Deserialize(&data[0], data.size()));
  out.VerifyTestValues();
}


// Ordinary values.
class SimpleValues : public SerializableObject {
 public:
  SimpleValues() {
    AddSerializableMember(&value1_);
    AddSerializableMember(&value2_);
    AddSerializableMember(&value3_);
    AddSerializableMember(&value4_);
  }

  virtual void SetTestValues() {
    value1_ = 452;
    value2_ = 'Z';
    value3_ = false;
    value4_ = 9276554;
  }

  virtual void VerifyTestValues() {
    EXPECT_EQ(452, value1_);
    EXPECT_EQ('Z', value2_);
    EXPECT_EQ(false, value3_);
    EXPECT_EQ(9276554, value4_);
  }

 private:
  int value1_;
  char value2_;
  bool value3_;
  int value4_;
};

TEST(SerializableObjectTest, SimpleValues) {
  TestSerializeRoundTrip<SimpleValues>();
}


// Strings.
const TCHAR kString1[] = _T("an example\tvalue\n");
const TCHAR kString2[] = _T("");
const TCHAR kString3[] = _T("and the mome raths outgrabe");

class StringValues : public SerializableObject {
 public:
  StringValues() {
    AddSerializableMember(&string1_);
    AddSerializableMember(&string2_);
    AddSerializableMember(&string3_);
  }

  virtual void SetTestValues() {
    string1_ = kString1;
    string2_ = kString2;
    string3_ = kString3;
  }

  virtual void VerifyTestValues() {
    EXPECT_STREQ(kString1, string1_);
    EXPECT_STREQ(kString2, string2_);
    EXPECT_STREQ(kString3, string3_);
  }

 private:
  CString string1_;
  CString string2_;
  CString string3_;
};

TEST(SerializableObjectTest, StringValues) {
  TestSerializeRoundTrip<StringValues>();
}


// Nested objects.
class NestedObjects : public SerializableObject {
 public:
  NestedObjects() {
    AddSerializableMember(&simple_values_);
    AddSerializableMember(&string_values_);
  }

  virtual void SetTestValues() {
    simple_values_.SetTestValues();
    string_values_.SetTestValues();
  }

  virtual void VerifyTestValues() {
    simple_values_.VerifyTestValues();
    string_values_.VerifyTestValues();
  }

 private:
  SimpleValues simple_values_;
  StringValues string_values_;
};

TEST(SerializableObjectTest, NestedObjects) {
  TestSerializeRoundTrip<NestedObjects>();
}


// Vector of values.
class ValueVector : public SerializableObject {
 public:
  ValueVector() {
    AddSerializableMember(&vector_);
    AddSerializableMember(&empty_vector_);
  }

  virtual void SetTestValues() {
    vector_.push_back(5);
    vector_.push_back(8);
    vector_.push_back(13);
  }

  virtual void VerifyTestValues() {
    EXPECT_EQ(0, empty_vector_.size());
    EXPECT_EQ(3, vector_.size());
    EXPECT_EQ(5, vector_[0]);
    EXPECT_EQ(8, vector_[1]);
    EXPECT_EQ(13, vector_[2]);
  }

 private:
  std::vector<int> vector_;
  std::vector<int> empty_vector_;
};

TEST(SerializableObjectTest, ValueVector) {
  TestSerializeRoundTrip<ValueVector>();
}


// Vector of objects.
class InnerObject : public SerializableObject {
 public:
  InnerObject() : value_(-1) {
    AddSerializableMember(&value_);
  }

  explicit InnerObject(int i) : value_(i) {
    AddSerializableMember(&value_);
  }

  InnerObject(const InnerObject& other) {
    AddSerializableMember(&value_);
    value_ = other.value_;
  }

  InnerObject& operator=(const InnerObject& other) {
    value_ = other.value_;
    return *this;
  }

  int value() {
    return value_;
  }

 private:
  int value_;
};

class ObjectVector : public SerializableObject {
 public:
  ObjectVector() {
    AddSerializableMember(&vector_);
  }

  virtual void SetTestValues() {
    vector_.push_back(InnerObject(21));
    vector_.push_back(InnerObject(34));
    vector_.push_back(InnerObject(55));
  }

  virtual void VerifyTestValues() {
    EXPECT_EQ(3, vector_.size());
    EXPECT_EQ(21, vector_[0].value());
    EXPECT_EQ(34, vector_[1].value());
    EXPECT_EQ(55, vector_[2].value());
  }

  virtual bool SerializeVectorNestedObject(std::vector<byte>* data,
                                           const byte* ptr) const {
    EXPECT_TRUE(data);
    EXPECT_TRUE(ptr);
    if (ptr == reinterpret_cast<const byte*>(&vector_))
      return SerializeVectorNestedObjectHelper(data, &vector_);
    return false;
  }

  virtual bool DeserializeVectorNestedObject(byte** data,
                                             int size,
                                             byte* ptr,
                                             uint32 version) {
    EXPECT_TRUE(data);
    EXPECT_TRUE(ptr);
    if (ptr == reinterpret_cast<byte*>(&vector_)) {
      return DeserializeVectorNestedObjectHelper(data, size,
                                                 &vector_, version);
    }
    return false;
  }

 private:
  std::vector<InnerObject> vector_;
};

TEST(SerializableObjectTest, ObjectVector) {
  TestSerializeRoundTrip<ObjectVector>();
}

}  // namespace omaha

