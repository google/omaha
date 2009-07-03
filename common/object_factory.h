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

#ifndef OMAHA_COMMON_OBJECT_FACTORY_H__
#define OMAHA_COMMON_OBJECT_FACTORY_H__

#include <map>

namespace omaha {

// Factory creates instances of objects based on a unique type id.
//
// AbstractProduct - base class of the product hierarchy.
// TypeId - type id for each type in the hierarchy.
// ProductCreator - callable entity to create objects.

template <class AbstractProduct,
          typename TypeId,
          typename ProductCreator = AbstractProduct* (*)()>
class Factory {
 public:
  Factory() {}

  // Registers a creator for the type id. Returns true if the creator has
  // been registered succesfully.
  bool Register(const TypeId& id, ProductCreator creator) {
    return id_to_creators_.insert(Map::value_type(id, creator)).second;
  }

  // Unregisters a type id.
  bool Unregister(const TypeId& id) {
    return id_to_creators_.erase(id) == 1;
  }

  // Creates an instance of the abstract product.
  AbstractProduct* CreateObject(const TypeId& id) {
    typename Map::const_iterator it = id_to_creators_.find(id);
    if (it != id_to_creators_.end()) {
      return (it->second)();
    } else {
      return NULL;
    }
  }

 private:
  typedef std::map<TypeId, ProductCreator> Map;
  Map id_to_creators_;

  DISALLOW_EVIL_CONSTRUCTORS(Factory);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_OBJECT_FACTORY_H__

