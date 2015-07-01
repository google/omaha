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

#include "omaha/common/experiment_labels.h"

#include "omaha/base/debug.h"
#include "omaha/base/safe_format.h"
#include "omaha/common/app_registry_utils.h"

namespace omaha {

ExperimentLabels::ExperimentLabels() : labels_(), preserve_expired_(false) {}

ExperimentLabels::~ExperimentLabels() {}

size_t ExperimentLabels::NumLabels() const {
  return labels_.size();
}

bool ExperimentLabels::ContainsKey(const CString& key) const {
  ASSERT1(!key.IsEmpty());
  return labels_.find(key) != labels_.end();
}

void ExperimentLabels::GetLabelByIndex(int index, CString* key, CString* value,
                                       time64* expiration) const {
  ASSERT1(index >= 0);
  ASSERT1(static_cast<LabelMap::size_type>(index) < labels_.size());

  LabelMap::const_iterator cit = labels_.begin();
  std::advance(cit, static_cast<LabelMap::size_type>(index));
  if (key) {
    *key = cit->first;
  }
  if (value) {
    *value = cit->second.first;
  }
  if (expiration) {
    *expiration = cit->second.second;
  }
}

bool ExperimentLabels::FindLabelByKey(const CString& key, CString* value,
                                      time64* expiration) const {
  ASSERT1(!key.IsEmpty());
  LabelMap::const_iterator cit = labels_.find(key);
  if (labels_.end() == cit) {
    return false;
  }

  if (value) {
    *value = cit->second.first;
  }
  if (expiration) {
    *expiration = cit->second.second;
  }
  return true;
}

bool ExperimentLabels::SetLabel(const CString& key, const CString& value,
                                time64 expiration) {
  if (!IsLabelContentValid(key) || !IsLabelContentValid(value)) {
    return false;
  }
  if (expiration < GetCurrent100NSTime() && !preserve_expired_) {
    return false;
  }
  labels_[key] = std::make_pair(value, expiration);
  return true;
}

bool ExperimentLabels::ClearLabel(const CString& key) {
  LabelMap::iterator it = labels_.find(key);
  if (labels_.end() == it) {
    return false;
  }
  labels_.erase(it);
  return true;
}

void ExperimentLabels::ExpireLabels() {
  time64 current_time = GetCurrent100NSTime();
  for (LabelMap::iterator it = labels_.begin(); it != labels_.end(); ++it) {
    if (it->second.second < current_time) {
      it = labels_.erase(it);
      if (it == labels_.end()) {
        break;
      }
    }
  }
}

void ExperimentLabels::ClearAllLabels() {
  labels_.clear();
}

CString ExperimentLabels::Serialize() const {
  CString serialized;
  time64 current_time = GetCurrent100NSTime();
  for (LabelMap::const_iterator cit = labels_.begin();
       cit != labels_.end();
       ++cit) {
    if (preserve_expired_ || cit->second.second >= current_time) {
      if (!serialized.IsEmpty()) {
        serialized.Append(L";");
      }
      FILETIME ft = {};
      Time64ToFileTime(cit->second.second, &ft);
      SafeCStringAppendFormat(&serialized, L"%s=%s|%s",
                              cit->first,
                              cit->second.first,
                              ConvertTimeToGMTString(&ft));
    }
  }
  return serialized;
}

bool ExperimentLabels::Deserialize(const CString& label_list) {
  LabelMap new_labels;
  if (DoDeserialize(&new_labels, label_list, preserve_expired_)) {
    std::swap(labels_, new_labels);
    return true;
  }
  return false;
}

bool ExperimentLabels::DeserializeAndApplyDelta(const CString& label_list) {
  LabelMap merged_labels = labels_;
  if (DoDeserialize(&merged_labels, label_list, false)) {
    std::swap(labels_, merged_labels);
    return true;
  }
  return false;
}

void ExperimentLabels::SetPreserveExpiredLabels(bool preserve) {
  preserve_expired_ = preserve;
}

HRESULT ExperimentLabels::WriteToRegistry(bool is_machine,
                                          const CString& app_id) {
  return app_registry_utils::SetExperimentLabels(is_machine, app_id,
                                                 Serialize());
}

HRESULT ExperimentLabels::ReadFromRegistry(bool is_machine,
                                           const CString& app_id) {
  CString label_list;
  HRESULT hr = app_registry_utils::GetExperimentLabels(is_machine, app_id,
                                                       &label_list);
  if (FAILED(hr)) {
    return hr;
  }
  if (!Deserialize(label_list)) {
    return E_FAIL;
  }

  // If we're running as machine, check the ClientStateMedium key as well,
  // and integrate it into ClientState.
  if (is_machine) {
    hr = app_registry_utils::GetExperimentLabelsMedium(app_id, &label_list);
    if (FAILED(hr)) {
      return hr;
    }
    if (!label_list.IsEmpty()) {
      if (!DeserializeAndApplyDelta(label_list)) {
        return E_FAIL;
      }
    }
  }

  return S_OK;
}

bool ExperimentLabels::IsStringValidLabelSet(const CString& label_list) {
  ExperimentLabels labels;
  return labels.Deserialize(label_list);
}

bool ExperimentLabels::IsLabelContentValid(const CString& str) {
  if (str.IsEmpty()) {
    return false;
  }
  for (int i = 0; i < str.GetLength(); ++i) {
    wchar_t ch = str[i];
    if (!((ch >= L'+' && ch <= L'-') ||
          (ch >= L'0' && ch <= L':') ||
          (ch >= L'A' && ch <= L'Z') ||
          (ch >= L'a' && ch <= L'z') ||
          (ch == L'_') || (ch == L' ') ||
          (ch == L'/') || (ch == L'\\') ||
          (ch == L'.') )) {
      return false;
    }
  }
  return true;
}

bool ExperimentLabels::SplitCombinedLabel(const CString& combined, CString* key,
                                          CString* value, time64* expiration) {
  ASSERT1(!combined.IsEmpty());
  ASSERT1(key);
  ASSERT1(value);
  ASSERT1(expiration);

  int value_offset = combined.Find(L'=');
  if (value_offset <= 0 || value_offset == combined.GetLength()) {
    return false;
  }
  *key = combined.Left(value_offset);
  if (!IsLabelContentValid(*key)) {
    return false;
  }
  ++value_offset;

  int expiration_offset = combined.Find(L'|', value_offset);
  if (expiration_offset <= value_offset ||
      expiration_offset == combined.GetLength()) {
    return false;
  }
  *value = combined.Mid(value_offset, expiration_offset - value_offset);
  if (!IsLabelContentValid(*value)) {
    return false;
  }
  ++expiration_offset;

  CString expiration_string = combined.Mid(expiration_offset);
  if (!IsLabelContentValid(expiration_string)) {
    return false;
  }
  SYSTEMTIME system_time = {};
  if (!RFC822DateToSystemTime(expiration_string, &system_time, false)) {
    return false;
  }
  *expiration = SystemTimeToTime64(&system_time);

  return true;
}

bool ExperimentLabels::DoDeserialize(LabelMap* map, const CString& label_list,
                                     bool accept_expired) {
  ASSERT1(map);

  if (label_list.IsEmpty()) {
    return true;
  }

  time64 current_time = GetCurrent100NSTime();

  for (int offset = 0;;) {
    CString combined_label = label_list.Tokenize(L";", offset);
    if (combined_label.IsEmpty()) {
      if (offset < 0) {
        break;  // Natural end-of-string reached.
      }
      return false;
    }

    CString key;
    CString value;
    time64 expiration = 0;
    if (!SplitCombinedLabel(combined_label, &key, &value, &expiration)) {
      return false;
    }

    // If the label is well-formatted but expired, we accept the input, but
    // do not add it to the map and do not emit an error.  If there is already
    // a label in the map with that key, delete it.
    if (accept_expired || expiration > current_time) {
      (*map)[key] = std::make_pair(value, expiration);
    } else {
      map->erase(key);
    }
  }

  return true;
}

bool ExperimentLabels::MergeLabelSets(const CString& old_label_list,
                                      const CString& new_label_list,
                                      CString* merged_list) {
  ExperimentLabels labels;
  if (!labels.Deserialize(old_label_list)) {
    return false;
  }
  if (!labels.DeserializeAndApplyDelta(new_label_list)) {
    return false;
  }
  *merged_list = labels.Serialize();
  return true;
}

}  // namespace omaha

