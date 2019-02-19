/*
 * VeyonConnection.cpp - implementation of VeyonConnection
 *
 * Copyright (c) 2008-2019 Tobias Junghans <tobydox@veyon.io>
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

#include "rfb/rfbclient.h"

#include "AuthenticationCredentials.h"
#include "CryptoCore.h"
#include "PlatformUserFunctions.h"
#include "SocketDevice.h"
#include "VariantArrayMessage.h"
#include "VeyonConfiguration.h"
#include "VeyonConnection.h"
#include "VncFeatureMessageEvent.h"


static rfbClientProtocolExtension* __veyonProtocolExt = nullptr;
static const uint32_t __veyonSecurityTypes[2] = { rfbSecTypeVeyon, 0 };


rfbBool handleVeyonMessage( rfbClient* client, rfbServerToClientMsg* msg )
{
	auto connection = reinterpret_cast<VeyonConnection *>( VncConnection::clientData( client, VeyonConnection::VeyonConnectionTag ) );
	if( connection )
	{
		return connection->handleServerMessage( client, msg->type );
	}

	return false;
}



VeyonConnection::VeyonConnection( VncConnection* vncConnection ):
	m_vncConnection( vncConnection ),
	m_veyonAuthType( RfbVeyonAuth::Logon ),
	m_user(),
	m_userHomeDir()
{
	if( __veyonProtocolExt == nullptr )
	{
		__veyonProtocolExt = new rfbClientProtocolExtension;
		__veyonProtocolExt->encodings = nullptr;
		__veyonProtocolExt->handleEncoding = nullptr;
		__veyonProtocolExt->handleMessage = handleVeyonMessage;
		__veyonProtocolExt->securityTypes = __veyonSecurityTypes;
		__veyonProtocolExt->handleAuthentication = handleSecTypeVeyon;

		rfbClientRegisterExtension( __veyonProtocolExt );
	}

	if( VeyonCore::config().authenticationMethod() == VeyonCore::KeyFileAuthentication )
	{
		m_veyonAuthType = RfbVeyonAuth::KeyFile;
	}

	connect( m_vncConnection, &VncConnection::connectionPrepared, this, &VeyonConnection::registerConnection, Qt::DirectConnection );
}



VeyonConnection::~VeyonConnection()
{
	unregisterConnection();
}



void VeyonConnection::sendFeatureMessage( const FeatureMessage& featureMessage )
{
	if( m_vncConnection.isNull() )
	{
		qCritical( "VeyonConnection::sendFeatureMessage(): cannot call enqueueEvent as m_vncConnection is invalid" );
		return;
	}

	m_vncConnection->enqueueEvent( new VncFeatureMessageEvent( featureMessage ) );
}



bool VeyonConnection::handleServerMessage( rfbClient* client, uint8_t msg )
{
	if( msg == FeatureMessage::RfbMessageType )
	{
		SocketDevice socketDev( VncConnection::libvncClientDispatcher, client );
		FeatureMessage featureMessage;
		if( featureMessage.receive( &socketDev ) == false )
		{
			vDebug( "VeyonConnection: could not receive feature message" );

			return false;
		}

		vDebug() << "VeyonConnection: received feature message"
				 << featureMessage.command()
				 << "with arguments" << featureMessage.arguments();

		emit featureMessageReceived( featureMessage );

		return true;
	}
	else
	{
		qCritical( "VeyonConnection::handleServerMessage(): "
				   "unknown message type %d from server. Closing "
				   "connection. Will re-open it later.", static_cast<int>( msg ) );
	}

	return false;
}



void VeyonConnection::registerConnection()
{
	if( m_vncConnection.isNull() == false )
	{
		m_vncConnection->setClientData( VeyonConnectionTag, this );
	}
}



void VeyonConnection::unregisterConnection()
{
	if( m_vncConnection.isNull() == false )
	{
		m_vncConnection->setClientData( VeyonConnectionTag, nullptr );
	}
}



int8_t VeyonConnection::handleSecTypeVeyon( rfbClient* client, uint32_t authScheme )
{
	if( authScheme != rfbSecTypeVeyon )
	{
		return FALSE;
	}

	hookPrepareAuthentication( client );

	auto connection = static_cast<VeyonConnection *>( VncConnection::clientData( client, VeyonConnectionTag ) );
	if( connection == nullptr )
	{
		return FALSE;
	}

	SocketDevice socketDevice( VncConnection::libvncClientDispatcher, client );
	VariantArrayMessage message( &socketDevice );
	message.receive();

	int authTypeCount = message.read().toInt();

	QList<RfbVeyonAuth::Type> authTypes;
	authTypes.reserve( authTypeCount );

	for( int i = 0; i < authTypeCount; ++i )
	{
#if QT_VERSION < 0x050600
#warning Building legacy compat code for unsupported version of Qt
		authTypes.append( static_cast<RfbVeyonAuth::Type>( message.read().toInt() ) );
#else
		authTypes.append( message.read().value<RfbVeyonAuth::Type>() );
#endif
	}

	vDebug() << Q_FUNC_INFO << QThread::currentThreadId() << "received authentication types:" << authTypes;

	RfbVeyonAuth::Type chosenAuthType = RfbVeyonAuth::Token;
	if( authTypes.count() > 0 )
	{
		chosenAuthType = authTypes.first();

		// look whether the VncConnection recommends a specific
		// authentication type (e.g. VeyonAuthHostBased when running as
		// demo client)

		for( auto authType : authTypes )
		{
			if( connection->veyonAuthType() == authType )
			{
				chosenAuthType = authType;
			}
		}
	}

	vDebug() << Q_FUNC_INFO << QThread::currentThreadId() << "chose authentication type:" << authTypes;

	VariantArrayMessage authReplyMessage( &socketDevice );

	authReplyMessage.write( chosenAuthType );

	// send username which is used when displaying an access confirm dialog
	if( VeyonCore::authenticationCredentials().hasCredentials( AuthenticationCredentials::UserLogon ) )
	{
		authReplyMessage.write( VeyonCore::authenticationCredentials().logonUsername() );
	}
	else
	{
		authReplyMessage.write( VeyonCore::platform().userFunctions().currentUser() );
	}

	authReplyMessage.send();

	VariantArrayMessage authAckMessage( &socketDevice );
	authAckMessage.receive();

	switch( chosenAuthType )
	{
	case RfbVeyonAuth::KeyFile:
		if( VeyonCore::authenticationCredentials().hasCredentials( AuthenticationCredentials::PrivateKey ) )
		{
			VariantArrayMessage challengeReceiveMessage( &socketDevice );
			challengeReceiveMessage.receive();
			const auto challenge = challengeReceiveMessage.read().toByteArray();

			if( challenge.size() != CryptoCore::ChallengeSize )
			{
				qCritical() << Q_FUNC_INFO << QThread::currentThreadId() << "challenge size mismatch!";
				return FALSE;
			}

			// create local copy of private key so we can modify it within our own thread
			auto key = VeyonCore::authenticationCredentials().privateKey();
			if( key.isNull() || key.canSign() == false )
			{
				qCritical() << Q_FUNC_INFO << QThread::currentThreadId() << "invalid private key!";
				return FALSE;
			}

			const auto signature = key.signMessage( challenge, CryptoCore::DefaultSignatureAlgorithm );

			VariantArrayMessage challengeResponseMessage( &socketDevice );
			challengeResponseMessage.write( VeyonCore::instance()->authenticationKeyName() );
			challengeResponseMessage.write( signature );
			challengeResponseMessage.send();
		}
		break;

	case RfbVeyonAuth::HostWhiteList:
		// nothing to do - we just get accepted because the host white list contains our IP
		break;

	case RfbVeyonAuth::Logon:
	{
		VariantArrayMessage publicKeyMessage( &socketDevice );
		publicKeyMessage.receive();

		CryptoCore::PublicKey publicKey = CryptoCore::PublicKey::fromPEM( publicKeyMessage.read().toString() );

		if( publicKey.canEncrypt() == false )
		{
			qCritical() << Q_FUNC_INFO << QThread::currentThreadId() << "can't encrypt with given public key!";
			return FALSE;
		}

		CryptoCore::SecureArray plainTextPassword( VeyonCore::authenticationCredentials().logonPassword().toUtf8() );
		CryptoCore::SecureArray encryptedPassword = publicKey.encrypt( plainTextPassword, CryptoCore::DefaultEncryptionAlgorithm );
		if( encryptedPassword.isEmpty() )
		{
			qCritical() << Q_FUNC_INFO << QThread::currentThreadId() << "password encryption failed!";
			return FALSE;
		}

		VariantArrayMessage passwordResponse( &socketDevice );
		passwordResponse.write( encryptedPassword.toByteArray() );
		passwordResponse.send();
		break;
	}

	case RfbVeyonAuth::Token:
	{
		VariantArrayMessage tokenAuthMessage( &socketDevice );
		tokenAuthMessage.write( VeyonCore::authenticationCredentials().token() );
		tokenAuthMessage.send();
		break;
	}

	default:
		// nothing to do - we just get accepted
		break;
	}

	return TRUE;
}



void VeyonConnection::hookPrepareAuthentication( rfbClient* client )
{
	auto connection = static_cast<VncConnection *>( VncConnection::clientData( client, VncConnection::VncConnectionTag ) );
	if( connection )
	{
		// set our internal flag which indicates that we basically have communication with the client
		// which means that the host is reachable
		connection->setServerReachable();
	}
}
