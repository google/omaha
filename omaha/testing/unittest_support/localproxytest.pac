function FindProxyForURL(url, host) {
  if (shExpMatch(host, "*.omahaproxytest.com")) {
    return "PROXY omaha_unittest1; SOCKS should_not_appear:12345; PROXY omaha_unittest2:8080; DIRECT; PROXY should_not_be_included";
  }
  return "DIRECT";
}

