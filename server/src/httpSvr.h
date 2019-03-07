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
#include <QProcess>

#define PORT 60000 // TCP 端口1

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

    QProcess *mTranscodingProcess;

    QString mOutputString;

    QString outputFile;

    bool recording;
private slots:
    void startRecording();
    void processStarted();
    void processEnded();
    void stopRecording();
    void stopUI();
    void readyReadStandardOutput();
    void encodingFinished();
    void endRecordingAndClose();
signals:
};
