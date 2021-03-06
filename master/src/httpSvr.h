#pragma once
#include <QCoreApplication>
#include <QNetworkInterface>
#include <iostream>
#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDebug>
#include <QFileInfoList>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#define PORT 60000 // TCP 端口1
#ifdef Q_OS_LINUX
    #define SCREEN_RECORD_PATH "\/record"
#else
    #define SCREEN_RECORD_PATH "c:\\record"
#endif

class httpSvr : public QObject
{
    Q_OBJECT
public:
    explicit httpSvr(QObject *parent = 0);
    ~httpSvr();
    QTcpSocket *socket;
    int port();
public slots:
    void myConnection();
    void readMessage();
    QString getFileInfoList(QFileInfoList list);
    QString getFilesInDir(QDir dir);
private:
    QTcpServer *server;
    int _port;
signals:
};
