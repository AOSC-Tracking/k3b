/*
 *
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

#include "k3bexternalencoder.h"
#include "k3bexternalencodercommand.h"

#include <k3bprocess.h>
#include <k3bcore.h>

#include <kdebug.h>
#include <kconfig.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include <qregexp.h>
#include <qfile.h>
//Added by qt3to4:
#include <Q3CString>
#include <Q3ValueList>

#include <sys/types.h>
#include <sys/wait.h>


K3B_EXPORT_PLUGIN(k3bexternalencoder, K3bExternalEncoder)



static const char s_riffHeader[] =
{
    0x52, 0x49, 0x46, 0x46, // 0  "RIFF"
    0x00, 0x00, 0x00, 0x00, // 4  wavSize
    0x57, 0x41, 0x56, 0x45, // 8  "WAVE"
    0x66, 0x6d, 0x74, 0x20, // 12 "fmt "
    0x10, 0x00, 0x00, 0x00, // 16
    0x01, 0x00, 0x02, 0x00, // 20
    0x44, 0xac, 0x00, 0x00, // 24
    0x10, 0xb1, 0x02, 0x00, // 28
    0x04, 0x00, 0x10, 0x00, // 32
    0x64, 0x61, 0x74, 0x61, // 36 "data"
    0x00, 0x00, 0x00, 0x00  // 40 byteCount
};





static K3bExternalEncoderCommand commandByExtension( const QString& extension )
{
    Q3ValueList<K3bExternalEncoderCommand> cmds( K3bExternalEncoderCommand::readCommands() );
    for( Q3ValueList<K3bExternalEncoderCommand>::iterator it = cmds.begin(); it != cmds.end(); ++it )
        if( (*it).extension == extension )
            return *it;

    kDebug() << "(K3bExternalEncoder) could not find command for extension " << extension;

    return K3bExternalEncoderCommand();
}


class K3bExternalEncoder::Private
{
public:
    Private()
        : process(0) {
    }

    K3bProcess* process;
    QString fileName;
    QString extension;
    K3b::Msf length;

    K3bExternalEncoderCommand cmd;

    bool initialized;

    // the metaData we support
    QString artist;
    QString title;
    QString comment;
    QString trackNumber;
    QString cdArtist;
    QString cdTitle;
    QString cdComment;
    QString year;
    QString genre;
};


K3bExternalEncoder::K3bExternalEncoder( QObject* parent, const QVariantList& )
    : K3bAudioEncoder( parent)
{
    d = new Private();
}


K3bExternalEncoder::~K3bExternalEncoder()
{
    delete d->process;
    delete d;
}


void K3bExternalEncoder::setMetaDataInternal( K3bAudioEncoder::MetaDataField f, const QString& value )
{
    switch( f ) {
    case META_TRACK_TITLE:
        d->title = value;
        break;
    case META_TRACK_ARTIST:
        d->artist = value;
        break;
    case META_TRACK_COMMENT:
        d->comment = value;
        break;
    case META_TRACK_NUMBER:
        d->trackNumber = value;
        break;
    case META_ALBUM_TITLE:
        d->cdTitle = value;
        break;
    case META_ALBUM_ARTIST:
        d->cdArtist = value;
        break;
    case META_ALBUM_COMMENT:
        d->cdComment = value;
        break;
    case META_YEAR:
        d->year = value;
        break;
    case META_GENRE:
        d->genre = value;
        break;
    }
}


void K3bExternalEncoder::finishEncoderInternal()
{
    if( d->process ) {
        if( d->process->isRunning() ) {
            ::close( d->process->stdinFd() );

            // this is kind of evil...
            // but we need to be sure the process exited when this method returnes
            ::waitpid( d->process->pid(), 0, 0 );
        }
    }
}


void K3bExternalEncoder::slotExternalProgramFinished( K3Process* p )
{
    if( !p->normalExit() || p->exitStatus() != 0 )
        kDebug() << "(K3bExternalEncoder) program exited with error.";
}


bool K3bExternalEncoder::openFile( const QString& ext, const QString& filename, const K3b::Msf& length )
{
    d->fileName = filename;
    d->extension = ext;
    d->initialized = false;
    d->length = length;

    // delete existing files as some programs (like flac for example) might refuse to overwrite files
    if( QFile::exists( filename ) )
        QFile::remove( filename );

    return true;
}


void K3bExternalEncoder::closeFile()
{
    finishEncoderInternal();
}


bool K3bExternalEncoder::initEncoderInternal( const QString& extension )
{
    d->initialized = true;

    // find the correct command
    d->cmd = commandByExtension( extension );

    if( d->cmd.command.isEmpty() ) {
        setLastError( i18n("Invalid command: the command is empty.") );
        return false;
    }

    // setup the process
    delete d->process;
    d->process = new K3bProcess();
    d->process->setSplitStdout(true);
    d->process->setRawStdin(true);

    connect( d->process, SIGNAL(processExited(K3Process*)),
             this, SLOT(slotExternalProgramFinished(K3Process*)) );
    connect( d->process, SIGNAL(stderrLine(const QString&)),
             this, SLOT(slotExternalProgramOutputLine(const QString&)) );
    connect( d->process, SIGNAL(stdoutLine(const QString&)),
             this, SLOT(slotExternalProgramOutputLine(const QString&)) );


    // create the commandline
    QStringList params = QStringList::split( ' ', d->cmd.command, false );
    for( QStringList::iterator it = params.begin(); it != params.end(); ++it ) {
        (*it).replace( "%f", d->fileName );
        (*it).replace( "%a", d->artist );
        (*it).replace( "%t", d->title );
        (*it).replace( "%c", d->comment );
        (*it).replace( "%y", d->year );
        (*it).replace( "%m", d->cdTitle );
        (*it).replace( "%r", d->cdArtist );
        (*it).replace( "%x", d->cdComment );
        (*it).replace( "%n", d->trackNumber );
        (*it).replace( "%g", d->genre );

        *d->process << *it;
    }


    kDebug() << "***** external parameters:";
    const QList<QByteArray>& args = d->process->args();
    QString s;
    for( QList<QByteArray>::const_iterator it = args.begin(); it != args.end(); ++it ) {
        s += *it + " ";
    }
    kDebug() << s << flush;

    // set one general error message
    setLastError( i18n("Command failed: %1", s ) );

    if( d->process->start( K3Process::NotifyOnExit, K3Process::All ) ) {
        if( d->cmd.writeWaveHeader )
            return writeWaveHeader();
        else
            return true;
    }
    else {
        QString commandName = d->cmd.command.section( QRegExp("\\s+"), 0 );
        if( !KStandardDirs::findExe( commandName ).isEmpty() )
            setLastError( i18n("Could not find program '%1'",commandName) );

        return false;
    }
}


bool K3bExternalEncoder::writeWaveHeader()
{
    kDebug() << "(K3bExternalEncoder) writing wave header";

    // write the RIFF thing
    if( ::write( d->process->stdinFd(), s_riffHeader, 4 ) != 4 ) {
        kDebug() << "(K3bExternalEncoder) failed to write riff header.";
        return false;
    }

    // write the wave size
    qint32 dataSize( d->length.audioBytes() );
    qint32 wavSize( dataSize + 44 - 8 );
    char c[4];

    c[0] = (wavSize   >> 0 ) & 0xff;
    c[1] = (wavSize   >> 8 ) & 0xff;
    c[2] = (wavSize   >> 16) & 0xff;
    c[3] = (wavSize   >> 24) & 0xff;

    if( ::write( d->process->stdinFd(), c, 4 ) != 4 ) {
        kDebug() << "(K3bExternalEncoder) failed to write wave size.";
        return false;
    }

    // write static part of the header
    if( ::write( d->process->stdinFd(), s_riffHeader+8, 32 ) != 32 ) {
        kDebug() << "(K3bExternalEncoder) failed to write wave header.";
        return false;
    }

    c[0] = (dataSize   >> 0 ) & 0xff;
    c[1] = (dataSize   >> 8 ) & 0xff;
    c[2] = (dataSize   >> 16) & 0xff;
    c[3] = (dataSize   >> 24) & 0xff;

    if( ::write( d->process->stdinFd(), c, 4 ) != 4 ) {
        kDebug() << "(K3bExternalEncoder) failed to write data size.";
        return false;
    }

    return true;
}


long K3bExternalEncoder::encodeInternal( const char* data, Q_ULONG len )
{
    if( !d->initialized )
        if( !initEncoderInternal( d->extension ) )
            return -1;

    if( d->process ) {
        if( d->process->isRunning() ) {

            long written = 0;

            //
            // we swap the bytes to reduce user irritation ;)
            // This is a little confused: We used to swap the byte order
            // in older versions of this encoder since little endian seems
            // to "feel" more natural.
            // So now that we have a swap option we have to invert it to ensure
            // compatibility
            //
            if( !d->cmd.swapByteOrder ) {
                char* buffer = new char[len];
                for( unsigned int i = 0; i < len-1; i+=2 ) {
                    buffer[i] = data[i+1];
                    buffer[i+1] = data[i];
                }

                written = ::write( d->process->stdinFd(), (const void*)buffer, len );
                delete [] buffer;
            }
            else
                written = ::write( d->process->stdinFd(), (const void*)data, len );

            return written;
        }
        else
            return -1;
    }
    else
        return -1;
}


void K3bExternalEncoder::slotExternalProgramOutputLine( const QString& line )
{
    kDebug() << "(" << d->cmd.name << ") " << line;
}


QStringList K3bExternalEncoder::extensions() const
{
    QStringList el;
    Q3ValueList<K3bExternalEncoderCommand> cmds( K3bExternalEncoderCommand::readCommands() );
    for( Q3ValueList<K3bExternalEncoderCommand>::iterator it = cmds.begin(); it != cmds.end(); ++it )
        el.append( (*it).extension );

    return el;
}


QString K3bExternalEncoder::fileTypeComment( const QString& ext ) const
{
    return commandByExtension( ext ).name;
}

#include "k3bexternalencoder.moc"
