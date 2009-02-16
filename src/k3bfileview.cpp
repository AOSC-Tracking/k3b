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


#include "k3bfileview.h"
#include "k3b.h"
#include "k3bdiroperator.h"
#include "k3bapplication.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KDirLister>
#include <KFileFilterCombo>
#include <KFileItem>
#include <KLocale>
#include <KProgressDialog>
#include <KToolBar>
#include <KUrl>

K3bFileView::K3bFileView(QWidget *parent )
    : K3bContentsView( false, parent)
{
    setupGUI();
}


K3bFileView::~K3bFileView()
{
}


KActionCollection* K3bFileView::actionCollection() const
{
    return m_dirOp->actionCollection();
}


void K3bFileView::setupGUI()
{
    m_dirOp = new K3bDirOperator( KUrl(QDir::home().absolutePath()), this );
    m_toolBox = new KToolBar( this );
    m_toolBox->setToolButtonStyle( Qt::ToolButtonIconOnly );

    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->setMargin( 0 );
    layout->setSpacing( 0 );
    layout->addWidget( m_toolBox );
    layout->addWidget( m_dirOp, 1 );

    // setup actions
    QAction* actionBack = m_dirOp->actionCollection()->action("back");
    QAction* actionForward = m_dirOp->actionCollection()->action("forward");
    QAction* actionUp = m_dirOp->actionCollection()->action("up");
    QAction* actionReload = m_dirOp->actionCollection()->action("reload");

    m_toolBox->addAction( actionUp );
    m_toolBox->addAction( actionBack );
    m_toolBox->addAction( actionForward );
    m_toolBox->addAction( actionReload );
    m_toolBox->addSeparator();
    m_toolBox->addAction( m_dirOp->actionCollection()->action("short view") );
    m_toolBox->addAction( m_dirOp->actionCollection()->action("detailed view") );
    m_toolBox->addSeparator();
    m_toolBox->addAction( m_dirOp->bookmarkMenu() );
    m_toolBox->addSeparator();

    // create filter selection combobox
    QWidget* filterBox = new QWidget( m_toolBox );
    QHBoxLayout* filterLayout = new QHBoxLayout( filterBox );
    filterLayout->addWidget( new QLabel( i18n("Filter:"), filterBox ) );
    m_filterWidget = new KFileFilterCombo( filterBox );
    filterLayout->addWidget( m_filterWidget );
    m_toolBox->addWidget( filterBox );

    m_filterWidget->setEditable( true );
    QString filter = i18n("*|All Files");
    filter += "\n" + i18n("audio/x-mp3 audio/x-wav application/x-ogg |Sound Files");
    filter += "\n" + i18n("audio/x-wav |Wave Sound Files");
    filter += "\n" + i18n("audio/x-mp3 |MP3 Sound Files");
    filter += "\n" + i18n("application/x-ogg |Ogg Vorbis Sound Files");
    filter += "\n" + i18n("video/mpeg |MPEG Video Files");
    m_filterWidget->setFilter(filter);

    connect( m_filterWidget, SIGNAL(filterChanged()), SLOT(slotFilterChanged()) );

    connect( m_dirOp, SIGNAL(fileHighlighted(const KFileItem &)), this, SLOT(slotFileHighlighted(const KFileItem &)) );
    connect( m_dirOp, SIGNAL(urlEntered(const KUrl&)), this, SIGNAL(urlEntered(const KUrl&)) );
}


void K3bFileView::setDir( const QString& dir )
{
    KUrl url;
    url.setPath(dir);
    setUrl( url );
}


void K3bFileView::setUrl(const KUrl& url, bool forward)
{
    m_dirOp->setUrl( url, forward );
}


KUrl K3bFileView::url()
{
    return m_dirOp->url();
}


void K3bFileView::setAutoUpdate( bool b )
{
    m_dirOp->dirLister()->setAutoUpdate( b );
}


void K3bFileView::slotFileHighlighted( const KFileItem & )
{
}


void K3bFileView::slotFilterChanged()
{
    QString filter = m_filterWidget->currentFilter();
    m_dirOp->clearFilter();

    if( filter.indexOf( '/' ) > -1 ) {
        QStringList types = filter.split( ' ' );
        types.prepend( "inode/directory" );
        m_dirOp->setMimeFilter( types );
    }
    else
        m_dirOp->setNameFilter( filter );

    m_dirOp->rereadDir();
}


void K3bFileView::reload()
{
    m_dirOp->rereadDir();
}


void K3bFileView::saveConfig( KConfigGroup &grp )
{
    m_dirOp->writeConfig(grp);
}


void K3bFileView::readConfig( const KConfigGroup& grp )
{
    m_dirOp->readConfig(grp);
}

#include "k3bfileview.moc"
