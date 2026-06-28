#include "DbWorker.h"
#include "sqlite3.h"
#include <windows.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "PublicUtils.h"

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
	::localtime_s(&tm_struct, &now_time);
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

	// 🚀 创表核心：双表结构 (第三范式)
	const char* createTableSql =
		"PRAGMA foreign_keys = ON;" // 开启外键约束

		// 表 1：文件指纹库
		"CREATE TABLE IF NOT EXISTS file_fingerprints ("
		"file_hash TEXT PRIMARY KEY,"
		"file_path TEXT,"
		"file_size INTEGER,"
		"last_modified INTEGER"
		");"

		// 表 2：意见明细库
		"CREATE TABLE IF NOT EXISTS review_opinions ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"content TEXT UNIQUE,"
		"reply TEXT,"
		"source TEXT,"
		"file_hash TEXT,"
		"import_time TEXT,"
		"FOREIGN KEY(file_hash) REFERENCES file_fingerprints(file_hash) ON DELETE CASCADE"
		");";

	char* errMsg = nullptr;
	rc = sqlite3_exec(m_db, createTableSql, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		if (errMsg) sqlite3_free(errMsg);
		return false;
	}
	return true;
}

// =======================================================
// 🚀 增量入库核心接口群实现
// =======================================================

bool DbWorker::isFileUnchanged(const std::wstring& filePath, long long fileSize, long long lastModified) {
	if (!m_db) return false;

	const char* sql = "SELECT 1 FROM file_fingerprints WHERE file_path = ? AND file_size = ? AND last_modified = ?;";
	sqlite3_stmt* stmt = nullptr;
	bool unchanged = false;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		std::string pathUtf8 = StrUtil::wstring_to_utf8(filePath);
		sqlite3_bind_text(stmt, 1, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, fileSize);
		sqlite3_bind_int64(stmt, 3, lastModified);

		if (sqlite3_step(stmt) == SQLITE_ROW) {
			unchanged = true; // 查到记录了，说明大小和时间完全没变！
		}
	}
	sqlite3_finalize(stmt);
	return unchanged;
}

bool DbWorker::isHashExists(const std::wstring& fileHash) {
	if (!m_db) return false;

	const char* sql = "SELECT 1 FROM file_fingerprints WHERE file_hash = ?;";
	sqlite3_stmt* stmt = nullptr;
	bool exists = false;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		std::string hashUtf8 = StrUtil::wstring_to_utf8(fileHash);
		sqlite3_bind_text(stmt, 1, hashUtf8.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW) exists = true;
	}
	sqlite3_finalize(stmt);
	return exists;
}

bool DbWorker::updateFilePath(const std::wstring& fileHash, const std::wstring& newFilePath) {
	if (!m_db) return false;

	const char* sql = "UPDATE file_fingerprints SET file_path = ? WHERE file_hash = ?;";
	sqlite3_stmt* stmt = nullptr;
	bool success = false;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		std::string pathUtf8 = StrUtil::wstring_to_utf8(newFilePath);
		std::string hashUtf8 = StrUtil::wstring_to_utf8(fileHash);

		sqlite3_bind_text(stmt, 1, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, hashUtf8.c_str(), -1, SQLITE_TRANSIENT);

		if (sqlite3_step(stmt) == SQLITE_DONE) success = true;
	}
	sqlite3_finalize(stmt);
	return success;
}

bool DbWorker::insertFileMetaData(const FileMetaData& meta) {
	if (!m_db) return false;

	// 使用 REPLACE，如果 Hash 存在则覆盖更新时间戳等信息
	const char* sql = "INSERT OR REPLACE INTO file_fingerprints (file_hash, file_path, file_size, last_modified) VALUES (?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;
	bool success = false;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		std::string hashUtf8 = StrUtil::wstring_to_utf8(meta.fileHash);
		std::string pathUtf8 = StrUtil::wstring_to_utf8(meta.filePath);

		sqlite3_bind_text(stmt, 1, hashUtf8.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, meta.fileSize);
		sqlite3_bind_int64(stmt, 4, meta.lastModified);

		if (sqlite3_step(stmt) == SQLITE_DONE) success = true;
	}
	sqlite3_finalize(stmt);
	return success;
}

bool DbWorker::deleteOpinionsByHash(const std::wstring& fileHash)
{
	if (!m_db) return false;
	const char* sql = "DELETE FROM file_fingerprints WHERE file_hash = ?;";
	sqlite3_stmt* stmt = nullptr;
	bool success = false;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		std::string hashUtf8 = StrUtil::wstring_to_utf8(fileHash);
		sqlite3_bind_text(stmt, 1, hashUtf8.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_DONE) success = true;
	}
	sqlite3_finalize(stmt);
	return success;
}

bool DbWorker::deleteOpinionByContent(const std::wstring& content)
{
	if (!m_db) return false;

	const char* sql = "DELETE FROM review_opinions WHERE content = ?;";
	sqlite3_stmt* stmt = nullptr;
	bool success = false;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		std::string contentUtf8 = StrUtil::wstring_to_utf8(content);

		sqlite3_bind_text(stmt, 1, contentUtf8.c_str(), -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_DONE) {
			success = true;
		}
	}

	sqlite3_finalize(stmt);
	return success;
}

// 🚀 极速批量删除引擎：事务保护 + 预编译语句复用
bool DbWorker::batchDeleteOpinionsByContent(const std::vector<std::wstring>& contents)
{
	if (!m_db || contents.empty()) return false;

	// 1. 只编译一次 SQL 语句，拒绝重复造轮子
	const char* sql = "DELETE FROM review_opinions WHERE content = ?;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	// 2. 开启显式事务 (死死锁住数据库，全部写进内存缓存)
	beginTransaction();

	// 3. 疯狂复用 stmt 执行删除
	for (const auto& content : contents) {
		std::string contentUtf8 = StrUtil::wstring_to_utf8(content);
		sqlite3_bind_text(stmt, 1, contentUtf8.c_str(), -1, SQLITE_TRANSIENT);

		sqlite3_step(stmt);

		// 极其关键：重置 stmt 状态以便下一次 bind
		sqlite3_reset(stmt);
	}

	// 4. 一次性将内存里的变更刷入物理磁盘！
	bool success = commitTransaction();

	sqlite3_finalize(stmt);
	return success;
}

std::vector<OpinionRecord> DbWorker::searchOpinions(const std::wstring& keyword) {
	std::vector<OpinionRecord> results;
	if (!m_db) return results;

	sqlite3_stmt* stmt = nullptr;
	int rc = 0;

	std::vector<std::wstring> keywords;
	std::wstringstream wss(keyword);
	std::wstring token;
	while (wss >> token) {
		if (!token.empty()) keywords.push_back(token);
	}

	// 字段扩充：把 reply 和 file_hash 也查出来
	if (keywords.empty()) {
		const char* querySql = "SELECT content, reply, source, file_hash FROM review_opinions ORDER BY id DESC;";
		rc = sqlite3_prepare_v2(m_db, querySql, -1, &stmt, nullptr);
	}
	else {
		std::string querySql = "SELECT content, reply, source, file_hash FROM review_opinions WHERE ";
		for (size_t i = 0; i < keywords.size(); ++i) {
			if (i > 0) querySql += " AND ";
			// 🚀 核心纠偏 1：砍掉 OR source LIKE ?，确保只在审查意见原文中检索
			querySql += "content LIKE ?";
		}
		querySql += " ORDER BY id DESC;";

		rc = sqlite3_prepare_v2(m_db, querySql.c_str(), -1, &stmt, nullptr);

		if (rc == SQLITE_OK) {
			int bindIndex = 1;
			for (const auto& kw : keywords) {
				std::string bindKey = "%" + StrUtil::wstring_to_utf8(kw) + "%";
				// 🚀 核心纠偏 2：因为 SQL 语句里只有一个问号了，所以每个关键词只绑定 1 次
				sqlite3_bind_text(stmt, bindIndex++, bindKey.c_str(), -1, SQLITE_TRANSIENT);
			}
		}
	}

	if (rc != SQLITE_OK) return results;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* c_content = (const char*)sqlite3_column_text(stmt, 0);
		const char* c_reply = (const char*)sqlite3_column_text(stmt, 1);
		const char* c_source = (const char*)sqlite3_column_text(stmt, 2);
		const char* c_hash = (const char*)sqlite3_column_text(stmt, 3);

		OpinionRecord record;
		record.content = StrUtil::utf8_to_wstring(c_content ? c_content : "");
		record.reply = StrUtil::utf8_to_wstring(c_reply ? c_reply : "");
		record.source = StrUtil::utf8_to_wstring(c_source ? c_source : "");
		record.fileHash = StrUtil::utf8_to_wstring(c_hash ? c_hash : "");

		results.push_back(record);
	}

	sqlite3_finalize(stmt);
	return results;
}

bool DbWorker::executeInsertLoop(sqlite3_stmt* stmt, const std::vector<OpinionRecord>& records) {
	std::string timeUtf8 = StrUtil::wstring_to_utf8(getCurrentDateTimeStr());

	for (const auto& record : records) {
		sqlite3_reset(stmt);

		std::string utf8Content = StrUtil::wstring_to_utf8(record.content);
		std::string utf8Reply = StrUtil::wstring_to_utf8(record.reply);
		std::string utf8Source = StrUtil::wstring_to_utf8(record.source);
		std::string utf8Hash = StrUtil::wstring_to_utf8(record.fileHash);

		// 对应绑定的 5 个占位符
		sqlite3_bind_text(stmt, 1, utf8Content.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, utf8Reply.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, utf8Source.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, utf8Hash.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, timeUtf8.c_str(), -1, SQLITE_TRANSIENT);

		if (sqlite3_step(stmt) != SQLITE_DONE) return false;
	}
	return true;
}

bool DbWorker::batchInsertOpinions(const std::vector<OpinionRecord>& records) {
	if (!m_db) return false;

	// 扩充字段：加上 reply 和 file_hash
	const char* insertSql = "INSERT OR IGNORE INTO review_opinions (content, reply, source, file_hash, import_time) VALUES (?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt = nullptr;

	if (sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false; // 🚀 报错直接返回，让外层的 OpinionViewer 去统一 ROLLBACK
	}

	bool success = executeInsertLoop(stmt, records);
	sqlite3_finalize(stmt);

	return success; // 🚀 成功也直接返回，让外层的 OpinionViewer 去统一 COMMIT
}

bool DbWorker::clearDatabase() {
	if (!m_db) return false;

	// 1. 先删小弟（子表：意见明细）
	if (sqlite3_exec(m_db, "DELETE FROM review_opinions;", nullptr, nullptr, nullptr) != SQLITE_OK) {
		return false;
	}

	// 2. 重置子表的自增 ID，让下一次入库的 ID 重新从 1 开始
	sqlite3_exec(m_db, "DELETE FROM sqlite_sequence WHERE name='review_opinions';", nullptr, nullptr, nullptr);

	// 3. 再杀大哥（主表：文件指纹）。小弟都没了，外键约束自然就不会报警了！
	if (sqlite3_exec(m_db, "DELETE FROM file_fingerprints;", nullptr, nullptr, nullptr) != SQLITE_OK) {
		return false;
	}

	return true;
}

int DbWorker::getTotalCount() {
	if (!m_db) return 0;

	const char* sql = "SELECT COUNT(*) FROM review_opinions;";
	sqlite3_stmt* stmt = nullptr;
	int count = 0;

	if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return count;
}

bool DbWorker::beginTransaction() {
	if (!m_db) return false;
	return sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool DbWorker::commitTransaction() {
	if (!m_db) return false;
	return sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

void DbWorker::rollbackTransaction() {
	if (!m_db) return;
	sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
}