/* 
 *
 * $Id$
 * Copyright (C) 2002 Thomas Froescher <tfroescher@k3b.org>
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#include "k3bsonglistparser.h"
#include "k3bsongmanager.h"
#include "k3bsongcontainer.h"
#include "k3bsong.h"

#include <qobject.h>
#include <kdebug.h>

K3bSongListParser::K3bSongListParser( K3bSongManager *manager )
  : QXmlDefaultHandler() 
{
  m_manager = manager;
}

K3bSongListParser::~K3bSongListParser()
{
}

bool K3bSongListParser::startDocument()
{
  m_level = 0;
  return TRUE;
}

bool K3bSongListParser::startElement( const QString&, 
				      const QString&, 
				      const QString& qName, 
				      const QXmlAttributes& attr ){
  switch( m_level ) {
  case 0:
    kdDebug() << "Version: " << attr.value("version") << endl; // printout version
    break;

  case 1:
    {
      m_container = m_manager->getContainer( attr.value("basepath") ); // container
      if( m_container == 0 )
	kdDebug() << "(K3bSongListParser) ERROR: Found no entry for song container " 
		  << attr.value("basepath") << endl;
      break;
    }

  case 2: 
    {
      m_song = m_container->findSong( attr.value("filename") ); // song
      if( m_song == 0 ) {
	K3bSong* newSong = new K3bSong();
	newSong->setFilename( attr.value("filename") );
	newSong->setTrackNumber( attr.value("tracknumber").toInt() );
	newSong->setDiscId( attr.value("discid") );
	m_song = m_container->addSong( newSong );
      } 
      else {
	kdDebug() << "(K3bSongListParser) ERROR: Found no song for file " << attr.value("filename") << endl;
      }
      break;
    }

  case 3:
    m_contentTag = qName;
    break;

  default:
    break;
  }

  m_level++;
  return TRUE;
}

bool K3bSongListParser::endElement( const QString&, const QString&, const QString& ) 
{
  m_level--;
  return TRUE;
}

bool K3bSongListParser::characters( const QString& content ) 
{
  QString con = content.stripWhiteSpace();
  if( !con.isEmpty() ) {
    m_song->addContent( m_contentTag, con );
  }
  return TRUE;
}
