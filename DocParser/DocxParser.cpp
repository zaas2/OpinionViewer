#include "DocxParser.h"
#include "../DocParser/duckx/duckx.hpp"
#include <iostream>
#include <sstream>
#undef MAX_PATH
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include "PublicUtils.h"

namespace fs = std::filesystem;

std::vector<std::wstring> DocxParser::extractText(const std::wstring& filePath) {
	std::vector<std::wstring> lines;
	std::string pathStr = StrUtil::wstring_to_local8bit(filePath);

	try {
		duckx::Document doc(pathStr);
		doc.open();

		// 1. 提取普通段落
		for (auto p : doc.paragraphs()) {
			std::wstring paraText;
			for (auto r : p.runs()) {
				paraText += StrUtil::utf8_to_wstring(r.get_text());
			}
			paraText = StrUtil::trim(paraText);
			if (!paraText.empty() && std::find(lines.begin(), lines.end(), paraText) == lines.end()) {
				lines.push_back(paraText);
			}
		}

		// 2. 提取表格
		for (auto table : doc.tables()) {
			for (auto row : table.rows()) {
				for (auto cell : row.cells()) {
					for (auto p : cell.paragraphs()) {
						std::wstring cellText;
						for (auto r : p.runs()) {
							cellText += StrUtil::utf8_to_wstring(r.get_text());
						}

						// 按换行符切分
						std::wstringstream wss(cellText);
						std::wstring line;
						while (std::getline(wss, line, L'\n')) {
							line = StrUtil::trim(line);
							if (!line.empty() && std::find(lines.begin(), lines.end(), line) == lines.end()) {
								lines.push_back(line);
							}
						}
					}
				}
			}
		}
	}
	catch (...) {
		std::wcerr << L"DocxParser 解析异常，文件可能损坏或被占用: " << filePath << std::endl;
	}

	return lines;
}

std::vector<ParsedDoc> DocxParser::extractTextBatch(
	const std::vector<std::wstring>& filePaths,
	std::function<void(int, int, const std::wstring&)> progressCallback)
{
	std::vector<ParsedDoc> allFiles;
	if (filePaths.empty()) return allFiles;

	int totalFiles = static_cast<int>(filePaths.size());
	int currentIdx = 0;

	for (const auto& filePath : filePaths) {
		fs::path fPath(filePath);
		std::wstring fileName = fPath.filename().wstring();

		// 进度打点汇报
		if (progressCallback) {
			progressCallback(currentIdx, totalFiles, fileName);
		}

		std::vector<std::wstring> currentFileLines = extractText(filePath);

		if (!currentFileLines.empty()) {
			ParsedDoc doc;
			doc.fileName = fileName;
			doc.lines = currentFileLines;
			allFiles.push_back(doc);
		}

		currentIdx++;
		if (progressCallback) {
			progressCallback(currentIdx, totalFiles, fileName);
		}
	}

	return allFiles;
}