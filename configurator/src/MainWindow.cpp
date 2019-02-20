/*
 * MainWindow.cpp - implementation of MainWindow class
 *
 * Copyright (c) 2010-2019 Tobias Junghans <tobydox@veyon.io>
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

#include <QDir>
#include <QProcess>
#include <QCloseEvent>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>

#include "Configuration/JsonStore.h"
#include "Configuration/UiMapping.h"

#include "AboutDialog.h"
#include "ConfigurationPagePluginInterface.h"
#include "ConfigurationManager.h"
#include "VeyonConfiguration.h"
#include "MainWindow.h"
#include "PluginManager.h"

#include "Filesystem.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow( QWidget* parent ) :
	QMainWindow( parent ),
	ui( new Ui::MainWindow ),
	m_configChanged( false )
{
	ui->setupUi( this );

	setWindowTitle( tr( "%1 Configurator %2" ).arg( VeyonCore::applicationName(), VeyonCore::version() ) );

	loadConfigurationPagePlugins();

	// reset all widget's values to current configuration
	reset( true );

	// if local configuration is incomplete, re-enable the apply button
	if( VeyonConfiguration().data().size() < VeyonCore::config().data().size() )
	{
		configurationChanged();
	}

	const auto pages = findChildren<ConfigurationPage *>();
	for( auto page : pages )
	{
		page->connectWidgetsToProperties();
		connect( page, &ConfigurationPage::widgetsChanged, this, &MainWindow::updateView );
	}

	connect( ui->buttonBox, &QDialogButtonBox::clicked, this, &MainWindow::resetOrApply );

	connect( ui->actionLoadSettings, &QAction::triggered, this, &MainWindow::loadSettingsFromFile );
	connect( ui->actionSaveSettings, &QAction::triggered, this, &MainWindow::saveSettingsToFile );

	auto viewModeGroup = new QActionGroup( this );
	viewModeGroup->addAction( ui->viewModeStandard );
	viewModeGroup->addAction( ui->viewModeAdvanced );
	viewModeGroup->setExclusive( true );
	ui->viewModeStandard->setChecked( true );

	connect( viewModeGroup, &QActionGroup::triggered, this, &MainWindow::updateView );
	switchToStandardView();

	connect( ui->actionAboutQt, &QAction::triggered, QApplication::instance(), &QApplication::aboutQt );

	connect( &VeyonCore::config(), &VeyonConfiguration::configurationChanged, this, &MainWindow::configurationChanged );

	VeyonCore::enforceBranding( this );
}



MainWindow::~MainWindow()
{
	delete ui;
}



void MainWindow::reset( bool onlyUI )
{
	if( onlyUI == false )
	{
		VeyonCore::config().clear();
		VeyonCore::config().reloadFromStore();
	}

	const auto pages = findChildren<ConfigurationPage *>();
	for( auto page : pages )
	{
		page->resetWidgets();
	}

	ui->buttonBox->setEnabled( false );
	m_configChanged = false;
}




void MainWindow::apply()
{
	if( applyConfiguration() )
	{
		const auto pages = findChildren<ConfigurationPage *>();
		for( auto page : pages )
		{
			page->applyConfiguration();
		}

		ui->buttonBox->setEnabled( false );
		m_configChanged = false;
	}
}



void MainWindow::updateView()
{
	if( ui->viewModeAdvanced->isChecked() )
	{
		switchToAdvancedView();
	}
	else
	{
		switchToStandardView();
	}
}




void MainWindow::configurationChanged()
{
	ui->buttonBox->setEnabled( true );
	m_configChanged = true;
}




void MainWindow::resetOrApply( QAbstractButton *btn )
{
	if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Apply )
	{
		apply();
	}
	else if( ui->buttonBox->standardButton( btn ) & QDialogButtonBox::Reset )
	{
		reset();
	}
}



void MainWindow::loadSettingsFromFile()
{
	QString fileName = QFileDialog::getOpenFileName( this, tr( "Load settings from file" ),
													 QDir::homePath(), tr( "JSON files (*.json)" ) );
	if( !fileName.isEmpty() )
	{
		// write current configuration to output file
		Configuration::JsonStore( Configuration::JsonStore::System, fileName ).load( &VeyonCore::config() );
		reset( true );
		configurationChanged();	// give user a chance to apply possible changes
	}
}




void MainWindow::saveSettingsToFile()
{
	QString fileName = QFileDialog::getSaveFileName( this, tr( "Save settings to file" ),
													 QDir::homePath(), tr( "JSON files (*.json)" ) );
	if( !fileName.isEmpty() )
	{
		if( !fileName.endsWith( QStringLiteral(".json"), Qt::CaseInsensitive ) )
		{
			fileName += QStringLiteral(".json");
		}

		bool configChangedPrevious = m_configChanged;

		// write current configuration to output file
		Configuration::JsonStore( Configuration::JsonStore::System, fileName ).flush( &VeyonCore::config() );

		m_configChanged = configChangedPrevious;
		ui->buttonBox->setEnabled( m_configChanged );
	}
}



void MainWindow::resetConfiguration()
{
	if( QMessageBox::warning( this, tr( "Reset configuration" ),
							  tr( "Do you really want to reset the local configuration and revert "
								  "all settings to their defaults?" ), QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) ==
		QMessageBox::Yes )
	{
		ConfigurationManager().clearConfiguration();
		reset( false );
	}
}



void MainWindow::aboutVeyon()
{
	AboutDialog( this ).exec();
}



void MainWindow::switchToStandardView()
{
	const auto widgets = findChildren<QWidget *>();

	// hide widgets with advanced property flag
	for( auto widget : widgets )
	{
		const auto flags = widget->property( CONFIG_UI_PROPERTY_FLAGS ).value<Configuration::Object::PropertyFlags>();
		if( flags & Configuration::Object::StandardProperty )
		{
			widget->show();
		}
		else if( flags & Configuration::Object::AdvancedProperty )
		{
			widget->hide();
		}
	}

	// hide page selector items for advanced pages
	for( int i = 0; i < ui->pageSelector->count(); ++i )
	{
		auto item = ui->pageSelector->item( i );
		auto page = item->data( Qt::UserRole ).value<ConfigurationPage *>();
		if( page )
		{
			const auto flags = page->property( CONFIG_UI_PROPERTY_FLAGS ).value<Configuration::Object::PropertyFlags>();

			ui->pageSelector->setRowHidden( i, flags & Configuration::Object::AdvancedProperty );
		}
	}
}



void MainWindow::switchToAdvancedView()
{
	const auto widgets = findChildren<QWidget *>();
	for( auto widget : widgets )
	{
		const auto flags = widget->property( CONFIG_UI_PROPERTY_FLAGS ).value<Configuration::Object::PropertyFlags>();
		if( flags & Configuration::Object::AdvancedProperty )
		{
			widget->show();
		}
	}

	for( int i = 0; i < ui->pageSelector->count(); ++i )
	{
		ui->pageSelector->setRowHidden( i, false );
	}
}



bool MainWindow::applyConfiguration()
{
	ConfigurationManager configurationManager;

	if( configurationManager.saveConfiguration() == false ||
		configurationManager.applyConfiguration() == false )
	{
		qCritical() << Q_FUNC_INFO << configurationManager.errorString().toUtf8().constData();

		QMessageBox::critical( nullptr,
							   tr( "%1 Configurator" ).arg( VeyonCore::applicationName() ),
							   configurationManager.errorString() );
		return false;
	}
#ifdef Q_OS_LINUX
	
#else
    const auto confFilePath = QStringLiteral("c:\\record");  ;//VeyonCore::filesystem().expandPath(VeyonCore::config().screenRecordingDirectory());
    if( VeyonCore::filesystem().ensurePathExists( confFilePath ) == false )
    {
        const auto msg = tr( "Could not take a screenRecording as directory %1 doesn't exist and couldn't be created." ).arg( confFilePath );
        qCritical() << msg.toUtf8().constData();
        if( qobject_cast<QApplication *>( QCoreApplication::instance() ) )
        {
            QMessageBox::critical( nullptr, tr( "ScreenRecording" ), msg );
        }
    }else{
	    QString jsonFile = confFilePath+QDir::separator() + QStringLiteral("host.json");
    	saveSettingsToDefaultFile(jsonFile);
    }
	
#endif


	return true;
}



void MainWindow::loadConfigurationPagePlugins()
{
	for( auto pluginObject : qAsConst( VeyonCore::pluginManager().pluginObjects() ) )
	{
		auto pluginInterface = qobject_cast<PluginInterface *>( pluginObject );
		auto configurationPagePluginInterface = qobject_cast<ConfigurationPagePluginInterface *>( pluginObject );

		if( pluginInterface && configurationPagePluginInterface )
		{
			auto page = configurationPagePluginInterface->createConfigurationPage();
			if( page )
			{
				ui->configPages->addWidget( page );

				auto item = new QListWidgetItem( page->windowIcon(), page->windowTitle() );
				item->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
				item->setData( Qt::UserRole, QVariant::fromValue( page ) );
				ui->pageSelector->addItem( item );
			}
		}
	}

	// adjust minimum size
	ui->pageSelector->setMinimumSize( ui->pageSelector->sizeHintForColumn(0) + 3 * ui->pageSelector->spacing(),
									  ui->pageSelector->minimumHeight() );
}



void MainWindow::closeEvent( QCloseEvent *closeEvent )
{
	if( m_configChanged &&
		QMessageBox::question( this, tr( "Unsaved settings" ),
							   tr( "There are unsaved settings. "
								   "Quit anyway?" ),
							   QMessageBox::Yes | QMessageBox::No ) !=
		QMessageBox::Yes )
	{
		closeEvent->ignore();
		return;
	}

	closeEvent->accept();
	QMainWindow::closeEvent( closeEvent );
}

void MainWindow::saveSettingsToDefaultFile(QString& fileName)
{
    if( !fileName.isEmpty() )
    {
        if( !fileName.endsWith( QStringLiteral(".json"), Qt::CaseInsensitive ) )
        {
            fileName += QStringLiteral(".json");
        }

        bool configChangedPrevious = m_configChanged;

        // write current configuration to output file
        Configuration::JsonStore( Configuration::JsonStore::System, fileName ).flush( &VeyonCore::config() );

        m_configChanged = configChangedPrevious;
        ui->buttonBox->setEnabled( m_configChanged );
    }
}
