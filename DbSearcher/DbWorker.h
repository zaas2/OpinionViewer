#pragma once
#include <vector>
#include <string>
#include "sqlite3.h"

// 向前声明 SQLite 的核心结构体，避免在头文件中暴露第三方库指针
struct sqlite3;

// 🚀 奥卡姆剃刀：定义直白的结构体，彻底埋葬 std::pair
struct OpinionRecord {
	std::wstring content;  // 意见内容
	std::wstring source;   // 来源文件
};

class DbWorker {
public:
	DbWorker();
	~DbWorker();

	// 强行禁止拷贝构造和赋值，防止数据库句柄被无意间复制导致崩溃
	DbWorker(const DbWorker&) = delete;
	DbWorker& operator=(const DbWorker&) = delete;

	// 初始化数据库：连接/创建本地文件，并自动建立焊死 UNIQUE 约束的数据表
	bool initDatabase(const std::wstring& dbPath = L"opinions.db");

	// 显式关闭数据库连接
	void closeDatabase();

	std::vector<OpinionRecord> searchOpinions(const std::wstring& keyword = L"");

	bool batchInsertOpinions(const std::vector<OpinionRecord>& records);

	bool overwriteOpinions(const std::vector<OpinionRecord>& records);

	int getTotalCount();

private:
	sqlite3* m_db = nullptr;
	bool executeInsertLoop(sqlite3_stmt* stmt, const std::vector<OpinionRecord>& records);
	std::wstring getCurrentDateTimeStr();
};