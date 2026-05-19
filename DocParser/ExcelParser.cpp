#include "ExcelParser.h"
#include <windows.h>
#include <ole2.h>
#include <filesystem>
#include <iostream>
#include <functional>

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

namespace {
	HRESULT AutoWrap(int autoType, VARIANT* pvResult, IDispatch* pDisp, const wchar_t* ptName, int cArgs...) {
		va_list marker;
		va_start(marker, cArgs);
		if (!pDisp) return E_FAIL;

		DISPPARAMS dp = { NULL, NULL, 0, 0 };
		DISPID dispidNamed = DISPATCH_PROPERTYPUT;
		DISPID dispID;

		LPOLESTR nonConstName = const_cast<LPOLESTR>(ptName);
		HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, &nonConstName, 1, LOCALE_USER_DEFAULT, &dispID);
		if (FAILED(hr)) return hr;

		VARIANT* pArgs = new VARIANT[cArgs + 1];
		for (int i = 0; i < cArgs; i++) {
			pArgs[i] = va_arg(marker, VARIANT);
		}

		dp.cArgs = cArgs;
		dp.rgvarg = pArgs;

		if (autoType & DISPATCH_PROPERTYPUT) {
			dp.cNamedArgs = 1;
			dp.rgdispidNamedArgs = &dispidNamed;
		}

		hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, autoType, &dp, pvResult, NULL, NULL);
		va_end(marker);
		delete[] pArgs;
		return hr;
	}

	void ExtractSingleExcel(IDispatch* pSheet, std::vector<std::wstring>& outLines) {
		VARIANT resultUsedRange; VariantInit(&resultUsedRange);
		AutoWrap(DISPATCH_PROPERTYGET, &resultUsedRange, pSheet, L"UsedRange", 0);
		IDispatch* pUsedRange = resultUsedRange.pdispVal;

		if (pUsedRange) {
			VARIANT resultRows; VariantInit(&resultRows);
			AutoWrap(DISPATCH_PROPERTYGET, &resultRows, pUsedRange, L"Rows", 0);
			VARIANT resultRowCount; VariantInit(&resultRowCount);
			AutoWrap(DISPATCH_PROPERTYGET, &resultRowCount, resultRows.pdispVal, L"Count", 0);
			int rowCount = resultRowCount.lVal;
			SAFE_RELEASE(resultRows.pdispVal);

			VARIANT resultCols; VariantInit(&resultCols);
			AutoWrap(DISPATCH_PROPERTYGET, &resultCols, pUsedRange, L"Columns", 0);
			VARIANT resultColCount; VariantInit(&resultColCount);
			AutoWrap(DISPATCH_PROPERTYGET, &resultColCount, resultCols.pdispVal, L"Count", 0);
			int colCount = resultColCount.lVal;
			SAFE_RELEASE(resultCols.pdispVal);

			// 🚀 核心防爆装甲 1：物理硬截断！
			// 如果读到 100 万行的幽灵数据，直接按在 10000 行摩擦！列数也腰斩到 100！
			if (rowCount > 10000) rowCount = 10000;
			if (colCount > 100) colCount = 100;

			int emptyRowList = 0; // 🚀 记录连续空行的计数器

			for (int row = 1; row <= rowCount; ++row) {
				// 🚀 核心防爆装甲 2：软熔断打断！
				// 如果连续 20 行全是空单元格，说明绝对掉进了用户的“格式污染区”，直接一脚踹出大循环！
				if (emptyRowList > 20) {
					break;
				}

				bool rowHasData = false; // 标记这一行到底有没有真实数据

				for (int col = 1; col <= colCount; ++col) {
					VARIANT vRow, vCol;
					vRow.vt = VT_I4; vRow.lVal = row;
					vCol.vt = VT_I4; vCol.lVal = col;

					VARIANT resultCell; VariantInit(&resultCell);
					HRESULT hr = AutoWrap(DISPATCH_PROPERTYGET, &resultCell, pSheet, L"Cells", 2, vCol, vRow);

					if (SUCCEEDED(hr) && resultCell.pdispVal) {
						VARIANT cellText; VariantInit(&cellText);
						hr = AutoWrap(DISPATCH_PROPERTYGET, &cellText, resultCell.pdispVal, L"Text", 0);

						if (SUCCEEDED(hr) && cellText.vt == VT_BSTR && cellText.bstrVal != NULL) {
							std::wstring wText(cellText.bstrVal);
							wText.erase(0, wText.find_first_not_of(L" \t\r\n"));
							wText.erase(wText.find_last_not_of(L" \t\r\n") + 1);

							if (!wText.empty()) {
								outLines.push_back(wText);
								rowHasData = true; // 🚀 只要读到任何文本，标记本行为有效！
							}
						}
						VariantClear(&cellText);
						SAFE_RELEASE(resultCell.pdispVal);
					}
				}

				// 🚀 行结算：更新空行计数器
				if (!rowHasData) {
					emptyRowList++; // 这一行又是空的，累加！
				}
				else {
					emptyRowList = 0; // 撞到真数据了，警报解除，归零！
				}
			}
			SAFE_RELEASE(pUsedRange);
		}
	}
}

std::vector<ParsedDoc> ExcelParser::extractTextBatch(
	const std::vector<std::wstring>& filePaths,
	std::function<void(int, int, const std::wstring&)> progressCallback)
{
	std::vector<ParsedDoc> allFiles;
	if (filePaths.empty()) return allFiles;

	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr)) return allFiles;

	CLSID clsid;
	hr = CLSIDFromProgID(L"Excel.Application", &clsid);
	if (FAILED(hr)) { CoUninitialize(); return allFiles; }

	IDispatch* pExcelApp = NULL;
	hr = CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER, IID_IDispatch, (void**)&pExcelApp);
	if (FAILED(hr) || !pExcelApp) { CoUninitialize(); return allFiles; }

	VARIANT x; x.vt = VT_BOOL; x.boolVal = VARIANT_FALSE;
	AutoWrap(DISPATCH_PROPERTYPUT, NULL, pExcelApp, L"Visible", 1, x);

	VARIANT resultWorkbooks; VariantInit(&resultWorkbooks);
	AutoWrap(DISPATCH_PROPERTYGET, &resultWorkbooks, pExcelApp, L"Workbooks", 0);
	IDispatch* pWorkbooks = resultWorkbooks.pdispVal;

	int totalFiles = static_cast<int>(filePaths.size());
	int currentIdx = 0;

	for (const auto& filePath : filePaths) {
		std::filesystem::path fPath(filePath);
		std::wstring fileName = fPath.filename().wstring();

		if (progressCallback) progressCallback(currentIdx, totalFiles, fileName);

		VARIANT resultWorkbook; VariantInit(&resultWorkbook);
		VARIANT vPath; vPath.vt = VT_BSTR; vPath.bstrVal = SysAllocString(filePath.c_str());
		hr = AutoWrap(DISPATCH_METHOD, &resultWorkbook, pWorkbooks, L"Open", 1, vPath);
		SysFreeString(vPath.bstrVal);

		std::vector<std::wstring> currentFileLines;

		if (SUCCEEDED(hr) && resultWorkbook.pdispVal) {
			IDispatch* pWorkbook = resultWorkbook.pdispVal;
			VARIANT resultSheet; VariantInit(&resultSheet);
			AutoWrap(DISPATCH_PROPERTYGET, &resultSheet, pWorkbook, L"ActiveSheet", 0);
			IDispatch* pSheet = resultSheet.pdispVal;

			if (pSheet) {
				ExtractSingleExcel(pSheet, currentFileLines);
				SAFE_RELEASE(pSheet);
			}

			VARIANT vFalse; vFalse.vt = VT_BOOL; vFalse.boolVal = VARIANT_FALSE;
			AutoWrap(DISPATCH_METHOD, NULL, pWorkbook, L"Close", 1, vFalse);
			SAFE_RELEASE(pWorkbook);
		}

		// 🚀 结构化装车
		if (!currentFileLines.empty()) {
			ParsedDoc doc;
			doc.fileName = fileName;
			doc.lines = currentFileLines;
			allFiles.push_back(doc);
		}

		currentIdx++;
		if (progressCallback) progressCallback(currentIdx, totalFiles, fileName);
	}

	SAFE_RELEASE(pWorkbooks);
	AutoWrap(DISPATCH_METHOD, NULL, pExcelApp, L"Quit", 0);
	SAFE_RELEASE(pExcelApp);
	CoUninitialize();

	return allFiles;
}

std::vector<std::wstring> ExcelParser::extractText(const std::wstring& filePath) {
	auto batchResult = extractTextBatch({ filePath }, nullptr);
	if (!batchResult.empty()) return batchResult[0].lines; // 🚀 语义清晰
	return {};
}