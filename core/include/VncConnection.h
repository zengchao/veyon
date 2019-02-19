/*
 * VncConnection.h - declaration of VncConnection class
 *
 * Copyright (c) 2008-2019 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * code partly taken from KRDC / vncclientthread.h:
 * Copyright (C) 2007-2008 Urs Wolfer <uwolfer @ kde.org>
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

#include <QMutex>
#include <QQueue>
#include <QReadWriteLock>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>
#include <QImage>

#include "VeyonCore.h"
#include "SocketDevice.h"

typedef struct _rfbClient rfbClient;

class VncEvent;

class VEYON_CORE_EXPORT VncConnection : public QThread
{
	Q_OBJECT
public:
	enum class Quality
	{
		Thumbnail,
		Screenshot,
		RemoteControl,
		Default
	} ;

	enum class FramebufferState
	{
		Invalid,
		Initialized,
		Valid
	} ;

	enum class State
	{
		None,
		Disconnected,
		Connecting,
		HostOffline,
		ServiceUnreachable,
		AuthenticationFailed,
		ConnectionFailed,
		Connected
	} ;
	Q_ENUM(State)

	explicit VncConnection( QObject *parent = nullptr );
	~VncConnection() override;

	static void initLogging( bool debug );

	QImage image() const;

	void restart();
	void stop();
	void stopAndDeleteLater();

	void setHost( const QString& host );
	void setPort( int port );

	State state() const
	{
		return m_state;
	}

	bool isConnected() const
	{
		return state() == State::Connected && isRunning();
	}

	const QString& host() const
	{
		return m_host;
	}

	void setQuality( Quality quality )
	{
		m_quality = quality ;
	}

	Quality quality() const
	{
		return m_quality;
	}

	void setServerReachable();

	void enqueueEvent( VncEvent* event );
	bool isEventQueueEmpty();

	QSize framebufferSize() const
	{
		return m_image.size();
	}

	/** \brief Returns whether framebuffer data is valid, i.e. at least one full FB update received */
	bool hasValidFrameBuffer() const
	{
		return m_framebufferState == FramebufferState::Valid;
	}

	void setScaledSize( QSize s )
	{
		if( m_scaledSize != s )
		{
			m_scaledSize = s;
			setControlFlag( ControlFlag::ScaledScreenNeedsUpdate, true );
		}
	}

	QImage scaledScreen()
	{
		rescaleScreen();
		return m_scaledScreen;
	}

	void setFramebufferUpdateInterval( int interval );

	void rescaleScreen();

	static constexpr int VncConnectionTag = 0x590123;

	static void* clientData( rfbClient* client, int tag );
	void setClientData( int tag, void* data );

	static qint64 libvncClientDispatcher( char * buffer, const qint64 bytes,
										  SocketDevice::SocketOperation operation, void * user );

	void mouseEvent( int x, int y, int buttonMask );
	void keyEvent( unsigned int key, bool pressed );
	void clientCut( const QString& text );

signals:
	void connectionPrepared();
	void connectionEstablished();
	void imageUpdated( int x, int y, int w, int h );
	void framebufferUpdateComplete();
	void framebufferSizeChanged( int w, int h );
	void cursorPosChanged( int x, int y );
	void cursorShapeUpdated( const QPixmap& cursorShape, int xh, int yh );
	void gotCut( const QString& text );
	void stateChanged();

protected:
	void run() override;

private:
	// intervals and timeouts
	static const int ThreadTerminationTimeout = 30000;
	static const int ConnectionRetryInterval = 1000;
	static const int MessageWaitTimeout = 500;
	static const int SocketKeepaliveIdleTime = 1000;
	static const int SocketKeepaliveInterval = 500;
	static const int SocketKeepaliveCount = 5;

	// RFB parameters
	static const int RfbBitsPerSample = 8;
	static const int RfbSamplesPerPixel = 3;
	static const int RfbBytesPerPixel = 4;

	enum class ControlFlag {
		ScaledScreenNeedsUpdate = 0x01,
		ServerReachable = 0x02,
		TerminateThread = 0x04,
		RestartConnection = 0x08,
	};

	void establishConnection();
	void handleConnection();
	void closeConnection();

	void setState( State state );

	void setControlFlag( ControlFlag flag, bool on );
	bool isControlFlagSet( ControlFlag flag );

	bool initFrameBuffer( rfbClient* client );
	void finishFrameBufferUpdate();

	void sendEvents();

	// hooks for LibVNCClient
	static int8_t hookInitFrameBuffer( rfbClient* client );
	static void hookUpdateFB( rfbClient* client, int x, int y, int w, int h );
	static void hookFinishFrameBufferUpdate( rfbClient* client );
	static int8_t hookHandleCursorPos( rfbClient* client, int x, int y );
	static void hookCursorShape( rfbClient* client, int xh, int yh, int w, int h, int bpp );
	static void hookCutText( rfbClient* client, const char *text, int textlen );
	static void rfbClientLogDebug( const char* format, ... );
	static void rfbClientLogNone( const char* format, ... );
	static void framebufferCleanup( void* framebuffer );

	// states and flags
	std::atomic<State> m_state;
	std::atomic<FramebufferState> m_framebufferState;
	QAtomicInt m_controlFlags;

	// connection parameters and data
	rfbClient* m_client;
	Quality m_quality;
	QString m_host;
	int m_port;

	// thread and timing control
	QMutex m_globalMutex;
	QMutex m_eventQueueMutex;
	QWaitCondition m_updateIntervalSleeper;
	int m_framebufferUpdateInterval;

	// queue for RFB and custom events
	QQueue<VncEvent *> m_eventQueue;

	// framebuffer data and thread synchronization objects
	QImage m_image;
	QImage m_scaledScreen;
	QSize m_scaledSize;
	mutable QReadWriteLock m_imgLock;

} ;
