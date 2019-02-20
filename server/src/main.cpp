/*
 * main.cpp - main file for Veyon Server
 *
 * Copyright (c) 2006-2019 Tobias Junghans <tobydox@veyon.io>
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

#include "ComputerControlServer.h"
#include "VeyonConfiguration.h"
#include "Filesystem.h"

/*int main(int argc,char **argv)
{
    const auto dir = VeyonCore::filesystem().expandPath(VeyonCore::config().screenRecordingDirectory());
    qDebug() << dir;
    if( VeyonCore::filesystem().ensurePathExists( dir ) == false )
    {
        return 0;
    }
    const auto m_fileName =  QString( QStringLiteral( "_%1_%2.avi" ) ).arg(
                        QDate( QDate::currentDate() ).toString( Qt::ISODate ),
                        QTime( QTime::currentTime() ).toString( Qt::ISODate ) ).
                    replace( QLatin1Char(':'), QLatin1Char('-') );

    qDebug() << dir + QDir::separator() + m_fileName;

    return 1;
}*/

int main( int argc, char **argv )
{
	QCoreApplication app( argc, argv );

	VeyonCore core( &app, QStringLiteral("Server") );

	auto server = new ComputerControlServer;
	if( server->start() == false )
	{
		qInfo( "Failed to start server" );
		delete server;
		return -1;
	}

	qInfo( "Exec" );

	int ret = app.exec();

	delete server;

	qInfo( "Exec Done" );

	return ret;
}
