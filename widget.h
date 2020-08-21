#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QLabel>

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

    QTableWidget* page;

public slots:
    void getApi_PRICE();
    void getApi_PRICE24HR();
    void onApi_PRICE(QNetworkReply* reply);
    void onApi_PRICE24HR(QNetworkReply* reply);
    void editConfig();
    void requestFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* nam;
};

#endif // WIDGET_H
