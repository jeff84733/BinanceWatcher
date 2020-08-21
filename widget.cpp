#include "widget.h"

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
    PRICE,
    PRICE24HR,
    MAX
};



QString PairPropertyNames[6] = {
    "lastPrice", //最新
    "priceChangePercent", //漲跌
    "highPrice", //最高
    "lowPrice", //最低
    "weightedAvgPrice", //加權平均
    "quoteVolume" //成交量
};
qreal RecommendScalingRate;
QStringList PairNames;
QFont MyFont;
QMap<int, QString> ApiUrls;



void InitApp()
{
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    if(QFileInfo::exists(INI)) {
        QSettings ini(INI, QSettings::IniFormat);
        ini.setIniCodec("UTF-8");
        ini.beginGroup("pair");
        QStringList allkeys = ini.allKeys();
        for(int i=0; i<allkeys.count(); i++)
            if(allkeys.contains(QString::number(i)))
                PairNames.append(ini.value(QString::number(i)).toString());
        ini.endGroup();
    }
    QSize userSize  = QApplication::desktop()->screenGeometry().size();
    RecommendScalingRate = qMax(userSize.width() /1920.0, userSize.height() /1080.0);
    //MyFont.setFamily("consolas");
    int fontId = QFontDatabase::addApplicationFont("./font.ttc");
    MyFont.setFamily(QFontDatabase::applicationFontFamilies(fontId).at(0));
    MyFont.setPixelSize(SCALE(16));

    ApiUrls[ApiId::PRICE] = API"/api/v3/ticker/price";
    ApiUrls[ApiId::PRICE24HR] = API"/api/v3/ticker/24hr";
}



/****************************************************************************************************/
Widget::Widget(QWidget *parent) :
    QWidget(parent)
{
    setFont(MyFont);
    nam = new QNetworkAccessManager(this);
    QObject::connect(nam, &QNetworkAccessManager::finished, this, &Widget::requestFinished);

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

    QVBoxLayout *loV100 = new QVBoxLayout(this);
    loV100->addWidget(sa);

    page->setEditTriggers(QAbstractItemView::NoEditTriggers);
    page->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    page->horizontalHeader()->setStretchLastSection(false);
    page->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    page->setColumnCount(6);
    page->setHorizontalHeaderLabels({MSG(L"最新"), MSG(L"漲跌"), MSG(L"最高"), MSG(L"最低"), MSG(L"加權平均"), MSG(L"成交量")});
    page->verticalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    page->verticalHeader()->setStretchLastSection(false);
    page->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    page->setRowCount(PairNames.count());
    page->setVerticalHeaderLabels(PairNames);
    for(int i=0; i<PairNames.count(); i++) {
        for(int j=0; j<6; j++) {
            QTableWidgetItem* bar = new QTableWidgetItem;
            bar->setTextAlignment(Qt::AlignRight | Qt::AlignTop);
            page->setItem(i, j, bar);
        }
    }

    QPushButton* uiEdit = new QPushButton("Edit");
    QObject::connect(uiEdit, &QPushButton::clicked, this, &Widget::editConfig);
    QPushButton* uiRefresh = new QPushButton("Refresh");
    QObject::connect(uiRefresh, &QPushButton::clicked, this, &Widget::getApi_PRICE24HR);
    QHBoxLayout *loH100 = new QHBoxLayout;
    loH100->addWidget(uiEdit);
    loH100->addStretch();
    loH100->addWidget(uiRefresh);
    loV100->addLayout(loH100);
}
/****************************************************************************************************/
Widget::~Widget()
{
}
/****************************************************************************************************/
void Widget::requestFinished(QNetworkReply* reply) {
    bool bIsOk = (QNetworkReply::NoError == reply->error());
    if(bIsOk) {
        QVariant apiId = reply->property("ApiId");
        if(!apiId.isNull()) {
            switch(apiId.toInt()) {
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
    for(int i=0; i<PairNames.count(); i++) {
        QNetworkRequest request;
        request.setUrl(QUrl(ApiUrls[ApiId::PRICE24HR] +"?symbol=" +PairNames[i]));
        QNetworkReply* reply = nam->get(request);
        reply->setProperty("ApiId", ApiId::PRICE24HR);
        reply->setProperty("PairId", i);
    }
}
/****************************************************************************************************/
void Widget::onApi_PRICE(QNetworkReply* reply)
{
    QVariant symbol = reply->property("PairId");
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
        QJsonParseError e;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll(), &e);
        if(e.error == QJsonParseError::NoError && !jsonDoc.isNull()) {
            QJsonObject jsonObject = jsonDoc.object();
            QJsonValue jsonValue;
            for(int i=0; i<sizeof(PairPropertyNames) /sizeof(QString); i++) {
                QJsonValue jsonValue = jsonObject.value(PairPropertyNames[i]);
                if(!jsonValue.isNull()) {
                    QString szValue = jsonValue.toString();
                    double dValue = szValue.toDouble();
                    QTableWidgetItem* item = page->item(symbol.toInt(), i);
                    if(PairPropertyNames[i] == "priceChangePercent") {
                        if(dValue > 0) {
                            item->setForeground(QBrush(QColor(0x00, 0x80, 0x00)));
                            item->setText(" +" +QString::asprintf("%.2f", dValue) +"%");
                        }
                        else {
                            item->setForeground(QBrush(QColor(0xFF, 0x00, 0x00)));
                            item->setText(" " +QString::asprintf("%.2f", dValue) +"%");
                        }
                    }
                    else {
                        if(dValue >= 1)
                            szValue = QString::asprintf("%.2f", dValue);
                        item->setText(" " +szValue);
                    }
                }
            }
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

























