#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <windows.h>

namespace StrUtil {

	// 1. UTF-8 转 WString
	inline std::wstring utf8_to_wstring(const std::string& str) {
		if (str.empty()) return L"";
		int size_needed = ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		// 移除 UTF-8 BOM 头
		if (wstrTo.size() > 0 && wstrTo[0] == 0xFEFF) wstrTo.erase(0, 1);
		return wstrTo;
	}

	// 2. WString 转 UTF-8
	inline std::string wstring_to_utf8(const std::wstring& wstr) {
		if (wstr.empty()) return "";
		int size_needed = ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	// 3. WString 转 本地 ANSI (如 GBK，用于鸭子库读文件)
	inline std::string wstring_to_local8bit(const std::wstring& wstr) {
		if (wstr.empty()) return "";
		int size_needed = ::WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		::WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	// 4. 去除两端空白字符
	inline std::wstring trim(const std::wstring& s) {
		size_t start = s.find_first_not_of(L" \t\r\n\v\f\u00A0");
		if (start == std::wstring::npos) return L"";
		size_t end = s.find_last_not_of(L" \t\r\n\v\f\u00A0");
		return s.substr(start, end - start + 1);
	}

	// 5. 字符串分割
	inline std::vector<std::wstring> split(const std::wstring& s, wchar_t delimiter) {
		std::vector<std::wstring> tokens;
		std::wstring token;
		std::wistringstream tokenStream(s);
		while (std::getline(tokenStream, token, delimiter)) {
			if (!token.empty()) tokens.push_back(token);
		}
		return tokens;
	}
}