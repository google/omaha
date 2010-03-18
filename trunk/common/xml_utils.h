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
// xml_utils.h
//
// Utilities for working with XML files via MSXML.

#ifndef OMAHA_COMMON_XML_UTILS_H__
#define OMAHA_COMMON_XML_UTILS_H__

#include <windows.h>
#include <objbase.h>
#include <msxml.h>
#include <atlstr.h>
#include <utility>
#include <vector>

namespace omaha {

// Creates a DOMDocument that disallows external definitions to be included and
// resolved as part of the XML document stream at parse time.
HRESULT CoCreateSafeDOMDocument(IXMLDOMDocument** my_xmldoc);

// xmlfile can be any specified encoding.
HRESULT LoadXMLFromFile(const TCHAR* xmlfile,
                        bool preserve_whitespace,
                        IXMLDOMDocument** xmldoc);

// xmlstring must be UTF-16 or UCS-2.
HRESULT LoadXMLFromMemory(const TCHAR* xmlstring,
                          bool preserve_whitespace,
                          IXMLDOMDocument** xmldoc);

// xmldata can be any raw data supported by xml parser
HRESULT LoadXMLFromRawData(const std::vector<byte>& xmldata,
                           bool preserve_whitespace,
                           IXMLDOMDocument** xmldoc);

// xmlfile is in encoding specified in the XML document.
HRESULT SaveXMLToFile(IXMLDOMDocument* xmldoc, const TCHAR * xmlfile);

// xmlstring is in UCS-2
HRESULT SaveXMLToMemory(IXMLDOMDocument* xmldoc, CString* xmlstring);

// buffer is in the encoding specified in the XML document.
HRESULT SaveXMLToRawData(IXMLDOMDocument* xmldoc, std::vector<byte>* buffer);

// Canonicalizes the XML string so you can compute a signature on it.
// This is not the official canonicalization but a cheaper scheme which
// depends on the whitespace stripping capability of MSXML.
//
// xmlstring is in UTF-16 or UCS-2
HRESULT CanonicalizeXML(const TCHAR* xmlstring, CString* canonical_xmlstring);


// Dealing with element/attribute names: the combination of a base name
// and a namespace URI is a fully-qualified XML name, or: XMLFQName.

// We can't just typedef a std::pair because we need proper comparison operators
// in case we want to stick a XMLFQName into a standard collection.
struct XMLFQName {
  XMLFQName();
  XMLFQName(const TCHAR* u, const TCHAR* b);
  ~XMLFQName();

  CString uri;
  CString base;
};

bool operator==(const XMLFQName& u, const XMLFQName& v);
bool operator!=(const XMLFQName& u, const XMLFQName& v);
bool operator< (const XMLFQName& u, const XMLFQName& v);
bool operator> (const XMLFQName& u, const XMLFQName& v);
bool operator<=(const XMLFQName& u, const XMLFQName& v);
bool operator>=(const XMLFQName& u, const XMLFQName& v);

bool EqualXMLName(const XMLFQName& u, const XMLFQName& v);
bool EqualXMLName(IXMLDOMNode* pnode, const XMLFQName& u);
bool EqualXMLName(const XMLFQName& u, IXMLDOMNode* pnode);

// Returns the FQ name from the node.
HRESULT GetXMLFQName(IXMLDOMNode* node, XMLFQName* name);

// Returns a string version of an XMLFQName suitable for debugging use.
CString XMLFQNameToString(const XMLFQName& fqname);

// Returns a string version of a node's name suitable for debugging use.
CString NodeToString(IXMLDOMNode* pnode);

//
// Routines for dealing with fragments of DOM trees.
//
// Creates an XMLDOMNode of the given type with a given name and optional text.
HRESULT CreateXMLNode(IXMLDOMDocument* xmldoc,
                      int node_type,
                      const TCHAR* node_name,
                      const TCHAR* namespace_uri,
                      const TCHAR* text,
                      IXMLDOMNode** node_out);

// Adds newchild as a child node of xmlnode after all existing children.
HRESULT AppendXMLNode(IXMLDOMNode* xmlnode, IXMLDOMNode* new_child);

// Adds text as a child node of xmlnode after all existing children.
HRESULT AppendXMLNode(IXMLDOMNode* xmlnode, const TCHAR* text);

// Adds newchild as an attribute node of xmlnode replacing existing
// attribute with same name.
HRESULT AddXMLAttributeNode(IXMLDOMNode* xmlnode, IXMLDOMAttribute* new_child);

// Adds name/value pair as an attribute node of xmlnode replacing
// existing attribute with same name.
HRESULT AddXMLAttributeNode(IXMLDOMElement* xmlelement,
                            const TCHAR* attribute_name,
                            const TCHAR* attribute_value);

// Adds name/value pair as an attribute node of xmlnode replacing
// existing attribute with same name.
// Can add attributes to nodes other than IXMLDOMElement.
// Can add attributes with non-null namespaces.
HRESULT AddXMLAttributeNode(IXMLDOMNode* xmlnode,
                            const TCHAR* attribute_namespace,
                            const TCHAR* attribute_name,
                            const TCHAR* attribute_value);

// Removes all children of the given node that have the specified name.
HRESULT RemoveXMLChildrenByName(IXMLDOMNode* xmlnode, const XMLFQName& name);

// Gets a child of a given node by name
HRESULT GetXMLChildByName(IXMLDOMElement* xmlnode,
                          const TCHAR* child_name,
                          IXMLDOMNode** xmlchild);

// Adds newchild as a child node of xmlnode, before the exiting
// child item_number.
HRESULT InsertXMLBeforeItem(IXMLDOMNode* xmlnode,
                            IXMLDOMNode* new_child,
                            size_t item_number);

// Gets parse error information after a failed load.
HRESULT GetXMLParseError(IXMLDOMDocument* xmldoc,
                         IXMLDOMParseError** parse_error);

// Interprets parse error.
HRESULT InterpretXMLParseError(IXMLDOMParseError* parse_error,
                               HRESULT* error_code,
                               CString* message);

// Gets the number of children of this node.
HRESULT GetNumChildren(IXMLDOMNode* pnode, int* num_children);

// Gets the number of attributes of this node.
int GetNumAttributes(IXMLDOMNode* pnode);

// Maps over a list of XML DOM nodes of some kind, executing a function
// against each attribute in the list. Passes a cookie along to each
// function call useful for accumulating results.
// Template class List is usually a IXMLDOMNodeList or a IXMLDOMNamedNodeMap.
template <class List, class Cookie>
HRESULT ForEachNodeInList(List list,
                          HRESULT (*fun)(CComPtr<IXMLDOMNode>, Cookie),
                          Cookie cookie) {
  ASSERT1(list);  // List assumed to be a pointer type or smart pointer type
  ASSERT1(fun);

  long len = 0;   // NOLINT
  RET_IF_FAILED(list->get_length(&len));
  for (long i = 0; i != len; ++i) {   // NOLINT
    CComPtr<IXMLDOMNode> pnode;
    RET_IF_FAILED(list->get_item(i, &pnode));
    ASSERT1(pnode);
    RET_IF_FAILED(fun(pnode, cookie));
  }
  return S_OK;
}

// Maps over the attributes of a node, executing a function against each
// attribute. Passes a cookie along to each function call.
template <typename Cookie>
HRESULT ForEachAttribute(CComPtr<IXMLDOMNode> pnode,
                         HRESULT (*fun)(CComPtr<IXMLDOMNode>, Cookie),
                         Cookie cookie) {
  ASSERT1(pnode);
  ASSERT1(fun);

  CComPtr<IXMLDOMNamedNodeMap> attr_list;
  RET_IF_FAILED(pnode->get_attributes(&attr_list));
  ASSERT1(attr_list);
  RET_IF_FAILED(ForEachNodeInList(attr_list, fun, cookie));
  return S_OK;
}

// Maps over the children nodes of a node, executing a function against
// each child node. Passes a cookie along to each function call.
template <typename Cookie>
HRESULT ForEachChildNode(CComPtr<IXMLDOMNode> pnode,
                         HRESULT (*fun)(CComPtr<IXMLDOMNode>, Cookie),
                         Cookie cookie) {
  ASSERT1(pnode);
  ASSERT1(fun);

  CComPtr<IXMLDOMNodeList> child_list;
  RET_IF_FAILED(pnode->get_childNodes(&child_list));
  ASSERT1(child_list);
  RET_IF_FAILED(ForEachNodeInList(child_list, fun, cookie));
  return S_OK;
}

}  // namespace omaha

#endif  // OMAHA_COMMON_XML_UTILS_H__

