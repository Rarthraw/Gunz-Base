#include "stdafx.h"
#include "MXml.h"
#include "MLocale.h"
#include "MZFileSystem.h"
#include "MDebug.h"
#include <charconv>
#include <cstring>
#include <string>
#include <algorithm>
#include <cassert>
#include "MUtil.h"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"
#include "StringView.h"
#include "ArrayView.h"
#include "MZFile.h"
#include "MFile.h"

void MXmlNode::GetNodeName(char* sOutStr, int maxlen)
{
	if (m_pDomNode)
	{
		strcpy_safe(sOutStr, maxlen, m_pDomNode->name());
	}
}

StringView MXmlNode::GetNodeName() const
{
	if (!m_pDomNode)
		return nullptr;
	return StringView(m_pDomNode->name(), m_pDomNode->name_size());
}

void MXmlNode::GetText(char* sOutStr, int nMaxCharNum)
{
	if (!m_pDomNode)
		return;

	const char* value = m_pDomNode->value();
	if (!value) {
		if (nMaxCharNum > 0)
			sOutStr[0] = 0;
		return;
	}

	const size_t value_size = m_pDomNode->value_size();

	if (nMaxCharNum <= 0)
	{
		assert(false && "Invalid size passed to MXmlNode::GetText");
		return;
	}

	strncpy_safe(sOutStr, nMaxCharNum, value, value_size);
}

void MXmlNode::SetText(const char* sText)
{
	if (!m_pDomNode)
		return;

	auto PermanentString = m_pDomNode->document()->allocate_string(sText);
	m_pDomNode->value(PermanentString);
}

int	MXmlNode::GetChildNodeCount()
{
	if (!m_pDomNode)
		return 0;

	int count = 0;
	auto ptr = m_pDomNode->first_node();
	while (ptr)
	{
		ptr = ptr->next_sibling();
		++count;
	}

	return count;
}

MXmlDomNodeType MXmlNode::GetNodeType()
{
	if (!m_pDomNode)
	{
		assert(false);
		return rapidxml::node_element;
	}

	return m_pDomNode->type();
}

bool MXmlNode::HasChildNodes()
{
	if (!m_pDomNode)
		return false;

	return m_pDomNode->first_node() != nullptr;
}

MXmlNode MXmlNode::GetChildNode(int iIndex)
{
	if (!m_pDomNode)
		return MXmlNode{};

	auto ptr = m_pDomNode->first_node();
	for (int i = 0; i < iIndex; ++i)
	{
		if (!ptr)
			break;

		ptr = ptr->next_sibling();
	}

	return ptr;
}

void MXmlNode::NextSibling()
{
	if (m_pDomNode)
	{
		m_pDomNode = m_pDomNode->next_sibling();
	}
}

void MXmlNode::PreviousSibling()
{
	if (m_pDomNode)
	{
		m_pDomNode = m_pDomNode->previous_sibling();
	}
}

bool MXmlNode::FindChildNode(const char* sNodeName, MXmlNode* pOutNode)
{
	if (!m_pDomNode)
		return false;

	auto ptr = m_pDomNode->first_node(sNodeName);
	if (!ptr)
		return false;

	pOutNode->SetXmlDomNodePtr(ptr);
	return true;
}

bool MXmlNode::AppendChild(MXmlNode node)
{
	if (node.GetParent().GetXmlDomNodePtr() != nullptr)
		return true;

	m_pDomNode->append_node(node.GetXmlDomNodePtr());

	return true;
}

void MXmlElement::SetContents(int iValue)
{
	char szTemp[20];
	sprintf_safe(szTemp, "%d", iValue);
	SetContents(szTemp);
}

void MXmlElement::SetContents(float fValue)
{
	char szTemp[32];
	sprintf_safe(szTemp, "%12.4f", fValue);
	SetContents(szTemp);
}

void MXmlElement::SetContents(bool bValue)
{
	if (bValue)
	{
		SetContents("true");
	}
	else
	{
		SetContents("false");
	}
}

bool MXmlElement::AppendText(const char* sText)
{
	// This is only used for whitespace, which the printer adds anyway, so we'll just ignore it.
	return true;

	/*
	auto node = m_pDomNode->document()->allocate_node(rapidxml::node_data, 0, sText);
	m_pDomNode->append_node(node);

	return true;
	*/
}

bool MXmlElement::AppendChild(MXmlElement aChildElement)
{
	auto pElement = aChildElement.GetXmlDomNodePtr();
	m_pDomNode->append_node(pElement);

	return true;
}

bool MXmlElement::AppendChild(const char* sTagName, const char* sTagText)
{
	auto Doc = m_pDomNode->document();
	auto node = Doc->allocate_node(rapidxml::node_element,
		Doc->allocate_string(sTagName),
		Doc->allocate_string(sTagText));
	m_pDomNode->append_node(node);

	return true;
}

MXmlElement	MXmlElement::CreateChildElement(const char* sTagName)
{
	auto Doc = m_pDomNode->document();
	auto node = Doc->allocate_node(rapidxml::node_element,
		Doc->allocate_string(sTagName));
	m_pDomNode->append_node(node);

	return MXmlElement{ node };
}

cml::optional<StringView> MXmlElement::GetAttribute(StringView AttrName, bool CaseSensitive) const
{
	auto Attribute = m_pDomNode->first_attribute(AttrName.data(), AttrName.size(), CaseSensitive);
	if (!Attribute || !Attribute->value())
		return cml::nullopt;
	return StringView(Attribute->value(), Attribute->value_size());
}

bool MXmlElement::GetAttribute(char* sOutText, int maxlen, const char* sAttrName, const char* sDefaultText)
{
	auto Attribute = m_pDomNode->first_attribute(sAttrName);
	if (!Attribute || !Attribute->value())
	{
		if (sDefaultText)
		{
			strcpy_safe(sOutText, maxlen, sDefaultText);
		}

		return false;
	}

	strcpy_safe(sOutText, maxlen, Attribute->value());

	return true;
}

bool MXmlElement::GetAttribute(int* ipOutValue, const char* sAttrName, int nDefaultValue)
{
	auto attr = GetAttribute(sAttrName);
	if (!attr) {
		*ipOutValue = nDefaultValue;
		return false;
	}

	auto [ptr, ec] = std::from_chars(attr->data(), attr->data() + attr->size(), *ipOutValue);

	if (ec != std::errc()) {
		*ipOutValue = nDefaultValue;
		return false;
	}

	return true;
}

bool MXmlElement::GetAttribute(float* fpOutValue, const char* sAttrName, float fDefaultValue)
{
	auto attr = GetAttribute(sAttrName);
	if (!attr) {
		*fpOutValue = fDefaultValue;
		return false;
	}

	auto [ptr, ec] = std::from_chars(attr->data(), attr->data() + attr->size(), *fpOutValue);

	if (ec != std::errc()) {
		*fpOutValue = fDefaultValue;
		return false;
	}

	return true;
}
bool MXmlElement::GetAttribute(bool* bOutValue, const char* sAttrName, bool bDefaultValue)
{
	auto attr_opt = GetAttribute(sAttrName);
	if (!attr_opt) {
		*bOutValue = bDefaultValue;
		return false;
	}
	auto attr = *attr_opt;

	auto iequals = [](StringView a, StringView b) {
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); ++i)
			if (tolower(a[i]) != tolower(b[i]))
				return false;
		return true;
	};

	if (iequals(attr, "true")) {
		*bOutValue = true;
	}
	else if (iequals(attr, "false")) {
		*bOutValue = false;
	}
	else {
		*bOutValue = bDefaultValue;
	}

	return true;
}

bool MXmlElement::GetAttribute(std::string* pstrOutValue, const char* sAttrName, const char* sDefaultValue)
{
	auto attr = GetAttribute(sAttrName);
	if (!attr) {
		*pstrOutValue = sDefaultValue;
		return false;
	}

	pstrOutValue->assign(attr->data(), attr->size());
	return true;
}

int MXmlElement::GetAttributeCount()
{
	auto ptr = m_pDomNode->first_attribute();
	int count = 0;

	while (ptr)
	{
		ptr = ptr->next_attribute();
		++count;
	}

	return count;
}

void MXmlElement::GetAttribute(int index, char* szoutAttrName, int maxlen1, char* szoutAttrValue, int maxlen2)
{
	auto ptr = m_pDomNode->first_attribute();

	for (int i = 0; i < index; ++i)
		ptr = ptr->next_attribute();

	if (!ptr || !ptr->name() || !ptr->value())
	{
		strcpy_safe(szoutAttrName, maxlen1, "");
		strcpy_safe(szoutAttrValue, maxlen2, "");
		return;
	}

	strcpy_safe(szoutAttrName, maxlen1, ptr->name());
	strcpy_safe(szoutAttrValue, maxlen2, ptr->value());
}

bool MXmlElement::AddAttribute(const char* sAttrName, const char* sAttrText)
{
	auto Doc = m_pDomNode->document();
	auto Attribute = Doc->allocate_attribute(Doc->allocate_string(sAttrName),
		Doc->allocate_string(sAttrText));
	m_pDomNode->append_attribute(Attribute);

	return true;
}

bool MXmlElement::AddAttribute(const char* sAttrName, int iAttrValue)
{
	char szTemp[20];
	itoa_safe(iAttrValue, szTemp, 10);

	return AddAttribute(sAttrName, szTemp);
}

bool MXmlElement::AddAttribute(const char* sAttrName, bool bAttrValue)
{
	if (bAttrValue)
	{
		return AddAttribute(sAttrName, "true");
	}
	else
	{
		return AddAttribute(sAttrName, "false");
	}
	return false;
}

bool MXmlElement::SetAttribute(const char* sAttrName, char* sAttrText)
{
	if (!m_pDomNode)
	{
		return false;
	}

	auto Doc = m_pDomNode->document();
	if (!Doc)
	{
		assert(false);
		return false;
	}

	auto Attribute = m_pDomNode->first_attribute(sAttrName);
	if (!Attribute)
	{
		return AddAttribute(sAttrName, sAttrText);
	}

	Attribute->value(Doc->allocate_string(sAttrText));

	return true;
}

bool MXmlElement::SetAttribute(const char* sAttrName, int iAttrValue)
{
	char szTemp[20];
	itoa_safe(iAttrValue, szTemp, 10);

	return SetAttribute(sAttrName, szTemp);
}

bool MXmlElement::GetChildContents(char* sOutStr, const char* sChildTagName, int nMaxCharNum)
{
	MXmlNode node;

	if (FindChildNode(sChildTagName, &node))
	{
		node.GetText(sOutStr, nMaxCharNum);
		return true;
	}
	else
	{
		return false;
	}
}

bool MXmlElement::GetChildContents(int* iOutValue, const char* sChildTagName)
{
	char szBuf[256];
	if (GetChildContents(szBuf, sChildTagName))
	{
		try
		{
			*iOutValue = atoi(szBuf);
		}
		catch (...)
		{
			*iOutValue = 0;
			return false;
		}

		return true;
	}

	return false;
}

bool MXmlElement::GetChildContents(float* fOutValue, const char* sChildTagName)
{
	char szBuf[256];
	if (GetChildContents(szBuf, sChildTagName))
	{
		try
		{
			*fOutValue = (float)atof(szBuf);
		}
		catch (...)
		{
			*fOutValue = 0;
			return false;
		}

		return true;
	}

	return false;
}

bool MXmlElement::GetChildContents(bool* bOutValue, const char* sChildTagName)
{
	char szBuf[256];
	if (GetChildContents(szBuf, sChildTagName))
	{
		if (!_stricmp(szBuf, "true"))
		{
			*bOutValue = true;
		}
		else
		{
			*bOutValue = false;
		}
		return true;
	}

	return false;
}

void MXmlElement::GetContents(int* ipOutValue)
{
	if (!m_pDomNode || !m_pDomNode->value()) {
		*ipOutValue = 0;
		return;
	}

	StringView text{ m_pDomNode->value(), m_pDomNode->value_size() };
	if (text.empty()) {
		*ipOutValue = 0;
		return;
	}

	auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), *ipOutValue);

	if (ec != std::errc()) {
		*ipOutValue = 0;
	}
}

void MXmlElement::GetContents(bool* bpOutValue)
{
	if (!m_pDomNode || !m_pDomNode->value()) {
		*bpOutValue = false;
		return;
	}

	StringView text{ m_pDomNode->value(), m_pDomNode->value_size() };

	auto iequals = [](StringView a, StringView b) {
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); ++i)
			if (tolower(a[i]) != tolower(b[i]))
				return false;
		return true;
	};

	if (iequals(text, "true")) {
		*bpOutValue = true;
	}
	else {
		*bpOutValue = false;
	}
}

void MXmlElement::GetContents(float* fpOutValue)
{
	if (!m_pDomNode || !m_pDomNode->value()) {
		*fpOutValue = 0.0f;
		return;
	}

	StringView text{ m_pDomNode->value(), m_pDomNode->value_size() };
	if (text.empty()) {
		*fpOutValue = 0.0f;
		return;
	}

	auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), *fpOutValue);

	if (ec != std::errc()) {
		*fpOutValue = 0.0f;
	}
}

void MXmlElement::GetContents(std::string* pstrValue)
{
	if (!m_pDomNode || !m_pDomNode->value()) {
		pstrValue->clear();
		return;
	}

	pstrValue->assign(m_pDomNode->value(), m_pDomNode->value_size());
}

bool MXmlElement::RemoveAttribute(const char* sAttrName)
{
	auto Attribute = m_pDomNode->first_attribute(sAttrName);
	if (!Attribute)
		return false;

	m_pDomNode->remove_attribute(Attribute);

	return true;
}

bool MXmlDocument::LoadFromFile(const char* m_sFileName, MZFileSystem* FileSystem)
{
	MZFile File;
	if (!File.Open(m_sFileName, FileSystem))
		return false;

	const auto Size = File.GetLength();
	FileBuffer.resize(Size);
	if (!File.Read(&FileBuffer[0], Size))
		return false;

	return Parse(m_sFileName);
}

bool MXmlDocument::LoadFromMemory(const char* szBuffer, size_t Size)
{
	if (Size == -1)
		Size = strlen(szBuffer);
	FileBuffer = { szBuffer, Size };

	return Parse();
}

bool MXmlDocument::Parse(const char* Filename)
{
	try
	{
		Doc.parse<rapidxml::parse_default>(&FileBuffer[0], FileBuffer.size());
	}
	catch (rapidxml::parse_error& e)
	{
		if (Filename == nullptr)
		{
			MLog("RapidXML threw parse_error on file in memory!\n"
				"e.what() = %s, e.where() = %s\n",
				e.what(), e.where<char>());
		}
		else
		{
			MLog("RapidXML threw parse_error on %s!\n"
				"e.what() = %s, e.where() = %s\n",
				Filename,
				e.what(), e.where<char>());
		}
		return false;
	}

	Root = Doc.first_node("xml", 0, false);

	return Root != nullptr;
}

bool MXmlDocument::SaveToFile(const char* m_sFileName)
{
	MFile::RWFile File{ m_sFileName, MFile::Clear };

	if (File.error())
		return false;

	rapidxml::print(MFile::FileOutputIterator{ File }, Doc);

	if (File.error())
		return false;

	return true;
}

bool MXmlDocument::CreateProcessingInstruction(const char* szHeader)
{
	auto node = Doc.allocate_node(rapidxml::node_pi, 0, szHeader);
	Doc.append_node(node);
	return true;
}

bool MXmlDocument::Delete(MXmlNode* pNode)
{
	pNode->GetParent().GetXmlDomNodePtr()->remove_node(pNode->GetXmlDomNodePtr());

	return pNode->GetXmlDomNodePtr() != NULL;
}

bool MXmlDocument::AppendChild(MXmlNode node)
{
	if (node.GetParent().GetXmlDomNodePtr() != nullptr)
		return true;

	Doc.append_node(node.GetXmlDomNodePtr());

	return true;
}

MXmlElement	MXmlDocument::CreateElement(const char* sName)
{
	auto node = Doc.allocate_node(rapidxml::node_element, Doc.allocate_string(sName));
	Doc.append_node(node);

	return MXmlElement(node);
}