/*
 *
 * Copyright (C) 2005 Sebastian Trueg <trueg@k3b.org>
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

#include "k3bdatamultisessionimportdialog.h"

#include <qlabel.h>
#include <qlayout.h>
#include <q3header.h>
#include <qfont.h>
#include <qmap.h>
#include <qcursor.h>
//Added by qt3to4:
#include <Q3VBoxLayout>

#include <klocale.h>
#include <kiconloader.h>
#include <kmessagebox.h>

#include <k3bcore.h>
#include <k3bdatadoc.h>
#include <k3blistview.h>
#include <k3btoc.h>
#include <k3bdevice.h>
#include <k3bdevicemanager.h>
#include <k3bdiskinfo.h>
#include <k3biso9660.h>

#include "../k3bmedium.h"
#include "../k3bmediacache.h"
#include "../k3bapplication.h"
#include "../k3b.h"


class SessionInfo
{
public:
    SessionInfo()
        : sessionNumber( 0 ),
          device( 0 ) {}

    SessionInfo( int num, K3bDevice::Device* dev )
        : sessionNumber( num ),
          device( dev ) {}

    int sessionNumber;
    K3bDevice::Device* device;
};


class K3bDataMultisessionImportDialog::Private
{
public:
    K3bDataDoc* doc;
    K3bListView* sessionView;

    QMap<K3bListViewItem*, SessionInfo> sessions;
};


K3bDataDoc* K3bDataMultisessionImportDialog::importSession( K3bDataDoc* doc, QWidget* parent )
{
  K3bDataMultisessionImportDialog dlg( parent );
  dlg.importSession( doc );
  dlg.exec();
  return dlg.d->doc;
}


void K3bDataMultisessionImportDialog::slotOk()
{
    K3bListViewItem* selected = static_cast<K3bListViewItem*>( d->sessionView->selectedItem() );
    if ( selected ) {
        QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

        const SessionInfo& info = d->sessions[selected];
        K3bDevice::Device* dev = info.device;

        //
        // Mkisofs does not properly import joliet filenames from an old session
        //
        // See bug 79215 for details
        //
        K3bIso9660 iso( dev );
        if( iso.open() ) {
            if( iso.firstRRDirEntry() == 0 && iso.jolietLevel() > 0 )
                KMessageBox::sorry( this,
                                    i18n("<p>K3b found session containing Joliet information for long filenames "
                                         "but no Rock Ridge extensions."
                                         "<p>The filenames in the imported session will be converted to a restricted "
                                         "character set in the new session. This character set is based on the ISO9660 "
                                         "settings in the K3b project. K3b is not able to display these converted filenames yet."),
                                    i18n("Session Import Warning") );
            iso.close();
        }

        if( !d->doc ) {
            d->doc = static_cast<K3bDataDoc*>( k3bappcore->k3bMainWindow()->slotNewDataDoc() );
        }

        d->doc->setBurner( dev );
        d->doc->importSession( dev, info.sessionNumber );

        QApplication::restoreOverrideCursor();

        done( 0 );
    }
    else
        done( 1 );
}


void K3bDataMultisessionImportDialog::slotCancel()
{
    reject();
}


void K3bDataMultisessionImportDialog::importSession( K3bDataDoc* doc )
{
  d->doc = doc;
  updateMedia();
  slotSelectionChanged();
}


void K3bDataMultisessionImportDialog::updateMedia()
{
    d->sessionView->clear();
    d->sessions.clear();

    QList<K3bDevice::Device*> devices = k3bcore->deviceManager()->allDevices();

    bool haveMedium = false;
    for( QList<K3bDevice::Device *>::const_iterator it = devices.begin();
       it != devices.end(); ++it ) {
        K3bMedium medium = k3bappcore->mediaCache()->medium( *it );

        if ( medium.diskInfo().mediaType() & K3bDevice::MEDIA_WRITABLE &&
             medium.diskInfo().diskState() == K3bDevice::STATE_INCOMPLETE ) {
            addMedium( medium );
            haveMedium = true;
        }
        else if ( !medium.diskInfo().empty() &&
                  medium.diskInfo().mediaType() & ( K3bDevice::MEDIA_DVD_PLUS_RW|K3bDevice::MEDIA_DVD_RW_OVWR ) ) {
            addMedium( medium );
            haveMedium = true;
        }
    }

    if ( !haveMedium ) {
        K3bListViewItem* noMediaItem = new K3bListViewItem( d->sessionView,
                                                            i18n( "Please insert an appendable medium" ) );
        QFont fnt( font() );
        fnt.setItalic( true );
        noMediaItem->setFont( 0, fnt );
    }

    d->sessionView->setEnabled( haveMedium );
}


void K3bDataMultisessionImportDialog::addMedium( const K3bMedium& medium )
{
    K3bListViewItem* mediumItem = new K3bListViewItem( d->sessionView, medium.shortString( true ) );
    QFont fnt( font() );
    fnt.setBold( true );
    mediumItem->setFont( 0, fnt );
    mediumItem->setPixmap( 0, SmallIcon("media-optical-recordable") );

    // the medium item in case we have no session info (will always use the last session)
    d->sessions.insert( mediumItem, SessionInfo( 0, medium.device() ) );

    const K3bDevice::Toc& toc = medium.toc();
    K3bListViewItem* sessionItem = 0;
    int lastSession = 0;
    for ( K3bDevice::Toc::const_iterator it = toc.begin(); it != toc.end(); ++it ) {
        const K3bTrack& track = *it;

	if( track.session() != lastSession ) {
            lastSession = track.session();
            QString sessionInfo;
            if ( track.type() == K3bDevice::Track::DATA ) {
                K3bIso9660 iso( medium.device(), track.firstSector().lba() );
                if ( iso.open() ) {
                    sessionInfo = iso.primaryDescriptor().volumeId;
                }
            }
            else {
                int numAudioTracks = 1;
                while ( it != toc.end()
                        && ( *it ).type() == K3bDevice::Track::AUDIO
                        && ( *it ).session() == lastSession ) {
                    ++it;
                    ++numAudioTracks;
                }
                --it;
                sessionInfo = i18np("1 audio track", "%1 audio tracks", numAudioTracks );
            }

            sessionItem = new K3bListViewItem( mediumItem,
                                               sessionItem,
                                               i18n( "Session %1" ,
                                               lastSession )
                                               + ( sessionInfo.isEmpty() ? QString::null : " (" + sessionInfo + ')' ) );
            if ( track.type() == K3bDevice::Track::AUDIO )
                sessionItem->setPixmap( 0, SmallIcon( "audio-x-generic" ) );
            else
                sessionItem->setPixmap( 0, SmallIcon( "application-x-tar" ) );

            d->sessions.insert( sessionItem, SessionInfo( lastSession, medium.device() ) );

            // we have a session item, there is no need to select the medium as a whole
            mediumItem->setSelectable( false );
        }
    }

    mediumItem->setOpen( true );
}


void K3bDataMultisessionImportDialog::slotSelectionChanged()
{
    K3bListViewItem* selected = static_cast<K3bListViewItem*>( d->sessionView->selectedItem() );
    if ( selected ) {
        const SessionInfo& info = d->sessions[selected];
        showSessionInfo( info.device, info.sessionNumber );
        enableButton( Ok, true );
    }
    else {
        showSessionInfo( 0, 0 );
        enableButton( Ok, false );
    }
}


void K3bDataMultisessionImportDialog::showSessionInfo( K3bDevice::Device* dev, int session )
{
    // FIXME: show some information about the selected session
    if ( dev ) {

    }
    else {

    }
}


K3bDataMultisessionImportDialog::K3bDataMultisessionImportDialog( QWidget* parent )
  : KDialog( parent),
    d( new Private() )
{
    QWidget *widget = new QWidget();
    setMainWidget(widget);
    setButtons(Ok|Cancel);
    setDefaultButton(Ok);
    setModal(true);
    setCaption(i18n("Session Import"));
    Q3VBoxLayout* layout = new Q3VBoxLayout( widget );
    layout->setMargin( 0 );
    layout->setAutoAdd( true );

    ( void )new QLabel( i18n( "Please select a session to import." ), widget );
    d->sessionView = new K3bListView( widget );
    d->sessionView->addColumn( "1" );
    d->sessionView->header()->hide();
    d->sessionView->setFullWidth( true );

    connect( k3bappcore->mediaCache(), SIGNAL(mediumChanged(K3bDevice::Device*)),
             this, SLOT(updateMedia()) );
    connect( d->sessionView, SIGNAL( selectionChanged() ), this, SLOT( slotSelectionChanged() ) );
    connect(this,SIGNAL(okClicked()),this,SLOT(slotOk()));
    connect(this,SIGNAL(cancelClicked()),this,SLOT(slotCancel()));
}


K3bDataMultisessionImportDialog::~K3bDataMultisessionImportDialog()
{
    delete d;
}

#include "k3bdatamultisessionimportdialog.moc"
