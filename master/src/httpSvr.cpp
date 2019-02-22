#include "httpSvr.h"
#include <QFile>
#include <QChar>
using namespace std;

httpSvr::httpSvr(QObject *parent) : QObject(parent)
{
    socket = 0; // 客户端socket
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()),this, SLOT(myConnection()));
    qDebug()<< "httpSvr start listen, port:"<< PORT;
    if(server->listen(QHostAddress::Any, PORT)) {
        qDebug()<<"success";
        } else {
        qDebug()<<"failed";
    }
}

int httpSvr::port() {
    return PORT;
}

void httpSvr::readMessage()
{
     QByteArray barr;

     QString readmsg = QString::fromUtf8(socket->readAll());
     QStringList msgList = readmsg.split(QStringLiteral("\n"));
     QString getURL = msgList.first().toUpper();
     getURL.replace(QStringLiteral("GET /"), QStringLiteral(""));
     getURL.replace(QStringLiteral(" HTTP/1.1"), QStringLiteral(""));
     getURL.replace(QStringLiteral("\r"), QStringLiteral("")).replace(QStringLiteral("\n"), QStringLiteral(""));
     QString paras = QString::fromUtf8(QByteArray::fromPercentEncoding(getURL.toUtf8())).toLower();

     if (paras.compare(QString::fromUtf8("client"))==0)
     {
        QByteArray ba;
        QString getPath;
#ifdef Q_OS_LINUX

        getPath = QStringLiteral("\/record\/host.json");
#else
        getPath = QStringLiteral("c:\\record\\host.json");
#endif

        QFile f(getPath);
        if(!f.open(QIODevice::ReadOnly | QIODevice::Unbuffered))  {
            barr = "file not found";
        }else{
            barr = f.readAll();
        }
        f.close();

     }else{
        barr = "invalid parameters";
     }
     QString lens(QString::number( barr.length()));
     socket->write("HTTP/1.1 200 OK\r\n");
     socket->write("Content-Type: text/html\r\n");
     socket->write("Accept-Ranges: bytes\r\n");
     QString sLen(QStringLiteral("Content-Length: ")+lens+QStringLiteral("\r\n\r\n"));
     socket->write(sLen.toLatin1());
     socket->write(barr);
     socket->flush();
     connect(socket, SIGNAL(disconnected()),socket, SLOT(deleteLater()));
     socket->disconnectFromHost();
}

void httpSvr::myConnection()
{
    qInfo( "myConnection" );
    socket = server->nextPendingConnection();
    if (socket) {
        connect(socket, SIGNAL(disconnected()),socket, SLOT(deleteLater()));
        connect(socket,SIGNAL(readyRead()),this,SLOT(readMessage())); //有可读的信息，触发读函数槽
    }
}

httpSvr::~httpSvr()
{
    if (socket)
        socket->close();
}
