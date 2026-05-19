#pragma once
#include <string>
#include <vector>
#include <functional>
#include "ParserData.h" 

class TxtParser {
public:
	// 单文件接口
	static std::vector<std::wstring> extractText(const std::wstring& filePath);

	// 🚀 批量接口
	static std::vector<ParsedDoc> extractTextBatch(
		const std::vector<std::wstring>& filePaths,
		std::function<void(int, int, const std::wstring&)> progressCallback
	);
};