#pragma once
#include <string>
#include <vector>

struct ParsedDoc {
	std::wstring fileName;              // 来源文件名
	std::vector<std::wstring> lines;    // 提取出的该文件所有行
};