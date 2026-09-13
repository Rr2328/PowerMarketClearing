#ifndef CSV_UTILS_H
#define CSV_UTILS_H

#include <QString>
#include <QStringList>
#include <QVector>

// CSV 解析公共工具（数据模块：陈美伊）
//   独立 namespace 避免与主线 data_reader.cpp 内部同名 helper 产生
//   匿名命名空间注入歧义；文件尾部 using 声明保持全局直接可用。
namespace teammate
{

// 保存一行 CSV 数据及其行号
struct CsvRow
{
    int lineNumber = 0;
    QStringList columns;
};

QStringList splitCsvLine(const QString &line);
bool isIndexHeader(const QString &text);
bool matchesAny(const QString &text,const QStringList &options);
bool readCsvRows(const QString &filePath,QStringList &header,QVector<CsvRow> &rows,QStringList &errors);
void appendErrors(const QString &fileName,const QStringList &sourceErrors,QStringList &targetErrors);

} // namespace teammate

#endif // CSV_UTILS_H
