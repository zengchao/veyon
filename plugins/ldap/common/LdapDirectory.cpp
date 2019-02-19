/*
 * LdapDirectory.cpp - class representing the LDAP directory and providing access to directory entries
 *
 * Copyright (c) 2016-2019 Tobias Junghans <tobydox@veyon.io>
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

#include <QHostAddress>
#include <QHostInfo>

#include "LdapConfiguration.h"
#include "LdapDirectory.h"


LdapDirectory::LdapDirectory( const LdapConfiguration& configuration, QObject* parent ) :
	QObject( parent ),
	m_configuration( configuration ),
	m_client( configuration, QUrl(), this )
{
	m_usersDn = m_client.constructSubDn( m_configuration.userTree(), m_client.baseDn() );
	m_groupsDn = m_client.constructSubDn( m_configuration.groupTree(), m_client.baseDn() );
	m_computersDn = m_client.constructSubDn( m_configuration.computerTree(), m_client.baseDn() );

	if( m_configuration.computerGroupTree().isEmpty() == false )
	{
		m_computerGroupsDn = LdapClient::constructSubDn( m_configuration.computerGroupTree(), m_client.baseDn() );
	}
	else
	{
		m_computerGroupsDn.clear();
	}

	if( m_configuration.recursiveSearchOperations() )
	{
		m_defaultSearchScope = LdapClient::Scope::Sub;
	}
	else
	{
		m_defaultSearchScope = LdapClient::Scope::One;
	}

	m_userLoginAttribute = m_configuration.userLoginAttribute();
	m_groupMemberAttribute = m_configuration.groupMemberAttribute();
	m_computerDisplayNameAttribute = m_configuration.computerDisplayNameAttribute();
	m_computerHostNameAttribute = m_configuration.computerHostNameAttribute();
	m_computerHostNameAsFQDN = m_configuration.computerHostNameAsFQDN();
	m_computerMacAddressAttribute = m_configuration.computerMacAddressAttribute();
	m_locationNameAttribute = m_configuration.locationNameAttribute();
	if( m_locationNameAttribute.isEmpty() )
	{
		m_locationNameAttribute = QStringLiteral("cn");
	}

	m_usersFilter = m_configuration.usersFilter();
	m_userGroupsFilter = m_configuration.userGroupsFilter();
	m_computersFilter = m_configuration.computersFilter();
	m_computerGroupsFilter = m_configuration.computerGroupsFilter();
	m_computerContainersFilter = m_configuration.computerContainersFilter();

	m_identifyGroupMembersByNameAttribute = m_configuration.identifyGroupMembersByNameAttribute();

	m_computerLocationsByContainer = m_configuration.computerLocationsByContainer();
	m_computerLocationsByAttribute = m_configuration.computerLocationsByAttribute();
	m_computerLocationAttribute = m_configuration.computerLocationAttribute();

}



const QString& LdapDirectory::configInstanceId() const
{
	return m_configuration.instanceId();
}



/*!
 * \brief Disables any configured attributes which is required for some test scenarious
 */
void LdapDirectory::disableAttributes()
{
	m_userLoginAttribute.clear();
	m_computerDisplayNameAttribute.clear();
	m_computerHostNameAttribute.clear();
	m_computerMacAddressAttribute.clear();
}



/*!
 * \brief Disables any configured filters which is required for some test scenarious
 */
void LdapDirectory::disableFilters()
{
	m_usersFilter.clear();
	m_userGroupsFilter.clear();
	m_computersFilter.clear();
	m_computerGroupsFilter.clear();
	m_computerContainersFilter.clear();
}




QStringList LdapDirectory::users( const QString& filterValue )
{
	return m_client.queryDistinguishedNames( m_usersDn,
											 LdapClient::constructQueryFilter( m_userLoginAttribute, filterValue, m_usersFilter ),
											 m_defaultSearchScope );
}



QStringList LdapDirectory::groups( const QString& filterValue )
{
	return m_client.queryDistinguishedNames( m_groupsDn,
											 LdapClient::constructQueryFilter( QStringLiteral( "cn" ), filterValue ),
											 m_defaultSearchScope );
}



QStringList LdapDirectory::userGroups( const QString& filterValue )
{
	return m_client.queryDistinguishedNames( m_groupsDn,
											 LdapClient::constructQueryFilter( QStringLiteral( "cn" ), filterValue, m_userGroupsFilter ),
											 m_defaultSearchScope );
}



QStringList LdapDirectory::computersByDisplayName( const QString& filterValue )
{
	return m_client.queryDistinguishedNames( m_computersDn,
											 LdapClient::constructQueryFilter( m_computerDisplayNameAttribute, filterValue, m_computersFilter ),
											 m_defaultSearchScope );
}



/*!
 * \brief Returns list of computer object names matching the given hostname filter
 * \param filterValue A filter value which is used to query the host name attribute
 * \return List of DNs of all matching computer objects
 */
QStringList LdapDirectory::computersByHostName( const QString& filterValue )
{
	return m_client.queryDistinguishedNames( m_computersDn,
											 LdapClient::constructQueryFilter( m_computerHostNameAttribute, filterValue, m_computersFilter ),
											 m_defaultSearchScope );
}



QStringList LdapDirectory::computerGroups( const QString& filterValue )
{
	return m_client.queryDistinguishedNames( m_computerGroupsDn.isEmpty() ? m_groupsDn : m_computerGroupsDn,
											 LdapClient::constructQueryFilter( QStringLiteral( "cn" ), filterValue, m_computerGroupsFilter ) ,
											 m_defaultSearchScope );
}



QStringList LdapDirectory::computerLocations( const QString& filterValue )
{
	QStringList locations;

	if( m_computerLocationsByAttribute )
	{
		locations = m_client.queryAttributeValues( m_computersDn,
												   m_computerLocationAttribute,
												   LdapClient::constructQueryFilter( m_computerLocationAttribute, filterValue, m_computersFilter ),
												   m_defaultSearchScope );
	}
	else if( m_computerLocationsByContainer )
	{
		locations = m_client.queryAttributeValues( m_computersDn,
												   m_locationNameAttribute,
												   LdapClient::constructQueryFilter( m_locationNameAttribute, filterValue, m_computerContainersFilter ) ,
												   m_defaultSearchScope );
	}
	else
	{
		locations = m_client.queryAttributeValues( m_computerGroupsDn.isEmpty() ? m_groupsDn : m_computerGroupsDn,
												   m_locationNameAttribute,
												   LdapClient::constructQueryFilter( m_locationNameAttribute, filterValue, m_computerGroupsFilter ) ,
												   m_defaultSearchScope );
	}

	locations.removeDuplicates();

	std::sort( locations.begin(), locations.end() );

	return locations;
}



QStringList LdapDirectory::groupMembers( const QString& groupDn )
{
	return m_client.queryAttributeValues( groupDn, m_groupMemberAttribute );
}



QStringList LdapDirectory::groupsOfUser( const QString& userDn )
{
	const auto userId = groupMemberUserIdentification( userDn );
	if( m_groupMemberAttribute.isEmpty() || userId.isEmpty() )
	{
		return {};
	}

	return m_client.queryDistinguishedNames( m_groupsDn,
											 LdapClient::constructQueryFilter( m_groupMemberAttribute, userId, m_userGroupsFilter ),
											 m_defaultSearchScope );
}



QStringList LdapDirectory::groupsOfComputer( const QString& computerDn )
{
	const auto computerId = groupMemberComputerIdentification( computerDn );
	if( m_groupMemberAttribute.isEmpty() || computerId.isEmpty() )
	{
		return {};
	}

	return m_client.queryDistinguishedNames( m_computerGroupsDn.isEmpty() ? m_groupsDn : m_computerGroupsDn,
											 LdapClient::constructQueryFilter( m_groupMemberAttribute, computerId, m_computerGroupsFilter ),
											 m_defaultSearchScope );
}



QStringList LdapDirectory::locationsOfComputer( const QString& computerDn )
{
	if( m_computerLocationsByAttribute )
	{
		return m_client.queryAttributeValues( computerDn, m_computerLocationAttribute );
	}
	else if( m_computerLocationsByContainer )
	{
		return m_client.queryAttributeValues( LdapClient::parentDn( computerDn ), m_locationNameAttribute );
	}

	const auto computerId = groupMemberComputerIdentification( computerDn );
	if( m_groupMemberAttribute.isEmpty() || computerId.isEmpty() )
	{
		return {};
	}

	return m_client.queryAttributeValues( m_computerGroupsDn.isEmpty() ? m_groupsDn : m_computerGroupsDn,
										  m_locationNameAttribute,
										  LdapClient::constructQueryFilter( m_groupMemberAttribute, computerId, m_computerGroupsFilter ),
										  m_defaultSearchScope );
}



QString LdapDirectory::userLoginName( const QString& userDn )
{
	return m_client.queryAttributeValues( userDn, m_userLoginAttribute ).value( 0 );
}



QString LdapDirectory::computerDisplayName( const QString& computerDn )
{
	return m_client.queryAttributeValues( computerDn, m_computerDisplayNameAttribute ).value( 0 );

}



QString LdapDirectory::computerHostName( const QString& computerDn )
{
	if( computerDn.isEmpty() )
	{
		return QString();
	}

	return m_client.queryAttributeValues( computerDn, m_computerHostNameAttribute ).value( 0 );
}



QString LdapDirectory::computerMacAddress( const QString& computerDn )
{
	if( computerDn.isEmpty() )
	{
		return QString();
	}

	return m_client.queryAttributeValues( computerDn, m_computerMacAddressAttribute ).value( 0 );
}



QString LdapDirectory::groupMemberUserIdentification( const QString& userDn )
{
	if( m_identifyGroupMembersByNameAttribute )
	{
		return userLoginName( userDn );
	}

	return userDn;
}



QString LdapDirectory::groupMemberComputerIdentification( const QString& computerDn )
{
	if( m_identifyGroupMembersByNameAttribute )
	{
		return computerHostName( computerDn );
	}

	return computerDn;
}



QStringList LdapDirectory::computerLocationEntries( const QString& locationName )
{
	if( m_computerLocationsByAttribute )
	{
		return m_client.queryDistinguishedNames( m_computersDn,
												 LdapClient::constructQueryFilter( m_computerLocationAttribute, locationName, m_computersFilter ),
												 m_defaultSearchScope );
	}
	else if( m_computerLocationsByContainer )
	{
		const auto locationDnFilter = LdapClient::constructQueryFilter( m_locationNameAttribute, locationName, m_computerContainersFilter );
		const auto locationDns = m_client.queryDistinguishedNames( m_computersDn, locationDnFilter, m_defaultSearchScope );

		return m_client.queryDistinguishedNames( locationDns.value( 0 ),
												 LdapClient::constructQueryFilter( QString(), QString(), m_computersFilter ),
												 m_defaultSearchScope );
	}

	auto memberComputers = groupMembers( computerGroups( locationName ).value( 0 ) );

	// computer filter configured?
	if( m_computersFilter.isEmpty() == false )
	{
		auto memberComputersSet = memberComputers.toSet();

		// then return intersection of filtered computer list and group members
		return memberComputersSet.intersect( computersByHostName().toSet() ).toList();
	}

	return memberComputers;
}



QString LdapDirectory::hostToLdapFormat( const QString& host )
{
	QHostAddress hostAddress( host );

	// no valid IP address given?
	if( hostAddress.protocol() == QAbstractSocket::UnknownNetworkLayerProtocol )
	{
		// then try to resolve ist first
		QHostInfo hostInfo = QHostInfo::fromName( host );
		if( hostInfo.error() != QHostInfo::NoError || hostInfo.addresses().isEmpty() )
		{
			qWarning() << "LdapDirectory::hostToLdapFormat(): could not lookup IP address of host"
					   << host << "error:" << hostInfo.errorString();
			return QString();
		}

#if QT_VERSION < 0x050600
		hostAddress = hostInfo.addresses().value( 0 );
#else
		hostAddress = hostInfo.addresses().constFirst();
#endif
		vDebug() << "LdapDirectory::hostToLdapFormat(): no valid IP address given, resolved IP address of host"
				 << host << "to" << hostAddress.toString();
	}

	// now do a name lookup to get the full host name information
	QHostInfo hostInfo = QHostInfo::fromName( hostAddress.toString() );
	if( hostInfo.error() != QHostInfo::NoError )
	{
		qWarning() << "LdapDirectory::hostToLdapFormat(): could not lookup host name for IP"
				   << hostAddress.toString() << "error:" << hostInfo.errorString();
		return QString();
	}

	// are we working with fully qualified domain name?
	if( m_computerHostNameAsFQDN )
	{
		vDebug() << "LdapDirectory::hostToLdapFormat(): Resolved FQDN" << hostInfo.hostName();
		return hostInfo.hostName();
	}

	// return first part of host name which should be the actual machine name
	const QString hostName = hostInfo.hostName().split( QLatin1Char('.') ).value( 0 );

	vDebug() << "LdapDirectory::hostToLdapFormat(): resolved host name" << hostName;
	return hostName;
}



QString LdapDirectory::computerObjectFromHost( const QString& host )
{
	QString hostName = hostToLdapFormat( host );
	if( hostName.isEmpty() )
	{
		qWarning( "LdapDirectory::computerObjectFromHost(): could not resolve hostname, returning empty computer object" );
		return QString();
	}

	QStringList computerObjects = computersByHostName( hostName );
	if( computerObjects.count() == 1 )
	{
		return computerObjects.first();
	}

	// return empty result if not exactly one object was found
	qWarning( "LdapDirectory::computerObjectFromHost(): more than one computer object found, returning empty computer object!" );
	return QString();
}
