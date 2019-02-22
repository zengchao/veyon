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
     QString type = QStringLiteral("text/html");
     QString readmsg = QString::fromUtf8(socket->readAll());
     QStringList msgList = readmsg.split(QStringLiteral("\n"));
     QString getURL = msgList.first().toUpper();
     getURL.replace(QStringLiteral("GET /"), QStringLiteral(""));
     getURL.replace(QStringLiteral(" HTTP/1.1"), QStringLiteral(""));
     getURL.replace(QStringLiteral("\r"), QStringLiteral("")).replace(QStringLiteral("\n"), QStringLiteral(""));
     QString paras = QString::fromUtf8(QByteArray::fromPercentEncoding(getURL.toUtf8())).toLower();

     if (paras.compare(QString::fromUtf8("list"))==0)
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
            f.close();
         }
     }else if (paras.compare(QString::fromUtf8("file"))==0) {
         //screen recording files in the client machine
         //read all video files in the directory,output them--json
         QString path;
#ifdef Q_OS_LINUX
         path = QStringLiteral("\/record");
#else
         path = QStringLiteral("c:\\record");
#endif
         QString tmp = getFilesInDir(path);
         barr = tmp.toUtf8();
     }else if (paras.endsWith(QStringLiteral(".mp4"),Qt::CaseInsensitive)
               || paras.endsWith(QStringLiteral(".avi"),Qt::CaseInsensitive)
               || paras.endsWith(QStringLiteral(".mpg"),Qt::CaseInsensitive)
               || paras.endsWith(QStringLiteral(".mpeg"),Qt::CaseInsensitive)) {
         // download video file
         QByteArray ba;
         QString getPath;
#ifdef Q_OS_LINUX
         getPath = QStringLiteral("\/record\/")+paras;
#else
         getPath = QStringLiteral("c:\\record\\")+paras;
#endif
         QFile f(getPath);
         if(!f.open(QIODevice::ReadOnly | QIODevice::Unbuffered))  {
            barr = "file not found";
         }else{
            barr = f.readAll();
            f.close();
            type=QStringLiteral("application/mp4");
         }
     }else{
         barr = "invalid parameters";
     }
     QString lens(QString::number( barr.length()));
     socket->write("HTTP/1.1 200 OK\r\n");
     QString typeStr(QStringLiteral("Content-Type: ")+type+QStringLiteral("\r\n"));
     socket->write(typeStr.toLatin1());
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

QString httpSvr::getFileInfoList(QFileInfoList list)
{
    QJsonArray jsonArray;
    int p=0;
    for (unsigned int i = 0; i < list.count(); ++i)
    {
        QFileInfo tmpFileInfo = list.at(i);
        QString filename;
        if (!tmpFileInfo.isDir())
        {
            filename = tmpFileInfo.fileName();
            if (filename.endsWith(QStringLiteral(".mp4"),Qt::CaseInsensitive)==true)
            {
                jsonArray.insert(p, filename);
                p=p+1;
            }
            if (filename.endsWith(QStringLiteral(".avi"),Qt::CaseInsensitive)==true)
            {
                jsonArray.insert(p, filename);
                p=p+1;
            }
            if (filename.endsWith(QStringLiteral(".mpg"),Qt::CaseInsensitive)==true)
            {
                jsonArray.insert(p, filename);
                p=p+1;
            }
            if (filename.endsWith(QStringLiteral(".mpeg"),Qt::CaseInsensitive)==true)
            {
                jsonArray.insert(p, filename);
                p=p+1;
            }
        }
    }
    if (p==0) {
       return QStringLiteral("[]");
    }
    QJsonDocument document;
    document.setArray(jsonArray);
    QByteArray byte_array = document.toJson(QJsonDocument::Compact);
    QString json_str = QString::fromUtf8(byte_array);

    return json_str;
}

QString httpSvr::getFilesInDir(QDir dir)
{
    QStringList string;
    string << QStringLiteral("*");
    QFileInfoList list = dir.entryInfoList(string, QDir::AllEntries, QDir::DirsFirst);
    return getFileInfoList(list);
}
