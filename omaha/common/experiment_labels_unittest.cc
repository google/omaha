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

#include "omaha/common/experiment_labels.h"
#include "omaha/testing/resource.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

#define LABEL_DELIMITER_KV _T("=")
#define LABEL_DELIMITER_EX _T("|")
#define LABEL_DELIMITER_LA _T(";")

#define LABELONE_KEY      _T("test_key_1")
#define LABELONE_VALUE    _T("test_value_1")
#define LABELONE_EXP_STR  _T("Fri, 14 Aug 2015 16:13:03 GMT")
#define LABELONE_EXP_INT  130840423830000000uI64
#define LABELONE_COMBINED LABELONE_KEY \
                          LABEL_DELIMITER_KV \
                          LABELONE_VALUE \
                          LABEL_DELIMITER_EX \
                          LABELONE_EXP_STR

#define LABELTWO_KEY      _T("test_key_2")
#define LABELTWO_VALUE    _T("test_value_2")
#define LABELTWO_EXP_STR  _T("Thu, 27 Nov 2014 23:59:59 GMT")
#define LABELTWO_EXP_INT  130616063990000000uI64
#define LABELTWO_COMBINED LABELTWO_KEY \
                          LABEL_DELIMITER_KV \
                          LABELTWO_VALUE \
                          LABEL_DELIMITER_EX \
                          LABELTWO_EXP_STR

#define LABELOLD_KEY      _T("test_key_old")
#define LABELOLD_VALUE    _T("test_value_old")
#define LABELOLD_EXP_STR  _T("Mon, 04 Jan 2010 08:00:00 GMT")
#define LABELOLD_EXP_INT  129070656000000000uI64
#define LABELOLD_COMBINED LABELOLD_KEY \
                          LABEL_DELIMITER_KV \
                          LABELOLD_VALUE \
                          LABEL_DELIMITER_EX \
                          LABELOLD_EXP_STR


const TCHAR* const kLabelOneKey =      LABELONE_KEY;
const TCHAR* const kLabelOneValue =    LABELONE_VALUE;
const TCHAR* const kLabelOneExpStr =   LABELONE_EXP_STR;
const time64       kLabelOneExpInt =   LABELONE_EXP_INT;
const TCHAR* const kLabelOneCombined = LABELONE_COMBINED;

const TCHAR* const kLabelTwoKey =      LABELTWO_KEY;
const TCHAR* const kLabelTwoValue =    LABELTWO_VALUE;
const TCHAR* const kLabelTwoExpStr =   LABELTWO_EXP_STR;
const time64       kLabelTwoExpInt =   LABELTWO_EXP_INT;
const TCHAR* const kLabelTwoCombined = LABELTWO_COMBINED;

const TCHAR* const kLabelOldKey =      LABELOLD_KEY;
const TCHAR* const kLabelOldValue =    LABELOLD_VALUE;
const TCHAR* const kLabelOldExpStr =   LABELOLD_EXP_STR;
const time64       kLabelOldExpInt =   LABELOLD_EXP_INT;
const TCHAR* const kLabelOldCombined = LABELOLD_COMBINED;

const TCHAR* const kLabelNewCombined = LABELONE_COMBINED
                                       LABEL_DELIMITER_LA
                                       LABELTWO_COMBINED;

const TCHAR* const kLabelAllCombined = LABELONE_COMBINED
                                       LABEL_DELIMITER_LA
                                       LABELTWO_COMBINED
                                       LABEL_DELIMITER_LA
                                       LABELOLD_COMBINED;

}  // end namespace

TEST(ExperimentLabelsTest, Empty) {
  ExperimentLabels el;

  EXPECT_EQ(0, el.NumLabels());
  EXPECT_FALSE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));
  EXPECT_FALSE(el.FindLabelByKey(kLabelOneKey, NULL, NULL));
  EXPECT_FALSE(el.FindLabelByKey(kLabelTwoKey, NULL, NULL));
  EXPECT_FALSE(el.FindLabelByKey(kLabelOldKey, NULL, NULL));
}

TEST(ExperimentLabelsTest, BasicOperations) {
  ExperimentLabels el;

  EXPECT_EQ(0, el.NumLabels());
  EXPECT_FALSE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  // Start by adding a single label.
  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  // Add both future labels now.
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelTwoValue, kLabelTwoExpInt));
  EXPECT_EQ(2, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  // Clear the first label; it should not appear.
  EXPECT_TRUE(el.ClearLabel(kLabelOneKey));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_FALSE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  // ClearLabel should return false if the key isn't present.
  EXPECT_FALSE(el.ClearLabel(kLabelOldKey));

  // ClearAllLabels should clear all labels.
  el.ClearAllLabels();
  EXPECT_EQ(0, el.NumLabels());
  EXPECT_FALSE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));
}

TEST(ExperimentLabelsTest, SetInvalidParameters) {
  ExperimentLabels el;

  // Verify that our core test parameters are okay by adding a key, then
  // clearing all.
  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  el.ClearAllLabels();

  // Don't allow zero length names or values, or expirations in the past.
  EXPECT_FALSE(el.SetLabel(_T(""), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T(""), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, kLabelOneValue, 0));

  // Explicitly validate that names and values do not contain delimiters,
  // quotes, angle brackets, question marks, or ampersands, as these could
  // break parsing at the label-string, extraargs, or XML levels.
  EXPECT_FALSE(el.SetLabel(_T("=test"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("te=st"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test="), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("|test"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("te|st"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test|"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test<"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test>"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test&"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test?"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test^"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test'"), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(_T("test\""), kLabelOneValue, kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("=test"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("te=st"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test="), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("|test"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("te|st"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test|"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test<"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test>"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test&"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test?"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test^"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test'"), kLabelOneExpInt));
  EXPECT_FALSE(el.SetLabel(kLabelOneKey, _T("test\""), kLabelOneExpInt));
  EXPECT_EQ(0, el.NumLabels());

  // Allow an expiration in the past if and only if we override the default
  // preserve-expired-labels setting.
  el.SetPreserveExpiredLabels(true);
  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOldExpInt));
  EXPECT_EQ(1, el.NumLabels());
}

TEST(ExperimentLabelsTest, SetWillOverwrite) {
  ExperimentLabels el;
  CString value;
  time64 expiration;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));

  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelTwoValue, kLabelTwoExpInt));

  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelTwoValue);
  EXPECT_EQ(expiration, kLabelTwoExpInt);
}

TEST(ExperimentLabelsTest, FindLabelByKey) {
  ExperimentLabels el;
  CString value;
  time64 expiration;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelTwoValue, kLabelTwoExpInt));
  EXPECT_EQ(2, el.NumLabels());

  // Safely return partial values if either or both out parameters are NULL.
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, NULL, NULL));
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, NULL));
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, NULL, &expiration));
  EXPECT_EQ(expiration, kLabelOneExpInt);

  // Overwrite values if they already have values.
  EXPECT_TRUE(el.FindLabelByKey(kLabelTwoKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelTwoValue);
  EXPECT_EQ(expiration, kLabelTwoExpInt);

  // If the key isn't present, return false; both out-params are unmodified.
  EXPECT_FALSE(el.FindLabelByKey(kLabelOldKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelTwoValue);
  EXPECT_EQ(expiration, kLabelTwoExpInt);
}

TEST(ExperimentLabelsTest, Serialize_Empty) {
  ExperimentLabels el;

  CString serialized = el.Serialize();
  EXPECT_STREQ(serialized, _T(""));
}

TEST(ExperimentLabelsTest, Serialize_Single_Valid) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_EQ(1, el.NumLabels());

  CString serialized = el.Serialize();
  EXPECT_STREQ(serialized, kLabelOneCombined);
}

TEST(ExperimentLabelsTest, Serialize_Single_Expired) {
  ExperimentLabels el;

  el.SetPreserveExpiredLabels(true);
  EXPECT_TRUE(el.SetLabel(kLabelOldKey, kLabelOldValue, kLabelOldExpInt));
  EXPECT_EQ(1, el.NumLabels());

  el.SetPreserveExpiredLabels(false);
  CString serialized = el.Serialize();
  EXPECT_STREQ(serialized, _T(""));

  el.SetPreserveExpiredLabels(true);
  serialized = el.Serialize();
  EXPECT_STREQ(serialized, kLabelOldCombined);
}

TEST(ExperimentLabelsTest, Serialize_Multi_Valid) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelTwoValue, kLabelTwoExpInt));
  EXPECT_EQ(2, el.NumLabels());

  CString serialized = el.Serialize();
  EXPECT_STREQ(serialized, kLabelNewCombined);
}

TEST(ExperimentLabelsTest, Serialize_Multi_Valid_Expired) {
  ExperimentLabels el;

  el.SetPreserveExpiredLabels(true);
  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelTwoValue, kLabelTwoExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelOldKey, kLabelOldValue, kLabelOldExpInt));
  EXPECT_EQ(3, el.NumLabels());

  el.SetPreserveExpiredLabels(false);
  CString serialized = el.Serialize();
  EXPECT_STREQ(serialized, kLabelNewCombined);

  el.SetPreserveExpiredLabels(true);
  serialized = el.Serialize();
  EXPECT_STREQ(serialized, kLabelAllCombined);
}


TEST(ExperimentLabelsTest, Deserialize_EmptyString) {
  ExperimentLabels el;

  EXPECT_TRUE(el.Deserialize(_T("")));
  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, Deserialize_Single_Valid) {
  ExperimentLabels el;

  EXPECT_TRUE(el.Deserialize(kLabelOneCombined));
  EXPECT_EQ(1, el.NumLabels());

  CString key;
  CString value;
  time64 expiration;
  el.GetLabelByIndex(0, &key, &value, &expiration);
  EXPECT_STREQ(key, kLabelOneKey);
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);
}

TEST(ExperimentLabelsTest, Deserialize_Multi_Valid) {
  ExperimentLabels el;

  EXPECT_TRUE(el.Deserialize(kLabelNewCombined));
  EXPECT_EQ(2, el.NumLabels());

  CString key;
  CString value;
  time64 expiration;
  el.GetLabelByIndex(0, &key, &value, &expiration);
  EXPECT_STREQ(key, kLabelOneKey);
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);
  el.GetLabelByIndex(1, &key, &value, &expiration);
  EXPECT_STREQ(key, kLabelTwoKey);
  EXPECT_STREQ(value, kLabelTwoValue);
  EXPECT_EQ(expiration, kLabelTwoExpInt);
}

TEST(ExperimentLabelsTest, Deserialize_Single_Valid_Expired) {
  ExperimentLabels el;

  EXPECT_TRUE(el.Deserialize(kLabelOldCombined));
  EXPECT_EQ(0, el.NumLabels());

  el.SetPreserveExpiredLabels(true);
  EXPECT_TRUE(el.Deserialize(kLabelOldCombined));
  EXPECT_EQ(1, el.NumLabels());

  CString key;
  CString value;
  time64 expiration;
  el.GetLabelByIndex(0, &key, &value, &expiration);
  EXPECT_STREQ(key, kLabelOldKey);
  EXPECT_STREQ(value, kLabelOldValue);
  EXPECT_EQ(expiration, kLabelOldExpInt);
}

TEST(ExperimentLabelsTest, Deserialize_Multi_Valid_Expired) {
  ExperimentLabels el;

  EXPECT_TRUE(el.Deserialize(kLabelAllCombined));
  EXPECT_EQ(2, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  el.ClearAllLabels();

  el.SetPreserveExpiredLabels(true);
  EXPECT_TRUE(el.Deserialize(kLabelAllCombined));
  EXPECT_EQ(3, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_TRUE(el.ContainsKey(kLabelOldKey));
}

TEST(ExperimentLabelsTest, Deserialize_Invalid_ZeroLengthKey) {
  ExperimentLabels el;

  // Base case: "=valid_value|valid_exp"
  const TCHAR* const invalid_segment = LABEL_DELIMITER_KV
                                       LABELONE_VALUE
                                       LABEL_DELIMITER_EX
                                       LABELONE_EXP_STR;
  EXPECT_FALSE(el.Deserialize(invalid_segment));
  EXPECT_EQ(0, el.NumLabels());

  // Variant case 1: "=valid_value|valid_exp;valid_key=valid_value|valid_exp"
  CString variant1 = invalid_segment;
  variant1.Append(LABEL_DELIMITER_LA);
  variant1.Append(LABELTWO_COMBINED);
  EXPECT_FALSE(el.Deserialize(variant1));
  EXPECT_EQ(0, el.NumLabels());

  // Variant case 2: "valid_key=valid_value|valid_exp;=valid_value|valid_exp"
  CString variant2 = LABELTWO_COMBINED;
  variant2.Append(LABEL_DELIMITER_LA);
  variant2.Append(invalid_segment);
  EXPECT_FALSE(el.Deserialize(variant2));
  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, Deserialize_Invalid_ZeroLengthValue) {
  ExperimentLabels el;

  // Base case: "valid_key=|valid_exp"
  const TCHAR* const invalid_segment = LABELONE_KEY
                                       LABEL_DELIMITER_KV
                                       LABEL_DELIMITER_EX
                                       LABELONE_EXP_STR;
  EXPECT_FALSE(el.Deserialize(invalid_segment));
  EXPECT_EQ(0, el.NumLabels());

  // Variant case 1: "valid_key=|valid_exp;valid_key=valid_value|valid_exp"
  CString variant1 = invalid_segment;
  variant1.Append(LABEL_DELIMITER_LA);
  variant1.Append(LABELTWO_COMBINED);
  EXPECT_FALSE(el.Deserialize(variant1));
  EXPECT_EQ(0, el.NumLabels());

  // Variant case 2: "valid_key=valid_value|valid_exp;valid_key=|valid_exp"
  el.ClearAllLabels();
  CString variant2 = LABELTWO_COMBINED;
  variant2.Append(LABEL_DELIMITER_LA);
  variant2.Append(invalid_segment);
  EXPECT_FALSE(el.Deserialize(variant2));
  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, Deserialize_Invalid_ZeroLengthDate) {
  ExperimentLabels el;

  // Base case: "valid_key=valid_value|"
  const TCHAR* const invalid_segment = LABELONE_KEY
                                       LABEL_DELIMITER_KV
                                       LABELONE_VALUE
                                       LABEL_DELIMITER_EX;
  EXPECT_FALSE(el.Deserialize(invalid_segment));
  EXPECT_EQ(0, el.NumLabels());

  // Variant case 1: "valid_key=valid_value|;valid_key=valid_value|valid_exp"
  CString variant1 = invalid_segment;
  variant1.Append(LABEL_DELIMITER_LA);
  variant1.Append(LABELTWO_COMBINED);
  EXPECT_FALSE(el.Deserialize(variant1));
  EXPECT_EQ(0, el.NumLabels());

  // Variant case 2: "valid_key=valid_value|valid_exp;valid_key=valid_value|"
  CString variant2 = LABELTWO_COMBINED;
  variant2.Append(LABEL_DELIMITER_LA);
  variant2.Append(invalid_segment);
  EXPECT_FALSE(el.Deserialize(variant2));
  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, Deserialize_Invalid_ExtraDelimiters) {
  ExperimentLabels el;

  // Repeated equals: "k==v|e"
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABEL_DELIMITER_KV
                              LABEL_DELIMITER_KV
                              LABELONE_VALUE
                              LABEL_DELIMITER_EX
                              LABELONE_EXP_STR));

  // Repeated pipe: "k=v||e"
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABEL_DELIMITER_KV
                              LABELONE_VALUE
                              LABEL_DELIMITER_EX
                              LABEL_DELIMITER_EX
                              LABELONE_EXP_STR));

  // Degenerate: "=|;=|"
  EXPECT_FALSE(el.Deserialize(LABEL_DELIMITER_KV
                              LABEL_DELIMITER_EX
                              LABEL_DELIMITER_LA
                              LABEL_DELIMITER_KV
                              LABEL_DELIMITER_EX));

  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, Deserialize_Valid_ExtraLabelDelimiters) {
  ExperimentLabels el;

  // We support (but discourage) leading/trailing/repeated separators;
  // for example, "k=v|e;;;;k=v|e" should cleanly parse (two keys), as
  // should ";;;k=v|e" and "k=v|e;;;" (one key each) and ";;;;;" (no keys).

  // Repeated separator: "k=v|e;;k=v|e"
  EXPECT_TRUE(el.Deserialize(LABELONE_COMBINED
                             LABEL_DELIMITER_LA
                             LABEL_DELIMITER_LA
                             LABELTWO_COMBINED));
  EXPECT_EQ(2, el.NumLabels());
  el.ClearAllLabels();

  // Leading separator: ";k=v|e"
  EXPECT_TRUE(el.Deserialize(LABEL_DELIMITER_LA LABELONE_COMBINED));
  EXPECT_EQ(1, el.NumLabels());
  el.ClearAllLabels();

  // Trailing separator: "k=v|e;"
  EXPECT_TRUE(el.Deserialize(LABELONE_COMBINED LABEL_DELIMITER_LA));
  EXPECT_EQ(1, el.NumLabels());
  el.ClearAllLabels();

  // Degenerate: ";;;"
  EXPECT_TRUE(el.Deserialize(LABEL_DELIMITER_LA
                             LABEL_DELIMITER_LA
                             LABEL_DELIMITER_LA));
  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, Deserialize_Invalid_MissingDelimiters) {
  ExperimentLabels el;

  // Missing all delimiters, single string (kve)
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABELONE_VALUE
                              LABELONE_EXP_STR));

  // Missing expiration delimiter, single string (k=ve)
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABEL_DELIMITER_KV
                              LABELONE_VALUE
                              LABELONE_EXP_STR));

  // Missing value delimiter, single string (kv|e)
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABELONE_VALUE
                              LABEL_DELIMITER_EX
                              LABELONE_EXP_STR));

  // Missing label delimiter, multi-label (k=v|ek=v|e)
  EXPECT_FALSE(el.Deserialize(LABELONE_COMBINED LABELTWO_COMBINED));

  // Missing value+exp delimiter, multi-label, first (kve;k=v|e)
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABELONE_VALUE
                              LABELONE_EXP_STR
                              LABEL_DELIMITER_LA
                              LABELTWO_COMBINED));

  // Missing value+exp delimiter, multi-label, second (k=v|e;kve)
  EXPECT_FALSE(el.Deserialize(LABELONE_COMBINED
                              LABEL_DELIMITER_LA
                              LABELTWO_KEY
                              LABELTWO_VALUE
                              LABELTWO_EXP_STR));

  // Missing value delimiter, multi-label, first (kv|e;k=v|e)
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABELONE_VALUE
                              LABEL_DELIMITER_EX
                              LABELONE_EXP_STR
                              LABEL_DELIMITER_LA
                              LABELTWO_COMBINED));

  // Missing value delimiter, multi-label, second (k=v|e;kv|e)
  EXPECT_FALSE(el.Deserialize(LABELONE_COMBINED
                              LABEL_DELIMITER_LA
                              LABELTWO_KEY
                              LABELTWO_VALUE
                              LABEL_DELIMITER_EX
                              LABELTWO_EXP_STR));

  // Missing expiration delimiter, multi-label, first (k=ve;k=v|e)
  EXPECT_FALSE(el.Deserialize(LABELONE_KEY
                              LABEL_DELIMITER_KV
                              LABELONE_VALUE
                              LABELONE_EXP_STR
                              LABEL_DELIMITER_LA
                              LABELTWO_COMBINED));

  // Missing expiration delimiter, multi-label, second (k=v|e;k=ve)
  EXPECT_FALSE(el.Deserialize(LABELONE_COMBINED
                              LABEL_DELIMITER_LA
                              LABELTWO_KEY
                              LABEL_DELIMITER_KV
                              LABELTWO_VALUE
                              LABELTWO_EXP_STR));

  EXPECT_EQ(0, el.NumLabels());
}

TEST(ExperimentLabelsTest, DeserializeAndApplyDelta_Append) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.DeserializeAndApplyDelta(kLabelTwoCombined));
  EXPECT_EQ(2, el.NumLabels());

  CString value;
  time64 expiration;
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);
  EXPECT_TRUE(el.FindLabelByKey(kLabelTwoKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelTwoValue);
  EXPECT_EQ(expiration, kLabelTwoExpInt);
}

TEST(ExperimentLabelsTest, DeserializeAndApplyDelta_Append_Expired) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  EXPECT_TRUE(el.DeserializeAndApplyDelta(kLabelOldCombined));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));
}

TEST(ExperimentLabelsTest, DeserializeAndApplyDelta_Overwrite_Single) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOldValue, kLabelOneExpInt));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));

  CString value;
  time64 expiration;
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelOldValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);

  EXPECT_TRUE(el.DeserializeAndApplyDelta(kLabelOneCombined));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));

  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);
}

TEST(ExperimentLabelsTest, DeserializeAndApplyDelta_Overwrite_Multi) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOldValue, kLabelOneExpInt));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_FALSE(el.ContainsKey(kLabelTwoKey));

  EXPECT_TRUE(el.DeserializeAndApplyDelta(kLabelNewCombined));
  EXPECT_EQ(2, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));

  CString value;
  time64 expiration;
  EXPECT_TRUE(el.FindLabelByKey(kLabelOneKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelOneValue);
  EXPECT_EQ(expiration, kLabelOneExpInt);
  EXPECT_TRUE(el.FindLabelByKey(kLabelTwoKey, &value, &expiration));
  EXPECT_STREQ(value, kLabelTwoValue);
  EXPECT_EQ(expiration, kLabelTwoExpInt);
}

TEST(ExperimentLabelsTest, DeserializeAndApplyDelta_Overwrite_Single_Expired) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOldKey, kLabelOldValue, kLabelOneExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelTwoValue, kLabelTwoExpInt));
  EXPECT_EQ(2, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOldKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));

  EXPECT_TRUE(el.DeserializeAndApplyDelta(kLabelOldCombined));
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
}

TEST(ExperimentLabelsTest, DeserializeAndApplyDelta_Overwrite_Multi_Expired) {
  ExperimentLabels el;

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelTwoValue, kLabelOneExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelOneValue, kLabelTwoExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelOldKey, kLabelOldValue, kLabelOneExpInt));
  EXPECT_EQ(3, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_TRUE(el.ContainsKey(kLabelOldKey));

  EXPECT_TRUE(el.DeserializeAndApplyDelta(kLabelAllCombined));
  EXPECT_EQ(2, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));
}

TEST(ExperimentLabelsTest, Expire) {
  ExperimentLabels el;

  el.SetPreserveExpiredLabels(true);
  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOneExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelTwoKey, kLabelTwoValue, kLabelTwoExpInt));
  EXPECT_TRUE(el.SetLabel(kLabelOldKey, kLabelOldValue, kLabelOldExpInt));
  EXPECT_EQ(3, el.NumLabels());

  el.ExpireLabels();
  EXPECT_EQ(2, el.NumLabels());
  EXPECT_TRUE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));

  EXPECT_TRUE(el.SetLabel(kLabelOneKey, kLabelOneValue, kLabelOldExpInt));
  el.ExpireLabels();
  EXPECT_EQ(1, el.NumLabels());
  EXPECT_FALSE(el.ContainsKey(kLabelOneKey));
  EXPECT_TRUE(el.ContainsKey(kLabelTwoKey));
  EXPECT_FALSE(el.ContainsKey(kLabelOldKey));
}

}  // namespace omaha

