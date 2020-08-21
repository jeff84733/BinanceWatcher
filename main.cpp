#include "widget.h"
#include <QApplication>



extern void InitApp();



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    InitApp();
    Widget w;
    w.show();

    return a.exec();
}
