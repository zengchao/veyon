/*
 * ScreenRecordFeaturePlugin.h - declaration of ScreenRecordFeaturePlugin class
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

#pragma once

#include "FeatureProviderInterface.h"
#include <QProcess>

class LockWidget;

class ScreenRecordFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.ScreenRecord")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	ScreenRecordFeaturePlugin( QObject* parent = nullptr );
	~ScreenRecordFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
        return QStringLiteral("7c7fc1a7-ad24-4310-81dd-86c4a266add9");
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 1 );
	}

	QString name() const override
	{
		return QStringLiteral("ScreenRecord");
	}

	QString description() const override
	{
		return tr( "Lock screen and input devices of a computer" );
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon Community");
	}

	QString copyright() const override
	{
		return QStringLiteral("Tobias Junghans");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool stopFeature( VeyonMasterInterface& master, const Feature& feature,
					  const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonMasterInterface& master, const FeatureMessage& message,
							   ComputerControlInterface::Pointer computerControlInterface ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

private:
	enum Commands
	{
		StartRecordCommand,
		StopRecordCommand,
		CommandCount
	};

	const Feature m_screenRecordFeature;
	const FeatureList m_features;

    //LockWidget* m_lockWidget;

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

};
