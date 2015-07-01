// Copyright 2009 Google Inc.
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

#include "omaha/plugins/update/activex/update3web_control.h"
#include <atlbase.h>
#include <atlcom.h>
#include <windows.h>
#include "base/basictypes.h"
#include "base/error.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

#define STUBMETHOD(identifier, ...) \
  STDMETHOD(identifier)(__VA_ARGS__) {\
    return E_NOTIMPL;\
  }

#define STUBGETPROP(identifier, type) \
  STUBMETHOD(get_ ## identifier, type*)

#define STUBPUTPROP(identifier, type) \
  STUBMETHOD(put_ ## identifier, type)

#define STUBPROP(identifier, type) \
  STUBGETPROP(identifier, type)\
  STUBPUTPROP(identifier, type)

class ATL_NO_VTABLE MockWebBrowser2
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IServiceProviderImpl<MockWebBrowser2>,
      public IWebBrowser2 {
 public:
  MockWebBrowser2() : url_("something-like-a-url") {}
  virtual ~MockWebBrowser2() {}

  DECLARE_NOT_AGGREGATABLE(MockWebBrowser2);

  BEGIN_COM_MAP(MockWebBrowser2)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IWebBrowser2)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(MockWebBrowser2)
    SERVICE_ENTRY(SID_SWebBrowserApp)
  END_SERVICE_MAP()

  // IDispatch
  STUBMETHOD(GetIDsOfNames, REFIID, OLECHAR**, unsigned int, LCID, DISPID*);
  STUBMETHOD(GetTypeInfo, unsigned int, LCID, ITypeInfo**);
  STUBMETHOD(GetTypeInfoCount, UINT*);
  STUBMETHOD(Invoke, DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*,
                     EXCEPINFO*, unsigned int*);

  // IWebBrowser2
  STUBPROP(AddressBar, VARIANT_BOOL);
  STUBGETPROP(Application, IDispatch*);
  STUBGETPROP(Busy, VARIANT_BOOL);
  STUBMETHOD(ClientToWindow, int*, int*);
  STUBGETPROP(Container, IDispatch*);
  STUBGETPROP(Document, IDispatch*);
  STUBMETHOD(ExecWB, OLECMDID, OLECMDEXECOPT, VARIANT*, VARIANT*);
  STUBGETPROP(FullName, BSTR);
  STUBPROP(FullScreen, VARIANT_BOOL);
  STUBMETHOD(GetProperty, BSTR, VARIANT*);
  STUBMETHOD(GoBack, VOID);
  STUBMETHOD(GoForward, VOID);
  STUBMETHOD(GoHome, VOID);
  STUBMETHOD(GoSearch, VOID);
  STUBPROP(Height, LONG);
  STUBGETPROP(HWND, LONG_PTR);
  STUBPROP(Left, LONG);
  STUBGETPROP(LocationName, BSTR);
  STDMETHOD(get_LocationURL)(BSTR* url) {
    *url = CComBSTR(url_).Detach();
    return S_OK;
  }
  STUBPROP(MenuBar, VARIANT_BOOL);
  STUBGETPROP(Name, BSTR);
  STUBMETHOD(Navigate, BSTR, VARIANT*, VARIANT*, VARIANT*, VARIANT*);
  STUBMETHOD(Navigate2, VARIANT*, VARIANT*, VARIANT*, VARIANT*, VARIANT*);
  STUBPROP(Offline, VARIANT_BOOL);
  STUBGETPROP(Path, BSTR);
  STUBGETPROP(Parent, IDispatch*);
  STUBMETHOD(PutProperty, BSTR, VARIANT);
  STUBMETHOD(QueryStatusWB, OLECMDID, OLECMDF*);
  STUBMETHOD(Quit, VOID);
  STUBGETPROP(ReadyState, READYSTATE);
  STUBMETHOD(Refresh, VOID);
  STUBMETHOD(Refresh2, VARIANT*);
  STUBPROP(RegisterAsBrowser, VARIANT_BOOL);
  STUBPROP(RegisterAsDropTarget, VARIANT_BOOL);
  STUBPROP(Resizable, VARIANT_BOOL);
  STUBMETHOD(ShowBrowserBar, VARIANT*, VARIANT*, VARIANT*);
  STUBPROP(Silent, VARIANT_BOOL);
  STUBPROP(StatusBar, VARIANT_BOOL);
  STUBPROP(StatusText, BSTR);
  STUBMETHOD(Stop, VOID);
  STUBPROP(TheaterMode, VARIANT_BOOL);
  STUBPROP(ToolBar, int);
  STUBPROP(Top, LONG);
  STUBGETPROP(TopLevelContainer, VARIANT_BOOL);
  STUBPROP(Type, BSTR);
  STUBPROP(Visible, VARIANT_BOOL);
  STUBPROP(Width, LONG);

  void set_url(const char* url) { url_ = url; }

 private:
  CString url_;

  DISALLOW_COPY_AND_ASSIGN(MockWebBrowser2);
};

class ATL_NO_VTABLE MockHTMLDocument2
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IServiceProvider,
      public IOleClientSite,
      public IOleContainer,
      public IHTMLDocument2 {
 public:
  MockHTMLDocument2() {}
  virtual ~MockHTMLDocument2() {}

  DECLARE_NOT_AGGREGATABLE(MockHTMLDocument2);

  BEGIN_COM_MAP(MockHTMLDocument2)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IOleClientSite)
    COM_INTERFACE_ENTRY(IOleContainer)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
  END_COM_MAP()

  // IServiceProvider
  STUBMETHOD(QueryService, REFGUID, REFIID, void**);

  // IOleContainer
  STDMETHOD(GetContainer)(IOleContainer** container) {
    return QueryInterface(IID_PPV_ARGS(container));
  }
  STUBMETHOD(GetMoniker, DWORD, DWORD, IMoniker**);
  STUBMETHOD(OnShowWindow, BOOL);
  STUBMETHOD(RequestNewObjectLayout);
  STUBMETHOD(SaveObject);
  STUBMETHOD(ShowObject);

  // IOleContainer
  STUBMETHOD(EnumObjects, DWORD, IEnumUnknown**);
  STUBMETHOD(LockContainer, BOOL);

  // IDispatch
  STUBMETHOD(GetIDsOfNames, REFIID, OLECHAR**, unsigned int, LCID, DISPID*);
  STUBMETHOD(GetTypeInfo, unsigned int, LCID, ITypeInfo**);
  STUBMETHOD(GetTypeInfoCount, UINT*);
  STUBMETHOD(Invoke, DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*,
                     EXCEPINFO*, unsigned int*);

  // IParseDisplayName
  STUBMETHOD(ParseDisplayName, IBindCtx*, LPOLESTR, ULONG*, IMoniker**);

  // IHTMLDocument2
  STUBGETPROP(activeElement, IHTMLElement*);
  STUBPROP(alinkColor, VARIANT);
  STUBGETPROP(all, IHTMLElementCollection*);
  STUBGETPROP(anchors, IHTMLElementCollection*);
  STUBGETPROP(applets, IHTMLElementCollection*);
  STUBPROP(bgColor, VARIANT);
  STUBGETPROP(body, IHTMLElement*);
  STUBPROP(charset, BSTR);
  STUBMETHOD(clear, VOID);
  STUBMETHOD(close, VOID);
  STUBPROP(cookie, BSTR);
  STUBMETHOD(createElement, BSTR, IHTMLElement**);
  STUBMETHOD(createStyleSheet, BSTR, LONG, IHTMLStyleSheet**);
  STUBPROP(defaultCharset, BSTR);
  STUBPROP(designMode, BSTR);
  STUBPROP(domain, BSTR);
  STUBPROP(elementFromPoint, BSTR);
  STUBMETHOD(elementFromPoint, LONG, LONG, IHTMLElement**);
  STUBGETPROP(embeds, IHTMLElementCollection*);
  STUBMETHOD(execCommand, BSTR, VARIANT_BOOL, VARIANT, VARIANT_BOOL*);
  STUBMETHOD(execCommandShowHelp, BSTR, VARIANT_BOOL*);
  STUBPROP(expando, VARIANT_BOOL);
  STUBPROP(fgColor, VARIANT);
  STUBGETPROP(fileCreatedDate, BSTR);
  STUBGETPROP(fileModifiedDate, BSTR);
  STUBGETPROP(fileSize, BSTR);
  STUBGETPROP(fileUpdatedDate, BSTR);
  STUBGETPROP(forms, IHTMLElementCollection*);
  STUBGETPROP(frames, IHTMLFramesCollection2*);
  STUBGETPROP(images, IHTMLElementCollection*);
  STUBGETPROP(lastModified, BSTR);
  STUBPROP(linkColor, VARIANT);
  STUBGETPROP(links, IHTMLElementCollection*);
  STUBGETPROP(location, IHTMLLocation*);
  STUBGETPROP(mimeType, BSTR);
  STUBGETPROP(nameProp, BSTR);
  STUBPROP(onafterupdate, VARIANT);
  STUBPROP(onbeforeupdate, VARIANT);
  STUBPROP(onclick, VARIANT);
  STUBPROP(ondblclick, VARIANT);
  STUBPROP(ondragstart, VARIANT);
  STUBPROP(onerrorupdate, VARIANT);
  STUBPROP(onhelp, VARIANT);
  STUBPROP(onkeydown, VARIANT);
  STUBPROP(onkeypress, VARIANT);
  STUBPROP(onkeyup, VARIANT);
  STUBPROP(onmousedown, VARIANT);
  STUBPROP(onmousemove, VARIANT);
  STUBPROP(onmouseout, VARIANT);
  STUBPROP(onmouseover, VARIANT);
  STUBPROP(onmouseup, VARIANT);
  STUBPROP(onreadystatechange, VARIANT);
  STUBPROP(onrowenter, VARIANT);
  STUBPROP(onrowexit, VARIANT);
  STUBPROP(onselectstart, VARIANT);
  STUBMETHOD(open, BSTR, VARIANT, VARIANT, VARIANT, IDispatch**);
  STUBGETPROP(parentWindow, IHTMLWindow2*);
  STUBGETPROP(plugins, IHTMLElementCollection*);
  STUBPROP(protocol, BSTR);
  STUBMETHOD(queryCommandEnabled, BSTR, VARIANT_BOOL*);
  STUBMETHOD(queryCommandIndeterm, BSTR, VARIANT_BOOL*);
  STUBMETHOD(queryCommandState, BSTR, VARIANT_BOOL*);
  STUBMETHOD(queryCommandSupported, BSTR, VARIANT_BOOL*);
  STUBMETHOD(queryCommandText, BSTR, BSTR*);
  STUBMETHOD(queryCommandValue, BSTR, VARIANT*);
  STUBGETPROP(readyState, BSTR);
  STUBGETPROP(referrer, BSTR);
  STUBGETPROP(Script, IDispatch*);
  STUBGETPROP(security, BSTR);
  STUBGETPROP(selection, IHTMLSelectionObject*);
  STUBGETPROP(scripts, IHTMLElementCollection*);
  STUBGETPROP(styleSheets, IHTMLStyleSheetsCollection*);
  STUBPROP(title, BSTR);
  STUBMETHOD(toString, BSTR*);
  STDMETHOD(get_URL)(BSTR* url) {
    *url = CComBSTR("something-else-like-a-url").Detach();
    return S_OK;
  }
  STUBPUTPROP(URL, BSTR);
  STUBPROP(vlinkColor, VARIANT);
  STUBMETHOD(write, SAFEARRAY*);
  STUBMETHOD(writeln, SAFEARRAY*);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHTMLDocument2);
};

#undef STUBMETHOD
#undef STUBGETPROP
#undef STUBPUTPROP
#undef STUBPROP

template <typename T>
HRESULT CComObjectCreatorHelper(CComObject<T>** ptr) {
  if (!ptr) {
    return E_POINTER;
  }
  CComObject<T>* raw_ptr = NULL;
  RET_IF_FAILED(CComObject<T>::CreateInstance(&raw_ptr));
  raw_ptr->AddRef();
  *ptr = raw_ptr;
  return S_OK;
}


}  // namespace

class Update3WebControlTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_SUCCEEDED(CComObjectCreatorHelper(&control_));
  }

  virtual void TearDown() {
  }

  HRESULT GetCurrentBrowserUrl(CString* url) {
    return control_->site_lock_.GetCurrentBrowserUrl(control_, url);
  }

  CComPtr<CComObject<Update3WebControl> > control_;
};

TEST_F(Update3WebControlTest, SiteLock) {
  CComPtr<CComObject<MockWebBrowser2> > browser;
  ASSERT_SUCCEEDED(CComObjectCreatorHelper(&browser));
  CComPtr<IUnknown> unknown;
  ASSERT_SUCCEEDED(browser.QueryInterface(&unknown));
  ASSERT_SUCCEEDED(control_->SetSite(unknown));
  browser->set_url("http://www.google.com/pack/page.html");
  EXPECT_EQ(E_POINTER,
      control_->getInstalledVersion(CComBSTR(), VARIANT_FALSE, NULL));
}

TEST_F(Update3WebControlTest, SiteLock_Negative) {
  CComPtr<IWebBrowser2> browser;
  ASSERT_SUCCEEDED(CComCoClass<MockWebBrowser2>::CreateInstance(&browser));
  ASSERT_SUCCEEDED(control_->SetSite(browser));
  EXPECT_EQ(GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED,
      control_->getInstalledVersion(CComBSTR(), VARIANT_FALSE, NULL));
}

TEST_F(Update3WebControlTest, GetCurrentBrowserUrl_FailIfNoSite) {
  CString url;
  EXPECT_FALSE(SUCCEEDED(GetCurrentBrowserUrl(&url)));
}

TEST_F(Update3WebControlTest, GetCurrentBrowserUrl_WithIWebBrowser2) {
  CComPtr<IUnknown> browser;
  ASSERT_SUCCEEDED(CComCoClass<MockWebBrowser2>::CreateInstance(&browser));
  ASSERT_SUCCEEDED(control_->SetSite(browser));
  CString url;
  EXPECT_EQ(S_OK, GetCurrentBrowserUrl(&url));
  EXPECT_STREQ(L"something-like-a-url", url);
}

TEST_F(Update3WebControlTest, GetCurrentBrowserUrl_WithIHTMLDocument2) {
  CComPtr<IUnknown> document;
  ASSERT_SUCCEEDED(CComCoClass<MockHTMLDocument2>::CreateInstance(&document));
  ASSERT_SUCCEEDED(control_->SetSite(document));
  CString url;
  EXPECT_EQ(S_OK, GetCurrentBrowserUrl(&url));
  EXPECT_STREQ(L"something-else-like-a-url", url);
}

}  // namespace omaha
