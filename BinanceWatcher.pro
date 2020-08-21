QT       += core gui widgets network concurrent

TARGET = BinanceWatcher
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
RC_FILE = res/ms_resource.rc

SOURCES += main.cpp\
        widget.cpp

HEADERS  += widget.h
