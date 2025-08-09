#include "widget.h"

#include <qt_windows.h>
#pragma comment(lib, "Version.lib")

#include <QDebug>
#include <QApplication>
#include <QDesktopWidget>
#include <QHeaderView>
#include <QDir>
#include <QMap>
#include <QFontDatabase>
#include <QFileInfo>
#include <QSettings>
#include <QBoxLayout>
#include <QPushButton>
#include <QThread>
#include <QtConcurrent>



#define MSG(x) QString::fromUtf16(reinterpret_cast<const char16_t*>(x))
#define SCALE(x) qRound(x *RecommendScalingRate)
#define INI "./config.ini"
#define API "https://api.binance.com"



enum ApiId
{
    EXCHANGEINFO,
    PRICE,
    PRICE24HR,
    MAX
};



static int MyEvent;
static QMap<QString, QVariant> UiSettings;
static QString AppVersion;
static QString PairPropertyNames[6] = {
    "lastPrice", //當前
    "priceChangePercent", //振幅
    "highPrice", //最高
    "lowPrice", //最低
    "weightedAvgPrice", //加權平均
    "quoteVolume" //成交量
};
static qreal RecommendScalingRate;
static QStringList PairNames;
static QFont MyFont;
static QMap<int, QString> ApiUrls;



static void InitAppVersion()
{
    QString fName = QCoreApplication::applicationFilePath();
    DWORD dwHandle;
    DWORD dwLen = GetFileVersionInfoSize(fName.toStdWString().c_str(), &dwHandle);
    BYTE* lpData = new BYTE[dwLen];
    if(GetFileVersionInfo(fName.toStdWString().c_str(), dwHandle, dwLen, lpData)) {
        VS_FIXEDFILEINFO* lpBuffer = nullptr;
        UINT uLen;
        if(VerQueryValue(lpData, QString("\\").toStdWString().c_str(), reinterpret_cast<LPVOID*>(&lpBuffer), &uLen)) {
            AppVersion = QString::number((lpBuffer->dwFileVersionMS >> 16) & 0xffff) + "." +
                    QString::number((lpBuffer->dwFileVersionMS) & 0xffff) + "." +
                    QString::number((lpBuffer->dwFileVersionLS >> 16) & 0xffff) + "." +
                    QString::number((lpBuffer->dwFileVersionLS) & 0xffff);
        }
    }
    delete[] lpData;
}

void InitApp()
{
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    MyEvent = QEvent::registerEventType();
    InitAppVersion();
    if(QFileInfo::exists(INI)) {
        QSettings ini(INI, QSettings::IniFormat);
        ini.setIniCodec("UTF-8");
        ini.beginGroup("ui");
        QStringList allkeys = ini.allKeys();
        foreach(QString key, allkeys)
            UiSettings[key] = ini.value(key);
        ini.endGroup();
        ini.beginGroup("pair");
        allkeys = ini.allKeys();
        for(int i=0; i<allkeys.count(); i++)
            if(allkeys.contains(QString::number(i)))
                PairNames.append(ini.value(QString::number(i)).toString());
        ini.endGroup();
    }
    QSize userSize  = QApplication::desktop()->screenGeometry().size();
    RecommendScalingRate = qMax(userSize.width() /1920.0, userSize.height() /1080.0);
    int fontId = QFontDatabase::addApplicationFont("./font.ttc");
    MyFont.setFamily(QFontDatabase::applicationFontFamilies(fontId).at(0));
    MyFont.setPixelSize(SCALE(16));

    ApiUrls[ApiId::EXCHANGEINFO] = API"/api/v3/exchangeInfo";
    ApiUrls[ApiId::PRICE] = API"/api/v3/ticker/price";
    ApiUrls[ApiId::PRICE24HR] = API"/api/v3/ticker/24hr";
}



/****************************************************************************************************/
Widget::Widget(QWidget *parent) :
    QWidget(parent)
{
    //qDebug() << QSslSocket::supportsSsl() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::sslLibraryVersionString();
    setFont(MyFont);
    QVBoxLayout *loV100 = new QVBoxLayout(this);

    nam = new QNetworkAccessManager(this);
    QObject::connect(nam, &QNetworkAccessManager::finished, this, &Widget::requestFinished);
    tmrRefreshPairInfos.setSingleShot(true);
    tmrRefreshPairInfos.setInterval(100);
    QObject::connect(&tmrRefreshPairInfos, &QTimer::timeout, this, &Widget::refreshUi);
    tmrAutoRefresh.setSingleShot(false);
    tmrAutoRefresh.setInterval(qMax(8000, UiSettings["interval"].toInt() *1000));
    QObject::connect(&tmrAutoRefresh, &QTimer::timeout, this, &Widget::getApi_PRICE24HR);

    uiAutoRefresh = new QCheckBox(MSG(L"自動刷新"));
    uiAutoRefresh->setChecked(UiSettings["autoRefresh"].toBool());
    QHBoxLayout *loH99 = new QHBoxLayout;
    QObject::connect(uiAutoRefresh, &QCheckBox::stateChanged, this, &Widget::onUiAutoRefreshstateChanged);
    loH99->addWidget(uiAutoRefresh);
    loH99->addStretch();

    QLabel* lbSort = new QLabel(MSG(L"排序方式"));
    uiSortType = new QComboBox;
    uiSortType->setFocusPolicy(Qt::StrongFocus);
    uiSortType->addItems({
                             MSG(L"無"),
                             MSG(L"（降冪）交易對"),
                             MSG(L"（降冪）當前"),
                             MSG(L"（降冪）振幅"),
                             MSG(L"（降冪）最高"),
                             MSG(L"（降冪）最低"),
                             MSG(L"（降冪）加權平均"),
                             MSG(L"（降冪）成交量"),
                             MSG(L"（降冪）阻力"),
                             MSG(L"（升冪）交易對"),
                             MSG(L"（升冪）當前"),
                             MSG(L"（升冪）振幅"),
                             MSG(L"（升冪）最高"),
                             MSG(L"（升冪）最低"),
                             MSG(L"（升冪）加權平均"),
                             MSG(L"（升冪）成交量"),
                             MSG(L"（升冪）阻力"),
                         });
    uiSortType->setCurrentIndex(UiSettings["sort"].toInt());
    QObject::connect(uiSortType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Widget::onUiSortTypeCurrentIndexChanged);
    loH99->addWidget(lbSort);
    loH99->addWidget(uiSortType);
    loV100->addLayout(loH99);

    QScrollArea* sa = new QScrollArea;
    sa->setWidgetResizable(true);
    page = new QTableWidget(this);

    QPalette pal = page->palette();
    pal.setColor(QPalette::Background, Qt::white);
    page->setAutoFillBackground(true);
    page->setPalette(pal);
    page->setFont(MyFont);
    sa->setWidget(page);
    sa->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    loV100->addWidget(sa);

    page->setEditTriggers(QAbstractItemView::NoEditTriggers);
    page->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    page->horizontalHeader()->setStretchLastSection(false);
    page->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    page->setColumnCount(7);
    page->setHorizontalHeaderLabels({MSG(L"當前"), MSG(L"振幅"), MSG(L"最高"), MSG(L"最低"), MSG(L"加權平均"), MSG(L"成交量"), MSG(L"阻力")});
    page->verticalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    page->verticalHeader()->setStretchLastSection(false);
    page->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    page->setRowCount(PairNames.count());
    page->setVerticalHeaderLabels(PairNames);
    for(int i=0; i<PairNames.count(); i++) {
        pairInfos.append(PAIR_INFO({i, QString(), QString(), PairNames[i], PAIR_PROPERTY()}));
        pairRanks.append(&pairInfos[i]);
        for(int j=0; j<7; j++) {
            QTableWidgetItem* bar = new QTableWidgetItem("0.00000000");
            bar->setTextAlignment(Qt::AlignRight | Qt::AlignTop);
            page->setItem(i, j, bar);
        }
    }

    QPushButton* uiEdit = new QPushButton(MSG(L"設定"));
    QObject::connect(uiEdit, &QPushButton::clicked, this, &Widget::editConfig);
    QPushButton* uiRefresh = new QPushButton(MSG(L"刷新"));
    QObject::connect(uiRefresh, &QPushButton::clicked, this, &Widget::getApi_PRICE24HR);

    QHBoxLayout *loH100 = new QHBoxLayout;
    loH100->addWidget(uiEdit);
    loH100->addStretch();
    loH100->addWidget(uiRefresh);
    loV100->addLayout(loH100);

    setWindowTitle(UiSettings["title"].toString() +" " +AppVersion);
    this->resize(UiSettings["width"].toInt(), UiSettings["height"].toInt());
    this->move(qMax(0, UiSettings["x"].toInt()), qMax(0, UiSettings["y"].toInt()));
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(MyEvent)));
}
/****************************************************************************************************/
Widget::~Widget()
{
}
/****************************************************************************************************/
void Widget::requestFinished(QNetworkReply* reply) {
    //qDebug() << "requestFinished";
    bool bIsOk = (QNetworkReply::NoError == reply->error());
    if(bIsOk) {
        QVariant apiId = reply->property("ApiId");
        if(!apiId.isNull()) {
            switch(apiId.toInt()) {
            case ApiId::EXCHANGEINFO:
                onApi_EXCHANGEINFO(reply);
                break;
            case ApiId::PRICE:
                onApi_PRICE(reply);
                break;
            case ApiId::PRICE24HR:
                onApi_PRICE24HR(reply);
                break;
            default:
                break;
            }
        }
    } else {
        return;
    }
}
/****************************************************************************************************/
void Widget::customEvent(QEvent* event)
{
    if(event->type() == MyEvent) {
        getApi_EXCHANGEINFO();
        if(uiAutoRefresh->isChecked()) {
            getApi_PRICE24HR();
            tmrAutoRefresh.start();
        }
    }
}
/****************************************************************************************************/
void Widget::closeEvent(QCloseEvent* event)
{
    if(QFileInfo::exists(INI)) {
        QSettings ini(INI, QSettings::IniFormat);
        ini.setIniCodec("UTF-8");
        ini.beginGroup("ui");
        ini.setValue("x", this->x());
        ini.setValue("y", this->y());
        ini.setValue("width", this->width());
        ini.setValue("height", this->height());
        ini.setValue("autoRefresh", this->uiAutoRefresh->isChecked());
        ini.setValue("sort", this->uiSortType->currentIndex());
        ini.endGroup();
        ini.sync();
    }
    QWidget::closeEvent(event);
}
/****************************************************************************************************/
void Widget::getApi_EXCHANGEINFO()
{
    QNetworkRequest request;
    request.setUrl(QUrl(ApiUrls[ApiId::EXCHANGEINFO]));
    QNetworkReply* reply = nam->get(request);
    reply->setProperty("ApiId", ApiId::EXCHANGEINFO);
}
/****************************************************************************************************/
void Widget::getApi_PRICE()
{
    for(int i=0; i<PairNames.count(); i++) {
        QNetworkRequest request;
        request.setUrl(QUrl(ApiUrls[ApiId::PRICE] +"?symbol=" +PairNames[i]));
        QNetworkReply* reply = nam->get(request);
        reply->setProperty("ApiId", ApiId::PRICE);
        reply->setProperty("PairId", i);
    }
}
/****************************************************************************************************/
void Widget::getApi_PRICE24HR()
{
    //qDebug() << "getApi_PRICE24HR";
    for(int i=0; i<PairNames.count(); i++) {
        QNetworkRequest request;
        request.setUrl(QUrl(ApiUrls[ApiId::PRICE24HR] +"?symbol=" +PairNames[i]));
        QNetworkReply* reply = nam->get(request);
        reply->setProperty("ApiId", ApiId::PRICE24HR);
        reply->setProperty("PairId", i);
    }
}
/****************************************************************************************************/
void Widget::onApi_EXCHANGEINFO(QNetworkReply* reply)
{
    QJsonParseError e;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll(), &e);
    //qDebug() << "onApi_EXCHANGEINFO";
    if(e.error == QJsonParseError::NoError && !jsonDoc.isNull()) {
        QJsonArray symbols = jsonDoc.object().value("symbols").toArray();
        foreach(const QJsonValue& v, symbols) {
            QJsonObject jsonObject = v.toObject();
            //qDebug() << jsonObject.value("symbol").toString();
            QString szSymbol = jsonObject.value("symbol").toString();
            for(int i=0; i<PairNames.count(); i++)
                if(PairNames[i] == szSymbol) {
                    pairInfos[i].szBaseAsset = jsonObject.value("baseAsset").toString();
                    pairInfos[i].szQuoteAsset = jsonObject.value("quoteAsset").toString();
                    //qDebug() << "onApi_EXCHANGEINFO" << pairInfos[i].szBaseAsset << pairInfos[i].szQuoteAsset;
                }
        }
    }
}
/****************************************************************************************************/
void Widget::onApi_PRICE(QNetworkReply* reply)
{
    QVariant symbol = reply->property("PairId");
    //qDebug() << "onApi_PRICE";
    if(!symbol.isNull()) {
        QJsonParseError e;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll(), &e);
        if(e.error == QJsonParseError::NoError && !jsonDoc.isNull()) {
            QJsonValue jsonValue = jsonDoc.object().value("price");
            if(!jsonValue.isNull())
                page->item(symbol.toInt(), 0)->setText(" " +jsonValue.toString());
        }
    }
}
/****************************************************************************************************/
void Widget::onApi_PRICE24HR(QNetworkReply* reply)
{
    QVariant symbol = reply->property("PairId");
    if(!symbol.isNull()) {
        int nPairId = symbol.toInt();
        QJsonParseError e;
        QByteArray ba = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(ba, &e);
        //qDebug() << "onApi_PRICE24HR" << jsonDoc;
        if(e.error == QJsonParseError::NoError && !jsonDoc.isNull()) {
            QJsonObject jsonObject = jsonDoc.object();
            QJsonValue jsonValue;
            for(int i=0; i<6; i++) {
                QJsonValue jsonValue = jsonObject.value(PairPropertyNames[i]);
                if(!jsonValue.isNull())
                    pairInfos[nPairId].propertys.id[i] = jsonValue.toString().toDouble();
            }
            //qDebug() << pairInfos[nPairId].propertys.name.dLastPrice;
            {
                //計算阻力 = 成交量 /(最高 -最低)百分比
                pairInfos[nPairId].propertys.id[6] =
                        pairInfos[nPairId].propertys.id[5] /((pairInfos[nPairId].propertys.id[2] /pairInfos[nPairId].propertys.id[3] -1) *100);
            }
            tmrRefreshPairInfos.start();
        }
        /*else
            qDebug().noquote() << ba;*/
    }
}
/****************************************************************************************************/
static void SetPairProperty(QTableWidgetItem* item, const double& dValue)
{
    QString szValue;
    if(dValue >= 1)
        szValue = QString::asprintf("%.2f", dValue);
    else
        szValue = QString::asprintf("%.8f", dValue);
    item->setText(" " +szValue);
}
/****************************************************************************************************/
static void SetPairPriceChangePercent(QTableWidgetItem* item, const double& dValue)
{
    QString szValue;
    if(dValue > 0) {
        item->setForeground(QBrush(QColor(0x00, 0x80, 0x00)));
        item->setText(" +" +QString::asprintf("%.2f", dValue) +"%");
    }
    else {
        item->setForeground(QBrush(QColor(0xFF, 0x00, 0x00)));
        item->setText(" " +QString::asprintf("%.2f", dValue) +"%");
    }
}
/****************************************************************************************************/
static bool FnSortPairInfo_0(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->id < info2->id;
}
//降冪排序
static bool FnSortPairInfo_1(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return (info1->szBaseAsset.localeAwareCompare(info2->szBaseAsset) > 0);
}
static bool FnSortPairInfo_2(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dLastPrice > info2->propertys.name.dLastPrice;
}
static bool FnSortPairInfo_3(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dPriceChangePercent > info2->propertys.name.dPriceChangePercent;
}
static bool FnSortPairInfo_4(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dHighPrice > info2->propertys.name.dHighPrice;
}
static bool FnSortPairInfo_5(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dLowPrice > info2->propertys.name.dLowPrice;
}
static bool FnSortPairInfo_6(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dWeightedAvgPrice > info2->propertys.name.dWeightedAvgPrice;
}
static bool FnSortPairInfo_7(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dQuoteVolume > info2->propertys.name.dQuoteVolume;
}
static bool FnSortPairInfo_8(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dObstacle > info2->propertys.name.dObstacle;
}
//升冪排序
static bool FnSortPairInfo_9(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return (info1->szBaseAsset.localeAwareCompare(info2->szBaseAsset) < 0);
}
static bool FnSortPairInfo_10(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dLastPrice < info2->propertys.name.dLastPrice;
}
static bool FnSortPairInfo_11(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dPriceChangePercent < info2->propertys.name.dPriceChangePercent;
}
static bool FnSortPairInfo_12(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dHighPrice < info2->propertys.name.dHighPrice;
}
static bool FnSortPairInfo_13(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dLowPrice < info2->propertys.name.dLowPrice;
}
static bool FnSortPairInfo_14(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dWeightedAvgPrice < info2->propertys.name.dWeightedAvgPrice;
}
static bool FnSortPairInfo_15(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dQuoteVolume < info2->propertys.name.dQuoteVolume;
}
static bool FnSortPairInfo_16(const PAIR_INFO* &info1, const PAIR_INFO* &info2)
{
    return info1->propertys.name.dObstacle < info2->propertys.name.dObstacle;
}
/****************************************************************************************************/
void Widget::refreshPairRanks()
{
    switch(uiSortType->currentIndex()) {
    default: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_0); break;
    case 1: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_1); break;
    case 2: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_2); break;
    case 3: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_3); break;
    case 4: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_4); break;
    case 5: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_5); break;
    case 6: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_6); break;
    case 7: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_7); break;
    case 8: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_8); break;
    case 9: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_9); break;
    case 10: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_10); break;
    case 11: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_11); break;
    case 12: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_12); break;
    case 13: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_13); break;
    case 14: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_14); break;
    case 15: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_15); break;
    case 16: std::sort(pairRanks.begin(), pairRanks.end(), FnSortPairInfo_16); break;
    }
}
/****************************************************************************************************/
void Widget::onUiAutoRefreshstateChanged(int state)
{
    switch(state) {
    case Qt::Unchecked:
        tmrAutoRefresh.stop();
        break;
    case Qt::Checked:
        getApi_PRICE24HR();
        tmrAutoRefresh.start();
        break;
    default:
        break;
    }
}
/****************************************************************************************************/
void Widget::onUiSortTypeCurrentIndexChanged(int)
{
    refreshUi();
}
/****************************************************************************************************/
void Widget::refreshUi()
{
    refreshPairRanks();
    for(int i=0; i<pairRanks.count(); i++) {
        page->verticalHeaderItem(i)->setText(pairRanks[i]->szBaseAsset +"/" +pairRanks[i]->szQuoteAsset);
        for(int j=0; j<7; j++) {
            QTableWidgetItem* item = page->item(i, j);
            if(j == 1)
                SetPairPriceChangePercent(item, pairRanks[i]->propertys.id[j]);
            else
                SetPairProperty(item, pairRanks[i]->propertys.id[j]);
        }
    }
}
/****************************************************************************************************/
void Widget::editConfig()
{
    QFileInfo fiIni(INI);
    if(fiIni.exists()){
        QStringList arguments({fiIni.absoluteFilePath()});
        QProcess::startDetached("Notepad", arguments);
    }
}
/****************************************************************************************************/

























