/*
 *
 * $Id: k3bblankingdialog.cpp 619556 2007-01-03 17:38:12Z trueg $
 * Copyright (C) 2007 Sebastian Trueg <trueg@k3b.org>
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

#include "k3bmediaformattingdialog.h"

#include "k3bapplication.h"
#include "k3bmediacache.h"
#include "k3bmedium.h"

#include <k3bdvdformattingjob.h>
#include <k3bblankingjob.h>

#include <k3bdevice.h>
#include <k3bdevicemanager.h>
#include <k3bglobals.h>
#include <k3bcore.h>
#include <k3bwriterselectionwidget.h>
#include <k3bwritingmodewidget.h>
#include <k3bjobprogressdialog.h>

#include <klocale.h>
#include <kmessagebox.h>
#include <kconfig.h>
#include <kapplication.h>

#include <q3groupbox.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <q3frame.h>
#include <qpushbutton.h>
#include <qtooltip.h>
#include <q3whatsthis.h>
//Added by qt3to4:
#include <Q3GridLayout>


K3bMediaFormattingDialog::K3bMediaFormattingDialog( QWidget* parent, const char* name, bool modal )
  : K3bInteractionDialog( parent, name,
			  i18n("Format and Erase"),
			  i18n( "CD-RW" ) + '/' + i18n("DVD%1RW").arg("�") + '/' + i18n( "BD-RW" ),
			  START_BUTTON|CANCEL_BUTTON,
			  START_BUTTON,
			  "Formatting and Erasing", // config group
			  modal )
{
  QWidget* frame = mainWidget();

  m_writerSelectionWidget = new K3bWriterSelectionWidget( frame );
  m_writerSelectionWidget->setWantedMediumType( K3bDevice::MEDIA_REWRITABLE );
  // we need state empty here for preformatting DVD+RW.
  m_writerSelectionWidget->setWantedMediumState( K3bDevice::STATE_COMPLETE|
						 K3bDevice::STATE_INCOMPLETE|
						 K3bDevice::STATE_EMPTY );
  m_writerSelectionWidget->setSupportedWritingApps( K3b::DVD_RW_FORMAT );
  m_writerSelectionWidget->setForceAutoSpeed(true);

  Q3GroupBox* groupWritingMode = new Q3GroupBox( 1, Qt::Vertical, i18n("Writing Mode"), frame );
  groupWritingMode->layout()->setMargin( marginHint() );
  groupWritingMode->layout()->setSpacing( spacingHint() );
  m_writingModeWidget = new K3bWritingModeWidget( K3b::WRITING_MODE_INCR_SEQ|K3b::WRITING_MODE_RES_OVWR,
						  groupWritingMode );


  Q3GroupBox* groupOptions = new Q3GroupBox( 2, Qt::Vertical, i18n("Settings"), frame );
  groupOptions->layout()->setMargin( marginHint() );
  groupOptions->layout()->setSpacing( spacingHint() );
  m_checkForce = new QCheckBox( i18n("Force"), groupOptions );
  m_checkQuickFormat = new QCheckBox( i18n("Quick format"), groupOptions );

  Q3GridLayout* grid = new Q3GridLayout( frame );
  grid->setMargin( 0 );
  grid->setSpacing( spacingHint() );

  grid->addMultiCellWidget( m_writerSelectionWidget, 0, 0, 0, 1 );
  grid->addWidget( groupWritingMode, 1, 0 );
  grid->addWidget( groupOptions, 1, 1 );
  grid->setRowStretch( 1, 1 );

// FIXME: check if we need Blu-ray comments here
  QToolTip::add( m_checkForce, i18n("Force formatting of empty DVDs") );
  Q3WhatsThis::add( m_checkForce, i18n("<p>If this option is checked K3b will format a "
				      "DVD-RW even if it is empty. It may also be used to "
				      "force K3b to format a DVD+RW or a DVD-RW in restricted "
				      "overwrite mode."
				      "<p><b>Caution:</b> It is not recommended to often format a DVD "
				      "since it may already be unusable after 10-20 reformat procedures."
				      "<p>DVD+RW media only needs to be formatted once. After that it "
				      "just needs to be overwritten. The same applies to DVD-RW in "
				      "restricted overwrite mode.") );

  QToolTip::add( m_checkQuickFormat, i18n("Try to perform quick formatting") );
  Q3WhatsThis::add( m_checkQuickFormat, i18n("<p>If this option is checked K3b will tell the writer "
					    "to perform a quick format."
					    "<p>Erasing a rewritable medium completely can take a very long "
					    "time and some writers perform a full format even if "
					    "quick format is enabled." ) );

  connect( m_writerSelectionWidget, SIGNAL(writerChanged()), this, SLOT(slotToggleAll()) );

  slotToggleAll();
}


K3bMediaFormattingDialog::~K3bMediaFormattingDialog()
{
}


void K3bMediaFormattingDialog::setDevice( K3bDevice::Device* dev )
{
  m_writerSelectionWidget->setWriterDevice( dev );
}


void K3bMediaFormattingDialog::slotStartClicked()
{
    K3bMedium medium = k3bappcore->mediaCache()->medium( m_writerSelectionWidget->writerDevice() );

    K3bJobProgressDialog dlg( kapp->mainWidget(), "formattingProgress", false );

    K3bJob* theJob = 0;

    if( medium.diskInfo().mediaType() & K3bDevice::MEDIA_CD_ALL ) {
        K3bBlankingJob* job = new K3bBlankingJob( &dlg, this );

        job->setDevice( m_writerSelectionWidget->writerDevice() );
        job->setSpeed( m_writerSelectionWidget->writerSpeed() );
        job->setForce( m_checkForce->isChecked() );
        job->setWritingApp( m_writerSelectionWidget->writingApp() );
        // no support for all the strange erasing modes anymore, they did not work anyway
        job->setMode( m_checkQuickFormat->isChecked() ? K3bBlankingJob::Fast : K3bBlankingJob::Complete );

        theJob = job;
    }
    else if ( medium.diskInfo().mediaType() & K3bDevice::MEDIA_DVD_ALL ) {
        K3bDvdFormattingJob* job = new K3bDvdFormattingJob( &dlg, this );

        job->setDevice( m_writerSelectionWidget->writerDevice() );
        job->setMode( m_writingModeWidget->writingMode() );
        job->setForce( m_checkForce->isChecked() );
        job->setQuickFormat( m_checkQuickFormat->isChecked() );

        theJob = job;
    }
    else {
        KMessageBox::sorry( this, "Ups", "No formatting support for this source media type yet." );
        return;
    }

    if( !exitLoopOnHide() )
        hide();

    dlg.startJob( theJob );

    delete theJob;

    if( KConfigGroup( k3bcore->config(), "General Options" ).readBoolEntry( "keep action dialogs open", false ) &&
        !exitLoopOnHide() )
        show();
    else
        close();
}


void K3bMediaFormattingDialog::toggleAll()
{
    K3bMedium medium = k3bappcore->mediaCache()->medium( m_writerSelectionWidget->writerDevice() );
    int modes = 0;
    if ( medium.diskInfo().mediaType() & K3bDevice::MEDIA_DVD_RW ) {
        modes |=  K3b::WRITING_MODE_INCR_SEQ|K3b::WRITING_MODE_RES_OVWR;
    }
    m_writingModeWidget->setSupportedModes( modes );
    setButtonEnabled( START_BUTTON, m_writerSelectionWidget->writerDevice() != 0 );
}


void K3bMediaFormattingDialog::loadUserDefaults( KConfigBase* c )
{
    m_checkForce->setChecked( c->readBoolEntry( "force", false ) );
    m_checkQuickFormat->setChecked( c->readBoolEntry( "quick format", true ) );
    m_writerSelectionWidget->loadConfig( c );
    m_writingModeWidget->loadConfig( c );
}


void K3bMediaFormattingDialog::saveUserDefaults( KConfigBase* c )
{
    c->writeEntry( "force", m_checkForce->isChecked() );
    c->writeEntry( "quick format", m_checkQuickFormat->isChecked() );
    m_writerSelectionWidget->saveConfig( c );
    m_writingModeWidget->saveConfig( c );
}


void K3bMediaFormattingDialog::loadK3bDefaults()
{
    m_writerSelectionWidget->loadDefaults();
    m_checkForce->setChecked( false );
    m_checkQuickFormat->setChecked( true );
    m_writingModeWidget->setWritingMode( K3b::WRITING_MODE_AUTO );
}

#include "k3bmediaformattingdialog.moc"
