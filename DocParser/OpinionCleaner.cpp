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

void OpinionCleaner::loadConfig(const std::wstring& iniPath) {
	// 默认规则（注意 C++ 代码里的双斜杠是没问题的，因为编译器会转义）
	m_stripNumberingRegex.assign(L"^\\s*([A-Za-z]+\\d+|\\d+)[\\.\\u3001\\:\\uff1a]\\s*(?!\\d)");

	std::ifstream file(iniPath);
	if (!file.is_open()) {
		std::wcerr << L"警告：找不到配置文件，将使用默认规则！" << std::endl;
		return;
	}

	std::string line;
	std::wstring currentGroup = L"";
	int loadedRegexCount = 0;

	while (std::getline(file, line)) {
		std::wstring wline = StrUtil::trim(StrUtil::utf8_to_wstring(line));

		// 🛡️ 核心修复 1：拔除 Windows UTF-8 文件的隐形炸弹 BOM (\xFEFF)
		if (!wline.empty() && wline[0] == 0xFEFF) {
			wline.erase(0, 1);
		}

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

			// 🛡️ 核心修复 2：将 INI 中为了防转义写的双斜杠 `\\`，还原回 `\` 给 wregex 使用
			// 因为 std::ifstream 读到的 `\\s` 就是字面意义的两个字符 '\' 和 's'
			size_t pos = 0;
			while ((pos = value.find(L"\\\\", pos)) != std::wstring::npos) {
				value.replace(pos, 2, L"\\");
				pos += 1;
			}

			if (currentGroup == L"General" && key == L"DefaultScanPath") {
				m_defaultScanPath = value;
			}
			else if (currentGroup == L"OpinionFormat" && key == L"StripNumberingRegex") {
				m_stripNumberingRegex.assign(value);
			}
			else if (currentGroup == L"CleanRules" && key == L"ExactBlacklist") {
				m_exactBlacklist = StrUtil::split(value, L'|');
			}
			else if (currentGroup == L"CleanRules" && key == L"PrefixBlacklist") {
				m_prefixBlacklist = StrUtil::split(value, L'|');
			}
			else if (currentGroup == L"RegexBlacklist") {
				try {
					m_regexFilters.push_back(std::wregex(value));
					loadedRegexCount++;
				}
				// 🛡️ 核心修复 3：把生吞的异常吐出来，到底错在哪一目了然！
				catch (const std::regex_error& e) {
					std::wcerr << L"❌ 致命错误：正则表达式语法不合法 [" << key << L"] = " << value
						<< L"\n   原因: " << e.what() << std::endl;
				}
			}
		}
	}
	m_isConfigLoaded = true;
	std::wcout << L"原生 INI 配置文件加载成功，成功加载正则规则数：" << loadedRegexCount << std::endl;
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