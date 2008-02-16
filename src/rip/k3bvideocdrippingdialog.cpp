/*
*
* Copyright (C) 2003 Christian Kvasny <chris@k3b.org>
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


// kde include
#include <klocale.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kurlrequester.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

// qt includes
#include <q3groupbox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qlayout.h>
#include <qtooltip.h>

#include <qdir.h>
#include <qfileinfo.h>
#include <qstringlist.h>

//Added by qt3to4:
#include <Q3GridLayout>

// k3b includes
#include "k3bvideocdrippingdialog.h"
#include "k3bvideocdrip.h"

#include <k3bjobprogressdialog.h>
#include <k3bcore.h>
#include <k3bglobals.h>
#include <k3bstdguiitems.h>
#include <kvbox.h>

K3bVideoCdRippingDialog::K3bVideoCdRippingDialog( K3bVideoCdRippingOptions* options, QWidget* parent )
  : K3bInteractionDialog( parent,
			  i18n( "Video CD Ripping" ),
			  QString(),
			  START_BUTTON|CANCEL_BUTTON,
			  START_BUTTON,
			  "Video CD Ripping" ), // config group
    m_videooptions( options )
{
  setupGui();
  setupContextHelp();
}


K3bVideoCdRippingDialog::~K3bVideoCdRippingDialog()
{
}


void K3bVideoCdRippingDialog::setupGui()
{
    QWidget * frame = mainWidget();
    Q3GridLayout* MainLayout = new Q3GridLayout( frame );
    MainLayout->setSpacing( KDialog::spacingHint() );
    MainLayout->setMargin( 0 );

    // ---------------------------------------------------- Directory group ---
    Q3GroupBox* groupDirectory = new Q3GroupBox( 0, Qt::Vertical, i18n( "Destination Directory" ), frame );
    groupDirectory->layout() ->setSpacing( KDialog::spacingHint() );
    groupDirectory->layout() ->setMargin( KDialog::marginHint() );

    Q3GridLayout* groupDirectoryLayout = new Q3GridLayout( groupDirectory->layout() );
    groupDirectoryLayout->setAlignment( Qt::AlignTop );

    QLabel* rippathLabel = new QLabel( i18n( "Rip files to:" ), groupDirectory );
    m_editDirectory = new KUrlRequester( groupDirectory );
    m_editDirectory->setUrl( QDir::homePath() );
    m_editDirectory->setMode( KFile::Directory | KFile::ExistingOnly | KFile::LocalOnly );

    rippathLabel->setBuddy( m_editDirectory );

    KHBox* freeSpaceBox = new KHBox( groupDirectory );
    freeSpaceBox->setSpacing( KDialog::spacingHint() );
    ( void ) new QLabel( i18n( "Free space in directory:" ), freeSpaceBox, "FreeSpaceLabel" );
    m_labelFreeSpace = new QLabel( "                       ", freeSpaceBox, "m_labelFreeSpace" );
    m_labelFreeSpace->setAlignment( int( Qt::AlignVCenter | Qt::AlignRight ) );

    KHBox* necessarySizeBox = new KHBox( groupDirectory );
    necessarySizeBox->setSpacing( KDialog::spacingHint() );
    ( void ) new QLabel( i18n( "Necessary storage size:" ), necessarySizeBox, "StorSize" );
    m_labelNecessarySize = new QLabel( "                        ", necessarySizeBox, "m_labelNecessarySize" );
    m_labelNecessarySize->setAlignment( int( Qt::AlignVCenter | Qt::AlignRight ) );


    groupDirectoryLayout->addWidget( rippathLabel, 0, 0 );
    groupDirectoryLayout->addWidget( m_editDirectory, 0, 1 );
    groupDirectoryLayout->addWidget( freeSpaceBox, 1, 1 );
    groupDirectoryLayout->addWidget( necessarySizeBox, 2, 1 );

    // ---------------------------------------------------- Options group ---
    Q3GroupBox* groupOptions = new Q3GroupBox( 4, Qt::Vertical, i18n( "Settings" ), frame );

    m_ignoreExt = new QCheckBox( i18n( "Ignore /EXT/PSD_X.VCD" ), groupOptions );

    m_sector2336 = new QCheckBox( i18n( "Use 2336 byte sector mode for image file" ), groupOptions );
    // Only available for image file ripping
    m_sector2336->setEnabled( false );
    m_sector2336->setChecked( false );

    m_extractXML = new QCheckBox( i18n( "Extract XML structure" ), groupOptions );


    MainLayout->addWidget( groupDirectory, 0, 0 );
    MainLayout->addWidget( groupOptions, 1, 0 );
    MainLayout->setRowStretch( 0, 1 );

    setStartButtonText( i18n( "Start Ripping" ), i18n( "Starts extracting the selected VideoCd tracks" ) );
    // ----------------------------------------------------------------------------------

    connect( m_editDirectory, SIGNAL(textChanged(const QString&)), this, SLOT(slotUpdateFreeSpace()) );

    m_labelNecessarySize ->setText( KIO::convertSize( m_videooptions ->getVideoCdSize() ) );    
}


void K3bVideoCdRippingDialog::setupContextHelp()
{
    m_labelFreeSpace->setToolTip( i18n("Free space on destination directory: %1", m_editDirectory ->url().url() ) );

    m_labelNecessarySize->setToolTip( i18n("Necessary space for extracted files") );

    m_ignoreExt->setToolTip( i18n("Ignore extended PSD") );
    m_ignoreExt->setWhatsThis( i18n("<p>Ignore extended PSD (located in the ISO-9660 filesystem under `/EXT/PSD_X.VCD') and use the <em>standard</em> PSD.</p>") );

    m_sector2336->setToolTip( i18n("Assume a 2336-byte sector mode") );
    m_sector2336->setWhatsThis( i18n("<p>This option only makes sense if you are reading from a BIN CD disk image. This indicates to `vcdxrip' to assume a 2336-byte sector mode for image file.</p>"
                                                            "<b>Note: This option is slated to disappear.</b>") );

    m_extractXML->setToolTip( i18n("Create XML description file.") );
    m_extractXML->setWhatsThis( i18n("<p>This option creates an XML description file with all video CD information.</p>"
					"<p>This file will always contain all of the information.</p>"
					"<p>Example: If you only extract sequences, the description file will also hold the information for files and segments.</p>"
					"<p>The filename is the same as the video CD name, with a .xml extension. The default is VIDEOCD.xml.</p>") );
}

void K3bVideoCdRippingDialog::slotStartClicked()
{

    QStringList filesExists;
    QDir d;
    d.setPath( m_editDirectory ->url().url() );
    if( !d.exists() ) {
      if( KMessageBox::warningYesNo( this, i18n("Image folder '%1' does not exist. Do you want K3b to create it?", m_editDirectory->url().url() ) )
	  == KMessageBox::Yes ) {
	if( !KStandardDirs::makeDir( m_editDirectory->url().url() ) ) {
	  KMessageBox::error( this, i18n("Failed to create folder '%1'.", m_editDirectory->url().url() ) );
	  return;
	}
      }
    }
    foreach( QFileInfo fi, d.entryInfoList() )
    {
       if ( fi.fileName() != "." && fi .fileName() != ".." )
           filesExists.append( QString( "%1 (%2)" ).arg( QString(QFile::encodeName( fi.fileName() )) ).arg( KIO::convertSize( fi.size() ) ) );
    } 

    if( !filesExists.isEmpty() )
        if( KMessageBox::questionYesNoList( this,
                                i18n("Continue although the folder is not empty?"),
                                filesExists,
                                i18n("Files Exist"),KStandardGuiItem::cont(),KStandardGuiItem::cancel() ) == KMessageBox::No )
        return;

    m_videooptions ->setVideoCdIgnoreExt( m_ignoreExt ->isChecked() );
    m_videooptions ->setVideoCdSector2336( m_sector2336 ->isChecked() );
    m_videooptions ->setVideoCdExtractXml( m_extractXML ->isChecked() );
    m_videooptions ->setVideoCdDestination( m_editDirectory ->url().url() );

    K3bJobProgressDialog ripDialog( kapp->mainWidget(), "Ripping" );
    K3bVideoCdRip * rip = new K3bVideoCdRip( &ripDialog, m_videooptions );

    hide();
    ripDialog.startJob( rip );

    delete rip;

    close();
}

void K3bVideoCdRippingDialog::slotFreeSpace(const QString&,
						  unsigned long,
						  unsigned long,
						  unsigned long kbAvail)
{
    m_labelFreeSpace->setText( KIO::convertSizeFromKiB(kbAvail) );

    m_freeSpace = kbAvail;

    if( m_freeSpace < m_videooptions ->getVideoCdSize() /1024 )
        m_labelNecessarySize->setPaletteForegroundColor( Qt::red );
    else
        m_labelNecessarySize->setPaletteForegroundColor( m_labelFreeSpace->paletteForegroundColor() );

    QTimer::singleShot( 1000, this, SLOT(slotUpdateFreeSpace()) );
}


void K3bVideoCdRippingDialog::slotUpdateFreeSpace()
{
    QString path = m_editDirectory->url().url();

    if( !QFile::exists( path ) )
        path.truncate( path.lastIndexOf('/') );

    unsigned long size, avail;
    if( K3b::kbFreeOnFs( path, size, avail ) )
        slotFreeSpace( path, size, 0, avail );
    else
        m_labelFreeSpace->setText("-");
}

void K3bVideoCdRippingDialog::loadK3bDefaults()
{
    m_editDirectory->setUrl( QDir::homePath() );
    m_ignoreExt ->setChecked( false );
    m_sector2336 ->setChecked( false );
    m_extractXML ->setChecked( false );

    slotUpdateFreeSpace();
}

void K3bVideoCdRippingDialog::loadUserDefaults( const KConfigGroup& c )
{
    m_editDirectory ->setUrl( c.readEntry( "last ripping directory", QDir::homePath() ) );
    m_ignoreExt ->setChecked( c.readEntry( "ignore ext", false ) );
    m_sector2336 ->setChecked( c.readEntry( "sector 2336", false ) );
    m_extractXML ->setChecked( c.readEntry( "extract xml", false ) );

    slotUpdateFreeSpace();
}

void K3bVideoCdRippingDialog::saveUserDefaults( KConfigGroup& c )
{
    c.writeEntry( "last ripping directory", m_editDirectory->url() );
    c.writeEntry( "ignore ext", m_ignoreExt ->isChecked( ) );
    c.writeEntry( "sector 2336", m_sector2336 ->isChecked( ) );
    c.writeEntry( "extract xml", m_extractXML ->isChecked( ) );
}

#include "k3bvideocdrippingdialog.moc"
