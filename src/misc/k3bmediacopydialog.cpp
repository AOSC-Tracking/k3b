/*
 *
 * Copyright (C) 2007-2008 Sebastian Trueg <trueg@k3b.org>
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

#include "k3bmediacopydialog.h"

#include "k3bmediaselectioncombobox.h"

#include <k3bcdcopyjob.h>
#include <k3bclonejob.h>
#include <k3bdvdcopyjob.h>

#include <k3bwriterselectionwidget.h>
#include <k3btempdirselectionwidget.h>
#include <k3bcore.h>
#include <k3bstdguiitems.h>
#include <k3bdevice.h>
#include <k3bdevicemanager.h>
#include <k3bburnprogressdialog.h>
#include <k3bglobals.h>
#include <k3bexternalbinmanager.h>
#include <k3bthememanager.h>
#include <k3bwritingmodewidget.h>
#include <k3bapplication.h>
#include <k3bmediacache.h>

#include <kguiitem.h>
#include <klocale.h>
#include <kstdguiitem.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kconfig.h>
#include <kapplication.h>
#include <kiconloader.h>

#include <qcheckbox.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qlist.h>
#include <qlabel.h>
#include <qtooltip.h>
#include <qtabwidget.h>


#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qsizepolicy.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <QGridLayout>


K3bMediaCopyDialog::K3bMediaCopyDialog( QWidget *parent )
    : K3bInteractionDialog( parent,
                            i18n("Disk Copy"),
                            i18n("and CD Cloning"),
                            START_BUTTON|CANCEL_BUTTON,
                            START_BUTTON,
                            "Disk Copy" )
{
    QWidget* main = mainWidget();

    QGridLayout* mainGrid = new QGridLayout( main );
    mainGrid->setSpacing( spacingHint() );
    mainGrid->setMargin( 0 );

    QGroupBox* groupSource = new QGroupBox( i18n("Source Medium"), main );
    m_comboSourceDevice = new K3bMediaSelectionComboBox( groupSource );
    m_comboSourceDevice->setWantedMediumType( K3bDevice::MEDIA_ALL );
    m_comboSourceDevice->setWantedMediumState( K3bDevice::STATE_COMPLETE|K3bDevice::STATE_INCOMPLETE );
    QHBoxLayout* groupSourceLayout = new QHBoxLayout( groupSource );
    groupSourceLayout->setSpacing( spacingHint() );
    groupSourceLayout->setMargin( marginHint() );
    groupSourceLayout->addWidget( m_comboSourceDevice );

    m_writerSelectionWidget = new K3bWriterSelectionWidget( main );
    m_writerSelectionWidget->setWantedMediumState( K3bDevice::STATE_EMPTY );

    // tab widget --------------------
    QTabWidget* tabWidget = new QTabWidget( main );

    //
    // option tab --------------------
    //
    QWidget* optionTab = new QWidget( tabWidget );
    QGridLayout* optionTabGrid = new QGridLayout( optionTab );
    optionTabGrid->setSpacing( spacingHint() );
    optionTabGrid->setMargin( marginHint() );

    QGroupBox* groupCopyMode = new QGroupBox( i18n("Copy Mode"), optionTab );
    m_comboCopyMode = new QComboBox( groupCopyMode );
    m_comboCopyMode->addItem( i18n("Normal Copy") );
    m_comboCopyMode->addItem( i18n("Clone Copy") );
    QHBoxLayout* groupCopyModeLayout = new QHBoxLayout( groupCopyMode );
    groupCopyModeLayout->setSpacing( spacingHint() );
    groupCopyModeLayout->setMargin( marginHint() );
    groupCopyModeLayout->addWidget( m_comboCopyMode );

    QGroupBox* groupWritingMode = new QGroupBox( i18n("Writing Mode"), optionTab );
    m_writingModeWidget = new K3bWritingModeWidget( groupWritingMode );
    QHBoxLayout* groupWritingModeLayout = new QHBoxLayout( groupWritingMode );
    groupWritingModeLayout->setSpacing( spacingHint() );
    groupWritingModeLayout->setMargin( marginHint() );
    groupWritingModeLayout->addWidget( m_writingModeWidget );

    QGroupBox* groupCopies = new QGroupBox( i18n("Copies"), optionTab );
    QLabel* pixLabel = new QLabel( groupCopies );
    pixLabel->setPixmap( SmallIcon( "tools-media-optical-copy", KIconLoader::SizeMedium ) );
    pixLabel->setScaledContents( false );
    m_spinCopies = new QSpinBox( groupCopies );
    m_spinCopies->setRange( 1, 999 );
    QHBoxLayout* groupCopiesLayout = new QHBoxLayout( groupCopies );
    groupCopiesLayout->setSpacing( spacingHint() );
    groupCopiesLayout->setMargin( marginHint() );
    groupCopiesLayout->addWidget( pixLabel );
    groupCopiesLayout->addWidget( m_spinCopies );

    QGroupBox* groupOptions = new QGroupBox( i18n("Settings"), optionTab );
    m_checkSimulate = K3bStdGuiItems::simulateCheckbox( groupOptions );
    m_checkCacheImage = K3bStdGuiItems::createCacheImageCheckbox( groupOptions );
    m_checkOnlyCreateImage = K3bStdGuiItems::onlyCreateImagesCheckbox( groupOptions );
    m_checkDeleteImages = K3bStdGuiItems::removeImagesCheckbox( groupOptions );
    m_checkVerifyData = K3bStdGuiItems::verifyCheckBox( groupOptions );
    QVBoxLayout* groupOptionsLayout = new QVBoxLayout( groupOptions );
    groupOptionsLayout->setSpacing( spacingHint() );
    groupOptionsLayout->setMargin( marginHint() );
    groupOptionsLayout->addWidget( m_checkSimulate );
    groupOptionsLayout->addWidget( m_checkCacheImage );
    groupOptionsLayout->addWidget( m_checkOnlyCreateImage );
    groupOptionsLayout->addWidget( m_checkDeleteImages );
    groupOptionsLayout->addWidget( m_checkVerifyData );
    groupOptionsLayout->addStretch( 1 );

    optionTabGrid->addWidget( groupCopyMode, 0, 0 );
    optionTabGrid->addWidget( groupWritingMode, 1, 0 );
    optionTabGrid->addWidget( groupOptions, 0, 1, 3, 1 );
    optionTabGrid->addWidget( groupCopies, 2, 0 );
    optionTabGrid->setRowStretch( 2, 1 );
    optionTabGrid->setColumnStretch( 1, 1 );

    tabWidget->addTab( optionTab, i18n("&Options") );


    //
    // image tab ------------------
    //
    QWidget* imageTab = new QWidget( tabWidget );
    QGridLayout* imageTabGrid = new QGridLayout( imageTab );
    imageTabGrid->setSpacing( spacingHint() );
    imageTabGrid->setMargin( marginHint() );

    m_tempDirSelectionWidget = new K3bTempDirSelectionWidget( imageTab );

    imageTabGrid->addWidget( m_tempDirSelectionWidget, 0, 0 );

    tabWidget->addTab( imageTab, i18n("&Image") );


    //
    // advanced tab ------------------
    //
    QWidget* advancedTab = new QWidget( tabWidget );
    QGridLayout* advancedTabGrid = new QGridLayout( advancedTab );
    advancedTabGrid->setSpacing( spacingHint() );
    advancedTabGrid->setMargin( marginHint() );

    m_groupAdvancedDataOptions = new QGroupBox( i18n("Data"), advancedTab );
    QGridLayout* groupAdvancedDataOptionsLayout = new QGridLayout( m_groupAdvancedDataOptions );
    groupAdvancedDataOptionsLayout->setSpacing( spacingHint() );
    groupAdvancedDataOptionsLayout->setMargin( marginHint() );
    m_spinDataRetries = new QSpinBox( m_groupAdvancedDataOptions );
    m_spinDataRetries->setRange( 1, 128 );
    m_checkIgnoreDataReadErrors = K3bStdGuiItems::ignoreAudioReadErrorsCheckBox( m_groupAdvancedDataOptions );
    m_checkNoCorrection = new QCheckBox( i18n("No error correction"), m_groupAdvancedDataOptions );
    groupAdvancedDataOptionsLayout->addWidget( new QLabel( i18n("Read retries:"), m_groupAdvancedDataOptions ), 0, 0 );
    groupAdvancedDataOptionsLayout->addWidget( m_spinDataRetries, 0, 1 );
    groupAdvancedDataOptionsLayout->addWidget( m_checkIgnoreDataReadErrors, 1, 0, 1, 2 );
    groupAdvancedDataOptionsLayout->addWidget( m_checkNoCorrection, 2, 0, 1, 2 );
    groupAdvancedDataOptionsLayout->setRowStretch( 4, 1 );

    m_groupAdvancedAudioOptions = new QGroupBox( i18n("Audio"), advancedTab );
    QGridLayout* groupAdvancedAudioOptionsLayout = new QGridLayout( m_groupAdvancedAudioOptions );
    groupAdvancedAudioOptionsLayout->setSpacing( spacingHint() );
    groupAdvancedAudioOptionsLayout->setMargin( marginHint() );
    m_spinAudioRetries = new QSpinBox( m_groupAdvancedAudioOptions );
    m_spinAudioRetries->setRange( 1, 128 );
    m_checkIgnoreAudioReadErrors = K3bStdGuiItems::ignoreAudioReadErrorsCheckBox( m_groupAdvancedAudioOptions );
    m_comboParanoiaMode = K3bStdGuiItems::paranoiaModeComboBox( m_groupAdvancedAudioOptions );
    m_checkReadCdText = new QCheckBox( i18n("Copy CD-Text"), m_groupAdvancedAudioOptions );
    groupAdvancedAudioOptionsLayout->addWidget( new QLabel( i18n("Read retries:"), m_groupAdvancedAudioOptions ), 0, 0 );
    groupAdvancedAudioOptionsLayout->addWidget( m_spinAudioRetries, 0, 1 );
    groupAdvancedAudioOptionsLayout->addWidget( m_checkIgnoreAudioReadErrors, 1, 0, 1, 2 );
    groupAdvancedAudioOptionsLayout->addWidget( new QLabel( i18n("Paranoia mode:"), m_groupAdvancedAudioOptions ), 2, 0 );
    groupAdvancedAudioOptionsLayout->addWidget( m_comboParanoiaMode, 2, 1 );
    groupAdvancedAudioOptionsLayout->addWidget( m_checkReadCdText, 3, 0, 1, 2 );
    groupAdvancedAudioOptionsLayout->setRowStretch( 4, 1 );

    advancedTabGrid->addWidget( m_groupAdvancedDataOptions, 0, 1 );
    advancedTabGrid->addWidget( m_groupAdvancedAudioOptions, 0, 0 );

    tabWidget->addTab( advancedTab, i18n("&Advanced") );

    mainGrid->addWidget( groupSource, 0, 0  );
    mainGrid->addWidget( m_writerSelectionWidget, 1, 0  );
    mainGrid->addWidget( tabWidget, 2, 0 );
    mainGrid->setRowStretch( 2, 1 );


    connect( m_comboSourceDevice, SIGNAL(selectionChanged(K3bDevice::Device*)), this, SLOT(slotToggleAll()) );
    connect( m_comboSourceDevice, SIGNAL(selectionChanged(K3bDevice::Device*)),
             this, SLOT(slotToggleAll()) );
    connect( m_writerSelectionWidget, SIGNAL(writerChanged()), this, SLOT(slotToggleAll()) );
    connect( m_writerSelectionWidget, SIGNAL(writerChanged(K3bDevice::Device*)),
             m_writingModeWidget, SLOT(setDevice(K3bDevice::Device*)) );
    connect( m_writingModeWidget, SIGNAL(writingModeChanged(int)), this, SLOT(slotToggleAll()) );
    connect( m_checkCacheImage, SIGNAL(toggled(bool)), this, SLOT(slotToggleAll()) );
    connect( m_checkSimulate, SIGNAL(toggled(bool)), this, SLOT(slotToggleAll()) );
    connect( m_checkOnlyCreateImage, SIGNAL(toggled(bool)), this, SLOT(slotToggleAll()) );
    connect( m_comboCopyMode, SIGNAL(activated(int)), this, SLOT(slotToggleAll()) );
    connect( m_checkReadCdText, SIGNAL(toggled(bool)), this, SLOT(slotToggleAll()) );

    m_checkIgnoreDataReadErrors->setToolTip( i18n("Skip unreadable data sectors") );
    m_checkNoCorrection->setToolTip( i18n("Disable the source drive's error correction") );
    m_checkReadCdText->setToolTip( i18n("Copy CD-Text from the source CD if available.") );

    m_checkNoCorrection->setWhatsThis( i18n("<p>If this option is checked K3b will disable the "
                                            "source drive's ECC/EDC error correction. This way sectors "
                                            "that are unreadable by intention can be read."
                                            "<p>This may be useful for cloning CDs with copy "
                                            "protection based on corrupted sectors.") );
    m_checkReadCdText->setWhatsThis( i18n("<p>If this option is checked K3b will search for CD-Text on the source CD. "
                                          "Disable it if your CD drive has problems with reading CD-Text or you want "
                                          "to stick to Cddb info.") );
    m_checkIgnoreDataReadErrors->setWhatsThis( i18n("<p>If this option is checked and K3b is not able to read a data sector from the "
                                                    "source medium it will be replaced with zeros on the resulting copy.") );

    m_comboCopyMode->setWhatsThis(
        "<p><b>" + i18n("Normal Copy") + "</b>"
       + i18n("<p>This is the normal copy mode for DVD, Blu-ray, and most CD media types. "
               "It allows copying Audio CDs, multi and single session Data Media, and "
               "Enhanced Audio CDs (an Audio CD containing an additional data session)."
               "<p>For VideoCDs please use the CD Cloning mode.")
        + "<p><b>" + i18n("Clone Copy") + "</b>"
        + i18n("<p>In CD Cloning mode K3b performs a raw copy of the CD. That means it does "
               "not care about the content but simply copies the CD bit by bit. It may be used "
               "to copy VideoCDs or CDs which contain erroneous sectors."
               "<p><b>Caution:</b> Only single session CDs can be cloned.") );
}


K3bMediaCopyDialog::~K3bMediaCopyDialog()
{
}


void K3bMediaCopyDialog::init()
{
    slotToggleAll();
}


void K3bMediaCopyDialog::setReadingDevice( K3bDevice::Device* dev )
{
    m_comboSourceDevice->setSelectedDevice( dev );
}


K3bDevice::Device* K3bMediaCopyDialog::readingDevice() const
{
    return m_comboSourceDevice->selectedDevice();
}


void K3bMediaCopyDialog::slotStartClicked()
{
    //
    // Let's check the available size
    //
    if( m_checkCacheImage->isChecked() || m_checkOnlyCreateImage->isChecked() ) {
        if( neededSize()/1024 > m_tempDirSelectionWidget->freeTempSpace() ) {
            if( KMessageBox::warningContinueCancel( this, i18n("There seems to be not enough free space in temporary directory. "
                                                               "Write anyway?") ) == KMessageBox::Cancel )
                return;
        }
    }

    K3bDevice::Device* readDev = m_comboSourceDevice->selectedDevice();
    K3bDevice::Device* burnDev = m_writerSelectionWidget->writerDevice();
    K3bMedium sourceMedium = k3bappcore->mediaCache()->medium( readDev );
    K3bMedium burnMedium = k3bappcore->mediaCache()->medium( burnDev );

    K3bJobProgressDialog* dlg = 0;
    if( m_checkOnlyCreateImage->isChecked() ) {
        dlg = new K3bJobProgressDialog( kapp->activeWindow() );
    }
    else {
        dlg = new K3bBurnProgressDialog( kapp->activeWindow() );
    }

    K3bBurnJob* burnJob = 0;

    if( m_comboCopyMode->currentIndex() == 1 ) {

        //
        // check for m_tempDirSelectionWidget->tempPath() and
        // m_tempDirSelectionWidget-tempPath() + ".toc"
        //
        if( QFileInfo( m_tempDirSelectionWidget->tempPath() ).isFile() ) {
            if( KMessageBox::warningContinueCancel( this,
                                                    i18n("Do you want to overwrite %1?",m_tempDirSelectionWidget->tempPath()),
                                                    i18n("File Exists"),
                                                    KStandardGuiItem::overwrite() )
                != KMessageBox::Continue )
                return;
        }

        if( QFileInfo( m_tempDirSelectionWidget->tempPath() + ".toc" ).isFile() ) {
            if( KMessageBox::warningContinueCancel( this,
                                                    i18n("Do you want to overwrite %1?",m_tempDirSelectionWidget->tempPath() + ".toc"),
                                                    i18n("File Exists"),
                                                    KStandardGuiItem::overwrite() )
                != KMessageBox::Continue )
                return;
        }

        K3bCloneJob* job = new K3bCloneJob( dlg, this );

        job->setWriterDevice( m_writerSelectionWidget->writerDevice() );
        job->setReaderDevice( m_comboSourceDevice->selectedDevice() );
        job->setImagePath( m_tempDirSelectionWidget->tempPath() );
        job->setNoCorrection( m_checkNoCorrection->isChecked() );
        job->setRemoveImageFiles( m_checkDeleteImages->isChecked() && !m_checkOnlyCreateImage->isChecked() );
        job->setOnlyCreateImage( m_checkOnlyCreateImage->isChecked() );
        job->setSimulate( m_checkSimulate->isChecked() );
        job->setWriteSpeed( m_writerSelectionWidget->writerSpeed() );
        job->setCopies( m_checkSimulate->isChecked() ? 1 : m_spinCopies->value() );
        job->setReadRetries( m_spinDataRetries->value() );

        burnJob = job;
    }
    else if ( sourceMedium.diskInfo().mediaType() & K3bDevice::MEDIA_CD_ALL ) {
        K3bCdCopyJob* job = new K3bCdCopyJob( dlg, this );

        job->setWriterDevice( m_writerSelectionWidget->writerDevice() );
        job->setReaderDevice( m_comboSourceDevice->selectedDevice() );
        job->setSpeed( m_writerSelectionWidget->writerSpeed() );
        job->setSimulate( m_checkSimulate->isChecked() );
        job->setOnTheFly( !m_checkCacheImage->isChecked() );
        job->setKeepImage( !m_checkDeleteImages->isChecked() || m_checkOnlyCreateImage->isChecked() );
        job->setOnlyCreateImage( m_checkOnlyCreateImage->isChecked() );
        job->setTempPath( m_tempDirSelectionWidget->plainTempPath() );
        job->setCopies( m_checkSimulate->isChecked() ? 1 : m_spinCopies->value() );
        job->setParanoiaMode( m_comboParanoiaMode->currentText().toInt() );
        job->setDataReadRetries( m_spinDataRetries->value() );
        job->setAudioReadRetries( m_spinAudioRetries->value() );
        job->setCopyCdText( m_checkReadCdText->isChecked() );
        job->setIgnoreDataReadErrors( m_checkIgnoreDataReadErrors->isChecked() );
        job->setIgnoreAudioReadErrors( m_checkIgnoreAudioReadErrors->isChecked() );
        job->setNoCorrection( m_checkNoCorrection->isChecked() );
        job->setWritingMode( m_writingModeWidget->writingMode() );

        burnJob = job;
    }
    else if ( sourceMedium.diskInfo().mediaType() & ( K3bDevice::MEDIA_DVD_ALL|K3bDevice::MEDIA_BD_ALL ) ) {
        K3bDvdCopyJob* job = new K3bDvdCopyJob( dlg, this );

        job->setWriterDevice( m_writerSelectionWidget->writerDevice() );
        job->setReaderDevice( m_comboSourceDevice->selectedDevice() );
        job->setImagePath( m_tempDirSelectionWidget->tempPath() );
        job->setRemoveImageFiles( m_checkDeleteImages->isChecked() && !m_checkOnlyCreateImage->isChecked() );
        job->setOnlyCreateImage( m_checkOnlyCreateImage->isChecked() );
        job->setSimulate( m_checkSimulate->isChecked() );
        job->setOnTheFly( !m_checkCacheImage->isChecked() );
        job->setWriteSpeed( m_writerSelectionWidget->writerSpeed() );
        job->setCopies( m_checkSimulate->isChecked() ? 1 : m_spinCopies->value() );
        job->setWritingMode( m_writingModeWidget->writingMode() );
        job->setIgnoreReadErrors( m_checkIgnoreDataReadErrors->isChecked() );
        job->setReadRetries( m_spinDataRetries->value() );
        job->setVerifyData( m_checkVerifyData->isChecked() );

        burnJob = job;
    }
    else {
        KMessageBox::sorry( this, "Ups", "No copy support for this source media type yet." );
        return;
    }

    if( !exitLoopOnHide() )
        hide();

    dlg->startJob( burnJob );

    delete dlg;
    delete burnJob;

    if( KConfigGroup( k3bcore->config(), "General Options" ).readEntry( "keep action dialogs open", false ) &&
        !exitLoopOnHide() )
        show();
    else
        close();
}


void K3bMediaCopyDialog::toggleAll()
{
    updateOverrideDevice();

    K3bDevice::Device* readDev = m_comboSourceDevice->selectedDevice();
    K3bDevice::Device* burnDev = m_writerSelectionWidget->writerDevice();
    K3bMedium sourceMedium = k3bappcore->mediaCache()->medium( readDev );
    K3bMedium burnMedium = k3bappcore->mediaCache()->medium( burnDev );

    if ( burnDev ) {
        if( readDev != burnDev &&
            burnMedium.diskInfo().mediaType() & K3bDevice::MEDIA_DVD_PLUS_ALL ) {
            // no simulation support for DVD+R(W) media
            m_checkSimulate->setChecked(false);
            m_checkSimulate->setEnabled(false);
        }
        else {
            m_checkSimulate->setDisabled( m_checkOnlyCreateImage->isChecked() );
        }
    }
    else {
        m_checkSimulate->setEnabled( !m_checkOnlyCreateImage->isChecked() );
    }

    m_checkDeleteImages->setEnabled( !m_checkOnlyCreateImage->isChecked() && m_checkCacheImage->isChecked() );
    m_spinCopies->setDisabled( m_checkSimulate->isChecked() || m_checkOnlyCreateImage->isChecked() );
    m_tempDirSelectionWidget->setDisabled( !m_checkCacheImage->isChecked() && !m_checkOnlyCreateImage->isChecked() );
    m_writerSelectionWidget->setDisabled( m_checkOnlyCreateImage->isChecked() );
    m_checkCacheImage->setEnabled( !m_checkOnlyCreateImage->isChecked() );
    m_writingModeWidget->setEnabled( !m_checkOnlyCreateImage->isChecked() );

    // FIXME: no verification for CD yet
    m_checkVerifyData->setDisabled( sourceMedium.diskInfo().mediaType() & K3bDevice::MEDIA_CD_ALL ||
                                    sourceMedium.content() & K3bMedium::CONTENT_AUDIO ||
                                    m_checkSimulate->isChecked() );

    // we can only clone single session CDs
    if( K3bDevice::isCdMedia( sourceMedium.diskInfo().mediaType() ) ) {
        m_writerSelectionWidget->setWantedMediumType( K3bDevice::MEDIA_WRITABLE_CD );
        m_writerSelectionWidget->setSupportedWritingApps( K3b::CDRECORD );

        if ( sourceMedium.toc().sessions() == 1 ) {
            m_comboCopyMode->setEnabled( true );
        }
        else {
            m_comboCopyMode->setEnabled( false );
            m_comboCopyMode->setCurrentIndex( 0 );
        }
    }
    else {
        m_writerSelectionWidget->setSupportedWritingApps( K3b::GROWISOFS|K3b::CDRECORD );

        m_comboCopyMode->setEnabled( false );
        m_comboCopyMode->setCurrentIndex( 0 );

        // FIXME: at some point the media combo should also handle media sizes!

        if ( K3bDevice::isDvdMedia( sourceMedium.diskInfo().mediaType() ) ) {
            m_writerSelectionWidget->setWantedMediumType( sourceMedium.diskInfo().numLayers() > 1 &&
                                                          sourceMedium.diskInfo().size().mode1Bytes() > 4700372992LL
                                                          ? K3bDevice::MEDIA_WRITABLE_DVD_DL
                                                          : K3bDevice::MEDIA_WRITABLE_DVD );
        }
        else {
            // FIXME: do the same single layer/dual layer thing like with DVD
            m_writerSelectionWidget->setWantedMediumType( K3bDevice::MEDIA_WRITABLE_BD );
        }
    }

    // CD Cloning
    if( m_comboCopyMode->currentIndex() == 1 ) {
        // cdrecord does not support cloning on-the-fly
        m_checkCacheImage->setChecked(true);
        m_checkCacheImage->setEnabled(false);

        m_writingModeWidget->setSupportedModes( K3b::RAW );
    }

    // Normal CD/DVD/Blue-Ray copy
    else {
        //
        // If the same device is used for reading and writing all we can present is a fuzzy
        // selection of the writing mode
        //
        if( burnDev == readDev ) {
            int modes = 0;
            if ( sourceMedium.diskInfo().mediaType() & K3bDevice::MEDIA_CD_ALL ) {
                modes = K3b::TAO|K3b::DAO|K3b::RAW;
            }
            else if ( sourceMedium.diskInfo().mediaType() & K3bDevice::MEDIA_DVD_ALL ) {
                // only auto for DVD+R(W)
                if( burnDev->writeCapabilities() & (K3bDevice::MEDIA_DVD_R|K3bDevice::MEDIA_DVD_RW) ) {
                    modes |= K3b::DAO|K3b::WRITING_MODE_RES_OVWR;
                    if( burnDev->featureCurrent( K3bDevice::FEATURE_INCREMENTAL_STREAMING_WRITABLE ) != 0 )
                        modes |= K3b::WRITING_MODE_INCR_SEQ;
                }

                // TODO: once we have layer jump support: this is where it goes
//               if ( burnDev->supportsWritingMode( K3bDevice::WRITING_MODE_LAYER_JUMP ) ) {
//                   modes |= K3bDevice::WRITING_MODE_LAYER_JUMP;
//               }
            }
            else if ( sourceMedium.diskInfo().mediaType() & K3bDevice::MEDIA_BD_ALL ) {
                // no modes, only auto
            }

            m_writingModeWidget->setSupportedModes( modes );
        }
        else {
            m_writingModeWidget->determineSupportedModesFromMedium( burnDev );
        }
    }

    m_tempDirSelectionWidget->setNeededSize( neededSize() );

    if( sourceMedium.toc().contentType() == K3bDevice::DATA &&
        sourceMedium.toc().count() == 1 ) {
        m_tempDirSelectionWidget->setSelectionMode( K3bTempDirSelectionWidget::FILE );
        m_tempDirSelectionWidget->setDefaultImageFileName( sourceMedium.volumeId().toLower()
                                                           + QString(".iso"), true );
    }
    else {
        m_tempDirSelectionWidget->setSelectionMode( K3bTempDirSelectionWidget::DIR );

        if ( sourceMedium.content() & K3bMedium::CONTENT_DATA && !sourceMedium.volumeId().isEmpty() ) {
            m_tempDirSelectionWidget->setTempPath( m_tempDirSelectionWidget->tempDirectory() + sourceMedium.volumeId().toLower() );
        }
        else if ( sourceMedium.content() & K3bMedium::CONTENT_AUDIO && !sourceMedium.cdText().title().isEmpty() ) {
            m_tempDirSelectionWidget->setTempPath( m_tempDirSelectionWidget->tempDirectory() + sourceMedium.cdText().title() );
        }
        else {
            m_tempDirSelectionWidget->setTempPath( m_tempDirSelectionWidget->tempDirectory() ); // let the copy job figure it out
        }
    }

    m_groupAdvancedAudioOptions->setEnabled( sourceMedium.content() & K3bMedium::CONTENT_AUDIO && m_comboCopyMode->currentIndex() == 0 );
    m_groupAdvancedDataOptions->setEnabled( sourceMedium.content() & K3bMedium::CONTENT_DATA );

    setButtonEnabled( START_BUTTON,
                      m_comboSourceDevice->selectedDevice() &&
                      (burnDev || m_checkOnlyCreateImage->isChecked()) );

    K3bInteractionDialog::toggleAll();
}


void K3bMediaCopyDialog::updateOverrideDevice()
{
    if( !m_checkCacheImage->isChecked() ) {
        m_writerSelectionWidget->setOverrideDevice( 0 );
        m_writerSelectionWidget->setIgnoreDevice( m_comboSourceDevice->selectedDevice() );
    }
    else {
        m_writerSelectionWidget->setOverrideDevice( m_comboSourceDevice->selectedDevice(),
                                                    i18n("Use the same device for burning"),
                                                    i18n("<qt>Use the same device for burning <i>(Or insert another medium)</i>") );
        m_writerSelectionWidget->setIgnoreDevice( 0 );
    }
}


void K3bMediaCopyDialog::loadUserDefaults( const KConfigGroup& c )
{
    m_writerSelectionWidget->loadConfig( c );
    m_comboSourceDevice->setSelectedDevice( k3bcore->deviceManager()->findDevice( c.readEntry( "source_device" ) ) );
    m_writingModeWidget->loadConfig( c );
    m_checkSimulate->setChecked( c.readEntry( "simulate", false ) );
    m_checkCacheImage->setChecked( !c.readEntry( "on_the_fly", false ) );
    m_checkDeleteImages->setChecked( c.readEntry( "delete_images", true ) );
    m_checkOnlyCreateImage->setChecked( c.readEntry( "only_create_image", false ) );
    m_comboParanoiaMode->setCurrentIndex( c.readEntry( "paranoia_mode", 0 ) );
    m_checkVerifyData->setChecked( c.readEntry( "verify data", false ) );

    m_spinCopies->setValue( c.readEntry( "copies", 1 ) );

    m_tempDirSelectionWidget->readConfig( c );

    if( c.readEntry( "copy mode", "normal" ) == "normal" )
        m_comboCopyMode->setCurrentIndex( 0 );
    else
        m_comboCopyMode->setCurrentIndex( 1 );

    m_checkReadCdText->setChecked( c.readEntry( "copy cdtext", true ) );
    m_checkIgnoreDataReadErrors->setChecked( c.readEntry( "ignore data read errors", false ) );
    m_checkIgnoreAudioReadErrors->setChecked( c.readEntry( "ignore audio read errors", true ) );
    m_checkNoCorrection->setChecked( c.readEntry( "no correction", false ) );

    m_spinDataRetries->setValue( c.readEntry( "data retries", 128 ) );
    m_spinAudioRetries->setValue( c.readEntry( "audio retries", 5 ) );

    slotToggleAll();
}


void K3bMediaCopyDialog::saveUserDefaults( KConfigGroup& c )
{
    m_writingModeWidget->saveConfig( c );
    c.writeEntry( "simulate", m_checkSimulate->isChecked() );
    c.writeEntry( "on_the_fly", !m_checkCacheImage->isChecked() );
    c.writeEntry( "delete_images", m_checkDeleteImages->isChecked() );
    c.writeEntry( "only_create_image", m_checkOnlyCreateImage->isChecked() );
    c.writeEntry( "paranoia_mode", m_comboParanoiaMode->currentText().toInt() );
    c.writeEntry( "copies", m_spinCopies->value() );
    c.writeEntry( "verify data", m_checkVerifyData->isChecked() );

    m_writerSelectionWidget->saveConfig( c );
    m_tempDirSelectionWidget->saveConfig( c );

    c.writeEntry( "source_device", m_comboSourceDevice->selectedDevice() ? m_comboSourceDevice->selectedDevice()->blockDeviceName() : QString() );

    c.writeEntry( "copy cdtext", m_checkReadCdText->isChecked() );
    c.writeEntry( "ignore data read errors", m_checkIgnoreDataReadErrors->isChecked() );
    c.writeEntry( "ignore audio read errors", m_checkIgnoreAudioReadErrors->isChecked() );
    c.writeEntry( "no correction", m_checkNoCorrection->isChecked() );
    c.writeEntry( "data retries", m_spinDataRetries->value() );
    c.writeEntry( "audio retries", m_spinAudioRetries->value() );

    QString s;
    if( m_comboCopyMode->currentIndex() == 1 )
        s = "clone";
    else
        s = "normal";
    c.writeEntry( "copy mode", s );
}


void K3bMediaCopyDialog::loadK3bDefaults()
{
    m_writingModeWidget->setWritingMode( K3b::WRITING_MODE_AUTO );
    m_writerSelectionWidget->loadDefaults();
    m_checkSimulate->setChecked( false );
    m_checkCacheImage->setChecked( true );
    m_checkDeleteImages->setChecked( true );
    m_checkOnlyCreateImage->setChecked( false );
    m_comboParanoiaMode->setCurrentIndex(0);
    m_spinCopies->setValue(1);
    m_checkReadCdText->setChecked(true);
    m_checkIgnoreDataReadErrors->setChecked(false);
    m_checkIgnoreAudioReadErrors->setChecked(true);
    m_checkNoCorrection->setChecked(false);
    m_comboCopyMode->setCurrentIndex( 0 ); // normal
    m_spinDataRetries->setValue(128);
    m_spinAudioRetries->setValue(5);
    m_tempDirSelectionWidget->setTempPath( K3b::defaultTempPath() );
    m_checkVerifyData->setChecked( false );

    slotToggleAll();
}


KIO::filesize_t K3bMediaCopyDialog::neededSize() const
{
    K3bMedium medium = k3bappcore->mediaCache()->medium( m_comboSourceDevice->selectedDevice() );

    if( medium.diskInfo().diskState() == K3bDevice::STATE_NO_MEDIA )
        return 0;
    else if( medium.diskInfo().mediaType() & (K3bDevice::MEDIA_DVD_RW_OVWR|K3bDevice::MEDIA_DVD_PLUS_RW) )
        return (KIO::filesize_t)medium.iso9660Descriptor().volumeSpaceSize * (KIO::filesize_t)2048;
    else if ( m_comboCopyMode->currentIndex() == 0 )
        return medium.diskInfo().size().mode1Bytes();
    else
        return medium.diskInfo().size().rawBytes();
}

#include "k3bmediacopydialog.moc"
