#include "data_reader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
// DataReader V1.4 单元测试：校验真实数据读取、各类异常识别与跨文件关系校验。
namespace
{
<<<<<<< HEAD
// 自可执行文件目录上溯查找项目根目录。
QString findProjectRoot()
{QDir dir(QCoreApplication::applicationDirPath());
    while (true)
    {if (QFileInfo::exists(dir.filePath("CMakeLists.txt")) &&QFileInfo::exists(dir.filePath("data/data_reader.cpp")))
=======

int failedTests = 0;

// 测试结果输出
void check(bool condition,const QString &testName)
{if (condition)
    {qInfo().noquote()<< "[PASS]" << testName;
    }
    else
    {qCritical().noquote()<< "[FAIL]" << testName;
        ++failedTests;
    }
}

// 查找项目根目录
QString searchRepoRoot(const QString &startPath)
{QDir dir(startPath);
    while (true)
    {if (dir.exists("ClearingSim") && dir.exists("data/samples"))
>>>>>>> 197592d (刚刚那一版没有Highs，重新改了一版)
        {return dir.absolutePath();
        }
        if (!dir.cdUp())
        {break;
        }
    }
    return QString();
}
<<<<<<< HEAD
// 由项目根目录再上溯一层得到仓库根目录。
QString findRepositoryRoot(const QString &projectRoot)
{if (projectRoot.isEmpty())
    {return QString();
    }
    QDir dir(projectRoot);
    if (!dir.cdUp())
    {return QString();
    }
    return dir.absolutePath();
}
// 拼接 demo 场景数据目录路径。
QString scenarioDirectory(const QString &repositoryRoot)
{return QDir(repositoryRoot).filePath("data/samples/scenario");
}
// 真实场景数据文件名清单（4 类 × 24/96 时段）。
QStringList scenarioFileNames()
{return {"generator_bids_24period.csv","consumer_bids_24period.csv","load_curve_24period.csv","renewable_output_24period.csv","generator_bids_96period.csv","consumer_bids_96period.csv","load_curve_96period.csv","renewable_output_96period.csv"};
}
// 统计目录中存在的场景文件数。
int countScenarioFiles(const QString &directory)
{int count = 0;
    for (const QString &fileName : scenarioFileNames())
    {if (QFileInfo::exists(QDir(directory).filePath(fileName)))
        {++count;
        }
    }
    return count;
}
// 按时段数生成一组四文件路径。
DataFileSet makeFileSet(const QString &directory,int periodCount)
{const QString suffix =QString::number(periodCount) +"period";
    DataFileSet files;
    files.generatorBidsFile =QDir(directory).filePath("generator_bids_" +suffix +".csv");
    files.consumerBidsFile =QDir(directory).filePath("consumer_bids_" +suffix +".csv");
    files.loadCurveFile =QDir(directory).filePath("load_curve_" +suffix +".csv");
    files.renewableOutputFile =QDir(directory).filePath("renewable_output_" +suffix +".csv");
    return files;
}
// 以 UTF-8 写入文本内容到文件。
bool writeTextFile(const QString &filePath,const QString &content)
{QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly |QIODevice::Text))
    {return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    return true;
}
// 按步长换算时段对应时刻。
QString makeTime(int period,int periodCount)
{int stepMinutes = 60;
    if (periodCount == 96)
    {stepMinutes = 15;
=======

// 自动定位项目根目录
QString findRepoRoot()
{QString root;
    root = searchRepoRoot(QCoreApplication::applicationDirPath());
    if (!root.isEmpty())
    {return root;
    }
    root = searchRepoRoot(QDir::currentPath());
    if (!root.isEmpty())
    {return root;
    }
    const QFileInfo sourceFile(QString::fromUtf8(__FILE__));
    return searchRepoRoot(sourceFile.absolutePath());
}

// 创建临时 CSV 文件
bool writeTextFile(const QString &filePath,const QString &content)
{QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {return false;
    }
    QTextStream out(&file);
    out << content;
    return true;
}

// 输出错误信息
void printErrors(const QStringList &errors)
{for (const QString &error : errors)
    {qInfo().noquote()<< "   " << error;
>>>>>>> 197592d (刚刚那一版没有Highs，重新改了一版)
    }
    const int totalMinutes =(period - 1) *stepMinutes;
    const int hour =totalMinutes / 60;
    const int minute =totalMinutes % 60;
    return QString("%1:%2").arg(hour,2,10,QChar('0')).arg(minute,2,10,QChar('0'));
}
<<<<<<< HEAD
// 发电侧宽表表头。
QStringList generatorHeader()
{QStringList header;
    header << "period" << "电厂名称" << "机组编号" << "机组类型";
    for (int segment = 1;segment <= 10;++segment)
    {header << QString("出力P%1(MW)").arg(segment) << QString("报价C%1(元/MWh)").arg(segment);
    }
    return header;
}
// 用户侧宽表表头。
QStringList consumerHeader()
{QStringList header;
    header << "period" << "用户名称" << "用户编号";
    for (int segment = 1;segment <= 10;++segment)
    {header << QString("购电量P%1(MWh)").arg(segment) << QString("报价C%1(元/MWh)").arg(segment);
    }
    return header;
}
// 系统负荷表表头。
QStringList loadHeader()
{return {"period","时刻","系统负荷(MW)"};
}
// 新能源出力表表头。
QStringList renewableHeader()
{return {"period","时刻","机组编号","机组名称","机组类型","可用出力(MW)"};
}
// 生成一行发电侧申报记录。
QString makeGeneratorRow(int period,const QString &id,const QString &type,const QVector<QPair<QString, QString>> &segments,const QString &name ="测试电厂")
{QStringList columns;
    columns << QString::number(period) << name << id << type;
    for (int i = 0;i < 10;++i)
    {if (i < segments.size())
        {columns << segments[i].first << segments[i].second;
        }
        else
        {columns << "" << "";
        }
    }
    return columns.join(',');
}
// 生成一行用户侧申报记录。
QString makeConsumerRow(int period,const QString &id,const QVector<QPair<QString, QString>> &segments,const QString &name ="测试用户")
{QStringList columns;
    columns << QString::number(period) << name << id;
    for (int i = 0;i < 10;++i)
    {if (i < segments.size())
        {columns << segments[i].first << segments[i].second;
        }
        else
        {columns << "" << "";
        }
    }
    return columns.join(',');
}
// 生成一行系统负荷记录。
QString makeLoadRow(int period,int periodCount,const QString &load)
{QStringList columns;
    columns << QString::number(period) << makeTime(period,periodCount) << load;
    return columns.join(',');
}
// 生成一行新能源出力记录。
QString makeRenewableRow(int period,int periodCount,const QString &id,const QString &type,const QString &output,const QString &name ="测试新能源机组")
{QStringList columns;
    columns << QString::number(period) << makeTime(period,periodCount) << id << name << type << output;
    return columns.join(',');
}
// 表头与数据行拼装为 CSV 文本。
QString makeCsv(const QStringList &header,const QStringList &rows)
{QString content =header.join(',') +'\n';
    for (const QString &row : rows)
    {content +=row +'\n';
    }
    return content;
}
// 构造发电侧 CSV（光伏模式前 6 时段出力为 0）。
QString makeGeneratorCsv(int periodCount,const QString &type ="火电")
{QStringList rows;
    for (int period = 1;period <= periodCount;++period)
    {rows.push_back(makeGeneratorRow(period,"G1",type,{{type == "光伏" && period <= 6 ? "0" : "50.0",type == "光伏" ? "0.000" : "150.000"},{type == "光伏" ? "" : "30.0",type == "光伏" ? "" : "200.000"}}));
    }
    return makeCsv(generatorHeader(),rows);
}
// 构造用户侧 CSV（两段递减报价）。
QString makeConsumerCsv(int periodCount)
{QStringList rows;
    for (int period = 1;period <= periodCount;++period)
    {rows.push_back(makeConsumerRow(period,"U1",{{"50.0","400.000"},{"30.0","350.000"}}));
    }
    return makeCsv(consumerHeader(),rows);
}
// 判断错误列表是否含指定关键字。
bool containsError(const QStringList &errors,const QString &keyword)
{for (const QString &error : errors)
    {if (error.contains(keyword))
        {return true;
        }
    }
    return false;
}
// 输出错误明细。
void printErrors(const QStringList &errors)
{for (const QString &error : errors)
    {qInfo().noquote()<< "   "<< error;
    }
}
// 断言并统计失败数。
void check(bool condition,const QString &testName,int &failedTests,const QStringList &errors =QStringList())
{if (condition)
    {qInfo().noquote()<< "[PASS]"<< testName;
    }
    else
    {++failedTests;
        qInfo().noquote()<< "[FAIL]"<< testName;
        printErrors(errors);
    }
}
// 判断是否包含指定时段的发电申报。
bool containsPeriod(const QVector<GeneratorBid> &data,int period)
{for (const GeneratorBid &bid : data)
    {if (bid.period ==period)
        {return true;
        }
    }
    return false;
}
// 统计指定时段机组的报价段数。
int countSegments(const QVector<GeneratorBid> &data,int period,const QString &id)
{int count = 0;
    for (const GeneratorBid &bid : data)
    {if (bid.period ==period &&bid.id ==id)
        {++count;
        }
    }
    return count;
}
// 构造一份 24 时段合法市场数据。
MarketData makeValidMarketData24()
{MarketData data;
    for (int period = 1;period <= 24;++period)
    {GeneratorBid thermal;
        thermal.id ="G1";
        thermal.name ="测试火电机组";
        thermal.type ="火电";
        thermal.segment =1;
        thermal.price =150.0;
        thermal.quantity =100.0;
        thermal.period =period;
        data.generatorBids.push_back(thermal);
        GeneratorBid wind;
        wind.id ="W1";
        wind.name ="测试风电机组";
        wind.type ="风电";
        wind.segment =1;
        wind.price =0.0;
        wind.quantity =50.0;
        wind.period =period;
        data.generatorBids.push_back(wind);
        ConsumerBid consumer;
        consumer.id ="U1";
        consumer.name ="测试用户";
        consumer.segment =1;
        consumer.price =400.0;
        consumer.quantity =100.0;
        consumer.period =period;
        data.consumerBids.push_back(consumer);
        LoadPoint load;
        load.period =period;
        load.time =makeTime(period,24);
        load.load =500.0;
        data.loadCurve.push_back(load);
        RenewableOutput renewable;
        renewable.generatorId ="W1";
        renewable.generatorType ="风电";
        renewable.period =period;
        renewable.output =50.0;
        data.renewableOutputs.push_back(renewable);
=======

// 生成时刻
QString timeText(int period)
{const int totalMinutes = period * 15;
    if (totalMinutes == 1440)
    {return "24:00";
    }
    return QString("%1:%2").arg(totalMinutes / 60,2,10,QChar('0')).arg(totalMinutes % 60,2,10,QChar('0'));
}

// 生成负荷测试文件
QString buildLoadCsv(int count)
{QString content = "时段,时刻,负荷(MW)\n";
    for (int period = 1; period <= count; ++period)
    {content += QString("%1,%2,700.0\n").arg(period).arg(timeText(period));
    }
    return content;
}

} // namespace

int main(int argc,char *argv[])
{QCoreApplication app(argc,argv);
    qInfo().noquote()<< "========== DataReader V1.3 Test ==========";
    const QString repoRoot = findRepoRoot();
    check(!repoRoot.isEmpty(),"定位项目根目录");
    if (repoRoot.isEmpty())
    {return 1;
    }
    qInfo().noquote()<< "Repo root:" << repoRoot;

    // 真实样例数据
    DataFileSet files;
    files.generatorBidsFile = repoRoot + "/data/samples/benchmark/generator_bids.csv";
    files.consumerBidsFile = repoRoot + "/data/samples/benchmark/consumer_bids.csv";
    files.loadCurveFile = repoRoot + "/data/samples/curves/load_curve.csv";
    MarketData marketData;
    QStringList errors;
    bool     ok = DataReader::readAll(files,marketData,errors);
    check(ok,"窄表基准例读取（自动展开 96 期）");
    if (!ok)
    {printErrors(errors);
    }
    else
    {qInfo().noquote()<< "Generator bids:" << marketData.generatorBids.size();
        qInfo().noquote()<< "Consumer bids:" << marketData.consumerBids.size();
        qInfo().noquote()<< "Load points:" << marketData.loadCurve.size();

        // 窄表展开：申报条数 = 原行数 × 96，且各时段同量同价
        const int genRows = marketData.generatorBids.size() / 96;
        const int conRows = marketData.consumerBids.size() / 96;
        check(genRows > 0 && marketData.generatorBids.size() == genRows * 96,"发电窄表展开为 96 期（行数 × 96）");
        check(conRows > 0 && marketData.consumerBids.size() == conRows * 96,"购电窄表展开为 96 期（行数 × 96）");
        bool expandConsistent = true;

        // 展开顺序：每个申报行连续展开 96 期（bids[i] 的 period = i%96+1，
        // 基准行 = 同组首行）
        for (int i = 0; i < marketData.generatorBids.size(); ++i)
        {const GeneratorBid &base = marketData.generatorBids[i - i % 96];
            const GeneratorBid &item = marketData.generatorBids[i];
            if (item.period != i % 96 + 1 || item.id != base.id || item.price != base.price || item.quantity != base.quantity)
            {expandConsistent = false;
                break;
            }
        }
        check(expandConsistent,"窄表展开各时段同量同价（对拍锚点等价性）");
    }

    // V1.3：跨文件校验不再阻断（新能源不进申报表；平衡偏差改 P1 提示）
    errors.clear();
    QStringList hints;
    ok = DataReader::validateRelations(marketData,errors,hints);
    check(ok,"跨文件校验通过（V1.3：无阻断项）");
    if (!ok)
    {printErrors(errors);
    }

    // 平衡提示真实落地检查（契约 §3.2-3）：合成数据三态验证——
    //   负荷 100/200/300/400，申报 100/200/300/300 → 时段4 缺口 100 MW
    {MarketData syn;
        const double loads[] = { 100.0, 200.0, 300.0, 400.0 };
        for (int t = 1; t <= 4; ++t)
        {LoadPoint lp;
            lp.period = t;
            lp.load = loads[t - 1];
            syn.loadCurve.append(lp);
        }
        for (int t = 1; t <= 4; ++t)
        {ConsumerBid cb;
            cb.name = "用户A";
            cb.id = "L1";
            cb.period = t;
            cb.quantity = (t == 4) ? 300.0 : loads[t - 1];   // 时段4 少报 100
            syn.consumerBids.append(cb);
        }
        QStringList synHints;
        const bool synOk = DataReader::validateRelations(syn, errors, synHints);
        check(synOk, "平衡校验恒不阻断（V1.3 §3.2）");
        const QString joined = synHints.join(QStringLiteral(" "));
        check(synHints.size() >= 2, "平衡提示含逐时段 + 总量两条");
        check(joined.contains("缺口") && joined.contains("100"),"逐时段提示识别最大缺口 100 MW");
        check(joined.contains("总量平衡") && joined.contains("-100"),"总量提示识别偏差 -100 MW");
        check(joined.contains("1/4"),"逐时段提示统计偏差时段数 1/4");

        // 平衡态：申报与负荷完全一致 → 无"缺口/过剩"字样
        MarketData bal = syn;
        for (auto &b : bal.consumerBids)
        {
            // 时段4 申报改回 400（与负荷一致）
            if (b.period == 4)
                b.quantity = 400.0;
        }
        QStringList balHints;
        DataReader::validateRelations(bal, errors, balHints);
        const QString balJoined = balHints.join(QStringLiteral(" "));
        check(balJoined.contains("总量平衡") && balJoined.contains("+0.0"),"平衡态总量偏差为 0");
        check(!balJoined.contains("缺口") && !balJoined.contains("过剩"),"平衡态无缺口/过剩字样");
    }

    // V1.3 长表真实样例（scenario 双侧逐时段申报）
    DataFileSet scenarioFiles;
    scenarioFiles.generatorBidsFile = repoRoot + "/data/samples/scenario_balanced/generator_bids.csv";
    scenarioFiles.consumerBidsFile = repoRoot + "/data/samples/scenario_balanced/consumer_bids.csv";
    scenarioFiles.loadCurveFile = repoRoot + "/data/samples/curves/load_curve.csv";
    MarketData scenarioData;
    errors.clear();
    ok = DataReader::readAll(scenarioFiles,scenarioData,errors);
    check(ok,"V1.3 长表样例读取（双侧逐时段）");
    if (!ok)
    {printErrors(errors);
    }
    else
    {
        // 每个主体（电厂名称+机组编号）必须覆盖 96 个时段
        QHash<QString, QSet<int>> genPeriods;
        for (const GeneratorBid &item : scenarioData.generatorBids)
        {genPeriods[item.name + "|" + item.id].insert(item.period);
        }
        bool allCovered = true;
        for (auto it = genPeriods.cbegin(); it != genPeriods.cend(); ++it)
        {if (it.value().size() != 96)
            {allCovered = false;
                break;
            }
        }
        check(allCovered && !genPeriods.isEmpty(),"长表发电主体全部覆盖 96 时段");
        QHash<QString, QSet<int>> conPeriods;
        for (const ConsumerBid &item : scenarioData.consumerBids)
        {conPeriods[item.name + "|" + item.id].insert(item.period);
        }
        bool conCovered = true;
        for (auto it = conPeriods.cbegin(); it != conPeriods.cend(); ++it)
        {if (it.value().size() != 96)
            {conCovered = false;
                break;
            }
        }
        check(conCovered && !conPeriods.isEmpty(),"长表购电主体全部覆盖 96 时段");

        // 跨厂重号检查：机组编号可重号（金陵 #1 / 龙潭 #1），身份 = 名称+编号
        check(genPeriods.contains(QStringLiteral("金陵电厂|#1机组")) && genPeriods.contains(QStringLiteral("龙潭电厂|#1机组")),"机组编号跨厂重号共存（身份 = 电厂名称+机组编号）");
>>>>>>> 197592d (刚刚那一版没有Highs，重新改了一版)
    }
    return data;
}
}

<<<<<<< HEAD
int main(int argc,char *argv[])
{QCoreApplication app(argc,argv);
    // 失败用例计数。
    int failedTests = 0;
    qInfo().noquote()<< "========== DataReader V1.4 Test ==========";
    // 定位项目根目录。
    const QString projectRoot =findProjectRoot();
    check(!projectRoot.isEmpty(),"定位 ClearingSim 项目根目录",failedTests);
    if (projectRoot.isEmpty())
    {return 1;
    }
    qInfo().noquote()<< "Project root:"<< QDir::toNativeSeparators(projectRoot);
    // 定位仓库根目录。
    const QString repositoryRoot =findRepositoryRoot(projectRoot);
    check(!repositoryRoot.isEmpty(),"定位仓库根目录",failedTests);
    qInfo().noquote()<< "Repository root:"<< QDir::toNativeSeparators(repositoryRoot);
    // 定位场景数据目录。
    const QString directory =scenarioDirectory(repositoryRoot);
    check(QDir(directory).exists(),"定位 scenario 场景数据目录",failedTests);
    qInfo().noquote()<< "Scenario directory:"<< QDir::toNativeSeparators(directory);
    // 校验 8 个场景 CSV 是否齐全。
    const int fileCount =countScenarioFiles(directory);
    check(fileCount == 8,QString("定位 24/96 时段八类 CSV 数据（%1/8）").arg(fileCount),failedTests);
    // 真实 24 时段数据
    {MarketData data;
        QStringList errors;
        const bool result =DataReader::readAll(makeFileSet(directory,24),data,errors);
        check(result,"读取完整 24 时段真实数据",failedTests,errors);
        if (result)
        {check(data.loadCurve.size() ==24,"24 时段负荷数量正确",failedTests);
            check(containsPeriod(data.generatorBids,1) &&containsPeriod(data.generatorBids,24),"24 时段发电申报 period 正确",failedTests);
        }
    }
    // 真实 96 时段数据
    {MarketData data;
        QStringList errors;
        const bool result =DataReader::readAll(makeFileSet(directory,96),data,errors);
        check(result,"读取完整 96 时段真实数据",failedTests,errors);
        if (result)
        {check(data.loadCurve.size() ==96,"96 时段负荷数量正确",failedTests);
            check(containsPeriod(data.generatorBids,1) &&containsPeriod(data.generatorBids,96),"96 时段发电申报 period 正确",failedTests);
        }
    }
    // 后续异常用例在临时目录中构造 CSV。
    QTemporaryDir tempDir;
    check(tempDir.isValid(),"创建临时测试目录",failedTests);
    if (!tempDir.isValid())
    {return 1;
    }
    // 宽表拆成 segment 对象
    {const QString path =tempDir.path() +"/segments.csv";
        writeTextFile(path,makeGeneratorCsv(24));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(result,"宽表申报读取成功",failedTests,errors);
        if (result)
        {check(countSegments(data,1,"G1") == 2,"P1/C1、P2/C2 拆成 2 个 GeneratorBid",failedTests);
        }
    }
    // 同一机组跨 period 合法
    {const QString path =tempDir.path() +"/multi_period.csv";
        writeTextFile(path,makeGeneratorCsv(24));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(result,"允许同一机组出现在不同 period",failedTests,errors);
    }
    // 同一 period 同一机组重复
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"}}));
            if (period == 1)
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"60.0","160.000"}}));
=======
    QTemporaryDir tempDir;
    check(tempDir.isValid(),"创建临时测试目录");
    if (tempDir.isValid())
    {QVector<GeneratorBid> generators;
        QVector<ConsumerBid> consumers;
        QVector<LoadPoint> loadPoints;

        // 表头错误
        const QString badHeaderFile = tempDir.path() + "/bad_header.csv";
        writeTextFile(badHeaderFile,"id,name,type,segment,price,quantity\n"
                                     "G1,一号火电,火电,1,150.000,100.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(badHeaderFile,generators,errors);
        check(!ok,"识别固定表头错误");

        // 超过 5 个申报段
        const QString tooManySegments = tempDir.path() + "/too_many_segments.csv";
        writeTextFile(tooManySegments,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                       "G1,一号火电,火电,1,100.000,10.0\n"
                                       "G1,一号火电,火电,2,110.000,10.0\n"
                                       "G1,一号火电,火电,3,120.000,10.0\n"
                                       "G1,一号火电,火电,4,130.000,10.0\n"
                                       "G1,一号火电,火电,5,140.000,10.0\n"
                                       "G1,一号火电,火电,6,150.000,10.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(tooManySegments,generators,errors);
        check(!ok,"识别超过 5 个申报段");

        // 申报段不连续
        const QString gapSegmentFile = tempDir.path() + "/gap_segment.csv";
        writeTextFile(gapSegmentFile,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                      "G1,一号火电,火电,1,100.000,10.0\n"
                                      "G1,一号火电,火电,3,120.000,10.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(gapSegmentFile,generators,errors);
        check(!ok,"识别申报段不连续");

        // 发电报价方向错误
        const QString generatorPriceFile = tempDir.path() + "/generator_price.csv";
        writeTextFile(generatorPriceFile,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                          "G1,一号火电,火电,1,200.000,10.0\n"
                                          "G1,一号火电,火电,2,150.000,10.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(generatorPriceFile,generators,errors);
        check(!ok,"识别发电报价单调错误");

        // 购电报价方向错误
        const QString consumerPriceFile = tempDir.path() + "/consumer_price.csv";
        writeTextFile(consumerPriceFile,"用户ID,用户名称,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                         "U1,一号用户,1,200.000,10.0\n"
                                         "U1,一号用户,2,300.000,10.0\n");
        errors.clear();
        ok = DataReader::readConsumerBids(consumerPriceFile,consumers,errors);
        check(!ok,"识别购电报价单调错误");

        // 电价越界
        const QString priceLimitFile = tempDir.path() + "/price_limit.csv";
        writeTextFile(priceLimitFile,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                      "G1,一号火电,火电,1,1501.000,10.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(priceLimitFile,generators,errors);
        check(!ok,"识别 0~1500 电价限制");

        // 电量为 0：V1.3 规则⑥允许（该时段不申报/停机）
        const QString zeroQuantityFile = tempDir.path() + "/zero_quantity.csv";
        writeTextFile(zeroQuantityFile,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                        "G1,一号火电,火电,1,150.000,0.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(zeroQuantityFile,generators,errors);
        check(ok,"申报电量 0 合法（V1.3 规则⑥：停机申报）");
        if (!ok)
        {printErrors(errors);
        }

        // 电量为负
        const QString negativeQuantityFile = tempDir.path() + "/negative_quantity.csv";
        writeTextFile(negativeQuantityFile,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                            "G1,一号火电,火电,1,150.000,-5.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(negativeQuantityFile,generators,errors);
        check(!ok,"识别申报电量为负");

        // 小数精度错误
        const QString precisionFile = tempDir.path() + "/precision.csv";
        writeTextFile(precisionFile,"机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
                                     "G1,一号火电,火电,1,150.00,100.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(precisionFile,generators,errors);
        check(!ok,"识别申报价格小数精度错误");

        // 长表表头段列不成对（5 列 = 3 基础列 + 1 段出力，缺段报价）
        const QString badLongHeaderFile = tempDir.path() + "/bad_long_header.csv";
        writeTextFile(badLongHeaderFile,"period,电厂名称,机组编号,第1段出力(MW)\n"
                                         "1,金陵电厂,#1机组,100.0\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(badLongHeaderFile,generators,errors);
        check(!ok,"识别长表表头段列不成对");
        if (!ok)
        {printErrors(errors);
        }

        // 长表时段覆盖不足（主体只申报 1 个时段）
        const QString shortLongFile = tempDir.path() + "/short_long.csv";
        writeTextFile(shortLongFile,"period,电厂名称,机组编号,第1段出力(MW),第1段报价(元/MWh)\n"
                                     "1,金陵电厂,#1机组,100.0,150.000\n");
        errors.clear();
        ok = DataReader::readGeneratorBids(shortLongFile,generators,errors);
        check(!ok,"识别长表主体未覆盖 96 时段");

        // 负荷不足 96 点
        const QString shortLoadFile = tempDir.path() + "/short_load.csv";
        writeTextFile(shortLoadFile,buildLoadCsv(95));
        errors.clear();
        ok = DataReader::readLoadCurve(shortLoadFile,loadPoints,errors);
        check(!ok,"识别负荷曲线不足 96 点");
    }

    // #98：窄表 × 长表等价性回归测试
    //   同一份基准数据（1 主体 × 2 段，固定量价）写成窄表与长表两份 CSV，
    //   分别读入后比对逐 period × segment 的 quantity / price —— 必须完全一致。
    //   覆盖 #96 抽出的 expandParsedBidsTo96Periods / materializeToGeneratorBids
    //   / materializeToConsumerBids 三 helper。
    if (tempDir.isValid())
    {
        // ---- 基准数据：1 主体 × 2 段 × 96 期同量同价 ----
        struct BidSpec
        {QString name; QString id;
            int segment;
            double quantity;
            double price;
        };
        const QVector<BidSpec> specs = {
                                         {QStringLiteral("测试电厂A"), QStringLiteral("#1机组"), 1, 80.0, 180.0},
                                         {QStringLiteral("测试电厂A"), QStringLiteral("#1机组"), 2, 40.0, 220.0},
                                         };

        // ---- 写窄表 CSV（每段一行，无 period）----
        QString narrowCsv = QStringLiteral("机组ID,机组名称,申报段,申报电价(元/MWh),申报电量(MWh)\n");
        for (const auto &s : specs)
        {narrowCsv += QStringLiteral("%1,%2,%3,%4,%5\n").arg(s.id, s.name).arg(s.segment).arg(s.price, 0, 'f', 3).arg(s.quantity, 0, 'f', 1);
        }
        const QString narrowFile = tempDir.path() + QStringLiteral("/narrow.csv");
        check(writeTextFile(narrowFile, narrowCsv),"写入窄表临时 CSV");

        // ---- 写长表 CSV（96 期 × 同一份基准数据）----
        QString longCsv = QStringLiteral("period,电厂名称,机组编号,第1段出力(MW),第1段报价(元/MWh),第2段出力(MW),第2段报价(元/MWh)\n");
        for (int p = 1; p <= 96; ++p)
        {
            // 段 1
            longCsv += QStringLiteral("%1,%2,%3,%4,%5,").arg(p).arg(specs[0].name, specs[0].id).arg(specs[0].quantity, 0, 'f', 1).arg(specs[0].price, 0, 'f', 3);
            // 段 2（最后一个无尾随逗）
            longCsv += QStringLiteral("%1,%2\n").arg(specs[1].quantity, 0, 'f', 1).arg(specs[1].price, 0, 'f', 3);
        }
        const QString longFile = tempDir.path() + QStringLiteral("/long.csv");
        check(writeTextFile(longFile, longCsv),"写入长表临时 CSV");

        // ---- 读入两边 ----
        QStringList errsN, errsL;
        QVector<GeneratorBid> narrowBids, longBids;
        const bool okN = DataReader::readGeneratorBids(narrowFile, narrowBids, errsN);
        if (!okN)
            qInfo().noquote() << "[DBG] errsN =" << errsN;
        const bool okL = DataReader::readGeneratorBids(longFile, longBids, errsL);
        if (!okL)
            qInfo().noquote() << "[DBG] errsL =" << errsL;
        check(okN, "窄表读入成功");
        check(okL, "长表读入成功");

        // ---- 比对：窄表展开后应有 1 主体 × 2 段 × 96 期 = 192 行 ----
        check(narrowBids.size() == 192,QStringLiteral("窄表展开 192 行（实际 = %1）").arg(narrowBids.size()));
        check(longBids.size() == 192,QStringLiteral("长表 192 行（实际 = %1）").arg(longBids.size()));

        // ---- 比对：按 (period, segment) 排序后逐行 (period/name/id/qty/price) 一致 ----
        // 窄表按"段展开 96 期"顺序存；长表按"逐 period × 段"展开；
        // 两者底层有序但不一致，统一按 (period, segment) 排序后逐项比较。
        auto cmp = [](const GeneratorBid &a, const GeneratorBid &b)
        {if (a.period != b.period) return a.period < b.period;
            return a.segment < b.segment;
        };
        QVector<GeneratorBid> sortedN = narrowBids;
        QVector<GeneratorBid> sortedL = longBids;
        std::sort(sortedN.begin(), sortedN.end(), cmp);
        std::sort(sortedL.begin(), sortedL.end(), cmp);
        bool allMatch = (sortedN.size() == sortedL.size());
        for (int i = 0; allMatch && i < sortedN.size(); ++i)
        {const auto &n = sortedN[i];
            const auto &l = sortedL[i];
            if (n.period != l.period || n.segment != l.segment || n.name != l.name || n.id != l.id || qAbs(n.quantity - l.quantity) > 1e-6 || qAbs(n.price - l.price) > 1e-6)
            {allMatch = false;
                qInfo().noquote()<< QStringLiteral("[DBG] diff @%1: narrow=(p=%2,s=%3,n=%4,id=%5,q=%6,price=%7) vs long=(p=%8,s=%9,n=%10,id=%11,q=%12,price=%13)").arg(i).arg(n.period).arg(n.segment).arg(n.name).arg(n.id).arg(n.quantity, 0, 'f', 4).arg(n.price, 0, 'f', 4).arg(l.period).arg(l.segment).arg(l.name).arg(l.id).arg(l.quantity, 0, 'f', 4).arg(l.price, 0, 'f', 4);
>>>>>>> 197592d (刚刚那一版没有Highs，重新改了一版)
            }
        }
        const QString path =tempDir.path() +"/duplicate.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"重复"),"识别同 period 重复机组",failedTests,errors);
    }
<<<<<<< HEAD
    // 新能源允许 0 出力申报
    {const QString path =tempDir.path() +"/solar.csv";
        writeTextFile(path,makeGeneratorCsv(24,"光伏"));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(result,"允许光伏夜间申报量为 0",failedTests,errors);
    }
    // 常规机组申报量不能为 0
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {const QString quantity =period == 1? "0": "50.0";
            rows.push_back(makeGeneratorRow(period,"G1","火电",{{quantity,"150.000"}}));
        }
        const QString path =tempDir.path() +"/thermal_zero.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"必须大于 0"),"识别常规机组零申报量",failedTests,errors);
    }
    // 发电报价必须递增
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {if (period == 1)
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","200.000"},{"30.0","180.000"}}));
            }
            else
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"}}));
            }
        }
        const QString path =tempDir.path() +"/generator_price.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"单调非递减"),"识别发电报价单调错误",failedTests,errors);
    }
    // 用户报价必须递减
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {if (period == 1)
            {rows.push_back(makeConsumerRow(period,"U1",{{"50.0","300.000"},{"30.0","350.000"}}));
            }
            else
            {rows.push_back(makeConsumerRow(period,"U1",{{"50.0","400.000"}}));
            }
        }
        const QString path =tempDir.path() +"/consumer_price.csv";
        writeTextFile(path,makeCsv(consumerHeader(),rows));
        QVector<ConsumerBid> data;
        QStringList errors;
        const bool result =DataReader::readConsumerBids(path,data,errors);
        check(!result &&containsError(errors,"单调非递增"),"识别购电报价单调错误",failedTests,errors);
    }
    // 用户申报量不能为 0
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {const QString quantity =period == 1? "0": "50.0";
            rows.push_back(makeConsumerRow(period,"U1",{{quantity,"400.000"}}));
        }
        const QString path =tempDir.path() +"/consumer_zero.csv";
        writeTextFile(path,makeCsv(consumerHeader(),rows));
        QVector<ConsumerBid> data;
        QStringList errors;
        const bool result =DataReader::readConsumerBids(path,data,errors);
        check(!result &&containsError(errors,"必须大于 0"),"识别用户零申报量",failedTests,errors);
    }
    // 报价超过允许范围
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {const QString price =period == 1? "541.000": "150.000";
            rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0",price}}));
        }
        const QString path =tempDir.path() +"/price_range.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"0～540"),"识别报价超过允许范围",failedTests,errors);
    }
    // 报价最多保留 3 位小数
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {const QString price =period == 1? "150.0000": "150.000";
            rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0",price}}));
        }
        const QString path =tempDir.path() +"/price_precision.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"最多保留 3 位小数"),"识别报价小数位数错误",failedTests,errors);
    }
    // 报价段填写不完整
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {if (period == 1)
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0",""}}));
            }
            else
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"}}));
            }
        }
        const QString path =tempDir.path() +"/incomplete_segment.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"报价不完整"),"识别报价段填写不完整",failedTests,errors);
    }
    // 报价段不能断裂
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {if (period == 1)
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"},{"",""},{"20.0","220.000"}}));
            }
            else
            {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"}}));
            }
        }
        const QString path =tempDir.path() +"/segment_gap.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"报价段不连续"),"识别报价段不连续",failedTests,errors);
    }
    // 发电侧表头错误
    {QStringList header = generatorHeader();
        header[0] = "wrong";
        QStringList rows;
        for (int period = 1;period <= 24;++period)
        {rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"}}));
        }
        const QString path =tempDir.path() +"/bad_header.csv";
        writeTextFile(path,makeCsv(header,rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"第 1 列"),"识别发电侧错误表头",failedTests,errors);
    }
    // 发电申报缺失 period
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {if (period == 12)
            {continue;
            }
            rows.push_back(makeGeneratorRow(period,"G1","火电",{{"50.0","150.000"}}));
        }
        const QString path =tempDir.path() +"/missing_period.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"缺少时段 12"),"识别发电申报缺失时段",failedTests,errors);
    }
    // 同一机组跨时段类型必须一致
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {const QString type =period == 2? "风电": "火电";
            rows.push_back(makeGeneratorRow(period,"G1",type,{{"50.0","150.000"}}));
        }
        const QString path =tempDir.path() +"/generator_type.csv";
        writeTextFile(path,makeCsv(generatorHeader(),rows));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"类型与其他时段不一致"),"识别机组跨时段类型不一致",failedTests,errors);
    }
    // 负荷 period 重复
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {rows.push_back(makeLoadRow(period,24,"500.0"));
            if (period == 1)
            {rows.push_back(makeLoadRow(period,24,"510.0"));
            }
        }
        const QString path =tempDir.path() +"/load_duplicate.csv";
        writeTextFile(path,makeCsv(loadHeader(),rows));
        QVector<LoadPoint> data;
        QStringList errors;
        const bool result =DataReader::readLoadCurve(path,data,errors);
        check(!result &&containsError(errors,"重复"),"识别负荷时段重复",failedTests,errors);
    }
    // 负荷不能为负
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {const QString load =period == 1? "-1.0": "500.0";
            rows.push_back(makeLoadRow(period,24,load));
        }
        const QString path =tempDir.path() +"/negative_load.csv";
        writeTextFile(path,makeCsv(loadHeader(),rows));
        QVector<LoadPoint> data;
        QStringList errors;
        const bool result =DataReader::readLoadCurve(path,data,errors);
        check(!result &&containsError(errors,"不能为负数"),"识别负荷为负数",failedTests,errors);
    }
    // 新能源类型只能为风电或光伏
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {rows.push_back(makeRenewableRow(period,24,"R1","水电","50.0"));
        }
        const QString path =tempDir.path() +"/renewable_type.csv";
        writeTextFile(path,makeCsv(renewableHeader(),rows));
        QVector<RenewableOutput> data;
        QStringList errors;
        const bool result =DataReader::readRenewableOutput(path,data,errors);
        check(!result &&containsError(errors,"必须为风电或光伏"),"识别非法新能源类型",failedTests,errors);
    }
    // 新能源同一机组同 period 不能重复
    {QStringList rows;
        for (int period = 1;period <= 24;++period)
        {rows.push_back(makeRenewableRow(period,24,"W1","风电","50.0"));
            if (period == 1)
            {rows.push_back(makeRenewableRow(period,24,"W1","风电","60.0"));
            }
        }
        const QString path =tempDir.path() +"/renewable_duplicate.csv";
        writeTextFile(path,makeCsv(renewableHeader(),rows));
        QVector<RenewableOutput> data;
        QStringList errors;
        const bool result =DataReader::readRenewableOutput(path,data,errors);
        check(!result &&containsError(errors,"新能源出力重复"),"识别新能源重复时段数据",failedTests,errors);
    }
    // validateRelations 正常关系
    {MarketData data =makeValidMarketData24();
        QStringList errors;
        const bool result =DataReader::validateRelations(data,errors);
        check(result,"跨文件关系校验正常数据通过",failedTests,errors);
    }
    // 新能源 ID 必须存在于发电侧
    {MarketData data =makeValidMarketData24();
        for (int period = 1;period <= 24;++period)
        {RenewableOutput item;
            item.generatorId ="S9";
            item.generatorType ="光伏";
            item.period =period;
            item.output =20.0;
            data.renewableOutputs.push_back(item);
        }
        QStringList errors;
        const bool result =DataReader::validateRelations(data,errors);
        check(!result &&containsError(errors,"在发电侧申报中不存在"),"识别新能源机组 ID 不存在",failedTests,errors);
    }
    // 新能源类型必须和发电侧一致
    {MarketData data =makeValidMarketData24();
        for (RenewableOutput &item : data.renewableOutputs)
        {if (item.generatorId =="W1")
            {item.generatorType ="光伏";
            }
        }
        QStringList errors;
        const bool result =DataReader::validateRelations(data,errors);
        check(!result &&containsError(errors,"机组类型不一致"),"识别新能源跨文件类型不一致",failedTests,errors);
    }
    // 新能源机组必须存在出力数据
    {MarketData data =makeValidMarketData24();
        data.renewableOutputs.clear();
        QStringList errors;
        const bool result =DataReader::validateRelations(data,errors);
        check(!result &&containsError(errors,"缺少新能源出力数据"),"识别新能源机组缺少出力数据",failedTests,errors);
    }
    // 用户跨文件时段完整性
    {MarketData data =makeValidMarketData24();
        for (int i =data.consumerBids.size() - 1;i >= 0;--i)
        {if (data.consumerBids[i].period ==24)
            {data.consumerBids.removeAt(i);
            }
        }
        QStringList errors;
        const bool result =DataReader::validateRelations(data,errors);
        check(!result &&containsError(errors,"用户 U1 缺少时段 24"),"识别用户跨文件时段缺失",failedTests,errors);
    }
    // CSV 只有表头
    {const QString path =tempDir.path() +"/header_only.csv";
        writeTextFile(path,makeCsv(generatorHeader(),{}));
        QVector<GeneratorBid> data;
        QStringList errors;
        const bool result =DataReader::readGeneratorBids(path,data,errors);
        check(!result &&containsError(errors,"没有有效数据"),"识别 CSV 只有表头没有数据",failedTests,errors);
    }
    // 输出汇总，失败数决定退出码。
    qInfo().noquote()<< "========================================";
    if (failedTests == 0)
    {qInfo().noquote()<< "All DataReader V1.4 tests passed.";
    }
    else
    {qInfo().noquote()<< failedTests<< "test(s) failed.";
    }
    return failedTests == 0? 0: 1;
=======

    qInfo().noquote()<< "========================================";
    if (failedTests == 0)
    {qInfo().noquote()<< "All DataReader V1.3 tests passed.";
        return 0;
    }
    qCritical().noquote()<< failedTests << "test(s) failed.";
    return 1;
>>>>>>> 197592d (刚刚那一版没有Highs，重新改了一版)
}