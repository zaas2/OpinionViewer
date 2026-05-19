#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QStringConverter> // 引入编码转换模块
#include <iostream>
#include "../DocParser/DocxParser.h"
#include "../DocParser/OldDocParser.h"
#include "../DocParser/OpinionCleaner.h"

int main(int argc, char* argv[]) {
	QCoreApplication a(argc, argv);

	// ====================================================================
	// 【中文支持核心修复】
	// ====================================================================
	QTextStream out(stdout);
	QTextStream in(stdin);

	out.setEncoding(QStringConverter::System);
	in.setEncoding(QStringConverter::System);

	out << "====================================================\n";
	out << "    审查意见解析引擎 (DocParser) 测试终端         \n";
	out << "====================================================\n";

	// 1. 加载 INI 配置 (底层已变更为接收 std::wstring)
	QString iniPath = QCoreApplication::applicationDirPath() + QDir::separator() + "setting.ini";
	out << "[System] 正在加载配置文件...\n";
	out << "[System] 路径: " << iniPath << "\n";
	OpinionCleaner::loadConfig(iniPath.toStdWString()); // 👈 增加 toStdWString()
	out << "----------------------------------------------------\n";

	// 2. 进入交互式测试循环
	out << "\n请把你要测试的 .doc 或 .docx 文件拖拽到窗口中，或手动输入路径 (输入 'q' 退出)：\n> ";
	out.flush();

	QString filePath;
	while (true) {
		filePath = in.readLine().trimmed();

		if (filePath.isEmpty()) continue;
		if (filePath.toLower() == "q") break;

		// ====================================================================
		// 【路径净化处理】
		// ====================================================================
		if (filePath.startsWith("\"") && filePath.endsWith("\"")) {
			filePath = filePath.mid(1, filePath.length() - 2);
		}
		filePath.replace("\\", "/");

		QFileInfo fileInfo(filePath);
		if (!fileInfo.exists()) {
			out << "[Error] 文件不存在，请检查路径: " << filePath << "\n> ";
			out.flush();
			continue;
		}

		QString ext = fileInfo.suffix().toLower();

		// 👈 核心修改：接收结果的容器必须是标准库 vector，不再是 QStringList
		std::vector<std::wstring> rawLines;

		out << "\n>>> 开始解析文件: " << fileInfo.fileName() << " <<<\n";
		out << "[Step 1] 正在强行提取底层文本...\n";

		// 分流提取原始文本 (传入时需要 toStdWString())
		if (ext == "docx") {
			rawLines = DocxParser::extractText(filePath.toStdWString());
		}
		else if (ext == "doc") {
			rawLines = OldDocParser::extractText(filePath.toStdWString()); // 调用你之前写好的新版接口
		}
		else {
			out << "[Error] 暂不支持的文件格式: " << ext << "\n> ";
			out.flush();
			continue;
		}

		if (rawLines.empty()) { // 👈 换成 empty()
			out << "[Warning] 文件提取为空或遇到加密/损坏的文件！\n> ";
			out.flush();
			continue;
		}

		// 打印原始提取结果
		out << "\n==========【原始文本 (RAW)】 共 " << rawLines.size() << " 行 ==========\n";
		for (size_t i = 0; i < rawLines.size(); ++i) {
			// 👈 打印时需要从 std::wstring 桥接回 QString
			out << QString("[%1] ").arg(i + 1, 3, 10, QChar('0')) << QString::fromStdWString(rawLines[i]) << "\n";
		}
		out << "====================================================\n";

		// 清洗数据 (底层现在吃 vector，吐 vector)
		out << "\n[Step 2] 正在过正则黑名单与格式清洗...\n";
		std::vector<std::wstring> cleanLines = OpinionCleaner::clean(rawLines);

		// 打印净化后的结果
		out << "\n==========【净化意见 (CLEAN)】共 " << cleanLines.size() << " 行 ==========\n";
		for (size_t i = 0; i < cleanLines.size(); ++i) {
			out << QString("[%1] ").arg(i + 1, 3, 10, QChar('0')) << QString::fromStdWString(cleanLines[i]) << "\n";
		}
		out << "====================================================\n";

		out << "\n测试完成。垃圾过滤率: "
			<< QString::number((1.0 - (double)cleanLines.size() / rawLines.size()) * 100, 'f', 1) << "% \n";
		out << "\n请继续拖拽文件，或输入 'q' 退出：\n> ";
		out.flush();
	}

	return 0;
}