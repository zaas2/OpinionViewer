#include "DbWorker.h"
#include <windows.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "PublicUtils.h"
#include <vector>

DbWorker::DbWorker() : m_db(nullptr) {}

DbWorker::~DbWorker() {
	closeDatabase();
}

void DbWorker::closeDatabase() {
	if (m_db) {
		sqlite3_close(m_db);
		m_db = nullptr;
	}
}

std::wstring DbWorker::getCurrentDateTimeStr() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	std::tm tm_struct;
	::localtime_s(&tm_struct, &now_time); // MSVC 安全版本的本地时间函数
	std::wstringstream wss;
	wss << std::put_time(&tm_struct, L"%Y-%m-%d %H:%M:%S");
	return wss.str();
}

bool DbWorker::initDatabase(const std::wstring& dbPath) {
	closeDatabase();

	int rc = sqlite3_open16(dbPath.c_str(), &m_db);
	if (rc != SQLITE_OK) {
		::MessageBoxW(NULL, L"SQLite 宽字符句柄创建失败！", L"底层断裂", MB_OK | MB_ICONERROR);
		return false;
	}

	// 创表核心
	const char* createTableSql =
		"CREATE TABLE IF NOT EXISTS review_opinions ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"content TEXT UNIQUE,"
		"source TEXT,"
		"import_time TEXT"
		");";

	char* errMsg = nullptr;
	rc = sqlite3_exec(m_db, createTableSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		if (errMsg) {
			sqlite3_free(errMsg);
		}
		return false;
	}

	return true;
}

std::vector<OpinionRecord> DbWorker::searchOpinions(const std::wstring& keyword) {
	std::vector<OpinionRecord> results;
	if (!m_db) return results;

	sqlite3_stmt* stmt = nullptr;
	int rc = 0;

	// 1. 按空格将输入的 keyword 切分成多个关键词
	std::vector<std::wstring> keywords;
	std::wstringstream wss(keyword);
	std::wstring token;
	while (wss >> token) {
		if (!token.empty()) {
			keywords.push_back(token);
		}
	}

	if (keywords.empty()) {
		// 如果没有输入，或者输入的全是空格，执行全量查询
		const char* querySql = "SELECT content, source FROM review_opinions ORDER BY id DESC;";
		rc = sqlite3_prepare_v2(m_db, querySql, -1, &stmt, nullptr);
	}
	else {
		// 2. 动态构建 SQL 语句，几个关键词就拼几个 AND (content LIKE ? OR source LIKE ?)
		std::string querySql = "SELECT content, source FROM review_opinions WHERE ";
		for (size_t i = 0; i < keywords.size(); ++i) {
			if (i > 0) querySql += " AND ";
			querySql += "(content LIKE ? OR source LIKE ?)";
		}
		querySql += " ORDER BY id DESC;";

		rc = sqlite3_prepare_v2(m_db, querySql.c_str(), -1, &stmt, nullptr);

		if (rc == SQLITE_OK) {
			// 3. 循环绑定参数，每个关键词绑定两次 (给 content 和 source 各绑一次)
			int bindIndex = 1;
			for (const auto& kw : keywords) {
				std::string bindKey = "%" + StrUtil::wstring_to_utf8(kw) + "%";

				// SQLITE_TRANSIENT 告诉 SQLite 自己拷贝一份字符串，这样 bindKey 出作用域销毁了也没事
				sqlite3_bind_text(stmt, bindIndex++, bindKey.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, bindIndex++, bindKey.c_str(), -1, SQLITE_TRANSIENT);
			}
		}
	}

	if (rc != SQLITE_OK) {
		std::cerr << "SQL 查询执行失败: " << sqlite3_errmsg(m_db) << std::endl;
		return results;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* contentText = (const char*)sqlite3_column_text(stmt, 0);
		const char* sourceText = (const char*)sqlite3_column_text(stmt, 1);

		OpinionRecord record;
		record.content = StrUtil::utf8_to_wstring(contentText ? contentText : "");
		record.source = StrUtil::utf8_to_wstring(sourceText ? sourceText : "");

		results.push_back(record);
	}

	sqlite3_finalize(stmt);
	return results;
}

#include <Windows.h> // 如果用了 OutputDebugString
// 或者直接引入 #include <QMessageBox>

bool DbWorker::executeInsertLoop(sqlite3_stmt* stmt, const std::vector<OpinionRecord>& records) {
	std::string timeUtf8 = StrUtil::wstring_to_utf8(getCurrentDateTimeStr());

	for (const auto& record : records) {
		sqlite3_reset(stmt);

		std::string utf8Content = StrUtil::wstring_to_utf8(record.content);
		std::string utf8Source = StrUtil::wstring_to_utf8(record.source);

		sqlite3_bind_text(stmt, 1, utf8Content.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, utf8Source.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, timeUtf8.c_str(), -1, SQLITE_TRANSIENT);

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			//std::string errMsg = sqlite3_errmsg(m_db);
 			//::MessageBoxA(NULL, errMsg.c_str(), "SQLite 底层真凶", MB_OK | MB_ICONERROR);
			return false;
		}
	}
	return true;
}

bool DbWorker::batchInsertOpinions(const std::vector<OpinionRecord>& records) {
	if (!m_db) return false;

	sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	// 增量录入专属：带有 OR IGNORE 的防御装甲
	const char* insertSql = "INSERT OR IGNORE INTO review_opinions (content, source, import_time) VALUES (?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	bool success = executeInsertLoop(stmt, records);

	sqlite3_finalize(stmt);

	if (!success) {
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
	return true;
}

bool DbWorker::overwriteOpinions(const std::vector<OpinionRecord>& records) {
	if (!m_db) return false;

	sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

	if (sqlite3_exec(m_db, "DELETE FROM review_opinions;", nullptr, nullptr, nullptr) != SQLITE_OK) {
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}
	sqlite3_exec(m_db, "DELETE FROM sqlite_sequence WHERE name='review_opinions';", nullptr, nullptr, nullptr);

	// 覆写专属：空表直接暴力 INSERT，不需要 IGNORE 判定，性能拉满
	//const char* insertSql = "INSERT INTO review_opinions (content, source, import_time) VALUES (?, ?, ?);";
	const char* insertSql = "INSERT OR IGNORE INTO review_opinions (content, source, import_time) VALUES (?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	// 🚀 同样呼叫发射引擎！
	bool success = executeInsertLoop(stmt, records);

	sqlite3_finalize(stmt);

	if (!success) {
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
	return true;
}

int DbWorker::getTotalCount() {
	if (!m_db) return 0;

	const char* sql = "SELECT COUNT(*) FROM review_opinions;";
	sqlite3_stmt* stmt = nullptr;
	int count = 0;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			count = sqlite3_column_int(stmt, 0); // 取出第一列的统计数字
		}
	}
	sqlite3_finalize(stmt);
	return count;
}