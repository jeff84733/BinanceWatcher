#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>



typedef struct _PAIR_PROPERTY_NAME {
    double dLastPrice;
    double dPriceChangePercent;
    double dHighPrice;
    double dLowPrice;
    double dWeightedAvgPrice;
    double dQuoteVolume;
    double dObstacle;
} PAIR_PROPERTY_NAME;

typedef union _PAIR_PROPERTY {
    PAIR_PROPERTY_NAME name;
    double id[7];
} PAIR_PROPERTY;

typedef struct _PAIR_INFO
{
    int id;
    QString szSymbol;
    QString szBaseAsset;
    QString szQuoteAsset;
    PAIR_PROPERTY propertys;
} PAIR_INFO;



class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent=nullptr);
    ~Widget();

    QTableWidget* page;
    QList<PAIR_INFO> pairInfos;
    QList<const PAIR_INFO*> pairRanks;
    QTimer tmrAutoRefresh;
    QTimer tmrRefreshPairInfos;
    QCheckBox* uiAutoRefresh;
    QComboBox* uiSortType;

public slots:
    void getApi_EXCHANGEINFO();
    void getApi_PRICE();
    void getApi_PRICE24HR();
    void onApi_EXCHANGEINFO(QNetworkReply* reply);
    void onApi_PRICE(QNetworkReply* reply);
    void onApi_PRICE24HR(QNetworkReply* reply);
    void refreshPairRanks();
    void onUiAutoRefreshstateChanged(int state);
    void onUiSortTypeCurrentIndexChanged(int index);
    void refreshUi();
    void editConfig();
    void requestFinished(QNetworkReply* reply);

protected:
    void customEvent(QEvent* event);
    void closeEvent(QCloseEvent* event);

private:
    QNetworkAccessManager* nam;
};



























#endif // WIDGET_H
