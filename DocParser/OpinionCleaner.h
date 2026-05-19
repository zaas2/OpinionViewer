#pragma once
#include <vector>
#include <string>
#include <regex>
#include "ParserData.h" // 🚀 引入全局标准结构体

class OpinionCleaner {
public:
	static void loadConfig(const std::wstring& iniPath);
	static std::wstring getDefaultScanPath();

	static ParsedDoc clean(const ParsedDoc& rawDoc);
	static std::vector<ParsedDoc> cleanBatch(const std::vector<ParsedDoc>& rawDocs);

private:
	static std::wstring m_defaultScanPath;
	static std::vector<std::wstring> m_exactBlacklist;
	static std::vector<std::wstring> m_prefixBlacklist;
	static std::vector<std::wregex> m_regexFilters;
	static std::wregex m_stripNumberingRegex;
	static bool m_isConfigLoaded;
};