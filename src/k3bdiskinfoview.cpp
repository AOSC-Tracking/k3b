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

#include <config-k3b.h>


#include "k3bdiskinfoview.h"
#include "k3bthememanager.h"
#include "k3bapplication.h"

#include <k3bdiskinfo.h>
#include <k3bcdtext.h>
#include <k3bdeviceglobals.h>
#include <k3bglobals.h>
#include <k3biso9660.h>

#include <qlabel.h>
#include <qlayout.h>
#include <qfont.h>
#include <qcolor.h>
#include <qstring.h>
#include <qpainter.h>
#include <qpalette.h>
#include <qpixmap.h>
#include <QtGui/QTextBrowser>
#include <QtWebKit/QWebView>

#include <klocale.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kio/global.h>
#include <libkcddb/cdinfo.h>


namespace {
    QString sectionHeader( const QString& title ) {
        return "<h1 class=\"title\">" + title + "</h1>";
    }

    QString tableRow( const QString& s1, const QString& s2 ) {
        return "<tr><td class=\"infokey\">" + s1 + "</td><td class=\"infovalue\">" + s2 + "</td></tr>";
    }
}


K3bDiskInfoView::K3bDiskInfoView( QWidget* parent )
    : K3bMediaContentsView( true,
                            K3bMedium::CONTENT_ALL,
                            K3bDevice::MEDIA_ALL|K3bDevice::MEDIA_UNKNOWN,
                            K3bDevice::STATE_EMPTY|K3bDevice::STATE_INCOMPLETE|K3bDevice::STATE_COMPLETE|K3bDevice::STATE_UNKNOWN,
                            parent )
{
    m_infoView = new QWebView( this );
    setMainWidget( m_infoView );
}


K3bDiskInfoView::~K3bDiskInfoView()
{
}


void K3bDiskInfoView::reloadMedium()
{
    updateTitle();

    QString s;
    if( medium().diskInfo().diskState() == K3bDevice::STATE_NO_MEDIA ) {
        s = i18n("No medium present");
    }
    else {
        // FIXME: import a css stylesheet from the theme and, more importantly, create a nicer css
        s = "<head>"
            "<title></title>"
            "<style type=\"text/css\">";
        if( K3bTheme* theme = k3bappcore->themeManager()->currentTheme() ) {
            s += QString( ".title { padding:4px; font-size:medium; background-color:%1; foreground-color:%2 } " )
                 .arg(theme->palette().color( QPalette::Background ).name() )
                 .arg(theme->palette().color( QPalette::Foreground ).name() );
        }
        s +=  ".infovalue { font-weight:bold; padding-left:10px; } "
              ".trackheader { text-align:left; } "
              ".session { font-style:italic; } "
              ".cdtext { font-weight:bold; font-style:italic; } ";
        s += "</style>"
             "</head>"
             "<body>";

        s += sectionHeader( i18n( "Medium" ) );
        s += createMediaInfoItems( medium() );

        if( medium().content() & K3bMedium::CONTENT_DATA ) {
            s += sectionHeader( i18n("ISO9660 Filesystem Info") );
            s += createIso9660InfoItems( medium().iso9660Descriptor() );
        }

        if( !medium().toc().isEmpty() ) {
            s += sectionHeader( i18n("Tracks") );
            s += createTrackItems( medium() );
        }
    }

    s += "</body>";

    m_infoView->setHtml( s );
    kDebug() << s;
}


void K3bDiskInfoView::updateTitle()
{
    setTitle( medium().shortString( true ) );

    if( medium().diskInfo().diskState() == K3bDevice::STATE_NO_MEDIA ) {
        setRightPixmap( K3bTheme::MEDIA_NONE );
    }
    else {
        if( medium().diskInfo().empty() ) {
            setRightPixmap( K3bTheme::MEDIA_EMPTY );
        }
        else {
            switch( medium().toc().contentType() ) {
            case K3bDevice::AUDIO:
                setRightPixmap( K3bTheme::MEDIA_AUDIO );
                break;
            case K3bDevice::DATA: {
                if( medium().content() & K3bMedium::CONTENT_VIDEO_DVD ) {
                    setRightPixmap( K3bTheme::MEDIA_VIDEO );
                }
                else {
                    setRightPixmap( K3bTheme::MEDIA_DATA );
                }
                break;
            }
            case K3bDevice::MIXED:
                setRightPixmap( K3bTheme::MEDIA_MIXED );
                break;
            default:
                setRightPixmap( K3bTheme::MEDIA_NONE );
            }
        }
    }
}


namespace {
    QString trackItem( const K3bMedium& medium, int trackIndex ) {
        K3bDevice::Track track = medium.toc()[trackIndex];

        QString s = "<tr>";

        // FIXME: Icons

        // track number
        s += "<td class=\"tracknumber\">" + QString::number(trackIndex+1).rightJustified( 2, ' ' );
        s += "</td>";

        // track type
        s += "<td class=\"tracktype\">(";
        if( track.type() == K3bTrack::AUDIO ) {
            //    item->setPixmap( 0, SmallIcon( "audio-x-generic" ) );
            s += i18n("Audio");
        } else {
//            item->setPixmap( 0, SmallIcon( "application-x-tar" ) );
            if( track.mode() == K3bTrack::MODE1 )
                s += i18n("Data/Mode1");
            else if( track.mode() == K3bTrack::MODE2 )
                s += i18n("Data/Mode2");
            else if( track.mode() == K3bTrack::XA_FORM1 )
                s += i18n("Data/Mode2 XA Form1");
            else if( track.mode() == K3bTrack::XA_FORM2 )
                s += i18n("Data/Mode2 XA Form2");
            else
                s += i18n("Data");
        }
        s += ")</td>";

        // track attributes
        s += "<td class=\"trackattributes\">";
        s += QString( "%1/%2" )
             .arg( track.copyPermitted() ? i18n("copy") : i18n("no copy") )
             .arg( track.type() == K3bTrack::AUDIO
                   ? ( track.preEmphasis() ?  i18n("preemp") : i18n("no preemp") )
                   : ( track.recordedIncremental() ?  i18n("incremental") : i18n("uninterrupted") ) );
        s += "</td>";

        // track range
        s += "<td class=\"trackrange\">";
        s += QString("%1 - %2")
             .arg(track.firstSector().lba())
             .arg(track.lastSector().lba());
        s += "</td>";

        // track length
        s += "<td class=\"tracklength\">" + QString::number( track.length().lba() ) + " (" + track.length().toString() + ")</td>";

#ifdef K3B_DEBUG
        s += "<td>";
        if( track.type() == K3bTrack::AUDIO )
            s += QString( "%1 (%2)" ).arg(track.index0().toString()).arg(track.index0().lba());
        s += "</td>";
#endif

        s += "</tr>";

        if( !medium.cdText().isEmpty() ||
            medium.cddbInfo().isValid() ) {
            s += "<tr><td></td><td class=\"cdtext\" colspan=\"4\">";
            if( !medium.cdText().isEmpty() ) {
                K3bDevice::TrackCdText text = medium.cdText().track( trackIndex );
                s += text.performer() + " - " + text.title() + " (" + i18n("CD-Text") + ')';
            }
            else {
                KCDDB::TrackInfo info = medium.cddbInfo().track( trackIndex );
                s += info.get( KCDDB::Artist ).toString() + " - " + info.get( KCDDB::Title ).toString();
            }
            s += "</td></tr>";
        }

        return s;
    }

    QString sessionItem( int session ) {
        return "<tr><td class=\"session\" colspan=\"5\">" + i18n( "Session %1", session ) + "</td></tr>";
    }
}

QString K3bDiskInfoView::createTrackItems( const K3bMedium& medium )
{
    QString s = "<table>";

    // table header
    s += "<tr><th class=\"trackheader\" colspan=\"2\">"
         + i18n("Type")
         + "</th><th class=\"trackheader\">"
         + i18n("Attributes")
         + "</th><th class=\"trackheader\">"
         + i18n("First-Last Sector")
         + "</th><th class=\"trackheader\">"
         + i18n("Length");
#ifdef K3B_DEBUG
    s += "</td></td>Index0";
#endif
    s += "</th></tr>";

    int lastSession = 0;

    // if we have multiple sessions we create a header item for every session
    if( medium.diskInfo().numSessions() > 1 && medium.toc()[0].session() > 0 ) {
        s += sessionItem( 1 );
        lastSession = 1;
    }

    // create items for the tracks
    for( int index = 0; index < medium.toc().count(); ++index ) {
        K3bDevice::Track track = medium.toc()[index];
        if( medium.diskInfo().numSessions() > 1 && track.session() != lastSession ) {
            lastSession = track.session();
            s += sessionItem( lastSession );
        }
        s += trackItem( medium, index );
    }

    s += "</table>";

    return s;
}


QString K3bDiskInfoView::createMediaInfoItems( const K3bMedium& medium )
{
    const K3bDevice::DiskInfo& info = medium.diskInfo();

    QString s = "<table>";

    QString typeStr;
    if( info.mediaType() != K3bDevice::MEDIA_UNKNOWN )
        typeStr = K3bDevice::mediaTypeString( info.mediaType() );
    else
        typeStr = i18n("Unknown (probably CD-ROM)");
    s += tableRow( i18n( "Type:" ), typeStr );
    if( info.isDvdMedia() )
        s += tableRow( i18n("Media ID:"), !info.mediaId().isEmpty() ? QString::fromLatin1( info.mediaId() ) : i18n("unknown") );
    s += tableRow( i18n("Capacity:"), i18n("%1 min",info.capacity().toString()) + " (" + KIO::convertSize(info.capacity().mode1Bytes()) + ')' );
    if( !info.empty() )
        s += tableRow( i18n("Used Capacity:"), i18n("%1 min",info.size().toString()) + " (" + KIO::convertSize(info.size().mode1Bytes()) + ')' );
    if( info.appendable() )
        s += tableRow( i18n("Remaining:"), i18n("%1 min",info.remainingSize().toString() ) + " (" + KIO::convertSize(info.remainingSize().mode1Bytes()) + ')' );
    s += tableRow( i18n("Rewritable:"), info.rewritable() ? i18nc("Availability", "yes") : i18nc("Availability", "no") );
    s += tableRow( i18n("Appendable:"), info.appendable() ? i18nc("Availability", "yes") : i18nc("Availability", "no") );
    s += tableRow( i18n("Empty:"), info.empty() ? i18nc("Availability", "yes") : i18nc("Availability", "no") );
    if( !( info.mediaType() & K3bDevice::MEDIA_CD_ALL ) )
        s += tableRow( i18nc("Number of layers on an optical medium", "Layers:"), QString::number( info.numLayers() ) );

    if( info.mediaType() == K3bDevice::MEDIA_DVD_PLUS_RW ) {
        QString bgf;
        switch( info.bgFormatState() ) {
        case K3bDevice::BG_FORMAT_NONE:
            bgf = i18n("not formatted");
            break;
        case K3bDevice::BG_FORMAT_INCOMPLETE:
            bgf = i18n("incomplete");
            break;
        case K3bDevice::BG_FORMAT_IN_PROGRESS:
            bgf = i18n("in progress");
            break;
        case K3bDevice::BG_FORMAT_COMPLETE:
            bgf = i18n("complete");
            break;
        }

        s += tableRow( i18n("Background Format:"), bgf );
    }

    s += tableRow( i18n("Sessions:"), QString::number( info.numSessions() ) );

    if( info.mediaType() & K3bDevice::MEDIA_WRITABLE ) {
        QString speedStr;
        if( medium.writingSpeeds().isEmpty() ) {
            speedStr = "-";
        }
        else {
            foreach( int speed, medium.writingSpeeds() ) {
                if( !speedStr.isEmpty() ) {
                    speedStr.append( "<br>" );
                }

                if( K3bDevice::isCdMedia( info.mediaType() ) )
                    speedStr.append( QString( "%1x (%2 KB/s)" ).arg( speed/K3bDevice::SPEED_FACTOR_CD ).arg( speed ) );
                else if( K3bDevice::isDvdMedia( info.mediaType() ) )
                    speedStr.append( QString().sprintf( "%.1fx (%d KB/s)", (double)speed / ( double )K3bDevice::SPEED_FACTOR_DVD, speed ) );
                else if ( K3bDevice::isBdMedia( info.mediaType() ) )
                    speedStr.append( QString().sprintf( "%.1fx (%d KB/s)", (double)speed / ( double )K3bDevice::SPEED_FACTOR_BD, speed ) );
            }
        }

        s += tableRow( i18n("Supported writing speeds:"), speedStr );
    }

    s += "</table>";

    return s;
}


QString K3bDiskInfoView::createIso9660InfoItems( const K3bIso9660SimplePrimaryDescriptor& iso )
{
    QString s = "<table>";

    s += tableRow( i18n("System Id:"), iso.systemId.isEmpty() ? QString("-") : iso.systemId );
    s += tableRow( i18n("Volume Id:"), iso.volumeId.isEmpty() ? QString("-") : iso.volumeId );
    s += tableRow( i18n("Volume Set Id:"), iso.volumeSetId.isEmpty() ? QString("-") : iso.volumeSetId );
    s += tableRow( i18n("Publisher Id:"), iso.publisherId.isEmpty() ? QString("-") : iso.publisherId );
    s += tableRow( i18n("Preparer Id:"), iso.preparerId.isEmpty() ? QString("-") : iso.preparerId );
    s += tableRow( i18n("Application Id:"), iso.applicationId.isEmpty() ? QString("-") : iso.applicationId );
    s += tableRow( i18n("Volume Size:"),
                   QString( "%1 (%2 * %3 = %4)" )
                   .arg(KIO::convertSize(iso.logicalBlockSize*iso.volumeSpaceSize))
                   .arg(i18nc( "Size of one block, always 2048", "%1 B", iso.logicalBlockSize) )
                   .arg(i18ncp( "Number of blocks (one block has 2048 bytes)", "1 block", "%1 blocks", iso.volumeSpaceSize) )
                   .arg(i18np( "1 B", "%1 B", iso.logicalBlockSize*iso.volumeSpaceSize ) ) );

    s += "</table>";

    return s;
}

#include "k3bdiskinfoview.moc"

