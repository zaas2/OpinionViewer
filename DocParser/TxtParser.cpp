#include "TxtParser.h"
#include <fstream>
#include <locale>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<std::wstring> TxtParser::extractText(const std::wstring& filePath) {
	std::vector<std::wstring> result;

	// 以二进制模式打开，避免跨平台换行符解析干扰
	std::wifstream file(filePath, std::ios::binary);
	if (!file.is_open()) {
		return result;
	}

	// 核心：顺从 Windows 系统的本地化编码
	file.imbue(std::locale(""));

	std::wstring line;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == L'\r') {
			line.pop_back();
		}

		line.erase(0, line.find_first_not_of(L" \t"));
		line.erase(line.find_last_not_of(L" \t") + 1);

		if (!line.empty()) {
			result.push_back(line);
		}
	}

	file.close();
	return result;
}

std::vector<ParsedDoc> TxtParser::extractTextBatch(
	const std::vector<std::wstring>& filePaths,
	std::function<void(int, int, const std::wstring&)> progressCallback)
{
	std::vector<ParsedDoc> allFiles;
	if (filePaths.empty()) return allFiles;

	// 🚀 强转抹杀 C4267 警告
	int totalFiles = static_cast<int>(filePaths.size());
	int currentIdx = 0;

	for (const auto& filePath : filePaths) {
		fs::path fPath(filePath);
		std::wstring fileName = fPath.filename().wstring();

		if (progressCallback) progressCallback(currentIdx, totalFiles, fileName);

		std::vector<std::wstring> currentFileLines = extractText(filePath);

		if (!currentFileLines.empty()) {
			ParsedDoc doc;
			doc.fileName = fileName;
			doc.lines = currentFileLines;
			allFiles.push_back(doc);
		}

		currentIdx++;
		if (progressCallback) progressCallback(currentIdx, totalFiles, fileName);
	}

	return allFiles;
}