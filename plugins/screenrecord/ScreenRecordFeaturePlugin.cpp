/*
 * ScreenRecordFeaturePlugin.cpp - implementation of ScreenRecordFeaturePlugin class
 *
 * Copyright (c) 2017-2019 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QCoreApplication>

#include "ScreenRecordFeaturePlugin.h"
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

ScreenRecordFeaturePlugin::ScreenRecordFeaturePlugin( QObject* parent ) :
    QObject( parent ),
    m_screenRecordFeature( Feature::Mode | Feature::AllComponents,
                         Feature::Uid( "3b48aa3b-62aa-4260-a959-a69856b11385" ),
                         Feature::Uid(),
                         tr( "开始录屏" ), tr( "结束录屏" ),
                         tr( "点击本按钮开始录制被控端的电脑屏幕，再次点击它会停止视频录制并保存文件到被控端硬盘上." ),
                         QStringLiteral(":/screenrecord/system-lock-screen.png") ),
    m_features( { m_screenRecordFeature } ),
    mTranscodingProcess(nullptr)
{
}



ScreenRecordFeaturePlugin::~ScreenRecordFeaturePlugin()
{
    delete mTranscodingProcess;
}

void ScreenRecordFeaturePlugin::readyReadStandardOutput()
{

}

void ScreenRecordFeaturePlugin::encodingFinished()
{
    this->processEnded();
}

void ScreenRecordFeaturePlugin::endRecordingAndClose()
{
    this->stopRecording();
}

bool ScreenRecordFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
                                            const ComputerControlInterfaceList& computerControlInterfaces )
{
    Q_UNUSED(master);

    if( feature == m_screenRecordFeature )
    {
        return sendFeatureMessage( FeatureMessage( m_screenRecordFeature.uid(), StartRecordCommand ),
                                   computerControlInterfaces );
    }

    return false;
}



bool ScreenRecordFeaturePlugin::stopFeature( VeyonMasterInterface& master, const Feature& feature,
                                           const ComputerControlInterfaceList& computerControlInterfaces )
{
    Q_UNUSED(master);

    if( feature == m_screenRecordFeature )
    {
        return sendFeatureMessage( FeatureMessage( m_screenRecordFeature.uid(), StopRecordCommand ),
                                   computerControlInterfaces );
    }

    return false;
}



bool ScreenRecordFeaturePlugin::handleFeatureMessage( VeyonMasterInterface& master, const FeatureMessage& message,
                                                    ComputerControlInterface::Pointer computerControlInterface )
{
    Q_UNUSED(master);
    Q_UNUSED(message);
    Q_UNUSED(computerControlInterface);

    return false;
}



bool ScreenRecordFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
                                                    const MessageContext& messageContext,
                                                    const FeatureMessage& message )
{
    Q_UNUSED(messageContext)

    if( m_screenRecordFeature.uid() == message.featureUid() )
    {
        if( server.featureWorkerManager().isWorkerRunning( m_screenRecordFeature ) == false &&
                message.command() != StopRecordCommand )
        {
            server.featureWorkerManager().startWorker( m_screenRecordFeature, FeatureWorkerManager::ManagedSystemProcess );
        }

        // forward message to worker
        server.featureWorkerManager().sendMessage( message );

        return true;
    }

    return false;
}



bool ScreenRecordFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
    Q_UNUSED(worker);

    if( m_screenRecordFeature.uid() == message.featureUid() )
    {
        switch( message.command() )
        {
        case StartRecordCommand:
            if( mTranscodingProcess == nullptr )
            {
                mTranscodingProcess = new QProcess(this);
                this->recording = false; //initialize the flag that considers the recording phase
                this->outputFile.clear();
                connect(mTranscodingProcess, SIGNAL(started()), this, SLOT(processStarted()));
                connect(mTranscodingProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
                connect(mTranscodingProcess, SIGNAL(finished(int)), this, SLOT(encodingFinished()));
                this->startRecording();
            }

            return true;

        case StopRecordCommand:

            this->stopRecording();

            delete mTranscodingProcess;
            mTranscodingProcess = nullptr;
            QCoreApplication::quit();

            return true;

        default:
            break;
        }
    }

    return false;
}

void ScreenRecordFeaturePlugin::processStarted()
{
    qDebug() << "processStarted()";
}

void ScreenRecordFeaturePlugin::processEnded()
{
    qDebug() << "processEnded()";
}

void ScreenRecordFeaturePlugin::stopRecording()
{
    if(this->recording)
    {
        mTranscodingProcess->write("q");
        mTranscodingProcess->waitForFinished(-1);
        this->stopUI();
    }
}

void ScreenRecordFeaturePlugin::stopUI()
{
    this->recording = false;
}

void ScreenRecordFeaturePlugin::startRecording()
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
                  << QStringLiteral("-q:v") << QStringLiteral("0.01")
                  << this->outputFile;

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
