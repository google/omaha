// Copyright 2011 Google Inc.
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

// Utility class to manage a set of experiment labels.  Experiment labels
// are a set of key/value pairs used to track an install's "membership" in
// A/B experiment groups issued by the Omaha server.  Keys are strings with
// a limited character set (Perl \w, plus a few characters), while values
// consist of a string and an expiration date.  Label sets are stored per-app
// and transmitted to the server as part of requests (pings / update checks),
// and a delta is potentially returned from the server with each response.
//
// Experiment labels are serialized with key/value separated by an equals, and
// value/expiration by a pipe symbol; the expiration is represented in RFC822
// format.  For example: "test_key=test_value|Fri, 14 Aug 2015 16:13:03 GMT".
// If an app participates in multiple experiments simultaneously, labels are
// concatenated with semicolon delimiters.

#ifndef OMAHA_COMMON_EXPERIMENT_LABELS_H_
#define OMAHA_COMMON_EXPERIMENT_LABELS_H_

#include <atlstr.h>

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "omaha/base/time.h"

namespace omaha {

class ExperimentLabels {
 public:
  ExperimentLabels();
  ~ExperimentLabels();

  // Returns the number of labels in the store.
  size_t NumLabels() const;

  // Returns true if a label with this key exists in the store.
  bool ContainsKey(const CString& key) const;

  // Returns the contents of the Nth label in the experiment set.  Output
  // parameters are allowed to be NULL.  If index is out of range, asserts
  // on a debug build.
  //
  // NOTE: This method is O(n), and indexes are not stable with respect to
  // insertion order; it should not be called from any hot path.  Unit tests
  // and diagnostic tools should use this to enumerate keys.
  void GetLabelByIndex(int index,
                       CString* key,
                       CString* value,
                       time64* expiration) const;

  // If a label with this key exists, returns true and writes the value and
  // expiration to the output parameters.  Output parameters are allowed
  // to be NULL.  If no such key exists, returns false and leaves the outputs
  // unchanged.
  bool FindLabelByKey(const CString& key,
                      CString* value,
                      time64* expiration) const;

  // Attempts to set a label in the store, either adding a new one or updating
  // the value/expiration of an existing one.  Returns true on success.
  bool SetLabel(const CString& key, const CString& value, time64 expiration);

  // Clears a label in the store.  Returns true if a label with that key exists
  // (it is removed); returns false if no label with that key exists.
  bool ClearLabel(const CString& key);

  // Removes any labels from the store whose expiration dates have passed.
  void ExpireLabels();

  // Removes all labels from the store.
  void ClearAllLabels();

  // Concatenates and emits all labels in the store in an XML-friendly format.
  CString Serialize() const;

  // Replaces the current contents of the store with new labels from an
  // XML-friendly string.  On success, returns true; the store reflects the
  // input exactly, with all prior contents are lost.  On failure, returns
  // false and the store's contents are unchanged.
  bool Deserialize(const CString& label_list);

  // Applies the contents of a label list as a delta against the current
  // contents of the store.  On success, returns true, and:
  // * Any expired labels in the list are deleted from the store.
  // * Unexpired labels in the list will be added to the store.  If any keys
  //   already exist in the store, they are overwritten with the input.
  // * Labels that are in the store but not in the input are left unmodified.
  // On failure, returns false, and the store's contents are unchanged.
  // This function's behavior is not modified by SetPreserveExpiredLabels()!
  bool DeserializeAndApplyDelta(const CString& label_list);

  // Modifies the store's handling of labels with expiration dates in the past.
  // By default, SetLabel() will fail to add expired labels, Deserialize() will
  // silently discard them, and Serialize() will not emit them.  Following a
  // call to SetPreserveExpiredLabels(true), these functions will accept and/or
  // include expired labels.
  void SetPreserveExpiredLabels(bool preserve);

  // Serializes a set of experiment labels to application data in the Registry.
  HRESULT WriteToRegistry(bool is_machine, const CString& app_id);

  // Deserializes a set of experiment labels from the Registry.
  HRESULT ReadFromRegistry(bool is_machine, const CString& app_id);

  // Returns true if the supplied string is a valid experiment label set.
  static bool IsStringValidLabelSet(const CString& label_list);

  // Takes the existing label list, applies a delta to it, and returns the new
  // label list.  Returns true on success, false on failure.
  static bool MergeLabelSets(const CString& old_label_list,
                             const CString& new_label_list,
                             CString* merged_list);

 private:
  typedef std::map<CString, std::pair<CString, time64> > LabelMap;

  // Returns true if all characters are in the range [a-zA-Z0-9_\-+,: ].
  // (Perl \w, plus the punctuation necessary for RFC822 dates.)
  static bool IsLabelContentValid(const CString& str);

  // Given an input string of the form "key=value|rfc822_date", converts it
  // into separate key, value, and integer expiration dates.
  static bool SplitCombinedLabel(const CString& combined,
                                 CString* key,
                                 CString* value,
                                 time64* expiration);

  // Splits an input string and applies it against an existing internal map.
  static bool DoDeserialize(LabelMap* map,
                            const CString& label_list,
                            bool accept_expired);

  LabelMap labels_;
  bool preserve_expired_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentLabels);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_EXPERIMENT_LABELS_H_

