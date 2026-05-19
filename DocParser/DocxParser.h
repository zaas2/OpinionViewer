#pragma once
#include <vector>
#include <string>
#include <functional> 
#include "ParserData.h" 

class DocxParser {
public:
	// 单文件接口
	static std::vector<std::wstring> extractText(const std::wstring& filePath);

	// 使用统一的 ParsedDoc 结构体体位
	static std::vector<ParsedDoc> extractTextBatch(
		const std::vector<std::wstring>& filePaths,
		std::function<void(int, int, const std::wstring&)> progressCallback
	);
};