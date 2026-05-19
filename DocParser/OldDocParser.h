#pragma once
#include <vector>
#include <string>
#include <functional>
#include "ParserData.h"

class OldDocParser {
public:
	// 单文件接口保持纯粹
	static std::vector<std::wstring> extractText(const std::wstring& filePath);

	// 批量接口使用极简结构体
	static std::vector<ParsedDoc> extractTextBatch(
		const std::vector<std::wstring>& filePaths,
		std::function<void(int, int, const std::wstring&)> progressCallback
	);

	static bool exportToWordDoc(
		const std::wstring& filePath,
		const std::vector<std::wstring>& headers,
		const std::vector<std::vector<std::wstring>>& rows
	);
};