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
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/const_goopdate.h"

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

CString ExperimentLabels::Serialize(SerializeOptions options) const {
  CString serialized;
  const time64 current_time = GetCurrent100NSTime();
  for (LabelMap::const_iterator cit = labels_.begin();
       cit != labels_.end();
       ++cit) {
    if (preserve_expired_ || cit->second.second >= current_time) {
      if (!serialized.IsEmpty()) {
        serialized.Append(L";");
      }
      SafeCStringAppendFormat(&serialized, L"%s=%s",
                              cit->first, cit->second.first);
      if (options & SerializeOptions::INCLUDE_TIMESTAMPS) {
        FILETIME ft = {};
        Time64ToFileTime(cit->second.second, &ft);
        SafeCStringAppendFormat(&serialized, L"|%s",
                                ConvertTimeToGMTString(&ft));
      }
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
  return RegKey::SetValue(
      app_registry_utils::GetAppClientStateKey(is_machine, app_id),
      kRegValueExperimentLabels,
      Serialize(SerializeOptions::INCLUDE_TIMESTAMPS));
}

HRESULT ExperimentLabels::ReadFromRegistry(bool is_machine,
                                           const CString& app_id) {
  CString label_list;
  const CString state_key(
      app_registry_utils::GetAppClientStateKey(is_machine, app_id));
  if (RegKey::HasValue(state_key, kRegValueExperimentLabels)) {
    VERIFY_SUCCEEDED(RegKey::GetValue(
        state_key, kRegValueExperimentLabels, &label_list));
  }

  if (!Deserialize(label_list)) {
    return E_FAIL;
  }

  if (!is_machine) {
    return S_OK;
  }

  // If we're running as machine, check the ClientStateMedium key as well,
  // and integrate it into ClientState.
  const CString med_state_key(
      app_registry_utils::GetAppClientStateMediumKey(true, app_id));
  if (RegKey::HasValue(med_state_key, kRegValueExperimentLabels)) {
    VERIFY_SUCCEEDED(RegKey::GetValue(
        med_state_key, kRegValueExperimentLabels, &label_list));
  }

  if (!label_list.IsEmpty()) {
    if (!DeserializeAndApplyDelta(label_list)) {
      return E_FAIL;
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

CString ExperimentLabels::CreateLabel(const CString& key,
                                      const CString& value,
                                      time64 expiration) {
  ExperimentLabels label;
  if (!label.SetLabel(key, value, expiration)) {
    return CString();
  }

  return label.Serialize(SerializeOptions::INCLUDE_TIMESTAMPS);
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

  *merged_list = labels.Serialize(SerializeOptions::INCLUDE_TIMESTAMPS);
  return true;
}

CString ExperimentLabels::ReadRegistry(bool is_machine, const CString& app_id) {
  ExperimentLabels stored_labels;
  VERIFY_SUCCEEDED(stored_labels.ReadFromRegistry(is_machine, app_id));
  return stored_labels.Serialize(SerializeOptions::INCLUDE_TIMESTAMPS);
}

HRESULT ExperimentLabels::WriteRegistry(bool is_machine,
                                        const CString& app_id,
                                        const CString& new_labels) {
  if (new_labels.IsEmpty()) {
    return S_OK;
  }

  // Read any existing experiment labels for this app in the Registry.
  ExperimentLabels labels;
  HRESULT hr = labels.ReadFromRegistry(is_machine, app_id);

  // If there are existing labels, attempt to merge them with the new labels. If
  // no existing labels, attempt to use just the new labels.
  bool deserialize_succeeded = SUCCEEDED(hr) ?
                               labels.DeserializeAndApplyDelta(new_labels) :
                               labels.Deserialize(new_labels);
  if (!deserialize_succeeded) {
    OPT_LOG(LE, (_T("[New experiment labels are unparsable][%s]"), new_labels));
    return E_INVALIDARG;
  }

  hr = labels.WriteToRegistry(is_machine, app_id);

  // If running as machine, delete any ClientStateMedium entry, since we have
  // already merged the ClientStateMedium data above using ReadFromRegistry,
  // Deserialize, and WriteToRegistry.
  if (is_machine) {
    const CString med_state_key(
        app_registry_utils::GetAppClientStateMediumKey(is_machine, app_id));
    if (RegKey::HasValue(med_state_key, kRegValueExperimentLabels)) {
      VERIFY_SUCCEEDED(
          RegKey::DeleteValue(med_state_key, kRegValueExperimentLabels));
    }
  }

  return hr;
}

CString ExperimentLabels::RemoveTimestamps(const CString& labels) {
  ExperimentLabels stored_labels;
  VERIFY_SUCCEEDED(stored_labels.Deserialize(labels));

  return stored_labels.Serialize(SerializeOptions::EXCLUDE_TIMESTAMPS);
}

}  // namespace omaha

