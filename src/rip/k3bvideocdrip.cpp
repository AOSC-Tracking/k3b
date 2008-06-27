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

#include <kconfig.h>
#include <kdebug.h>
#include <kio/global.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>
#include <kurl.h>
#include <kprocess.h>

#include <qdatetime.h>
#include <qdom.h>
#include <qfile.h>
#include <qstring.h>
#include <qregexp.h>
#include <qtimer.h>
#include <q3url.h>
#include <QList>

// K3b Includes
#include "k3bvideocdrip.h"
#include <k3bcore.h>
#include <k3bexternalbinmanager.h>
#include <k3bglobals.h>

K3bVideoCdRip::K3bVideoCdRip( K3bJobHandler* hdl, K3bVideoCdRippingOptions* options, QObject* parent )
  : K3bJob( hdl, parent ),
    m_ripsourceType( 0 ),
    m_subPosition ( 0 ),
    m_videooptions( options ),
    m_canceled( false ),
    m_process( 0 )
{}


K3bVideoCdRip::~K3bVideoCdRip()
{
    if ( m_process )
        delete m_process;

}


void K3bVideoCdRip::cancel()
{
    cancelAll();

    emit infoMessage( i18n( "Job canceled by user." ), K3bJob::ERROR );
    emit canceled();
    jobFinished( false );
}


void K3bVideoCdRip::cancelAll()
{
    m_canceled = true;

    if ( m_process->state() != QProcess::NotRunning ) {
        m_process->disconnect( this );
        m_process->kill();
    }
}


void K3bVideoCdRip::start()
{
    kDebug() << "(K3bVideoCdRip) starting job";

    jobStarted();
    m_canceled = false;

    vcdxRip();
}

void K3bVideoCdRip::vcdxRip()
{
    emit newTask( i18n( "Check files" ) );

    m_stage = stageUnknown;
    delete m_process;
    m_process = new KProcess(this);

    const K3bExternalBin* bin = k3bcore ->externalBinManager() ->binObject( "vcdxrip" );

    if ( !bin ) {
        kDebug() << "(K3bVideoCdRip) could not find vcdxrip executable";
        emit infoMessage( i18n( "Could not find %1 executable." , QString("vcdxrip") ), K3bJob::ERROR );
        emit infoMessage( i18n( "To rip VideoCD's you must install VcdImager Version %1." , QString(">= 0.7.12") ), K3bJob::INFO );
        emit infoMessage( i18n( "You can find this on your distribution disks or download it from http://www.vcdimager.org" ), K3bJob::INFO );
        cancelAll();
        jobFinished( false );
        return ;
    }

    if( bin->version < K3bVersion("0.7.12") ) {
        kDebug() << "(K3bVideoCdRip) vcdxrip executable too old!";
        emit infoMessage( i18n( "%1 executable too old! Need version %2 or greater" , QString("Vcdxrip") , QString("0.7.12") ), K3bJob::ERROR );
        emit infoMessage( i18n( "You can find this on your distribution disks or download it from http://www.vcdimager.org" ), K3bJob::INFO );
        cancelAll();
        jobFinished( false );
        return ;
    }
        
    if ( !bin->copyright.isEmpty() )
        emit infoMessage( i18n( "Using %1 %2 - Copyright (C) %3" , bin->name() , bin->version , bin->copyright ), INFO );


    *m_process << k3bcore ->externalBinManager() ->binPath( "vcdxrip" );

    // additional user parameters from config
    const QStringList& params = k3bcore->externalBinManager() ->program( "vcdxrip" ) ->userParameters();
    for ( QStringList::const_iterator it = params.begin(); it != params.end(); ++it )
        *m_process << *it;

    *m_process << "--gui" << "--progress";

     if ( !m_videooptions ->getVideoCdRipFiles() )
        *m_process << "--nofiles";

     if ( !m_videooptions ->getVideoCdRipSegments() )
        *m_process << "--nosegments";
        
     if ( !m_videooptions ->getVideoCdRipSequences() )
        *m_process << "--nosequences";

     if ( m_videooptions ->getVideoCdIgnoreExt() )
        *m_process << "--no-ext-psd";

     if ( m_videooptions ->getVideoCdSector2336() )
        *m_process << "--sector-2336";
        
     *m_process << "-i" << QFile::encodeName( m_videooptions ->getVideoCdSource() );

     if ( m_videooptions ->getVideoCdExtractXml() )
        *m_process << "-o" << QFile::encodeName( m_videooptions ->getVideoCdDescription() + ".xml" );
     else
        *m_process << "-o" << "/dev/null";
      

    m_process->setOutputChannelMode( KProcess::MergedChannels );
    connect( m_process, SIGNAL( readyReadStandardOutput() ),
             this, SLOT( slotParseVcdXRipOutput() ) );
    connect( m_process, SIGNAL( finished( int, QProcess::ExitStatus ) ),
             this, SLOT( slotVcdXRipFinished( int, QProcess::ExitStatus ) ) );

    m_process->setWorkingDirectory( Q3Url( m_videooptions ->getVideoCdDestination() ).path() );

    // vcdxrip commandline parameters
    kDebug() << "***** vcdxrip parameters:";
    QStringList args = m_process->program();
    args.removeFirst();
    QString s = args.join(" ");
    kDebug() << s << flush;
    emit debuggingOutput( "vcdxrip command:", s );

    emit newTask( i18n( "Extracting" ) );
    emit infoMessage( i18n( "Start extracting." ), K3bJob::INFO );
    emit infoMessage( i18n( "Extract files from %1 to %2." , m_videooptions ->getVideoCdSource() , m_videooptions ->getVideoCdDestination() ), K3bJob::INFO );

    m_process->start();
    if ( !m_process->waitForFinished(-1) ) {
        kDebug() << "(K3bVideoCdRip) could not start vcdxrip";
        emit infoMessage( i18n( "Could not start %1." , QString("vcdxrip") ), K3bJob::ERROR );
        cancelAll();
        jobFinished( false );
    }
}

void K3bVideoCdRip::slotParseVcdXRipOutput()
{
    QString buffer = QString::fromLocal8Bit( m_process->readAllStandardOutput() );

    // split to lines
    QStringList lines = buffer.split( '\n' );

    QDomDocument xml_doc;
    QDomElement xml_root;

    // do every line
    QStringList::Iterator end( lines.end());
    for ( QStringList::Iterator str = lines.begin(); str != end; ++str ) {
        *str = ( *str ).trimmed();

        emit debuggingOutput( "vcdxrip", *str );

        xml_doc.setContent( QString( "<?xml version='1.0'?><vcdxrip>" ) + *str + "</vcdxrip>" );

        xml_root = xml_doc.documentElement();

        for ( QDomNode node = xml_root.firstChild(); !node.isNull(); node = node.nextSibling() ) {
            QDomElement el = node.toElement();
            if ( el.isNull() )
                continue;

            const QString tagName = el.tagName().toLower();

            if ( tagName == "progress" ) {
                const QString oper = el.attribute( "operation" ).toLower();
                const unsigned long long overallPos = el.attribute( "position" ).toLong();
                const unsigned long long pos = overallPos - m_subPosition;
                const unsigned long long size = el.attribute( "size" ).toLong() - m_subPosition;

                if ( oper == "extract" ) {
                    emit subPercent( ( int ) ( 100.0 * ( double ) pos / ( double ) size ) );
                    emit processedSubSize( ( pos * 2352 ) / 1024 / 1024 , ( size * 2352 ) / 1024 / 1024 );

                    m_bytesFinished = pos;

                    kDebug() << "(slotParseVcdXRipOutput) overall: " << ((long)overallPos  * 2352)
			      << ", videocdsize: " << m_videooptions->getVideoCdSize() << endl;
                    double relOverallWritten = ( ( double ) overallPos  * 2352 ) / ( double ) m_videooptions ->getVideoCdSize() ;
                    int newpercent =  ( int ) ( 100 * relOverallWritten );
                    if ( newpercent > m_oldpercent ) {
                        emit percent(  newpercent );
                        m_oldpercent = newpercent;
                    }

                } else {
                    return ;
                }

            } else if ( tagName == "log" ) {
                QDomText tel = el.firstChild().toText();
                const QString level = el.attribute( "level" ).toLower();
                if ( tel.isText() ) {
                    const QString text = tel.data();
                    if ( level == "information" ) {
                        kDebug() << QString( "(K3bVideoCdRip) vcdxrip information, %1" ).arg( text );
                        parseInformation( text );
                    } else {
                        if ( level != "error" ) {
                            kDebug() << QString( "(K3bVideoCdRip) vcdxrip warning, %1" ).arg( text );
                            emit debuggingOutput( "vcdxrip", text );
                            parseInformation( text );
                        } else {
                            kDebug() << QString( "(K3bVideoCdRip) vcdxrip error, %1" ).arg( text );
                            emit infoMessage( text, K3bJob::ERROR );
                        }
                    }
                }
            }
        }
    }
}


void K3bVideoCdRip::slotVcdXRipFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
    if ( exitStatus == QProcess::NormalExit ) {
        // TODO: check the process' exitStatus()
        switch ( exitCode ) {
            case 0:
                emit infoMessage( i18n( "Files successfully extracted." ), K3bJob::SUCCESS );
                break;
            default:
                emit infoMessage( i18n( "%1 returned an unknown error (code %2)." , QString("vcdxrip" ), exitCode ), K3bJob::ERROR );
                emit infoMessage( i18n( "Please send me an email with the last output..." ), K3bJob::ERROR );
                cancelAll();
                jobFinished( false );
                return ;
        }
    } else {
        emit infoMessage( i18n( "%1 did not exit cleanly." , QString("Vcdxrip") ), K3bJob::ERROR );
        cancelAll();
        jobFinished( false );
        return ;
    }

    jobFinished( true );
}

void K3bVideoCdRip::parseInformation( QString text )
{
    // parse warning
    if ( text.contains( "encountered non-form2 sector" ) ) {
        // I think this is an error not a warning. Finish ripping with invalid mpegs.
        emit infoMessage( i18n( "%1 encountered non-form2 sector" ,QString("Vcdxrip")), K3bJob::ERROR );
        emit infoMessage( i18n( "leaving loop" ), K3bJob::ERROR );
        cancelAll();
        jobFinished( false );
        return;
    }
    
    // parse extra info
    else if ( text.contains( "detected extended VCD2.0 PBC files" ) )
        emit infoMessage( i18n( "detected extended VCD2.0 PBC files" ), K3bJob::INFO );

    // parse startposition and extracting sequence info
    // extracting avseq05.mpg... (start lsn 32603 (+28514))
    else if ( text.startsWith( "extracting" ) ) {
        if ( text.contains( "(start lsn" ) ) {
            int index = text.indexOf( "(start lsn" );
            int end = text.indexOf( " (+" );
            if ( end > 0) {
                m_subPosition = text.mid( index + 11, end - index - 11 ).trimmed().toLong();
            }
            else {
                // found segment here we can get only the start lsn :)
                // extracting item0001.mpg... (start lsn 225, 1 segments)
                int end = text.indexOf(  ',', index );
                int overallPos = text.mid( index + 11, end - index - 11 ).trimmed().toLong();
                double relOverallWritten = ( ( double ) overallPos  * 2352 ) / ( double ) m_videooptions ->getVideoCdSize()  ;
                int newpercent =  ( int ) ( 100 * relOverallWritten );
                if ( newpercent > m_oldpercent ) {
                    emit percent(  newpercent );
                    m_oldpercent = newpercent;
                }
            }


            index = 11;
            end = text.indexOf( "(start lsn" );
            emit newSubTask( i18n( "Extracting %1" , text.mid( index, end - index ).trimmed() ) );
        }
        // parse extracting files info
        // extracting CDI/CDI_IMAG.RTF to _cdi_cdi_imag.rtf (lsn 258, size 1315168, raw 1)
        else if ( text.contains( "(lsn" ) && text.contains( "size" ) ) {
            int index = 11;
            int end = text.indexOf( "to" );
            QString extractFileName = text.mid( index, end - index ).trimmed();
            index = text.indexOf( " to " );
            end = text.indexOf( " (lsn" );
            QString toFileName = text.mid( index + 4, end - index - 4 ).trimmed();
            emit newSubTask( i18n( "Extracting %1 to %2" , extractFileName ,toFileName ) );
        }
    }
}

QString K3bVideoCdRip::jobDescription() const
{
    return i18n( "Extracting %1" , m_videooptions ->getVideoCdDescription() );
}

QString K3bVideoCdRip::jobDetails() const
{
    return QString( "(%1)" ).arg ( KIO::convertSize( m_videooptions ->getVideoCdSize() ) );
}

#include "k3bvideocdrip.moc"
