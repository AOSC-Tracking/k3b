/*
 *
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
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


#include "k3bmovixdoc.h"
#include "k3bmovixjob.h"
#include "k3bmovixfileitem.h"

#include <k3bdiritem.h>
#include <k3bfileitem.h>
#include <k3bglobals.h>

#include <klocale.h>
#include <kdebug.h>
#include <kurl.h>
#include <kinputdialog.h>
#include <kmessagebox.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kapplication.h>

#include <qdom.h>
#include <qfileinfo.h>
#include <kglobal.h>


K3bMovixDoc::K3bMovixDoc( QObject* parent )
    : K3bDataDoc( parent )
{
    connect( this, SIGNAL(itemRemoved(K3bDataItem*)),
             this, SLOT(slotDataItemRemoved(K3bDataItem*)) );
}


K3bMovixDoc::~K3bMovixDoc()
{
}


K3bBurnJob* K3bMovixDoc::newBurnJob( K3bJobHandler* hdl, QObject* parent )
{
    return new K3bMovixJob( this, hdl, parent );
}


bool K3bMovixDoc::newDocument()
{
    m_loopPlaylist = 1;
    m_ejectDisk = false;
    m_reboot = false;
    m_shutdown = false;
    m_randomPlay = false;

    return K3bDataDoc::newDocument();
}


void K3bMovixDoc::addUrls( const KUrl::List& urls )
{
    for( KUrl::List::ConstIterator it = urls.begin(); it != urls.end(); ++it ) {
        addMovixFile( *it );
    }

    emit newMovixFileItems();
}


void K3bMovixDoc::addMovixFile( const KUrl& _url, int pos )
{
    KUrl url = K3b::convertToLocalUrl( _url );

    QFileInfo f( url.path() );
    if( !f.isFile() || !url.isLocalFile() )
        return;

    QString newName = f.fileName();
    if( nameAlreadyInDir( newName, root() ) ) {
        KConfigGroup dataProjectSettings = KGlobal::config()->group("Data project settings");

        bool dropDoubles = dataProjectSettings.readEntry( "Drop doubles", false );
        if( dropDoubles )
            return;

        bool ok = true;
        do {
            newName = KInputDialog::getText( i18n("Enter New Filename"),
                                             i18n("A file with that name already exists. Please enter a new name:"),
                                             newName, &ok, 0 );
        } while( ok && nameAlreadyInDir( newName, root() ) );

        if( !ok )
            return;
    }

    K3bMovixFileItem* newK3bItem = new K3bMovixFileItem( f.absoluteFilePath(), this, root(), newName );
    if( pos < 0 || pos > (int)m_movixFiles.count() )
        pos = m_movixFiles.count();

    m_movixFiles.insert( pos, newK3bItem );

    emit newMovixFileItems();

    setModified(true);
}


bool K3bMovixDoc::loadDocumentData( QDomElement* rootElem )
{
    if( !root() )
        newDocument();

    QDomNodeList nodes = rootElem->childNodes();

    if( nodes.item(0).nodeName() != "general" ) {
        kDebug() << "(K3bMovixDoc) could not find 'general' section.";
        return false;
    }
    if( !readGeneralDocumentData( nodes.item(0).toElement() ) )
        return false;


    // parse options
    // -----------------------------------------------------------------
    if( nodes.item(1).nodeName() != "data_options" ) {
        kDebug() << "(K3bMovixDoc) could not find 'data_options' section.";
        return false;
    }
    if( !loadDocumentDataOptions( nodes.item(1).toElement() ) )
        return false;
    // -----------------------------------------------------------------



    // parse header
    // -----------------------------------------------------------------
    if( nodes.item(2).nodeName() != "data_header" ) {
        kDebug() << "(K3bMovixDoc) could not find 'data_header' section.";
        return false;
    }
    if( !loadDocumentDataHeader( nodes.item(2).toElement() ) )
        return false;
    // -----------------------------------------------------------------



    // parse movix options
    // -----------------------------------------------------------------
    if( nodes.item(3).nodeName() != "movix_options" ) {
        kDebug() << "(K3bMovixDoc) could not find 'movix_options' section.";
        return false;
    }

    // load the options
    QDomNodeList optionList = nodes.item(3).childNodes();
    for( int i = 0; i < optionList.count(); i++ ) {

        QDomElement e = optionList.item(i).toElement();
        if( e.isNull() )
            return false;

        if( e.nodeName() == "shutdown")
            setShutdown( e.attributeNode( "activated" ).value() == "yes" );
        else if( e.nodeName() == "reboot")
            setReboot( e.attributeNode( "activated" ).value() == "yes" );
        else if( e.nodeName() == "eject_disk")
            setEjectDisk( e.attributeNode( "activated" ).value() == "yes" );
        else if( e.nodeName() == "random_play")
            setRandomPlay( e.attributeNode( "activated" ).value() == "yes" );
        else if( e.nodeName() == "no_dma")
            setNoDma( e.attributeNode( "activated" ).value() == "yes" );
        else if( e.nodeName() == "subtitle_fontset")
            setSubtitleFontset( e.text() );
        else if( e.nodeName() == "boot_message_language")
            setBootMessageLanguage( e.text() );
        else if( e.nodeName() == "audio_background")
            setAudioBackground( e.text() );
        else if( e.nodeName() == "keyboard_language")
            setKeyboardLayout( e.text() );
        else if( e.nodeName() == "codecs")
            setCodecs( QStringList::split( ',', e.text() ) );
        else if( e.nodeName() == "default_boot_label")
            setDefaultBootLabel( e.text() );
        else if( e.nodeName() == "additional_mplayer_options")
            setAdditionalMPlayerOptions( e.text() );
        else if( e.nodeName() == "unwanted_mplayer_options")
            setUnwantedMPlayerOptions( e.text() );
        else if( e.nodeName() == "loop_playlist")
            setLoopPlaylist( e.text().toInt() );
        else
            kDebug() << "(K3bMovixDoc) unknown movix option: " << e.nodeName();
    }
    // -----------------------------------------------------------------

    // parse files
    // -----------------------------------------------------------------
    if( nodes.item(4).nodeName() != "movix_files" ) {
        kDebug() << "(K3bMovixDoc) could not find 'movix_files' section.";
        return false;
    }

    // load file items
    QDomNodeList fileList = nodes.item(4).childNodes();
    for( int i = 0; i < fileList.count(); i++ ) {

        QDomElement e = fileList.item(i).toElement();
        if( e.isNull() )
            return false;

        if( e.nodeName() == "file" ) {
            if( !e.hasAttribute( "name" ) ) {
                kDebug() << "(K3bMovixDoc) found file tag without name attribute.";
                return false;
            }

            QDomElement urlElem = e.firstChild().toElement();
            if( urlElem.isNull() ) {
                kDebug() << "(K3bMovixDoc) found file tag without url child.";
                return false;
            }

            // create the item
            K3bMovixFileItem* newK3bItem = new K3bMovixFileItem( urlElem.text(),
                                                                 this,
                                                                 root(),
                                                                 e.attributeNode("name").value() );
            m_movixFiles.append( newK3bItem );

            // subtitle file?
            QDomElement subTitleElem = e.childNodes().item(1).toElement();
            if( !subTitleElem.isNull() && subTitleElem.nodeName() == "subtitle_file" ) {
                urlElem = subTitleElem.firstChild().toElement();
                if( urlElem.isNull() ) {
                    kDebug() << "(K3bMovixDoc) found subtitle_file tag without url child.";
                    return false;
                }

                QString name = K3bMovixFileItem::subTitleFileName( newK3bItem->k3bName() );
                K3bFileItem* subItem = new K3bFileItem( urlElem.text(), this, root(), name );
                newK3bItem->setSubTitleItem( subItem );
            }
        }
        else {
            kDebug() << "(K3bMovixDoc) found " << e.nodeName() << " node where 'file' was expected.";
            return false;
        }
    }
    // -----------------------------------------------------------------


    emit newMovixFileItems();

    return true;
}


bool K3bMovixDoc::saveDocumentData( QDomElement* docElem )
{
    QDomDocument doc = docElem->ownerDocument();

    saveGeneralDocumentData( docElem );

    QDomElement optionsElem = doc.createElement( "data_options" );
    saveDocumentDataOptions( optionsElem );

    QDomElement headerElem = doc.createElement( "data_header" );
    saveDocumentDataHeader( headerElem );

    QDomElement movixOptElem = doc.createElement( "movix_options" );
    QDomElement movixFilesElem = doc.createElement( "movix_files" );


    // save the movix options
    QDomElement propElem = doc.createElement( "shutdown" );
    propElem.setAttribute( "activated", shutdown() ? "yes" : "no" );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "reboot" );
    propElem.setAttribute( "activated", reboot() ? "yes" : "no" );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "eject_disk" );
    propElem.setAttribute( "activated", ejectDisk() ? "yes" : "no" );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "random_play" );
    propElem.setAttribute( "activated", randomPlay() ? "yes" : "no" );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "no_dma" );
    propElem.setAttribute( "activated", noDma() ? "yes" : "no" );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "subtitle_fontset" );
    propElem.appendChild( doc.createTextNode( subtitleFontset() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "boot_message_language" );
    propElem.appendChild( doc.createTextNode( bootMessageLanguage() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "audio_background" );
    propElem.appendChild( doc.createTextNode( audioBackground() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "keyboard_language" );
    propElem.appendChild( doc.createTextNode( keyboardLayout() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "codecs" );
    propElem.appendChild( doc.createTextNode( codecs().join(",") ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "default_boot_label" );
    propElem.appendChild( doc.createTextNode( defaultBootLabel() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "additional_mplayer_options" );
    propElem.appendChild( doc.createTextNode( additionalMPlayerOptions() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "unwanted_mplayer_options" );
    propElem.appendChild( doc.createTextNode( unwantedMPlayerOptions() ) );
    movixOptElem.appendChild( propElem );

    propElem = doc.createElement( "loop_playlist" );
    propElem.appendChild( doc.createTextNode( QString::number(loopPlaylist()) ) );
    movixOptElem.appendChild( propElem );


    // save the movix items
    Q_FOREACH( K3bMovixFileItem* item, m_movixFiles ) {
        QDomElement topElem = doc.createElement( "file" );
        topElem.setAttribute( "name", item->k3bName() );
        QDomElement urlElem = doc.createElement( "url" );
        urlElem.appendChild( doc.createTextNode( item->localPath() ) );
        topElem.appendChild( urlElem );
        if( item->subTitleItem() ) {
            QDomElement subElem = doc.createElement( "subtitle_file" );
            urlElem = doc.createElement( "url" );
            urlElem.appendChild( doc.createTextNode( item->subTitleItem()->localPath() ) );
            subElem.appendChild( urlElem );
            topElem.appendChild( subElem );
        }

        movixFilesElem.appendChild( topElem );
    }

    docElem->appendChild( optionsElem );
    docElem->appendChild( headerElem );
    docElem->appendChild( movixOptElem );
    docElem->appendChild( movixFilesElem );

    return true;
}


void K3bMovixDoc::slotDataItemRemoved( K3bDataItem* item )
{
    // check if it's a movix item
    if( K3bMovixFileItem* fi = dynamic_cast<K3bMovixFileItem*>(item) )
        if( m_movixFiles.contains( fi ) ) {
            emit movixItemRemoved( fi );
            m_movixFiles.removeAll( fi );
            setModified(true);
        }
}


int K3bMovixDoc::indexOf( K3bMovixFileItem* item )
{
    return m_movixFiles.lastIndexOf(item)+1;
}


void K3bMovixDoc::moveMovixItem( K3bMovixFileItem* item, K3bMovixFileItem* itemAfter )
{
    if( item == itemAfter )
        return;

    // take the current item
    item = m_movixFiles.takeAt( m_movixFiles.lastIndexOf( item ) );

    // if after == 0 lastIndexOf returnes -1
    int pos = m_movixFiles.lastIndexOf( itemAfter );
    m_movixFiles.insert( pos+1, item );

    emit newMovixFileItems();

    setModified(true);
}


void K3bMovixDoc::addSubTitleItem( K3bMovixFileItem* item, const KUrl& url )
{
    if( item->subTitleItem() )
        removeSubTitleItem( item );

    QFileInfo f( url.path() );
    if( !f.isFile() || !url.isLocalFile() )
        return;

    // check if there already is a file named like we want to name the subTitle file
    QString name = K3bMovixFileItem::subTitleFileName( item->k3bName() );

    if( nameAlreadyInDir( name, root() ) ) {
        KMessageBox::error( 0, i18n("Could not rename subtitle file. File with requested name %1 already exists.",name) );
        return;
    }

    K3bFileItem* subItem = new K3bFileItem( f.absoluteFilePath(), this, root(), name );
    item->setSubTitleItem( subItem );

    emit newMovixFileItems();

    setModified(true);
}


void K3bMovixDoc::removeSubTitleItem( K3bMovixFileItem* item )
{
    if( item->subTitleItem() ) {
        emit subTitleItemRemoved( item );

        delete item->subTitleItem();
        item->setSubTitleItem(0);

        setModified(true);
    }
}

#include "k3bmovixdoc.moc"
