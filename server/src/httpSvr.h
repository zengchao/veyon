#pragma once
#include <QCoreApplication>
#include <QNetworkInterface>
#include <iostream>
#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDebug>

#define PORT 61301 // TCP 端口1

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

private:
    QTcpServer *server;
    int _port;
signals:
};
