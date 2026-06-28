#include "OldDocParser.h"
#include <windows.h>
#include <comdef.h>
#include <filesystem>
#include <fstream>
#include "PublicUtils.h"
#include <iostream>
#include <algorithm>
#include <sstream>

#pragma warning(push)
#pragma warning(disable: 4146 4278)
#import "libid:2DF8D04C-5BFA-101B-BDE5-00AA0044DE52" auto_rename auto_search rename_namespace("Office")
#import "libid:00020905-0000-0000-C000-000000000046" auto_search auto_rename rename_namespace("WordX") rename("ExitWindows", "WordExitWindows") rename("FindText", "WordFindText")
#pragma warning(pop)

using namespace WordX;
namespace fs = std::filesystem;

static std::wstring trim(const std::wstring& s) {
	size_t start = s.find_first_not_of(L" \t\r\n\v\f\u00A0");
	if (start == std::wstring::npos) return L"";
	size_t end = s.find_last_not_of(L" \t\r\n\v\f\u00A0");
	return s.substr(start, end - start + 1);
}

std::vector<ParsedDoc> OldDocParser::extractTextBatch(const std::vector<std::wstring>& filePaths,std::function<void(int, int, const std::wstring&)> progressCallback)
{
	std::vector<ParsedDoc> allFiles;
	if (filePaths.empty()) return allFiles;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) return allFiles;

	WordX::_ApplicationPtr pWord = nullptr;
	try {
		hr = pWord.CreateInstance("Word.Application");
		if (FAILED(hr) || pWord == nullptr) { CoUninitialize(); return allFiles; }

		pWord->Visible = VARIANT_FALSE;
		pWord->DisplayAlerts = WordX::wdAlertsNone;
		pWord->AutomationSecurity = Office::msoAutomationSecurityForceDisable;

		_variant_t varFalse((short)FALSE);
		_variant_t varTrue((short)TRUE);
		_variant_t varConfirm(VARIANT_FALSE);
		WordX::DocumentsPtr pDocs = pWord->Documents;

		int totalFiles = static_cast<int>(filePaths.size());
		int currentIdx = 0;

		for (const auto& filePath : filePaths) {
			fs::path fPath(filePath);
			std::wstring nativePath = fPath.make_preferred().wstring();
			std::wstring fileName = fPath.filename().wstring();

			if (progressCallback) progressCallback(currentIdx, totalFiles, fileName);

			_variant_t varFile(nativePath.c_str());
			WordX::_DocumentPtr pDoc = nullptr;
			std::vector<std::wstring> currentFileLines;

			try {
				pDoc = pDocs->Open(&varFile, &varConfirm, &varTrue);
				if (pDoc) {
					WordX::RangePtr pRange = pDoc->Content;
					std::wstring rawText = (wchar_t*)pRange->Text;

					std::replace(rawText.begin(), rawText.end(), (wchar_t)0x07, L'\n');
					std::replace(rawText.begin(), rawText.end(), L'\v', L'\n');
					std::replace(rawText.begin(), rawText.end(), (wchar_t)0x0B, L'\n');
					std::replace(rawText.begin(), rawText.end(), L'\r', L'\n');

					std::wstringstream wss(rawText);
					std::wstring line;
					while (std::getline(wss, line, L'\n')) {
						line = trim(line);
						if (!line.empty()) {
							currentFileLines.push_back(line);
						}
					}
					pDoc->Close(&varFalse);
				}
			}
			catch (...) { if (pDoc != nullptr) { try { pDoc->Close(&varFalse); } catch (...) {} } }

			// 🚀 使用结构体干净利落地装车
			if (!currentFileLines.empty()) {
				ParsedDoc doc;
				doc.fileName = fileName;
				doc.lines = currentFileLines;
				allFiles.push_back(doc);
			}

			currentIdx++;
			if (progressCallback) progressCallback(currentIdx, totalFiles, fileName);
		}
		pWord->Quit(&varFalse);
	}
	catch (...) {
		if (pWord != nullptr) { try { _variant_t varQuitFalse((short)FALSE); pWord->Quit(&varQuitFalse); } catch (...) {} }
	}

	CoUninitialize();
	return allFiles;
}

std::vector<std::wstring> OldDocParser::extractText(const std::wstring& filePath)
{
	auto batchResult = extractTextBatch({ filePath }, nullptr);
	if (!batchResult.empty()) {
		return batchResult[0].lines;
	}
	return {};
}

bool OldDocParser::exportToWordDoc(
	const std::wstring& filePath,
	const std::vector<std::wstring>& headers,
	const std::vector<std::vector<std::wstring>>& rows)
{
	// 强制用 utf-8 写入
	std::string pathStr = StrUtil::wstring_to_local8bit(filePath);
	std::ofstream out(pathStr, std::ios::binary);
	if (!out.is_open()) return false;

	// 写入 UTF-8 BOM，防止某些老版本 Word 中文乱码
	const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
	out.write(reinterpret_cast<const char*>(bom), sizeof(bom));

	// HTML 头与 CSS 样式 (极致简约)
	out << "<html><head><meta charset='utf-8'><style>"
		<< "table { border-collapse: collapse; width: 100%; font-family: 'Microsoft YaHei', sans-serif; }"
		<< "th, td { border: 1px solid #000; padding: 8px; text-align: left; vertical-align: top; }"
		<< "th { background-color: #f2f2f2; font-weight: bold; }"
		<< "</style></head><body>"
		<< "<h2>" << StrUtil::wstring_to_utf8(L"审查意见汇总表") << "</h2>"
		<< "<table><tr>";

	// 动态写入表头
	for (const auto& head : headers) {
		out << "<th>" << StrUtil::wstring_to_utf8(head) << "</th>";
	}
	out << "</tr>\n";

	// 动态写入数据行，并替换换行符为 <br>
	for (const auto& row : rows) {
		out << "<tr>";
		for (const auto& cell : row) {
			std::string utf8Cell = StrUtil::wstring_to_utf8(cell);

			// 手动把纯文本换行替换为 HTML 换行
			size_t pos = 0;
			while ((pos = utf8Cell.find("\n", pos)) != std::string::npos) {
				utf8Cell.replace(pos, 1, "<br>");
				pos += 4;
			}
			out << "<td>" << utf8Cell << "</td>";
		}
		out << "</tr>\n";
	}

	out << "</table></body></html>";
	out.close();
	return true;
}