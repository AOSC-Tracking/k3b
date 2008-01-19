/*
 *
 * Copyright (C) 2004-2008 Sebastian Trueg <trueg@k3b.org>
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

#include "k3baudioimager.h"
#include "k3baudiodoc.h"
#include "k3baudiotrack.h"
#include "k3baudiodatasource.h"

#include <k3bthread.h>
#include <k3bwavefilewriter.h>

#include <klocale.h>
#include <kdebug.h>

#include <qfile.h>

#include <unistd.h>


class K3bAudioImager::Private
{
public:
    Private()
        : fd(-1) {
    }

    int fd;
    QStringList imageNames;
    K3bAudioImager::ErrorType lastError;
    K3bAudioDoc* doc;
};



K3bAudioImager::K3bAudioImager( K3bAudioDoc* doc, K3bJobHandler* jh, QObject* parent )
    : K3bThreadJob( jh, parent ),
      d( new Private() )
{
    d->doc = doc;
}


K3bAudioImager::~K3bAudioImager()
{
    delete d;
}


void K3bAudioImager::writeToFd( int fd )
{
    d->fd = fd;
}


void K3bAudioImager::setImageFilenames( const QStringList& p )
{
    d->imageNames = p;
    d->fd = -1;
}


K3bAudioImager::ErrorType K3bAudioImager::lastErrorType() const
{
    return d->lastError;
}


bool K3bAudioImager::run()
{
    d->lastError = K3bAudioImager::ERROR_UNKNOWN;

    QStringList::iterator imageFileIt = d->imageNames.begin();
    K3bWaveFileWriter waveFileWriter;

    K3bAudioTrack* track = d->doc->firstTrack();
    int trackNumber = 1;
    unsigned long long totalSize = d->doc->length().audioBytes();
    unsigned long long totalRead = 0;
    char buffer[2352 * 10];

    while( track ) {

        emit nextTrack( trackNumber, d->doc->numOfTracks() );

        //
        // Seek to the beginning of the track
        //
        if( !track->seek(0) ) {
            emit infoMessage( i18n("Unable to seek in track %1.", trackNumber), K3bJob::ERROR );
            return false;
        }

        //
        // Initialize the reading
        //
        int read = 0;
        unsigned long long trackRead = 0;

        //
        // Create the image file
        //
        if( d->fd == -1 ) {
            if( !waveFileWriter.open( *imageFileIt ) ) {
                emit infoMessage( i18n("Could not open %1 for writing", *imageFileIt), K3bJob::ERROR );
                return false;
            }
        }

        //
        // Read data from the track
        //
        while( (read = track->read( buffer, sizeof(buffer) )) > 0 ) {
            if( d->fd == -1 ) {
                waveFileWriter.write( buffer, read, K3bWaveFileWriter::BigEndian );
            }
            else {
                if( ::write( d->fd, reinterpret_cast<void*>(buffer), read ) != read ) {
                    kDebug() << "(K3bAudioImager::WorkThread) writing to fd " << d->fd << " failed.";
                    d->lastError = K3bAudioImager::ERROR_FD_WRITE;
                    return false;
                }
            }

            if( canceled() ) {
                return false;
            }

            //
            // Emit progress
            //
            totalRead += read;
            trackRead += read;

            emit subPercent( 100*trackRead/track->length().audioBytes() );
            emit percent( 100*totalRead/totalSize );
            emit processedSubSize( trackRead/1024/1024, track->length().audioBytes()/1024/1024 );
            emit processedSize( totalRead/1024/1024, totalSize/1024/1024 );
        }

        if( read < 0 ) {
            emit infoMessage( i18n("Error while decoding track %1.", trackNumber), K3bJob::ERROR );
            kDebug() << "(K3bAudioImager::WorkThread) read error on track " << trackNumber
                     << " at pos " << K3b::Msf(trackRead/2352) << endl;
            d->lastError = K3bAudioImager::ERROR_DECODING_TRACK;
            return false;
        }

        track = track->next();
        trackNumber++;
        imageFileIt++;
    }

    return true;
}

#include "k3baudioimager.moc"
