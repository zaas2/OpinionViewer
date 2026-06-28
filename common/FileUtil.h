#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QCryptographicHash>

class FileUtil
{
public:
    FileUtil() = delete;

    /**
     * @brief 极速分块计算文件 MD5 指纹
     * @param filePath 文件绝对路径
     * @return 成功返回 32 位小写十六进制 MD5 串，失败返回空 QString()
     */
    static inline QString calculateFileHash(const QString& filePath)
    {
        QFile file(filePath);        
        if (!file.open(QIODevice::ReadOnly))
        {
            return QString();
        }

        QCryptographicHash hash(QCryptographicHash::Md5);
        
        //核心流水线缓冲：8192 字节（8KB）
        char buffer[8192];
        qint64 bytesRead = 0;

        // 流式压榨磁盘 IO，一边读，一边将数据流塞进哈希引擎
        while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0)
        {
            hash.addData(QByteArrayView(buffer, bytesRead));
        }

        file.close();
        return QString(hash.result().toHex());
    }

    /**
     * @brief 获取文件大小（单位：字节）
     * @return 存在则返回实际大小，不存在返回 -1
     */
    static inline qint64 getFileSize(const QString& filePath)
    {
        QFileInfo info(filePath);
        return info.exists() ? info.size() : -1;
    }

    /**
     * @brief 获取文件最后修改时间
     * @return 存在则返回 QDateTime 实例，不存在返回空的 QDateTime()
     */
    static inline QDateTime getLastModifiedTime(const QString& filePath)
    {
        QFileInfo info(filePath);
        return info.exists() ? info.lastModified() : QDateTime();
    }
};

#endif // FILEUTIL_H