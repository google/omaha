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
// xml_utils.cpp
//
// Utilities for working with XML files via MSXML.

#include "omaha/common/xml_utils.h"

#include <msxml2.h>
#include <atlsafe.h>
#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

XMLFQName::XMLFQName() {}

XMLFQName::XMLFQName(const TCHAR* u, const TCHAR* b)
    : uri(u && ::_tcslen(u) ? u : 0),
      base(b && ::_tcslen(b) ? b : 0) {}

XMLFQName::~XMLFQName() {}

HRESULT CoCreateSafeDOMDocument(IXMLDOMDocument** my_xmldoc) {
  ASSERT1(my_xmldoc && !*my_xmldoc);
  if (!my_xmldoc) {
    UTIL_LOG(LE, (L"[CoCreateSafeDOMDocument E_INVALIDARG]"));
    return E_INVALIDARG;
  }
  *my_xmldoc = NULL;
  CComPtr<IXMLDOMDocument> xml_doc;
  HRESULT hr = xml_doc.CoCreateInstance(__uuidof(DOMDocument2));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[xml_doc.CoCreateInstance failed][0x%x]"), hr));
    return hr;
  }
  ASSERT1(xml_doc);
  hr = xml_doc->put_resolveExternals(VARIANT_FALSE);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[put_resolveExternals failed][0x%x]"), hr));
    return hr;
  }
  *my_xmldoc = xml_doc.Detach();
  return S_OK;
}

HRESULT LoadXMLFromFile(const TCHAR* xmlfile,
                        bool preserve_whitespace,
                        IXMLDOMDocument** xmldoc) {
  ASSERT1(xmlfile);
  ASSERT1(xmldoc);
  ASSERT1(!*xmldoc);

  *xmldoc = NULL;
  CComPtr<IXMLDOMDocument> my_xmldoc;
  HRESULT hr = CoCreateSafeDOMDocument(&my_xmldoc);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[CoCreateSafeDOMDocument failed][0x%x]"), hr));
    return hr;
  }
  hr = my_xmldoc->put_preserveWhiteSpace(VARIANT_BOOL(preserve_whitespace));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[put_preserveWhiteSpace failed][0x%x]"), hr));
    return hr;
  }
  CComBSTR my_xmlfile(xmlfile);
  VARIANT_BOOL is_successful(VARIANT_FALSE);
  hr = my_xmldoc->load(CComVariant(my_xmlfile), &is_successful);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[my_xmldoc->load failed][0x%x]"), hr));
    return hr;
  }
  if (!is_successful) {
    CComPtr<IXMLDOMParseError> error;
    CString error_message;
    hr = GetXMLParseError(my_xmldoc, &error);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[GetXMLParseError failed][0x%x]"), hr));
      return hr;
    }
    ASSERT1(error);
    HRESULT error_code = 0;
    hr = InterpretXMLParseError(error, &error_code, &error_message);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[InterpretXMLParseError failed][0x%x]"), hr));
      return hr;
    }
    UTIL_LOG(LE, (L"[LoadXMLFromFile '%s'][parse error: %s]",
                  xmlfile, error_message));
    ASSERT1(FAILED(error_code));
    return FAILED(error_code) ? error_code : CI_E_XML_LOAD_ERROR;
  }
  *xmldoc = my_xmldoc.Detach();
  return S_OK;
}

HRESULT LoadXMLFromMemory(const TCHAR* xmlstring,
                          bool preserve_whitespace,
                          IXMLDOMDocument** xmldoc) {
  ASSERT1(xmlstring);
  ASSERT1(xmldoc);
  ASSERT1(!*xmldoc);

  *xmldoc = NULL;
  CComPtr<IXMLDOMDocument> my_xmldoc;
  RET_IF_FAILED(CoCreateSafeDOMDocument(&my_xmldoc));
  RET_IF_FAILED(my_xmldoc->put_preserveWhiteSpace(
                               VARIANT_BOOL(preserve_whitespace)));
  CComBSTR xmlmemory(xmlstring);
  VARIANT_BOOL is_successful(VARIANT_FALSE);
  RET_IF_FAILED(my_xmldoc->loadXML(xmlmemory, &is_successful));
  if (!is_successful) {
    CComPtr<IXMLDOMParseError> error;
    CString error_message;
    RET_IF_FAILED(GetXMLParseError(my_xmldoc, &error));
    ASSERT1(error);
    HRESULT error_code = 0;
    RET_IF_FAILED(InterpretXMLParseError(error, &error_code, &error_message));
    UTIL_LOG(LE, (L"[LoadXMLFromMemory][parse error: %s]", error_message));
    ASSERT1(FAILED(error_code));
    return FAILED(error_code) ? error_code : CI_E_XML_LOAD_ERROR;
  }
  *xmldoc = my_xmldoc.Detach();
  return S_OK;
}

HRESULT LoadXMLFromRawData(const std::vector<byte>& xmldata,
                           bool preserve_whitespace,
                           IXMLDOMDocument** xmldoc) {
  ASSERT1(xmldoc);
  ASSERT1(!*xmldoc);

  *xmldoc = NULL;
  CComPtr<IXMLDOMDocument> my_xmldoc;
  RET_IF_FAILED(CoCreateSafeDOMDocument(&my_xmldoc));
  RET_IF_FAILED(my_xmldoc->put_preserveWhiteSpace(
                              VARIANT_BOOL(preserve_whitespace)));

  CComSafeArray<byte> xmlsa;
  xmlsa.Add(xmldata.size(), &xmldata.front());
  CComVariant xmlvar(xmlsa);

  VARIANT_BOOL is_successful(VARIANT_FALSE);
  RET_IF_FAILED(my_xmldoc->load(xmlvar, &is_successful));
  if (!is_successful) {
    CComPtr<IXMLDOMParseError> error;
    CString error_message;
    RET_IF_FAILED(GetXMLParseError(my_xmldoc, &error));
    ASSERT1(error);
    HRESULT error_code = 0;
    RET_IF_FAILED(InterpretXMLParseError(error, &error_code, &error_message));
    UTIL_LOG(LE, (_T("[LoadXMLFromRawData][parse error: %s]"), error_message));
    ASSERT1(FAILED(error_code));
    return FAILED(error_code) ? error_code : CI_E_XML_LOAD_ERROR;
  }
  *xmldoc = my_xmldoc.Detach();
  return S_OK;
}

HRESULT SaveXMLToFile(IXMLDOMDocument* xmldoc, const TCHAR* xmlfile) {
  ASSERT1(xmldoc);
  ASSERT1(xmlfile);

  CComBSTR my_xmlfile(xmlfile);
  RET_IF_FAILED(xmldoc->save(CComVariant(my_xmlfile)));
  return S_OK;
}

HRESULT SaveXMLToMemory(IXMLDOMDocument* xmldoc, CString* xmlstring) {
  ASSERT1(xmldoc);
  ASSERT1(xmlstring);

  CComBSTR xmlmemory;
  RET_IF_FAILED(xmldoc->get_xml(&xmlmemory));
  *xmlstring = xmlmemory;

  return S_OK;
}

HRESULT CanonicalizeXML(const TCHAR* xmlstring, CString* canonical_xmlstring) {
  ASSERT1(xmlstring);
  ASSERT1(canonical_xmlstring);

  // Round-trip through MSXML, having it strip whitespace.

  CComPtr<IXMLDOMDocument> xmldoc;
  RET_IF_FAILED(CoCreateSafeDOMDocument(&xmldoc));
  RET_IF_FAILED(xmldoc->put_preserveWhiteSpace(VARIANT_FALSE));
  {
    CComBSTR xmlmemory(StringAfterBOM(xmlstring));
    VARIANT_BOOL is_successful(VARIANT_FALSE);
    RET_IF_FAILED(xmldoc->loadXML(xmlmemory, &is_successful));
    if (!is_successful) {
      CComPtr<IXMLDOMParseError> error;
      CString error_message;
      RET_IF_FAILED(GetXMLParseError(xmldoc, &error));
      ASSERT1(error);
      HRESULT error_code = 0;
      RET_IF_FAILED(InterpretXMLParseError(error, &error_code, &error_message));
      UTIL_LOG(LE, (L"[CanonicalizeXML][parse error: %s]", error_message));
      ASSERT1(FAILED(error_code));
      return FAILED(error_code) ? error_code : CI_E_XML_LOAD_ERROR;
    }
  }
  std::vector<CString> lines;
  {
    CComBSTR xmlmemory2;
    RET_IF_FAILED(xmldoc->get_xml(&xmlmemory2));
    TextToLines(CString(xmlmemory2), L"\r\n", &lines);
  }
  {
    for (size_t i = 0; i < lines.size(); ++i) {
      TrimString(lines[i], L" \t");
    }
    LinesToText(lines, L"", canonical_xmlstring);
  }

  return S_OK;
}

bool operator==(const XMLFQName& u, const XMLFQName& v) {
  if (u.uri && v.uri) {
    // Both uris are non-null -> compare all the components.
    return !_tcscmp(u.uri, v.uri) && !_tcscmp(u.base, v.base);
  } else if (!u.uri && !v.uri) {
    // Both uris are null -> only compare the base names.
    return !_tcscmp(u.base ? u.base : __T(""), v.base ? v.base : __T(""));
  } else {
    // Either uri is null -> the names are in different namespaces.
    return false;
  }
}

bool operator!=(const XMLFQName& u, const XMLFQName& v) {
  return !(u == v);
}

bool operator<(const XMLFQName& u, const XMLFQName &v) {
  if (u.uri && v.uri) {
    return (_tcscmp(u.uri, v.uri) < 0) ||
            ((_tcscmp(u.uri, v.uri) == 0) && (_tcscmp(u.base, v.base) < 0));
  } else if (!u.uri && !v.uri) {
    return _tcscmp(u.base, v.base) < 0;
  } else {
    return false;
  }
}

bool operator>(const XMLFQName& u, const XMLFQName& v) {
  return v < u;
}

bool operator<=(const XMLFQName& u, const XMLFQName& v) {
  return !(v < u);
}

bool operator>=(const XMLFQName& u, const XMLFQName& v) {
  return !(u < v);
}

bool EqualXMLName(const XMLFQName& u, const XMLFQName& v) {
  return u == v;
}

// msxml returns a null uri for nodes that don't belong to a namespace.
bool EqualXMLName(IXMLDOMNode* pnode, const XMLFQName& u) {
  CComBSTR name;
  CComBSTR uri;
  if (FAILED(pnode->get_baseName(&name)) ||
      FAILED(pnode->get_namespaceURI(&uri))) {
    return false;
  }
  return EqualXMLName(XMLFQName(uri, name), u);
}

inline bool EqualXMLName(const XMLFQName& u, IXMLDOMNode* pnode) {
  return EqualXMLName(pnode, u);
}

HRESULT GetXMLFQName(IXMLDOMNode* node, XMLFQName* name) {
  ASSERT1(node);
  ASSERT1(name);

  CComBSTR basename, uri;
  RET_IF_FAILED(node->get_baseName(&basename));
  RET_IF_FAILED(node->get_namespaceURI(&uri));
  *name = XMLFQName(uri, basename);
  return S_OK;
}

CString XMLFQNameToString(const XMLFQName& fqname) {
  CString name;
  if (fqname.uri) {
    name += fqname.uri;
    name += L":";
  }
  if (fqname.base) {
    name += fqname.base;
  }
  return name;
}

CString NodeToString(IXMLDOMNode* pnode) {
  ASSERT1(pnode);

  XMLFQName node_name;
  if (SUCCEEDED(GetXMLFQName(pnode, &node_name))) {
    return XMLFQNameToString(node_name);
  }
  return L"";
}

HRESULT CreateXMLNode(IXMLDOMDocument* xmldoc,
                      int node_type,
                      const TCHAR* node_name,
                      const TCHAR* namespace_uri,
                      const TCHAR* text,
                      IXMLDOMNode** node_out) {
  ASSERT1(xmldoc);
  ASSERT1(node_name);
  // namespace_uri can be NULL
  // text can be NULL
  ASSERT1(node_out);
  ASSERT1(!*node_out);

  *node_out = NULL;
  CComPtr<IXMLDOMNode> new_node;
  CComBSTR node_name_string, namespace_uri_string;
  RET_IF_FAILED(node_name_string.Append(node_name));
  RET_IF_FAILED(namespace_uri_string.Append(namespace_uri));
  RET_IF_FAILED(xmldoc->createNode(CComVariant(node_type),
                                   node_name_string,
                                   namespace_uri_string,
                                   &new_node));
  ASSERT1(new_node);

  // If any text was supplied, put it in the node
  if (text && text[0]) {
    RET_IF_FAILED(new_node->put_text(CComBSTR(text)));
  }

  *node_out = new_node.Detach();
  return S_OK;
}

HRESULT AppendXMLNode(IXMLDOMNode* xmlnode, IXMLDOMNode* new_child) {
  ASSERT1(xmlnode);
  ASSERT1(new_child);

  CComPtr<IXMLDOMNode> useless;
  RET_IF_FAILED(xmlnode->appendChild(new_child, &useless));
  return S_OK;
}

HRESULT AppendXMLNode(IXMLDOMNode* xmlnode, const TCHAR* text) {
  ASSERT1(xmlnode);
  // text can be NULL

  if (text && text[0]) {
    CComPtr<IXMLDOMDocument> xml_doc;
    CComPtr<IXMLDOMText> text_node;
    RET_IF_FAILED(xmlnode->get_ownerDocument(&xml_doc));
    ASSERT1(xml_doc);
    RET_IF_FAILED(xml_doc->createTextNode(CComBSTR(text), &text_node));
    RET_IF_FAILED(AppendXMLNode(xmlnode, text_node));
  }
  return S_OK;
}

HRESULT AddXMLAttributeNode(IXMLDOMNode* xmlnode, IXMLDOMAttribute* new_child) {
  ASSERT1(xmlnode);
  ASSERT1(new_child);

  CComPtr<IXMLDOMNamedNodeMap> attributes;
  CComPtr<IXMLDOMNode> useless;
  RET_IF_FAILED(xmlnode->get_attributes(&attributes));
  RET_IF_FAILED(attributes->setNamedItem(new_child, &useless));
  return S_OK;
}

HRESULT AddXMLAttributeNode(IXMLDOMElement* xmlelement,
                            const TCHAR* attribute_name,
                            const TCHAR* attribute_value) {
  ASSERT1(xmlelement);
  ASSERT1(attribute_name);
  // attribute_value can be NULL

  RET_IF_FAILED(xmlelement->setAttribute(CComBSTR(attribute_name),
                                         CComVariant(attribute_value)));
  return S_OK;
}

HRESULT AddXMLAttributeNode(IXMLDOMNode* xmlnode,
                            const TCHAR* attribute_namespace,
                            const TCHAR* attribute_name,
                            const TCHAR* attribute_value) {
  ASSERT1(xmlnode);
  ASSERT1(attribute_name);
  // attribute_namespace can be NULL
  // attribute_value can be NULL

  CComPtr<IXMLDOMDocument> xmldoc;
  RET_IF_FAILED(xmlnode->get_ownerDocument(&xmldoc));
  ASSERT1(xmldoc);

  CComPtr<IXMLDOMNode> attribute_node;
  RET_IF_FAILED(CreateXMLNode(xmldoc,
                              NODE_ATTRIBUTE,
                              attribute_name,
                              attribute_namespace,
                              attribute_value,
                              &attribute_node));
  CComQIPtr<IXMLDOMAttribute> attribute(attribute_node);
  ASSERT1(attribute);
  RET_IF_FAILED(AddXMLAttributeNode(xmlnode, attribute));
  return S_OK;
}

HRESULT RemoveXMLChildrenByName(IXMLDOMNode* xmlnode, const XMLFQName& name) {
  ASSERT1(xmlnode);

  CComPtr<IXMLDOMNodeList> node_list;
  RET_IF_FAILED(xmlnode->get_childNodes(&node_list));
  ASSERT1(node_list);

  bool found = false;
  do {
    found = false;
    long count = 0;   // NOLINT
    RET_IF_FAILED(node_list->get_length(&count));
    RET_IF_FAILED(node_list->reset());

    for (int i = 0; i < count; ++i) {
      CComPtr<IXMLDOMNode> child_node, useless;
      RET_IF_FAILED(node_list->get_item(i, &child_node));
      ASSERT1(child_node);
      if (EqualXMLName(child_node, name)) {
        RET_IF_FAILED(xmlnode->removeChild(child_node, &useless));
        // Start loop over: the list is "alive" and changes when you remove a
        // node from it. Yes this seems to be n^2 but in fact we expect at
        // most one each of <Hash> and/or <Size> nodes.
        found = true;
        break;
      }
    }
  } while (found);

  return S_OK;
}

HRESULT GetXMLChildByName(IXMLDOMElement* xmlnode,
                          const TCHAR* child_name,
                          IXMLDOMNode** xmlchild) {
  ASSERT1(xmlnode);
  ASSERT1(child_name);
  ASSERT1(xmlchild);
  ASSERT1(!*xmlchild);

  *xmlchild = NULL;
  CComPtr<IXMLDOMNodeList> node_list;
  long node_list_length = 0;    // NOLINT
  RET_IF_FAILED(xmlnode->getElementsByTagName(CComBSTR(child_name),
                                              &node_list));
  ASSERT1(node_list);
  RET_IF_FAILED(node_list->get_length(&node_list_length));
  if (node_list_length <= 0) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }
  // Should only be one child node with name we're looking for.
  if (node_list_length > 1) {
    return CI_E_INVALID_MANIFEST;
  }
  RET_IF_FAILED(node_list->reset());
  RET_IF_FAILED(node_list->get_item(0, xmlchild));
  ASSERT1(*xmlchild);
  return S_OK;
}

HRESULT InsertXMLBeforeItem(IXMLDOMNode* xmlnode,
                            IXMLDOMNode* new_child,
                            size_t item_number) {
  ASSERT1(xmlnode);
  ASSERT1(new_child);

  CComPtr<IXMLDOMNodeList> child_list;
  CComPtr<IXMLDOMNode> refchild, useless;

  RET_IF_FAILED(xmlnode->get_childNodes(&child_list));
  ASSERT1(child_list);
  RET_IF_FAILED(child_list->get_item(item_number, &refchild));
  ASSERT1(refchild);
  RET_IF_FAILED(xmlnode->insertBefore(new_child,
                                      CComVariant(refchild),
                                      &useless));
  return S_OK;
}

HRESULT GetXMLParseError(IXMLDOMDocument* xmldoc,
                         IXMLDOMParseError** parse_error) {
  ASSERT1(xmldoc);
  ASSERT1(parse_error);
  ASSERT1(!*parse_error);

  *parse_error = NULL;
  CComPtr<IXMLDOMParseError> error;
  RET_IF_FAILED(xmldoc->get_parseError(&error));
  HRESULT error_code = 0;
  HRESULT hr = error->get_errorCode(&error_code);
  if (hr == S_OK) {
    *parse_error = error.Detach();
    return S_OK;
  } else if (hr == S_FALSE) {
    // No parse error
    return S_FALSE;
  } else {
    return hr;
  }
}

HRESULT InterpretXMLParseError(IXMLDOMParseError* parse_error,
                               HRESULT* error_code,
                               CString* message) {
  ASSERT1(parse_error);
  ASSERT1(error_code);
  ASSERT1(message);

  long line = 0;      // NOLINT
  long char_pos = 0;  // NOLINT
  CComBSTR src_text, reason;
  RET_IF_FAILED(parse_error->get_errorCode(error_code));
  RET_IF_FAILED(parse_error->get_line(&line));
  RET_IF_FAILED(parse_error->get_linepos(&char_pos));
  RET_IF_FAILED(parse_error->get_srcText(&src_text));
  RET_IF_FAILED(parse_error->get_reason(&reason));

  // Wild guess.
  size_t size_estimate = src_text.Length() + reason.Length() + 100;

  // TODO(omaha): think about replacing this call to _snwprintf with a
  // safestring function.
  std::vector<TCHAR> s(size_estimate);
  _snwprintf_s(&s.front(), size_estimate, _TRUNCATE,
               L"%d(%d) : error 0x%08lx: %s\n  %s",
               line, char_pos, *error_code,
               reason ? reason : L"",
               src_text ? src_text : L"<no source text>");
  // _snwprintf doesn't terminate the string with a null if
  // the formatted string fills the entire buffer.
  s[s.size()- 1] = L'\0';
  *message = &s.front();
  return S_OK;
}

HRESULT GetNumChildren(IXMLDOMNode* node, int* num_children) {
  ASSERT1(node);
  ASSERT1(num_children);

  *num_children = 0;
  CComPtr<IXMLDOMNodeList> children;
  RET_IF_FAILED(node->get_childNodes(&children));
  ASSERT1(children);

  long len = 0;   // NOLINT
  RET_IF_FAILED(children->get_length(&len));
  *num_children = len;
  return S_OK;
}

int GetNumAttributes(IXMLDOMNode* node) {
  ASSERT1(node);

  CComPtr<IXMLDOMNamedNodeMap> attr_map;
  if (FAILED(node->get_attributes(&attr_map))) {
    return 0;
  }
  ASSERT1(attr_map);
  long len = 0;   // NOLINT
  if (FAILED(attr_map->get_length(&len))) {
    return 0;
  }
  return len;
}

}  // namespace omaha

