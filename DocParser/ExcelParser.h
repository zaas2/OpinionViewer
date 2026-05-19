#pragma once
#include <vector>
#include <string>
#include <functional>
#include "ParserData.h"

class ExcelParser {
public:
	// 原有的单文件解析接口
	static std::vector<std::wstring> extractText(const std::wstring& filePath);

	static std::vector<ParsedDoc> extractTextBatch(
		const std::vector<std::wstring>& filePaths,
		std::function<void(int, int, const std::wstring&)> progressCallback
	);
};