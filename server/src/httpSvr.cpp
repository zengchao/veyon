#include "httpSvr.h"
#include <QFile>
#include <QChar>

#include <QCoreApplication>
#include "FeatureWorkerManager.h"
#include "LockWidget.h"
#include "PlatformCoreFunctions.h"
#include "VeyonServerInterface.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QMessageBox>
#include <QPainter>

#include "VeyonConfiguration.h"
#include "Filesystem.h"
#include "ComputerControlInterface.h"

using namespace std;

httpSvr::httpSvr(QObject *parent) : QObject(parent),
    mTranscodingProcess(nullptr),
    rtspServerUrl(QStringLiteral("")),
    recording(false)
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
     }else if (paras.compare(QString::fromUtf8("startrecording"))==0) {

         this->rtspServerUrl = QStringLiteral("");
         if(this->recording==false) {
             if( mTranscodingProcess == nullptr )
             {
                 mTranscodingProcess = new QProcess(this);
                 this->recording = false; //initialize the flag that considers the recording phase
                 this->outputFile.clear();
                 connect(mTranscodingProcess, SIGNAL(started()), this, SLOT(processStarted()));
                 connect(mTranscodingProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
                 connect(mTranscodingProcess, SIGNAL(finished(int)), this, SLOT(encodingFinished()));
                 this->startRecording();
             }else{
                 barr = "1004";//error:process exists
             }
         }else{
             barr = "1005";//error:It's recording
         }

     
     }else if (paras.startsWith(QString::fromUtf8("startrecording?url="),Qt::CaseInsensitive)==true) {
         //start recording
        paras.replace(QStringLiteral("startrecording"),QStringLiteral("")).replace(QStringLiteral("?url="),QStringLiteral(""));
        this->rtspServerUrl = QString::fromUtf8(QByteArray::fromPercentEncoding(paras.toUtf8())).toLower();
        /*
        if (paras.length()>0) {
            barr = QByteArray::fromPercentEncoding(paras.toUtf8());
        }*/

         if(this->recording==false) {
             if( mTranscodingProcess == nullptr )
             {
                 mTranscodingProcess = new QProcess(this);
                 this->recording = false; //initialize the flag that considers the recording phase
                 this->outputFile.clear();
                 connect(mTranscodingProcess, SIGNAL(started()), this, SLOT(processStarted()));
                 connect(mTranscodingProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
                 connect(mTranscodingProcess, SIGNAL(finished(int)), this, SLOT(encodingFinished()));
                 this->startRecording();
             }else{
                 barr = "1004";//error:process exists
             }
         }else{
             barr = "1005";//error:It's recording
         }
         
     }else if (paras.compare(QString::fromUtf8("stoprecording"))==0) {
         //stop recording
         this->stopRecording();

         delete mTranscodingProcess;
         mTranscodingProcess = nullptr;
         //QCoreApplication::quit();

         barr = "1002";//stopped successfully
     }else if (paras.compare(QString::fromUtf8("record_status"))==0) {
         if(this->recording) {
             barr = "1000";//record starting
         }else{
             barr = "1001";//record stopped
         }
     }else{
         barr = "1100";//invalid parameters
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
    delete mTranscodingProcess;
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

void httpSvr::startRecording()
{
    if(this->recording)
    {
        this->stopRecording();
    } else
    {

        QString program = QStringLiteral("ffmpeg");

        QStringList arguments;
        QString dir;

#ifdef Q_OS_LINUX
        dir = QStringLiteral("\/record");
#else
        dir = QStringLiteral("c:\\record");
#endif
        //VeyonCore::filesystem().expandPath(VeyonCore::config().screenRecordingDirectory());
        qDebug() << dir;
        if( VeyonCore::filesystem().ensurePathExists( dir ) == false )
        {
            const auto msg = tr( "Could not take a screenRecording as directory %1 doesn't exist and couldn't be created." ).arg( dir );
            qCritical() << msg.toUtf8().constData();
            if( qobject_cast<QApplication *>( QCoreApplication::instance() ) )
            {
                QMessageBox::critical( nullptr, tr( "ScreenRecording" ), msg );
            }
            return;
        }

        QString m_fileName =  QString( QStringLiteral( "%1_%2.mp4" ) ).arg(
                            QDate( QDate::currentDate() ).toString( Qt::ISODate ),
                            QTime( QTime::currentTime() ).toString( Qt::ISODate ) ).
                        replace( QLatin1Char(':'), QLatin1Char('-') );
#ifdef Q_OS_LINUX
        this->outputFile = dir + QStringLiteral("\/") + m_fileName;
#else
        this->outputFile = dir + QStringLiteral("\\") + m_fileName;
#endif
        arguments << QStringLiteral("-f") << QStringLiteral("gdigrab") << QStringLiteral("-i") << QStringLiteral("desktop")
                  << QStringLiteral("-r") << QStringLiteral("15") << QStringLiteral("-b:v") << QStringLiteral("200k")
                  << QStringLiteral("-q:v") << QStringLiteral("0.01") << this->outputFile;
        if (this->rtspServerUrl.length()>0) {
           arguments << QStringLiteral("-f") << QStringLiteral("flv") << this->rtspServerUrl;
        }

        qDebug() << arguments;
        if (QFile::exists(this->outputFile))
        {
            QFile::remove(this->outputFile);
            while(QFile::exists(this->outputFile)) {
               qDebug() << "output file still there";
            }
        }

        mTranscodingProcess->setProcessChannelMode(QProcess::MergedChannels);
        mTranscodingProcess->start(program, arguments);
        this->recording= true;
    }
}

void httpSvr::processStarted()
{
    qDebug() << "processStarted()";
}

void httpSvr::processEnded()
{
    qDebug() << "processEnded()";
}

void httpSvr::stopRecording()
{
    if(this->recording)
    {
        mTranscodingProcess->write("q");
        mTranscodingProcess->waitForFinished(-1);
        this->stopUI();
    }
}

void httpSvr::stopUI()
{
    this->recording = false;
}

void httpSvr::readyReadStandardOutput()
{

}

void httpSvr::encodingFinished()
{
    this->processEnded();
}

void httpSvr::endRecordingAndClose()
{
    this->stopRecording();
}
