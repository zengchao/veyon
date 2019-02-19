/*
 * ComputerSortFilterProxyModel.cpp - implementation of ComputerSortFilterProxyModel
 *
 * Copyright (c) 2018-2019 Tobias Junghans <tobydox@veyon.io>
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

#include "ComputerSortFilterProxyModel.h"

#if defined(QT_TESTLIB_LIB) && QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#endif


ComputerSortFilterProxyModel::ComputerSortFilterProxyModel( QObject* parent ) :
	QSortFilterProxyModel( parent ),
	m_stateRole( -1 ),
	m_stateFilter( ComputerControlInterface::State::None )
{
#if defined(QT_TESTLIB_LIB) && QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	new QAbstractItemModelTester( this, QAbstractItemModelTester::FailureReportingMode::Warning, this );
#endif

	setFilterCaseSensitivity( Qt::CaseInsensitive );
}



void ComputerSortFilterProxyModel::setStateRole( int role )
{
	beginResetModel();
	m_stateRole = role;
	endResetModel();
}



void ComputerSortFilterProxyModel::setStateFilter( ComputerControlInterface::State state )
{
	beginResetModel();
	m_stateFilter = state;
	endResetModel();
}



bool ComputerSortFilterProxyModel::filterAcceptsRow( int sourceRow, const QModelIndex& sourceParent ) const
{
	if( m_stateFilter != ComputerControlInterface::State::None &&
		m_stateRole >= 0 &&
		sourceModel()->data( sourceModel()->index( sourceRow, 0, sourceParent ),
							 m_stateRole ).value<ComputerControlInterface::State>() != m_stateFilter )
	{
		return false;
	}

	return QSortFilterProxyModel::filterAcceptsRow( sourceRow, sourceParent );
}
