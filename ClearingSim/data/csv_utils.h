#ifndef CSV_UTILS_H
#define CSV_UTILS_H

#include <QString>
#include <QStringList>
#include <QVector>

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

#endif // CSV_UTILS_H