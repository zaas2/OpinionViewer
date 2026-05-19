#include "OpinionCleaner.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include "PublicUtils.h"

std::wstring OpinionCleaner::m_defaultScanPath = L"./";
std::vector<std::wstring> OpinionCleaner::m_exactBlacklist;
std::vector<std::wstring> OpinionCleaner::m_prefixBlacklist;
std::vector<std::wregex> OpinionCleaner::m_regexFilters;
std::wregex OpinionCleaner::m_stripNumberingRegex;
bool OpinionCleaner::m_isConfigLoaded = false;

// 切割字符串
static std::vector<std::wstring> split(const std::wstring& s, wchar_t delimiter) {
	std::vector<std::wstring> tokens;
	std::wstring token;
	std::wistringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter)) {
		if (!token.empty()) tokens.push_back(token);
	}
	return tokens;
}

void OpinionCleaner::loadConfig(const std::wstring& iniPath) {
	m_stripNumberingRegex.assign(L"^\\s*([A-Za-z]+\\d+|\\d+)[\\.\\u3001\\:\\uff1a]\\s*(?!\\d)");

	std::ifstream file(iniPath);
	if (!file.is_open()) {
		std::wcerr << L"警告：找不到配置文件，将使用默认规则！" << std::endl;
		return;
	}

	std::string line;
	std::wstring currentGroup = L"";

	while (std::getline(file, line)) {
		std::wstring wline = StrUtil::trim(StrUtil::utf8_to_wstring(line));
		if (wline.empty() || wline[0] == L';' || wline[0] == L'#') continue;

		if (wline.front() == L'[' && wline.back() == L']') {
			currentGroup = wline.substr(1, wline.length() - 2);
			continue;
		}

		size_t equalPos = wline.find(L'=');
		if (equalPos != std::wstring::npos) {
			std::wstring key = StrUtil::trim(wline.substr(0, equalPos));
			std::wstring value = StrUtil::trim(wline.substr(equalPos + 1));

			if (value.length() >= 2 && value.front() == L'"' && value.back() == L'"') {
				value = value.substr(1, value.length() - 2);
			}

			if (currentGroup == L"General" && key == L"DefaultScanPath") {
				m_defaultScanPath = value;
			}
			else if (currentGroup == L"OpinionFormat" && key == L"StripNumberingRegex") {
				m_stripNumberingRegex.assign(value);
			}
			else if (currentGroup == L"CleanRules" && key == L"ExactBlacklist") {
				m_exactBlacklist = split(value, L'|');
			}
			else if (currentGroup == L"CleanRules" && key == L"PrefixBlacklist") {
				m_prefixBlacklist = split(value, L'|');
			}
			else if (currentGroup == L"RegexBlacklist") {
				try { m_regexFilters.push_back(std::wregex(value)); }
				catch (...) {}
			}
		}
	}
	m_isConfigLoaded = true;
	std::wcout << L"原生 INI 配置文件加载成功，加载正则规则数：" << m_regexFilters.size() << std::endl;
}

std::wstring OpinionCleaner::getDefaultScanPath() {
	return m_defaultScanPath;
}

ParsedDoc OpinionCleaner::clean(const ParsedDoc& rawDoc) {
	ParsedDoc cleanDoc;
	cleanDoc.fileName = rawDoc.fileName; // 传承文件名
	std::wregex spaceRegex(L"\\s+");

	for (std::wstring line : rawDoc.lines) {
		line = StrUtil::trim(line);
		if (line.length() < 2) continue;

		bool isGarbage = false;

		std::wstring noSpaceLine = std::regex_replace(line, spaceRegex, L"");
		for (const std::wstring& word : m_exactBlacklist) {
			if (noSpaceLine == word) { isGarbage = true; break; }
		}
		if (isGarbage) continue;

		for (const std::wstring& prefix : m_prefixBlacklist) {
			if (line.rfind(prefix, 0) == 0) { isGarbage = true; break; }
		}
		if (isGarbage) continue;

		for (const std::wregex& reg : m_regexFilters) {
			if (std::regex_search(line, reg)) { isGarbage = true; break; }
		}
		if (isGarbage) continue;

		if (line == L"合格" || line == L"合格。") continue;

		line = std::regex_replace(line, m_stripNumberingRegex, L"");
		line = StrUtil::trim(line);
		if (line.empty()) continue;

		if (std::find(cleanDoc.lines.begin(), cleanDoc.lines.end(), line) == cleanDoc.lines.end()) {
			cleanDoc.lines.push_back(line);
		}
	}

	return cleanDoc;
}

std::vector<ParsedDoc> OpinionCleaner::cleanBatch(const std::vector<ParsedDoc>& rawDocs) {
	std::vector<ParsedDoc> result;
	result.reserve(rawDocs.size());

	for (const auto& doc : rawDocs) {
		ParsedDoc cleaned = clean(doc);
		// 只有清洗后还有料的文档，才被装入最终的卡车
		if (!cleaned.lines.empty()) {
			result.push_back(cleaned);
		}
	}
	return result;
}