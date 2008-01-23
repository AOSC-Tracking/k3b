/*
 *
 * Copyright (C) 2003-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#include <qlabel.h>
#include <qlayout.h>
#include <q3groupbox.h>
#include <qcheckbox.h>
#include <qcombobox.h>

#include <q3whatsthis.h>
//Added by qt3to4:
#include <Q3GridLayout>

#include <kiconloader.h>
#include <klocale.h>
#include <knuminput.h>
#include <kmimetype.h>
#include <kurl.h>
#include <kio/global.h>
#include <klineedit.h>

#include "k3baudiotrackdialog.h"
#include "k3baudioeditorwidget.h"
#include "k3baudiotrackwidget.h"
#include "k3baudiotrack.h"
#include <k3bvalidators.h>
#include <k3bmsf.h>
#include <k3bcdtextvalidator.h>
#include <k3baudiodecoder.h>
#include <k3bmsfedit.h>
#include <kvbox.h>


// TODO: three modes:
//    1. Only one track with only one source
//         show decoder tech info, cdtext, options and the track editor without showing anything
//         about sources
//    2. Only one track with multible sources
//         like the above but with the possiblity to edit the sources
//    3. multible tracks
//         do only show cd-text and options (eventuelle index0)


K3bAudioTrackDialog::K3bAudioTrackDialog( const QList<K3bAudioTrack*>& tracks, QWidget *parent )
    : KDialog( parent)
{
    m_tracks = tracks;

    setCaption(i18n("Audio Track Properties"));
    setButtons(Ok|Cancel|Apply);
    setDefaultButton(Ok);
    setModal(true);
    connect(this,SIGNAL(okClicked()), this, SLOT(slotOk()));
    connect(this,SIGNAL(applyClicked()),this,SLOT(slotApply()));


    setupGui();
    setupConnections();
}

K3bAudioTrackDialog::~K3bAudioTrackDialog()
{
}


void K3bAudioTrackDialog::slotOk()
{
    slotApply();
    done(0);
}


void K3bAudioTrackDialog::slotApply()
{
    m_audioTrackWidget->save();

}


void K3bAudioTrackDialog::setupGui()
{
    QFrame* frame = new QFrame();
    setMainWidget( frame );


    Q3GridLayout* mainLayout = new Q3GridLayout( frame );
    mainLayout->setSpacing( spacingHint() );
    mainLayout->setMargin( 0 );

    m_audioTrackWidget = new K3bAudioTrackWidget( m_tracks, frame );
    mainLayout->addWidget( m_audioTrackWidget, 0, 0 );
}

void K3bAudioTrackDialog::setupConnections()
{
}


void K3bAudioTrackDialog::updateTrackLengthDisplay()
{
//   K3b::Msf len = m_editTrackEnd->msfValue() - m_editTrackStart->msfValue();
//   m_displayLength->setText( len.toString() );
//   m_displaySize->setText( KIO::convertSize(len.audioBytes()) );
}

#include "k3baudiotrackdialog.moc"
