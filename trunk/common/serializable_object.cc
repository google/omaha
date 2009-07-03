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
// Provides the base class framework for those objects to be serialized
//
// HACK:
//
// During the serialization/deserialization of vector<T> members, we
// coerce the type from vector<T> to vector<byte> since we are unable to
// get the real type vector<T> at later time. This is feasible because
// vector<T> in the vector library we are linking now keeps track of
// only front() and end() pointers and use them to calculate size().
// We need to check whether this approach is still OK if we upgrade the
// standard libraries.

#include "omaha/common/serializable_object.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"

namespace omaha {

// Serialize
bool SerializableObject::Serialize(std::vector<byte>* data) const {
  ASSERT(data, (_T("")));

  // Estimate how much memory we need
  int size = data->size();
  for (size_t i = 0; i < members_.size(); ++i)
    size += (members_[i].size > 0) ? members_[i].size : sizeof(int);

  // Reserve the estimated size fo vector memory
  data->reserve(size);

  // Copy over the data
  for (size_t i = 0; i < members_.size(); ++i) {
    switch (members_[i].type) {
      case SERIALIZABLE_VALUE_TYPE: {
        int pos = data->size();
        data->resize(data->size() + members_[i].size);
        memcpy(&(*data)[pos], members_[i].ptr, members_[i].size);
        break;
      }

      case SERIALIZABLE_CSTRING: {
        CString* s = reinterpret_cast<CString*>(members_[i].ptr);
        SerializeValueList(data,
                           reinterpret_cast<const byte*>(s->GetString()),
                           sizeof(TCHAR),
                           s->GetLength());
        break;
      }

      case SERIALIZABLE_NESTED_OBJECT: {
        SerializableObject* nested_obj =
            reinterpret_cast<SerializableObject*>(members_[i].ptr);
        if (!nested_obj->Serialize(data))
          return false;
        break;
      }

      case SERIALIZABLE_VECTOR | SERIALIZABLE_VALUE_TYPE: {
        // Hack: coerce vector<T> to vector<byte>
        std::vector<byte>* v =
            reinterpret_cast<std::vector<byte>*>(members_[i].ptr);
        if (v->size() != 0) {
          SerializeValueList(data,
                             &v->front(),
                             members_[i].size,
                             v->size() / members_[i].size);
        } else {
          SerializeValueList(data,
                             NULL,
                             members_[i].size,
                             v->size() / members_[i].size);
        }
        break;
      }

      case SERIALIZABLE_VECTOR | SERIALIZABLE_CSTRING: {
        std::vector<CString>* v =
            reinterpret_cast<std::vector<CString>*>(members_[i].ptr);
        SerializeSizeAndCount(data, 1, v->size());
        if (!v->empty()) {
          for (std::vector<CString>::const_iterator it = v->begin();
               it != v->end();
               ++it) {
            SerializeValueList(data,
                               reinterpret_cast<const byte*>(it->GetString()),
                               sizeof(TCHAR),
                               it->GetLength());
          }
        }
        break;
      }

      case SERIALIZABLE_VECTOR | SERIALIZABLE_NESTED_OBJECT: {
        if (!SerializeVectorNestedObject(data, members_[i].ptr))
          return false;
        break;
      }

      default:
        ASSERT(false, (_T("")));
        return false;
    }
  }

  return true;
}

// Serialize the size and count values
void SerializableObject::SerializeSizeAndCount(std::vector<byte>* data,
                                               int size,
                                               int count) const {
  ASSERT(data, (_T("")));
  ASSERT(size >= 0, (_T("")));

  // Get current size
  int pos = data->size();

  // Adjust the size of the data buffer
  data->resize(data->size() + 2 * sizeof(int));

  // Get pointer to the position of data buffer we start to write
  byte* ptr = &((*data)[pos]);

  // Push size
  memcpy(ptr, &size, sizeof(int));
  ptr += sizeof(int);

  // Push count
  memcpy(ptr, &count, sizeof(int));
  ptr += sizeof(int);
}

// Serialize a list of value-typed elements
//
// Args:
//   ser_data:  pointer to the vector for the serialized data
//   raw_data:  pointer to the raw data to be serialized
//   size:      the size of the element in the list
//   count:     the number of the elements in the list
void SerializableObject::SerializeValueList(std::vector<byte>* ser_data,
                                            const byte* raw_data,
                                            int size,
                                            int count) const {
  ASSERT(ser_data, (_T("")));
  ASSERT(size > 0, (_T("")));

  // Serialize the size and count values
  SerializeSizeAndCount(ser_data, size, count);

  // Push data
  if (count > 0) {
    // Get current size
    int pos = ser_data->size();

    // Adjust the size of the data buffer
    ser_data->resize(ser_data->size() + count * size);

    // Get pointer to the position of data buffer we start to write
    byte* ptr = &((*ser_data)[pos]);

    // Copy data
    memcpy(ptr, raw_data, count * size);
  }
}

// Deserialize
bool SerializableObject::Deserialize(byte* data, int size, uint32 version) {
  ASSERT(data, (_T("")));
  ASSERT(size > 0, (_T("")));

  byte* tail = data + size;
  byte** data_ptr = &data;
  if (!DeserializeHelper(data_ptr, size, version))
    return false;

  if (*data_ptr != tail) {
    UTIL_LOG(LE, (_T("[SerializableObject::Deserialize]")
                  _T("[failed to deserialize all data]")));
    return false;
  }

  return true;
}

// Deserialize helper
bool SerializableObject::DeserializeHelper(byte** data,
                                           int size,
                                           uint32 version) {
  ASSERT(data, (_T("")));
  ASSERT(size > 0, (_T("")));

  byte* tail = *data + size;

  for (size_t i = 0; i < members_.size(); ++i) {
    // Ignore those members which are persisted in newer versions
    if (version != kLatestSerializableVersion &&
        members_[i].version  > version) {
      continue;
    }

    switch (members_[i].type) {
      case SERIALIZABLE_VALUE_TYPE:
        if (*data + members_[i].size > tail) {
          UTIL_LOG(L6, (_T("[SerializableObject::DeserializeHelper]")
                        _T("[overflow when deserializing value type]")));
          return false;
        }
        memcpy(members_[i].ptr, *data, members_[i].size);
        *data += members_[i].size;
        break;

      case SERIALIZABLE_CSTRING: {
        std::vector<byte> deser_data;
        if (!DeserializeValueList(&deser_data,
                                  members_[i].size,
                                  data,
                                  tail - *data))
          return false;
        CString* s = reinterpret_cast<CString*>(members_[i].ptr);
        if (deser_data.size() != 0) {
          s->SetString(reinterpret_cast<const TCHAR*>(&deser_data.front()),
                       deser_data.size() / members_[i].size);
        } else {
          s->SetString(_T(""));
        }
        break;
      }

      case SERIALIZABLE_NESTED_OBJECT: {
        SerializableObject* nested_obj =
            reinterpret_cast<SerializableObject*>(members_[i].ptr);
        if (!nested_obj->DeserializeHelper(data, size, version))
          return false;
        break;
      }

      case SERIALIZABLE_VECTOR | SERIALIZABLE_VALUE_TYPE: {
        // Hack: coerce vector<T> to vector<byte>
        std::vector<byte>* v =
            reinterpret_cast<std::vector<byte>*>(members_[i].ptr);
        if (!DeserializeValueList(v, members_[i].size, data, tail - *data))
          return false;
        break;
      }

      case SERIALIZABLE_VECTOR | SERIALIZABLE_CSTRING: {
        std::vector<CString>* v =
              reinterpret_cast<std::vector<CString>*>(members_[i].ptr);
        int count = 0;
        if (!DeserializeSizeAndCount(&count, 1, data, tail - *data))
          return false;
        for (int j = 0; j < count; ++j) {
          std::vector<byte> deser_data;
          if (!DeserializeValueList(&deser_data,
                                    members_[i].size,
                                    data,
                                    tail - *data))
            return false;

          CString s;
          if (deser_data.size() != 0) {
            s = CString(reinterpret_cast<const TCHAR*>(&deser_data.front()),
                        deser_data.size() / members_[i].size);
          }
          v->push_back(s);
        }
        break;
      }

      case SERIALIZABLE_VECTOR | SERIALIZABLE_NESTED_OBJECT: {
        if (!DeserializeVectorNestedObject(data,
                                           tail - *data,
                                           members_[i].ptr,
                                           version))
          return false;
        break;
      }

      default:
        ASSERT(false, (_T("")));
        break;
    }
  }

  return true;
}

// Serialize the size and count values
bool SerializableObject::DeserializeSizeAndCount(int* count,
                                                 int size,
                                                 byte** ser_data,
                                                 int ser_size) const {
  ASSERT(ser_data, (_T("")));
  ASSERT(count, (_T("")));

  byte* ser_tail = *ser_data + ser_size;

  // Check to make sure that the serialization data should at least contain
  // 'size' and 'count'
  if (*ser_data + 2 * sizeof(int) > ser_tail) {
    UTIL_LOG(L6, (_T("[SerializableObject::DeserializeSizeAndCount]")
                  _T("[overflow when deserializing size and count]")));
    return false;
  }

  // Get size
  // If the passing size is 0, skip the size check
  int size2 = *(reinterpret_cast<const int*>(*ser_data));
  *ser_data += sizeof(int);
  if (size && size != size2)
    return false;

  // Get count
  *count = *(reinterpret_cast<const int*>(*ser_data));
  *ser_data += sizeof(int);

  return true;
}

// Deserialize a list of value-typed elements
//
// Args:
//   ser_data:  pointer to the vector for the serialized data
//   size:      the size of the element in the list
//   raw_data:  pointer to the raw data to be serialized
//   ser_size:  size of the serization data
bool SerializableObject::DeserializeValueList(std::vector<byte>* raw_data,
                                              int size,
                                              byte** ser_data,
                                              int ser_size) {
  ASSERT(raw_data, (_T("")));
  ASSERT(ser_data, (_T("")));

  byte* ser_tail = *ser_data + ser_size;

  // Deserialize the size and count values
  int count = 0;
  bool ret = DeserializeSizeAndCount(&count, size, ser_data, ser_size);
  if (!ret)
    return false;

  // Check to make sure that the serialization data is in the right size
  if (*ser_data + count * size > ser_tail) {
    UTIL_LOG(L6, (_T("[SerializableObject::DeserializeValueList]")
                  _T("[overflow when deserializing value list]")));
    return false;
  }

  // Get data
  raw_data->resize(size * count);
  if (count > 0) {
    memcpy(&raw_data->front(), *ser_data, count * size);
    *ser_data += count * size;
  }

  return true;
}

}  // namespace omaha

