/*
 *
 * $Id$
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


#include "k3bjobprogressdialog.h"
#include "k3bapplication.h"
#include "k3bemptydiscwaiter.h"
#include "k3bjobprogressosd.h"
#include "k3bdebuggingoutputdialog.h"
#include "k3bapplication.h"
//#include "k3bjobinterface.h"
#include "k3bthemedlabel.h"
#include <k3bjob.h>
#include <k3bdevice.h>
#include <k3bdevicemanager.h>
#include <k3bdeviceglobals.h>
#include <k3bglobals.h>
#include <k3bstdguiitems.h>
#include <k3bversion.h>
#include <k3bthememanager.h>

#include <q3groupbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <q3whatsthis.h>
#include <qdatetime.h>
#include <qstring.h>
#include <QEventLoop>
#include <QPointer>

#include <q3header.h>
#include <qscrollbar.h>
#include <qpoint.h>
#include <qfontmetrics.h>
#include <qtimer.h>
#include <qfont.h>
#include <qeventloop.h>
#include <qfile.h>
#include <qapplication.h>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QGridLayout>
#include <QFrame>
#include <QVBoxLayout>
#include <QCloseEvent>

#include <kprogressdialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <k3listview.h>
#include <kiconloader.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kmainwindow.h>
#include <kstdguiitem.h>
#include <kpushbutton.h>
#include <ksqueezedtextlabel.h>
#include <KNotification>
#include <kvbox.h>
#include <KStandardGuiItem>
#include <KGlobalSettings>


class K3bJobProgressDialog::Private
{
public:
    Private()
        : eventLoop( 0 ) {
    }

    int lastProgress;
    QEventLoop* eventLoop;

    QFrame* headerFrame;
    QFrame* progressHeaderFrame;
};



K3bJobProgressDialog::K3bJobProgressDialog( QWidget* parent,
                                            bool showSubProgress )
    : KDialog( parent ),
      m_osd(0)
{
    d = new Private;
    setupGUI();
    setupConnections();

    if( !showSubProgress ) {
        m_progressSubPercent->hide();
    }

    m_job = 0;
    m_timer = new QTimer( this );

    connect( m_timer, SIGNAL(timeout()), this, SLOT(slotUpdateTime()) );
}


/*
 *  Destroys the object and frees any allocated resources
 */
K3bJobProgressDialog::~K3bJobProgressDialog()
{
    delete d;
    delete m_osd;
}


void K3bJobProgressDialog::setupGUI()
{
    // KDialog does not allow to use Cancel and Close buttons at the same time!
    setButtons( KDialog::Cancel|KDialog::User1|KDialog::User2 );
    setButtonText( User1, i18n("Show Debugging Output") );
    setButtonGuiItem( User2, KStandardGuiItem::close() );

    QVBoxLayout* mainLayout = new QVBoxLayout( mainWidget() );
    mainLayout->setMargin( 0 );

    // header
    // ------------------------------------------------------------------------------------------
    d->headerFrame = new QFrame( mainWidget() );
    d->headerFrame->setFrameShape( QFrame::StyledPanel );
    d->headerFrame->setFrameShadow( QFrame::Sunken );
    d->headerFrame->setLineWidth( 1 );
    d->headerFrame->setAutoFillBackground( true );
    //d->headerFrame->setMargin( 1 );
    QHBoxLayout* headerLayout = new QHBoxLayout( d->headerFrame );
    headerLayout->setMargin( 2 ); // to make sure the frame gets displayed
    headerLayout->setSpacing( 0 );
    m_pixLabel = new K3bThemedLabel( d->headerFrame );
    headerLayout->addWidget( m_pixLabel );

    m_labelJob = new K3bThemedLabel( d->headerFrame );
    //TODO fix me
    //m_labelJob->setMinimumVisibleText( 40 );
    QFont m_labelJob_font(  m_labelJob->font() );
    m_labelJob_font.setPointSize( m_labelJob_font.pointSize() + 2 );
    m_labelJob_font.setBold( true );
    m_labelJob->setFont( m_labelJob_font );
    m_labelJob->setAlignment( Qt::AlignVCenter | Qt::AlignRight  );

    m_labelJobDetails = new K3bThemedLabel( d->headerFrame );
    m_labelJobDetails->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5,
                                                   (QSizePolicy::SizeType)5,
                                                   0, 1,
                                                   m_labelJobDetails->sizePolicy().hasHeightForWidth() ) );
    m_labelJobDetails->setAlignment( Qt::AlignVCenter | Qt::AlignRight  );

    QVBoxLayout* jobLabelsLayout = new QVBoxLayout;
    jobLabelsLayout->setMargin( KDialog::spacingHint() );
    jobLabelsLayout->addWidget( m_labelJob );
    jobLabelsLayout->addWidget( m_labelJobDetails );
    headerLayout->addLayout( jobLabelsLayout );

    mainLayout->addWidget( d->headerFrame );
    // ------------------------------------------------------------------------------------------


    m_viewInfo = new K3ListView( mainWidget() );
    m_viewInfo->addColumn( "" );
    m_viewInfo->addColumn( i18n( "Message" ) );
    m_viewInfo->setFullWidth( true );
    m_viewInfo->header()->hide();
    m_viewInfo->setSorting(-1);
    mainLayout->addWidget( m_viewInfo );


    // progress header
    // ------------------------------------------------------------------------------------------
    d->progressHeaderFrame = new QFrame( mainWidget() );
    d->progressHeaderFrame->setFrameShape( QFrame::StyledPanel );
    d->progressHeaderFrame->setFrameShadow( QFrame::Sunken );
    d->progressHeaderFrame->setLineWidth( 1 );
    d->progressHeaderFrame->setAutoFillBackground( true );
    //d->progressHeaderFrame->setMargin( 1 );

    QHBoxLayout* progressHeaderLayout = new QHBoxLayout( d->progressHeaderFrame );
    progressHeaderLayout->setMargin( 2 );
    progressHeaderLayout->setSpacing( 0 );

    m_labelTask = new K3bThemedLabel( d->progressHeaderFrame );
    QFont m_labelTask_font( m_labelTask->font() );
    m_labelTask_font.setPointSize( m_labelTask_font.pointSize() + 2 );
    m_labelTask_font.setBold( true );
    m_labelTask->setFont( m_labelTask_font );

    m_labelElapsedTime = new K3bThemedLabel( d->progressHeaderFrame );
    m_labelElapsedTime->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)5,
                                                    0, 1, m_labelElapsedTime->sizePolicy().hasHeightForWidth() ) );

    QVBoxLayout* jobProgressLayout = new QVBoxLayout( d->progressHeaderFrame );
    jobProgressLayout->setMargin( KDialog::spacingHint() );
    jobProgressLayout->addWidget( m_labelTask );
    jobProgressLayout->addWidget( m_labelElapsedTime );

    progressHeaderLayout->addLayout( jobProgressLayout );
    progressHeaderLayout->addWidget( new K3bThemedLabel( K3bTheme::PROGRESS_RIGHT, d->progressHeaderFrame ) );
    mainLayout->addWidget( d->progressHeaderFrame );
    // ------------------------------------------------------------------------------------------

    QHBoxLayout* layout3 = new QHBoxLayout( 0, 0, 6, "layout3");

    m_labelSubTask = new KSqueezedTextLabel( mainWidget() );
    m_labelSubTask->setTextElideMode( Qt::ElideRight );
    layout3->addWidget( m_labelSubTask );

    m_labelSubProcessedSize = new QLabel( mainWidget() );
    m_labelSubProcessedSize->setAlignment( Qt::AlignVCenter | Qt::AlignRight );
    layout3->addWidget( m_labelSubProcessedSize );
    mainLayout->addLayout( layout3 );

    m_progressSubPercent = new QProgressBar( mainWidget() );
    mainLayout->addWidget( m_progressSubPercent );

    QHBoxLayout* layout4 = new QHBoxLayout( 0, 0, 6, "layout4");

    QLabel* textLabel5 = new QLabel( i18n("Overall progress:"), mainWidget() );
    layout4->addWidget( textLabel5 );

    m_labelProcessedSize = new QLabel( mainWidget() );
    m_labelProcessedSize->setAlignment( Qt::AlignVCenter | Qt::AlignRight );
    layout4->addWidget( m_labelProcessedSize );
    mainLayout->addLayout( layout4 );

    m_progressPercent = new QProgressBar( mainWidget() );
    mainLayout->addWidget( m_progressPercent );

    m_frameExtraInfo = new QFrame( mainWidget() );
    m_frameExtraInfo->setFrameShape( QFrame::NoFrame );
    m_frameExtraInfo->setFrameShadow( QFrame::Raised );
    m_frameExtraInfoLayout = new QGridLayout( m_frameExtraInfo );
    m_frameExtraInfoLayout->setMargin(0);
    m_frameExtraInfoLayout->setSpacing( spacingHint() );
    mainLayout->addWidget( m_frameExtraInfo );

    m_pixLabel->setThemePixmap( K3bTheme::PROGRESS_WORKING );

    slotThemeChanged();

    connect( k3bappcore->themeManager(), SIGNAL(themeChanged()),
             this, SLOT(slotThemeChanged()) );
    connect( KGlobalSettings::self(), SIGNAL(appearanceChanged()),
             this, SLOT(slotThemeChanged()) );
}


void K3bJobProgressDialog::show()
{
    if( KConfigGroup( k3bcore->config(), "General Options" ).readEntry( "hide main window while writing", false ) )
        if( QWidget* w = kapp->mainWidget() )
            w->hide();

    if( m_osd ) {
        m_osd->readSettings( k3bcore->config() );
        m_osd->show();
    }

    KDialog::show();
}


void K3bJobProgressDialog::setExtraInfo( QWidget *extra )
{
    extra->reparent( m_frameExtraInfo, QPoint(0,0) );
    m_frameExtraInfoLayout->addWidget( extra, 0, 0 );
}


void K3bJobProgressDialog::closeEvent( QCloseEvent* e )
{
    if( button( User2 )->isVisible() ) {
        KDialog::closeEvent( e );
        if( QWidget* w = kapp->mainWidget() )
            w->show();

        if( !m_plainCaption.isEmpty() )
            if( KMainWindow* w = dynamic_cast<KMainWindow*>(kapp->mainWidget()) )
                w->setPlainCaption( m_plainCaption );

        if( m_osd ) {
            m_osd->hide();
#warning FIXME kde4
            //m_osd->saveSettings( KGlobal::config() );
        }
    }
    else
        e->ignore();
}


void K3bJobProgressDialog::setupConnections()
{
    connect( this, SIGNAL(cancelClicked()), this, SLOT(slotCancelButtonPressed()) );
    connect( this, SIGNAL(user2Clicked()), this, SLOT(close()) );
    connect( this, SIGNAL(user1Clicked()), this, SLOT(slotShowDebuggingOutput()) );
}


void K3bJobProgressDialog::slotProcessedSize( int processed, int size )
{
    m_labelProcessedSize->setText( i18n("%1 of %2 MB", processed, size ) );
}


void K3bJobProgressDialog::slotProcessedSubSize( int processedTrackSize, int trackSize )
{
    m_labelSubProcessedSize->setText( i18n("%1 of %2 MB",processedTrackSize, trackSize) );
}


void K3bJobProgressDialog::slotInfoMessage( const QString& infoString, int type )
{
    Q3ListViewItem* currentInfoItem = new Q3ListViewItem( m_viewInfo, m_viewInfo->lastItem(), QString(), infoString );
    currentInfoItem->setSelectable( false );

    // set the icon
    switch( type ) {
    case K3bJob::ERROR:
        currentInfoItem->setPixmap( 0, SmallIcon( "dialog-error" ) );
        break;
    case K3bJob::WARNING:
        currentInfoItem->setPixmap( 0, SmallIcon( "dialog-warning" ) );
        break;
    case K3bJob::SUCCESS:
        currentInfoItem->setPixmap( 0, SmallIcon( "dialog-ok" ) );
        break;
    case K3bJob::INFO:
    default:
        currentInfoItem->setPixmap( 0, SmallIcon( "dialog-information" ) );
    }

    // This should scroll down (hopefully!)
    m_viewInfo->ensureItemVisible( currentInfoItem );
}


void K3bJobProgressDialog::slotFinished( bool success )
{
    kDebug() << "(K3bJobProgressDialog) received finished signal!";

    m_logFile.close();

    if( success ) {
        m_pixLabel->setThemePixmap( K3bTheme::PROGRESS_SUCCESS );

        m_labelTask->setText( i18n("Success.") );
        m_labelTask->setPaletteForegroundColor( Qt::darkGreen );
        m_labelSubTask->setText( QString() );

        m_progressPercent->setValue(100);
        m_progressSubPercent->setValue(100);
        slotProgress(100);

        // one last time update to be sure no remaining time is displayed anymore
        slotUpdateTime();

        if( m_osd )
            m_osd->setText( i18n("Success.") );

        KNotification::event("SuccessfullyFinished", i18n("Successfully finished."),QPixmap() ,0);
    }
    else {
        m_pixLabel->setThemePixmap( K3bTheme::PROGRESS_FAIL );

        m_labelTask->setPaletteForegroundColor( Qt::red );

        if( m_bCanceled ) {
            m_labelTask->setText( i18n("Canceled.") );
            if( m_osd )
                m_osd->setText( i18n("Canceled.") );
        }
        else {
            m_labelTask->setText( i18n("Error.") );
            if( m_osd )
                m_osd->setText( i18n("Error.") );
        }
        KNotification::event("FinishedWithError", i18n("Finished with errors") ,QPixmap() ,0);
    }

    showButton( Cancel, false );
    showButton( User1, true );
    showButton( User2, true );
    m_timer->stop();
}


void K3bJobProgressDialog::slotCanceled()
{
    m_bCanceled = true;
}


void K3bJobProgressDialog::setJob( K3bJob* job )
{
    m_bCanceled = false;

    // clear everything
    showButton( Cancel, true );
    showButton( User1, false );
    showButton( User2, false );
    enableButtonCancel( true );

    m_viewInfo->clear();
    m_progressPercent->setValue(0);
    m_progressSubPercent->setValue(0);
    m_labelTask->setText("");
    m_labelSubTask->setText("");
    m_labelProcessedSize->setText("");
    m_labelSubProcessedSize->setText("");
    m_labelTask->setPaletteForegroundColor( k3bappcore->themeManager()->currentTheme()->foregroundColor() );
    m_logCache.clear();

    // disconnect from the former job
    if( m_job )
        disconnect( m_job );
    m_job = job;

    if( job ) {

        // connect to all the shit
        connect( job, SIGNAL(infoMessage(const QString&,int)), this, SLOT(slotInfoMessage(const QString&,int)) );

        connect( job, SIGNAL(percent(int)), m_progressPercent, SLOT(setValue(int)) );
        connect( job, SIGNAL(percent(int)), this, SLOT(slotProgress(int)) );
        connect( job, SIGNAL(subPercent(int)), m_progressSubPercent, SLOT(setValue(int)) );

        connect( job, SIGNAL(processedSubSize(int, int)), this, SLOT(slotProcessedSubSize(int, int)) );
        connect( job, SIGNAL(processedSize(int, int)), this, SLOT(slotProcessedSize(int, int)) );

        connect( job, SIGNAL(newTask(const QString&)), this, SLOT(slotNewTask(const QString&)) );
        connect( job, SIGNAL(newSubTask(const QString&)), this, SLOT(slotNewSubTask(const QString&)) );
        connect( job, SIGNAL(started()), this, SLOT(slotStarted()) );
        connect( job, SIGNAL(finished(bool)), this, SLOT(slotFinished(bool)) );
        connect( job, SIGNAL(canceled()), this, SLOT(slotCanceled()) );

        connect( job, SIGNAL(debuggingOutput(const QString&, const QString&)),
                 this, SLOT(slotDebuggingOutput(const QString&, const QString&)) );

        m_labelJob->setText( m_job->jobDescription() );
        m_labelJobDetails->setText( m_job->jobDetails() );

        setCaption( m_job->jobDescription() );

        if( KConfigGroup( k3bcore->config(), "General Options" ).readEntry( "Show progress OSD", true ) ) {
            if( !m_osd )
                m_osd = new K3bJobProgressOSD( this );
        }
        else
            delete m_osd;

        if( m_osd ) {
            m_osd->setText( job->jobDescription() );
            // FIXME: use a setJob method and let the osd also change the text color to red/green
            //      connect( job, SIGNAL(newTask(const QString&)), m_osd, SLOT(setText(const QString&)) );
            connect( job, SIGNAL(percent(int)), m_osd, SLOT(setProgress(int)) );
        }
    }
}


void K3bJobProgressDialog::slotCancelButtonPressed()
{
    if( m_job && m_job->active() )
        if( KMessageBox::questionYesNo( this, i18n("Do you really want to cancel?"), i18n("Cancel Confirmation") ) == KMessageBox::Yes ) {
            if( m_job ) {
                m_job->cancel();
                enableButtonCancel( false );  // do not cancel twice
            }
        }
}


void K3bJobProgressDialog::slotNewSubTask(const QString& name)
{
    m_labelSubTask->setText(name);
    m_labelSubProcessedSize->setText("");
    m_progressSubPercent->setValue(0);
}

void K3bJobProgressDialog::slotNewTask(const QString& name)
{
    m_labelTask->setText( name );
}


void K3bJobProgressDialog::slotStarted()
{
    d->lastProgress = 0;
    m_timer->start( 1000 );
    m_startTime = QDateTime::currentDateTime();
    if( KMainWindow* w = dynamic_cast<KMainWindow*>(kapp->mainWidget()) )
        m_plainCaption = w->windowTitle();

    m_logFile.open();
}


void K3bJobProgressDialog::slotUpdateTime()
{
    int elapsedDays = m_startTime.daysTo( QDateTime::currentDateTime() );
    int elapsedSecs = m_startTime.secsTo( QDateTime::currentDateTime() ) % (24*60*60);

    QString s = i18n("Elapsed time") + ": ";
    if ( elapsedDays > 0 ) {
        s += i18np( "1 Day", "%1 Days", elapsedDays ) + ", ";
    }
    s += QTime().addSecs(elapsedSecs).toString();

    if( d->lastProgress > 0 && d->lastProgress < 100 ) {
        int rem = m_startTime.secsTo( m_lastProgressUpdateTime ) * (100-d->lastProgress) / d->lastProgress;
        s += " / " + i18n("Remaining" ) + ": ";
        if ( rem >= 24*60*60 ) {
            s += i18np( "1 Day", "%1 Days", rem/(24*60*60) ) + ", ";
        }
        s += QTime().addSecs(rem%(24*60*60)).toString();
    }

    m_labelElapsedTime->setText( s );
}


void K3bJobProgressDialog::slotDebuggingOutput( const QString& type, const QString& output )
{
    m_logCache.addOutput( type, output );
    m_logFile.addOutput( type, output );
}


void K3bJobProgressDialog::slotShowDebuggingOutput()
{
    K3bDebuggingOutputDialog debugWidget( this );
    debugWidget.setOutput( m_logCache.toString() );
    debugWidget.exec();
}


void K3bJobProgressDialog::slotProgress( int percent )
{
    if( percent > d->lastProgress ) {
        d->lastProgress = percent;
        m_lastProgressUpdateTime = QDateTime::currentDateTime();
        if( KMainWindow* w = dynamic_cast<KMainWindow*>(kapp->mainWidget()) ) {
            w->setPlainCaption( QString( "(%1%) %2" ).arg(percent).arg(m_plainCaption) );
        }

        setCaption( QString( "(%1%) %2" ).arg(percent).arg(m_job->jobDescription()) );
    }
}


void K3bJobProgressDialog::keyPressEvent( QKeyEvent *e )
{
    e->accept();

    switch ( e->key() ) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        // if the process finished this closes the dialog
        if( button( User2 )->isVisible() )
            close();
        break;
    case Qt::Key_Escape:
        // simulate button clicks
        if( m_job && m_job->active() )
            slotCancelButtonPressed();
        else if( button( User2 )->isVisible() )
            close();
        break;
    default:
        // nothing
        break;
    }
}


QSize K3bJobProgressDialog::sizeHint() const
{
    QSize s = layout()->totalSizeHint();
    if( s.width() < s.height() )
        s.setWidth( s.height() );
    return s;
}


int K3bJobProgressDialog::startJob( K3bJob* job )
{
    if( job ) {
        setJob( job );
        //FIXME kde4
        //k3bappcore->jobInterface()->setJob( job );
    }
    else if( !m_job ) {
        kError() << "(K3bJobProgressDialog) null job!" << endl;
        return -1;
    }

    // the following code is mainly taken from QDialog::exec

    if ( d->eventLoop ) {
        kError() << "(K3bJobProgressDialog::startJob) Recursive call detected." << endl;
        return -1;
    }

    bool deleteOnClose = testAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_DeleteOnClose, false);

    bool wasShowModal = testAttribute(Qt::WA_ShowModal);
    setAttribute(Qt::WA_ShowModal, true);
    setResult(0);

    show();

    // start the job after showing the dialog
    m_job->start();

    QEventLoop eventLoop;
    d->eventLoop = &eventLoop;
    QPointer<QDialog> guard = this;
    (void) eventLoop.exec();
    if (guard.isNull())
        return QDialog::Rejected;
    d->eventLoop = 0;

    setAttribute(Qt::WA_ShowModal, wasShowModal);

    int res = result();
    if (deleteOnClose)
        delete this;
    return res;
}


void K3bJobProgressDialog::setVisible( bool visible )
{
    // we need to reimplement this since
    // QDialog does not know if we are in a loop from startJob

    KDialog::setVisible( visible );

    if ( !visible && d->eventLoop ) {
        d->eventLoop->exit();
    }
}


int K3bJobProgressDialog::waitForMedia( K3bDevice::Device* device,
                                        int mediaState,
                                        int mediaType,
                                        const QString& message )
{
    return K3bEmptyDiscWaiter::wait( device, mediaState, mediaType, message, this );
}


bool K3bJobProgressDialog::questionYesNo( const QString& text,
                                          const QString& caption,
                                          const QString& yesText,
                                          const QString& noText )
{
    return ( KMessageBox::questionYesNo( this,
                                         text,
                                         caption,
                                         yesText.isEmpty() ? KStandardGuiItem::yes() : KGuiItem(yesText),
                                         noText.isEmpty() ? KStandardGuiItem::no() : KGuiItem(noText) ) == KMessageBox::Yes );
}


void K3bJobProgressDialog::blockingInformation( const QString& text,
                                                const QString& caption )
{
    KMessageBox::information( this, text, caption );
}


void K3bJobProgressDialog::slotThemeChanged()
{
    if( K3bTheme* theme = k3bappcore->themeManager()->currentTheme() ) {
        d->progressHeaderFrame->setPaletteBackgroundColor( theme->backgroundColor() );
        d->progressHeaderFrame->setPaletteForegroundColor( theme->backgroundColor() );
        d->headerFrame->setPaletteBackgroundColor( theme->backgroundColor() );
        d->headerFrame->setPaletteForegroundColor( theme->backgroundColor() );
    }
}

#include "k3bjobprogressdialog.moc"
