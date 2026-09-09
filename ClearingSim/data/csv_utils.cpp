#include "csv_utils.h"

#include <QFile>
#include <QTextStream>

// 拆分 CSV 行并清理字段
QStringList splitCsvLine(const QString &line)
{
    QStringList columns =line.split(',',Qt::KeepEmptyParts);//保留空字符
    for (QString &column : columns)
    {
        column =column.trimmed();//删除字符两侧空白
    }
    if (!columns.isEmpty() &&columns[0].startsWith(QChar(0xFEFF)))//非空&首列开头是否有UTF-8字符
    {
        columns[0].remove(0,1);
    }
    return columns;
}

// 判断首列是否为序号
bool isIndexHeader(const QString &text)
{
    const QString value =text.trimmed();
    return value.isEmpty() ||value.compare("index",Qt::CaseInsensitive) == 0;//是否为空/是否为序号（忽略大小写）
}

// 判断字段是否匹配指定选项
bool matchesAny(const QString &text,const QStringList &options)
{
    for (const QString &option : options)
    {
        if (text == option)
        {
            return true;
        }
    }
    return false;
}

// 读取 CSV 文件的表头和数据行
bool readCsvRows(const QString &filePath,QStringList &header,QVector<CsvRow> &rows,QStringList &errors)
{
    header.clear();
    rows.clear();
    QFile file(filePath);

    if (!file.exists())
    {
        errors.append("文件不存在：" +filePath);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly |QIODevice::Text))//只读/按文本处理
    {
        errors.append("文件无法打开：" +filePath);
        return false;
    }

    QTextStream in(&file);//文本读取器，从file读取

    if (in.atEnd())
    {
        errors.append("CSV 文件为空：" +filePath);
        return false;
    }

    header =splitCsvLine(in.readLine());

    if (header.isEmpty())
    {
        errors.append("CSV 表头为空：" +filePath);
        return false;
    }

    const bool hasIndex =isIndexHeader(header.first());//取表头

    if (hasIndex)
    {
        header.removeFirst();
    }

    int lineNumber =1;

    while (!in.atEnd())
    {
        ++lineNumber;
        const QString line =in.readLine().trimmed();

        if (line.isEmpty())
        {
            continue;
        }

        QStringList columns =splitCsvLine(line);

        if (hasIndex)
        {
            if (columns.isEmpty())
            {
                errors.append(QString("第 %1 行缺少 index").arg(lineNumber));
                continue;
            }
            columns.removeFirst();//删除首项
        }

        if (columns.size() !=header.size())
        {
            errors.append(QString("第 %1 行列数错误：应为 %2 列，实际为 %3 列").arg(lineNumber).arg(header.size()).arg(columns.size()));
            continue;
        }

        CsvRow row;
        row.lineNumber =lineNumber;
        row.columns =columns;
        rows.push_back(row);
    }

    if (rows.isEmpty())
    {
        if (errors.isEmpty())
        {
            errors.append("CSV 文件中没有有效数据：" +filePath);
        }
        return false;
    }

    return errors.isEmpty();
}

// 添加文件名称前缀到错误信息
void appendErrors(const QString &fileName,const QStringList &sourceErrors,QStringList &targetErrors)
{
    for (const QString &error : sourceErrors)
    {
        targetErrors.append("[" +fileName +"] " +error);
    }
}