#include "TxtParser.h"
#include <fstream>
#include <filesystem>
#include <string>
#include "../common/PublicUtils.h"

// 如果你的 TxtParser.h 里没包含 StrUtil，记得在这里 include 一下
// #include "StrUtil.h" 

namespace fs = std::filesystem;

// 🚀 核心黑科技：纯 C++ 字节流 UTF-8 合法性校验器
static bool isValidUtf8(const std::string& data) {
	int n = 0;
	for (char ch : data) {
		unsigned char c = static_cast<unsigned char>(ch);
		if (n > 0) {
			// 如果处于多字节字符的后续字节中，最高两位必须是 10
			if ((c & 0xC0) != 0x80) return false;
			n--;
		}
		else {
			if ((c & 0x80) == 0) n = 0; // 1字节 (ASCII)
			else if ((c & 0xE0) == 0xC0) n = 1; // 2字节
			else if ((c & 0xF0) == 0xE0) n = 2; // 3字节 (绝大多数中文)
			else if ((c & 0xF8) == 0xF0) n = 3; // 4字节
			else return false; // 非法首字节，必定不是纯 UTF-8
		}
	}
	return n == 0; // 遍历完没截断，就是合法的 UTF-8
}

std::vector<std::wstring> TxtParser::extractText(const std::wstring& filePath) {
	std::vector<std::wstring> result;

	// 1. 转为本地编码路径并以二进制模式打开
	std::string pathStr = StrUtil::wstring_to_local8bit(filePath);
	std::ifstream file(pathStr, std::ios::binary);
	if (!file.is_open()) {
		return result;
	}

	// 2. 一次性将文件全部读入 std::string 作为“生肉”字节流
	std::string rawData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	if (rawData.empty()) {
		return result;
	}

	std::wstring textContent;

	// 3. 【编码雷达介入】
	if (rawData.size() >= 3 &&
		static_cast<unsigned char>(rawData[0]) == 0xEF &&
		static_cast<unsigned char>(rawData[1]) == 0xBB &&
		static_cast<unsigned char>(rawData[2]) == 0xBF)
	{
		// 情况 A：带有标准的 UTF-8 BOM 签名，跳过前三个字节解码
		textContent = StrUtil::utf8_to_wstring(rawData.substr(3));
	}
	else if (isValidUtf8(rawData))
	{
		// 情况 B：没有 BOM，但全文内容完美符合 UTF-8 编码规范
		textContent = StrUtil::utf8_to_wstring(rawData);
	}
	else
	{
		// 情况 C：既没 BOM 也不符合 UTF-8 规范，判定为 GBK/ANSI
		// 假设你的 StrUtil 有 local8bit_to_wstring (或者对应的本地转码函数)
		textContent = StrUtil::local8bit_to_wstring(rawData);
	}

	// 4. 手动按换行符切分，并进行 C++ 风格的字符串清理
	size_t start = 0;
	size_t end = 0;
	while ((end = textContent.find(L'\n', start)) != std::wstring::npos) {
		std::wstring line = textContent.substr(start, end - start);
		start = end + 1;

		// 剔除 \r
		if (!line.empty() && line.back() == L'\r') {
			line.pop_back();
		}

		// 剔除首尾空白
		line.erase(0, line.find_first_not_of(L" \t"));
		if (!line.empty()) {
			line.erase(line.find_last_not_of(L" \t") + 1);
		}

		if (!line.empty()) {
			result.push_back(line);
		}
	}

	// 处理最后一行（如果没有以 \n 结尾的情况）
	if (start < textContent.size()) {
		std::wstring line = textContent.substr(start);
		if (!line.empty() && line.back() == L'\r') line.pop_back();
		line.erase(0, line.find_first_not_of(L" \t"));
		if (!line.empty()) line.erase(line.find_last_not_of(L" \t") + 1);
		if (!line.empty()) result.push_back(line);
	}

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