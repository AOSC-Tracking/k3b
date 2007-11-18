/*
 *
 * Copyright (C) 2003-2007 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2007 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#include "k3bmiscoptiontab.h"

#include <k3bpluginmanager.h>
#include <k3bcore.h>
#include <k3bservicemenuinstaller.h>
#include <k3binteractiondialog.h>
#include <k3bintmapcombobox.h>

#include <qcheckbox.h>
#include <qfileinfo.h>
#include <qradiobutton.h>

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdialog.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kurlrequester.h>
#include <kcombobox.h>
#include <kglobal.h>


K3bMiscOptionTab::K3bMiscOptionTab(QWidget *parent )
    : base_K3bMiscOptionTab(parent)
{
    m_editTempDir->setMode( KFile::Directory );
    connect( m_buttonConfigureAudioOutput, SIGNAL(clicked()),
             this, SLOT(slotConfigureAudioOutput()) );

    m_comboActionDialogSettings->insertItem( K3bInteractionDialog::LOAD_K3B_DEFAULTS,
                                             i18n("Default Settings"),
                                             i18n("Load the K3b Defaults at dialog startup.") );
    m_comboActionDialogSettings->insertItem( K3bInteractionDialog::LOAD_SAVED_SETTINGS,
                                             i18n("Saved Settings"),
                                             i18n("Load the settings saved by the user at dialog startup.") );
    m_comboActionDialogSettings->insertItem( K3bInteractionDialog::LOAD_LAST_SETTINGS,
                                             i18n("Last Used Settings"),
                                             i18n("Load the last used settings at dialog startup.") );
    m_comboActionDialogSettings->addGlobalWhatsThisText( i18n("K3b handles three sets of settings in action dialogs "
                                                              "(action dialogs include the CD Copy dialog or the Audio CD "
                                                              "project dialog):"),
                                                         i18n("One of these sets is loaded once an action dialog is opened. "
                                                              "This setting defines which set it will be.") );
}


K3bMiscOptionTab::~K3bMiscOptionTab()
{
}


void K3bMiscOptionTab::readSettings()
{
    KConfigGroup c = KGlobal::config()->group( "General Options" );

    m_checkSaveOnExit->setChecked( c.readEntry( "ask_for_saving_changes_on_exit", true ) );
    m_checkShowSplash->setChecked( c.readEntry("Show splash", true) );
    m_checkShowProgressOSD->setChecked( c.readEntry( "Show progress OSD", true ) );
    m_checkHideMainWindowWhileWriting->setChecked( c.readEntry( "hide main window while writing", false ) );
    m_checkKeepDialogsOpen->setChecked( c.readEntry( "keep action dialogs open", false ) );
    m_comboActionDialogSettings->setSelectedValue( c.readEntry( "action dialog startup settings",
                                                                ( int )K3bInteractionDialog::LOAD_SAVED_SETTINGS ) );
    m_checkSystemConfig->setChecked( c.readEntry( "check system config", true ) );

    QString tempdir = c.readPathEntry( "Temp Dir", KGlobal::dirs()->resourceDirs( "tmp" ).first() );
    m_editTempDir->setUrl( tempdir );

//   if( c.readEntry( "Multiple Instances", "smart" ) == "smart" )
//     m_radioMultipleInstancesSmart->setChecked(true);
//   else
//     m_radioMultipleInstancesNew->setChecked(true);

    K3bServiceInstaller si;
    m_checkKonqiIntegration->setChecked( si.allInstalled() );
}


bool K3bMiscOptionTab::saveSettings()
{
    KConfigGroup c = KGlobal::config()->group( "General Options" );

    c.writeEntry( "ask_for_saving_changes_on_exit", m_checkSaveOnExit->isChecked() );
    c.writeEntry( "Show splash", m_checkShowSplash->isChecked() );
    c.writeEntry( "Show progress OSD", m_checkShowProgressOSD->isChecked() );
    c.writeEntry( "hide main window while writing", m_checkHideMainWindowWhileWriting->isChecked() );
    c.writeEntry( "keep action dialogs open", m_checkKeepDialogsOpen->isChecked() );
    c.writeEntry( "check system config", m_checkSystemConfig->isChecked() );
    c.writeEntry( "action dialog startup settings", m_comboActionDialogSettings->selectedValue() );

    QString tempDir = m_editTempDir->url().toLocalFile();
    QFileInfo fi( tempDir );

    if( fi.isRelative() ) {
        fi.setFile( fi.absoluteFilePath() );
    }

    if( !fi.exists() ) {
        if( KMessageBox::questionYesNo( this,
                                        i18n("Directory (%1) does not exist. Create?",tempDir),
                                        i18n("Create Directory"),
                                        KGuiItem( i18n("Create") ),
                                        KStandardGuiItem::cancel() ) == KMessageBox::Yes ) {
            if( !KStandardDirs::makeDir( fi.absoluteFilePath() ) ) {
                KMessageBox::error( this, i18n("Unable to create directory %1",tempDir) );
                return false;
            }
        }
        else {
            // the dir does not exist and the user doesn't want to create it
            return false;
        }
    }

    if( fi.isFile() ) {
        KMessageBox::information( this, i18n("You specified a file for the temporary directory. "
                                             "K3b will use its base path as the temporary directory."),
                                  i18n("Warning"),
                                  "temp file only using base path" );
        fi.setFile( fi.dirPath() );
    }

    // check for writing permission
    if( !fi.isWritable() ) {
        KMessageBox::error( this, i18n("You do not have permission to write to %1.",fi.absoluteFilePath()) );
        return false;
    }

    m_editTempDir->setUrl( fi.absoluteFilePath() );

    c.writePathEntry( "Temp Dir", m_editTempDir->url().toLocalFile() );

//   if( m_radioMultipleInstancesSmart->isChecked() )
//     c.writeEntry( "Multiple Instances", "smart" );
//   else
//     c.writeEntry( "Multiple Instances", "always_new" );

    K3bServiceInstaller si;
    if( m_checkKonqiIntegration->isChecked() )
        si.install( this );
    else
        si.remove( this );

    return true;
}

#include "k3bmiscoptiontab.moc"
