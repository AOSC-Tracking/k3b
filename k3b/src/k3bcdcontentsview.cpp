/* 
 *
 * $Id$
 * Copyright (C) 2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2004 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include "k3bcdcontentsview.h"

#include <k3bthemedheader.h>

#include <qlabel.h>
#include <qlayout.h>
#include <qpixmap.h>



K3bCdContentsView::K3bCdContentsView( bool withHeader,
				      QWidget* parent, const char* name )
  : QWidget( parent, name ),
    m_centerWidget(0)
{
  if( withHeader ) {
    QGridLayout* mainGrid = new QGridLayout( this );
    mainGrid->setMargin( 2 );
    mainGrid->setSpacing( 0 );

    m_header = new K3bThemedHeader( this );
    mainGrid->addWidget( m_header, 0, 0 );

    m_header->setLeftPixmap( K3bTheme::MEDIA_LEFT );
    m_header->setRightPixmap( K3bTheme::MEDIA_NONE );
  }
}


K3bCdContentsView::~K3bCdContentsView()
{
}


QWidget* K3bCdContentsView::mainWidget()
{
  if( !m_centerWidget )
    setMainWidget( new QWidget( this ) );
  return m_centerWidget;
}


void K3bCdContentsView::reload()
{
}


void K3bCdContentsView::setMainWidget( QWidget* w )
{
  m_centerWidget = w;
  ((QGridLayout*)layout())->addWidget( w, 1, 0 );
}


void K3bCdContentsView::setTitle( const QString& s )
{
  m_header->setTitle( s );
}


void K3bCdContentsView::setLeftPixmap( K3bTheme::PixmapType s )
{
  m_header->setLeftPixmap( s );
}


void K3bCdContentsView::setRightPixmap( K3bTheme::PixmapType s )
{
  m_header->setRightPixmap( s );
}

#include "k3bcdcontentsview.moc"
