#pragma once
#include <vector>
#include <string>

// 向前声明 SQLite 的核心结构体
struct sqlite3;
struct sqlite3_stmt;

// 文件指纹元数据 (对应数据库表 FileFingerprints)
struct FileMetaData {
	std::wstring fileHash;  // 主键：文件的 MD5 指纹
	std::wstring filePath;  // 文件的绝对路径
	long long fileSize;     // 文件大小 (字节)
	long long lastModified; // 最后修改时间戳 (秒级或毫秒级)
};

// 意见数据记录 (对应数据库表 Opinions)
struct OpinionRecord {
	std::wstring content;   // 意见内容
	std::wstring reply;     // 意见回复 (配合你 UI 表格的第二列)
	std::wstring source;    // 来源文件名称或短路径 (用于 UI 极速展示)
	std::wstring fileHash;  // 核心枢纽：所属文件的 MD5 (外键)
};

class DbWorker {
public:
	DbWorker();
	~DbWorker();

	DbWorker(const DbWorker&) = delete;
	DbWorker& operator=(const DbWorker&) = delete;

	bool initDatabase(const std::wstring& dbPath = L"opinions.db");
	void closeDatabase();

	std::vector<OpinionRecord> searchOpinions(const std::wstring& keyword = L"");

	// 预检 1：判断文件大小和时间戳是否完全吻合 (极速放行)
	bool isFileUnchanged(const std::wstring& filePath, long long fileSize, long long lastModified);

	// 预检 2：核对 MD5 指纹是否存在 (处理文件被移动或改名的情况)
	bool isHashExists(const std::wstring& fileHash);

	// 补救动作：文件没变但路径变了，静默更新数据库里的路径
	bool updateFilePath(const std::wstring& fileHash, const std::wstring& newFilePath);

	// 入库动作 A：记录新文件的指纹信息
	bool insertFileMetaData(const FileMetaData& meta);

	// =======================================================

	bool batchInsertOpinions(const std::vector<OpinionRecord>& records);

	// 针对增量系统，可以增加一个按 Hash 删除的功能 (用于清理脏数据)
	bool deleteOpinionsByHash(const std::wstring& fileHash);
	bool deleteOpinionByContent(const std::wstring& content);
	bool batchDeleteOpinionsByContent(const std::vector<std::wstring>& contents);

	bool clearDatabase();

	int getTotalCount();

	bool beginTransaction();
	bool commitTransaction();
	void rollbackTransaction();
private:
	sqlite3* m_db = nullptr;

	bool executeInsertLoop(sqlite3_stmt* stmt, const std::vector<OpinionRecord>& records);
	std::wstring getCurrentDateTimeStr();
};