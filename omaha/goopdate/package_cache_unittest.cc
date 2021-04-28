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

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/package_cache.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR* kFile1Sha256Hash =
    _T("49b45f78865621b154fa65089f955182345a67f9746841e43e2d6daa288988d0");
const TCHAR* kFile2Sha256Hash =
    _T("f0bbd84d7ec364f6c33161d781b49d840ed792b8b10668c4180b9e6e128d0bc9");

}  // namespace

class PackageCacheTest : public testing::TestWithParam<bool> {
 protected:
  typedef PackageCache::Key Key;

  PackageCacheTest()
      : cache_root_(GetUniqueTempDirectoryName()),
      size_file1_(0),
      size_file2_(0) {
    EXPECT_FALSE(cache_root_.IsEmpty());

    CString executable_path(app_util::GetCurrentModuleDirectory());

    source_file1_ = ConcatenatePath(
        executable_path,
        _T("unittest_support\\download_cache_test\\")
        _T("{89640431-FE64-4da8-9860-1A1085A60E13}\\gears-win32-opt.msi"));
    hash_file1_ = kFile1Sha256Hash;
    size_file1_ = 870400;

    source_file2_ = ConcatenatePath(
        executable_path,
        _T("unittest_support\\download_cache_test\\")
        _T("{7101D597-3481-4971-AD23-455542964072}\\livelysetup.exe"));
    hash_file2_ = kFile2Sha256Hash;
    size_file2_ = 479848;

    EXPECT_TRUE(File::Exists(source_file1_));
    EXPECT_TRUE(File::Exists(source_file2_));

    EXPECT_SUCCEEDED(source_file1_file_.OpenShareMode(source_file1_,
                                                      false,
                                                      false,
                                                      FILE_SHARE_READ));
    EXPECT_SUCCEEDED(source_file2_file_.OpenShareMode(source_file2_,
                                                      false,
                                                      false,
                                                      FILE_SHARE_READ));
  }

  virtual void SetUp() {
    EXPECT_FALSE(String_EndsWith(cache_root_, _T("\\"), true));
    EXPECT_HRESULT_SUCCEEDED(package_cache_.Initialize(cache_root_));
    EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAll());
  }

  virtual void TearDown() {
    EXPECT_HRESULT_SUCCEEDED(DeleteDirectory(cache_root_));
  }

  HRESULT BuildCacheFileNameForKey(const Key& key, CString* filename) const {
    return package_cache_.BuildCacheFileNameForKey(key, filename);
  }

  HRESULT ExpireCache(const Key& key) {
    FILETIME expiration_time = package_cache_.GetCacheExpirationTime();

    CString cached_file_name;
    EXPECT_HRESULT_SUCCEEDED(BuildCacheFileNameForKey(key, &cached_file_name));

    // Change file time a little big earlier than the expiration time.
    ULARGE_INTEGER file_time = {0};
    file_time.LowPart = expiration_time.dwLowDateTime;
    file_time.HighPart = expiration_time.dwHighDateTime;
    file_time.QuadPart -= 1;

    expiration_time.dwLowDateTime = file_time.LowPart;
    expiration_time.dwHighDateTime = file_time.HighPart;

    return File::SetFileTime(cached_file_name,
                             &expiration_time,
                             &expiration_time,
                             &expiration_time);
  }

  void SetCacheSizeLimitMB(int limit_mb) {
    package_cache_.cache_size_limit_bytes_ = 1024 * 1024 *
      static_cast<uint64>(limit_mb);
  }

  void SetCacheTimeLimitDays(int limit_days) {
    package_cache_.cache_time_limit_days_ = limit_days;
  }

  const CString cache_root_;
  CString source_file1_;
  File source_file1_file_;
  CString hash_file1_;
  uint64  size_file1_;

  CString source_file2_;
  File source_file2_file_;
  CString hash_file2_;
  uint64  size_file2_;

  PackageCache package_cache_;
};

// Tests the members of key when the constructor arguments are empty strings.
TEST_F(PackageCacheTest, DefaultVersion) {
  Key key(_T(""), _T(""), _T(""));
  EXPECT_STREQ(_T(""), key.app_id());
  EXPECT_STREQ(_T("0.0.0.0"), key.version());
  EXPECT_STREQ(_T(""), key.package_name());
}

TEST_F(PackageCacheTest, Initialize) {
  EXPECT_FALSE(package_cache_.cache_root().IsEmpty());
  EXPECT_STREQ(cache_root_, package_cache_.cache_root());
  EXPECT_EQ(0, package_cache_.Size());
}

TEST_F(PackageCacheTest, InitializeErrors) {
  PackageCache package_cache;
  EXPECT_EQ(E_INVALIDARG, package_cache.Initialize(NULL));
  EXPECT_EQ(E_INVALIDARG, package_cache.Initialize(_T("")));
  EXPECT_EQ(E_INVALIDARG, package_cache.Initialize(_T("foo")));
}

TEST_F(PackageCacheTest, BuildCacheFileName) {
  CString actual;
  EXPECT_HRESULT_SUCCEEDED(
      BuildCacheFileNameForKey(Key(_T("1"), _T("2"), _T("3")), &actual));
  CString expected = cache_root_ + _T("\\1\\2\\3");
  EXPECT_STREQ(expected, actual);

  EXPECT_HRESULT_SUCCEEDED(
      BuildCacheFileNameForKey(Key(_T("1"), _T("2"), _T("3\\4")), &actual));
  expected = cache_root_ + _T("\\1\\2\\3\\4");
  EXPECT_STREQ(expected, actual);

  EXPECT_EQ(E_INVALIDARG,
            BuildCacheFileNameForKey(Key(_T("1"), _T("2"), _T("..\\3")),
                                     &actual));
}

// Tests Put, Get, IsCached, and Purge calls.
TEST_F(PackageCacheTest, BasicTest) {
  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAll());

  Key key1(_T("app1"), _T("ver1"), _T("package1"));

  // Check the file is not in the cache.
  EXPECT_FALSE(package_cache_.IsCached(key1, hash_file1_));

  // Cache one file.
  EXPECT_SUCCEEDED(package_cache_.Put(key1, &source_file1_file_, hash_file1_));
  EXPECT_EQ(size_file1_, package_cache_.Size());

  // Check the file is in the cache.
  EXPECT_TRUE(package_cache_.IsCached(key1, hash_file1_));

  // Check the source file is not deleted after caching it.
  EXPECT_TRUE(File::Exists(source_file1_));

  // Get the package from the cache into a temporary file.
  CString destination_file = GetTempFilename(_T("ut_"));
  EXPECT_FALSE(destination_file.IsEmpty());

  // Get the file two times.
  EXPECT_SUCCEEDED(package_cache_.Get(key1, destination_file, hash_file1_));
  EXPECT_TRUE(File::Exists(destination_file));

  EXPECT_SUCCEEDED(PackageCache::VerifyHash(destination_file, hash_file1_));

  EXPECT_SUCCEEDED(package_cache_.Get(key1, destination_file, hash_file1_));
  EXPECT_TRUE(File::Exists(destination_file));

  EXPECT_SUCCEEDED(PackageCache::VerifyHash(destination_file, hash_file1_));

  EXPECT_TRUE(::DeleteFile(destination_file));

  // Cache another file.
  Key key2(_T("app2"), _T("ver2"), _T("package2"));

  EXPECT_SUCCEEDED(package_cache_.Put(key2, &source_file2_file_, hash_file2_));
  EXPECT_EQ(size_file1_ + size_file2_, package_cache_.Size());
  EXPECT_TRUE(package_cache_.IsCached(key2, hash_file2_));

  // Cache the same file again. It should be idempotent.
  EXPECT_SUCCEEDED(package_cache_.Put(key2, &source_file2_file_, hash_file2_));
  EXPECT_EQ(size_file1_ + size_file2_, package_cache_.Size());

  EXPECT_TRUE(package_cache_.IsCached(key2, hash_file2_));
  EXPECT_TRUE(File::Exists(source_file2_));

  EXPECT_SUCCEEDED(package_cache_.Purge(key1));
  EXPECT_FALSE(package_cache_.IsCached(key1, hash_file1_));
  EXPECT_EQ(size_file2_, package_cache_.Size());

  EXPECT_SUCCEEDED(package_cache_.Purge(key2));
  EXPECT_FALSE(package_cache_.IsCached(key2, hash_file2_));
  EXPECT_EQ(0, package_cache_.Size());

  // Try getting a purged files.
  EXPECT_HRESULT_FAILED(package_cache_.Get(key1,
                                           destination_file,
                                           hash_file1_));
  EXPECT_FALSE(File::Exists(destination_file));

  EXPECT_HRESULT_FAILED(package_cache_.Get(key2,
                                           destination_file,
                                           hash_file2_));
  EXPECT_FALSE(File::Exists(destination_file));
}

TEST_F(PackageCacheTest, PutBadHashTest) {
  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAll());

  Key key1(_T("app1"), _T("ver1"), _T("package1"));

  // Check the file is not in the cache.
  EXPECT_FALSE(package_cache_.IsCached(key1, hash_file1_));

  // Try caching one file when the hash is not correct.
  CString bad_hash =
      _T("0000bad0000364f6c33161d781b49d840ed792b8b10668c4180b9e6e128d0bc9");
  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE, package_cache_.Put(key1,
                                                         &source_file1_file_,
                                                         bad_hash));
  // Check the file is not the cache.
  EXPECT_FALSE(package_cache_.IsCached(key1, hash_file1_));
}

// The key must include the app id, version, and package name for Put and Get
// operations. If the version is not provided, "0.0.0.0" is used internally.
TEST_F(PackageCacheTest, BadKeyTest) {
  Key key_empty_app(_T(""), _T("b"), _T("c"));
  Key key_empty_name(_T("a"), _T("b"), _T(""));

  CString bad_hash;
  bad_hash = _T("b");
  EXPECT_EQ(E_INVALIDARG, package_cache_.Get(key_empty_app, _T("a"), bad_hash));
  EXPECT_EQ(E_INVALIDARG,
      package_cache_.Get(key_empty_name, _T("a"), bad_hash));

  EXPECT_EQ(E_INVALIDARG, package_cache_.Put(key_empty_app,
                                             &source_file1_file_,
                                             bad_hash));
  EXPECT_EQ(E_INVALIDARG, package_cache_.Put(key_empty_name,
                                             &source_file1_file_,
                                             bad_hash));
}

TEST_F(PackageCacheTest, PurgeVersionTest) {
  // Cache two files for two versions of the same app.
  Key key11(_T("app1"), _T("ver1"), _T("package1"));
  Key key12(_T("app1"), _T("ver1"), _T("package2"));
  Key key21(_T("app1"), _T("ver2"), _T("package1"));
  Key key22(_T("app1"), _T("ver2"), _T("package2"));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key11,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key12,
                                              &source_file2_file_,
                                              hash_file2_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key21,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key22,
                                              &source_file2_file_,
                                              hash_file2_));

  EXPECT_TRUE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(2 * (size_file1_ + size_file2_), package_cache_.Size());

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeVersion(_T("app1"), _T("ver1")));

  EXPECT_FALSE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(size_file1_ + size_file2_, package_cache_.Size());

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeVersion(_T("app1"), _T("ver2")));

  EXPECT_FALSE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_FALSE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(0, package_cache_.Size());
}

TEST_F(PackageCacheTest, PurgeAppTest) {
  // Cache two files for two apps.
  Key key11(_T("app1"), _T("ver1"), _T("package1"));
  Key key12(_T("app1"), _T("ver1"), _T("package2"));
  Key key21(_T("app2"), _T("ver2"), _T("package1"));
  Key key22(_T("app2"), _T("ver2"), _T("package2"));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key11,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key12,
                                              &source_file2_file_,
                                              hash_file2_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key21,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key22,
                                              &source_file2_file_,
                                              hash_file2_));

  EXPECT_TRUE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(2 * (size_file1_ + size_file2_), package_cache_.Size());

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeApp(_T("app1")));

  EXPECT_FALSE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(size_file1_ + size_file2_, package_cache_.Size());

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeApp(_T("app2")));

  EXPECT_FALSE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_FALSE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(0, package_cache_.Size());
}

TEST_F(PackageCacheTest, PurgeAppLowerVersionsTest) {
  EXPECT_EQ(E_INVALIDARG, package_cache_.PurgeAppLowerVersions(_T("app1"),
                                                               _T("1")));

  Key key_10_1(_T("app1"), _T("1.0.0.0"), _T("package1"));
  Key key_10_2(_T("app1"), _T("1.0.0.0"), _T("package2"));
  Key key_11_1(_T("app1"), _T("1.1.0.0"), _T("package1"));
  Key key_21_2(_T("app1"), _T("2.1.0.0"), _T("package2"));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key_10_1,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key_10_2,
                                              &source_file2_file_,
                                              hash_file2_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key_11_1,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key_21_2,
                                              &source_file2_file_,
                                              hash_file2_));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAppLowerVersions(_T("app1"),
                                                                _T("1.0.0.0")));
  EXPECT_TRUE(package_cache_.IsCached(key_10_1, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key_10_2, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key_11_1, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key_21_2, hash_file2_));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAppLowerVersions(_T("app1"),
                                                                _T("1.1.0.0")));
  EXPECT_FALSE(package_cache_.IsCached(key_10_1, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key_10_2, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key_11_1, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key_21_2, hash_file2_));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAppLowerVersions(_T("app1"),
                                                                _T("2.1.0.0")));
  EXPECT_FALSE(package_cache_.IsCached(key_11_1, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key_21_2, hash_file2_));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAppLowerVersions(_T("app1"),
                                                                _T("2.2.0.0")));
  EXPECT_FALSE(package_cache_.IsCached(key_21_2, hash_file2_));

  EXPECT_EQ(0, package_cache_.Size());
}

TEST_F(PackageCacheTest, PurgeAll) {
  // Cache two files for two apps.
  Key key11(_T("app1"), _T("ver1"), _T("package1"));
  Key key12(_T("app1"), _T("ver1"), _T("package2"));
  Key key21(_T("app2"), _T("ver2"), _T("package1"));
  Key key22(_T("app2"), _T("ver2"), _T("package2"));

  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key11,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key12,
                                              &source_file2_file_,
                                              hash_file2_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key21,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key22,
                                              &source_file2_file_,
                                              hash_file2_));

  EXPECT_TRUE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(2 * (size_file1_ + size_file2_), package_cache_.Size());

  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAll());

  EXPECT_FALSE(package_cache_.IsCached(key11, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key12, hash_file2_));
  EXPECT_FALSE(package_cache_.IsCached(key21, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key22, hash_file2_));

  EXPECT_EQ(0, package_cache_.Size());
}

TEST_F(PackageCacheTest, PurgeOldPackagesIfOverSizeLimit) {
  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAll());

  const int kCacheSizeLimitMB = 2;
  SetCacheSizeLimitMB(kCacheSizeLimitMB);

  const uint64 kSizeLimitBytes = 1024LL * 1024 * kCacheSizeLimitMB;

  Key key0(_T("app0"), _T("version0"), _T("package0"));

  // Keep adding packages until we exceed cache limit.
  uint64 current_size = 0;
  int i = 0;
  while (current_size <= kSizeLimitBytes) {
    CString app;
    CString version;
    CString package;
    app.Format(_T("app%d"), i);
    version.Format(_T("version%d"), i);
    package.Format(_T("package%d"), i);
    EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(Key(app, version, package),
                                                &source_file1_file_,
                                                hash_file1_));
    current_size += size_file1_;
    EXPECT_EQ(current_size, package_cache_.Size());
    ++i;

    // Delay a little bit so that next cache file will have a newer
    // timestamp than the one we just added. On FAT file system, the
    // resolution is 10ms, so wait for a value longer that that.
    ::Sleep(20);
  }

  // Verify that cache size limit is exceeded.
  EXPECT_GT(package_cache_.Size(), kSizeLimitBytes);
  EXPECT_TRUE(package_cache_.IsCached(key0, hash_file1_));

  package_cache_.PurgeOldPackagesIfNecessary();

  // Verify that the oldes package is purged and the cache size is below limit.
  EXPECT_FALSE(package_cache_.IsCached(key0, hash_file1_));
  EXPECT_LE(package_cache_.Size(), kSizeLimitBytes);
}

TEST_F(PackageCacheTest, PurgeExpiredCacheFiles) {
  EXPECT_HRESULT_SUCCEEDED(package_cache_.PurgeAll());

  const int kCacheLifeLimitDays = 100;
  SetCacheTimeLimitDays(kCacheLifeLimitDays);

  Key key1(_T("app1"), _T("version1"), _T("package1"));
  Key key2(_T("app2"), _T("version2"), _T("package2"));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key1,
                                              &source_file1_file_,
                                              hash_file1_));
  EXPECT_HRESULT_SUCCEEDED(package_cache_.Put(key2,
                                              &source_file2_file_,
                                              hash_file2_));
  EXPECT_TRUE(package_cache_.IsCached(key1, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key2, hash_file2_));

  // Expires one package and verifies it is purged.
  EXPECT_HRESULT_SUCCEEDED(ExpireCache(key1));
  package_cache_.PurgeOldPackagesIfNecessary();
  EXPECT_FALSE(package_cache_.IsCached(key1, hash_file1_));
  EXPECT_TRUE(package_cache_.IsCached(key2, hash_file2_));

  // Expires another package and verifies it is purged.
  EXPECT_HRESULT_SUCCEEDED(ExpireCache(key2));
  package_cache_.PurgeOldPackagesIfNecessary();
  EXPECT_FALSE(package_cache_.IsCached(key1, hash_file1_));
  EXPECT_FALSE(package_cache_.IsCached(key2, hash_file2_));
}

TEST_F(PackageCacheTest, VerifyHash) {
  EXPECT_HRESULT_SUCCEEDED(PackageCache::VerifyHash(source_file1_,
                                                    hash_file1_));
  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE,
            PackageCache::VerifyHash(source_file1_, hash_file2_));
}

}  // namespace omaha

