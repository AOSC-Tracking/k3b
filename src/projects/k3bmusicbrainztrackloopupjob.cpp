/*
 *
 * Copyright (C) 2005-2008 Sebastian Trueg <trueg@k3b.org>
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

#include "k3bmusicbrainztrackloopupjob.h"
#include "k3btrm.h"
#include "k3bmusicbrainz.h"
#include <k3baudiotrack.h>

#include <KLocale>


class K3bMusicBrainzTrackLookupJob::Private
{
public:
    K3bAudioTrack* track;
    K3bTRM trm;
    K3bMusicBrainz mb;
    int results;
};


K3bMusicBrainzTrackLookupJob::K3bMusicBrainzTrackLookupJob( K3bJobHandler* hdl, QObject* parent )
    : K3bThreadJob( hdl, parent ),
      d( new Private() )
{
    d->track = 0;
}


K3bMusicBrainzTrackLookupJob::~K3bMusicBrainzTrackLookupJob()
{
    delete d;
}


void K3bMusicBrainzTrackLookupJob::setAudioTrack( K3bAudioTrack* track )
{
    d->track = track;
}


int K3bMusicBrainzTrackLookupJob::results()
{
    return d->results;
}


QString K3bMusicBrainzTrackLookupJob::title( int i ) const
{
    return d->mb.title( i );
}


QString K3bMusicBrainzTrackLookupJob::artist( int i ) const
{
    return d->mb.artist( i );
}


bool K3bMusicBrainzTrackLookupJob::run()
{
    if ( !d->track ) {
        emit infoMessage( "Internal error: no track set. This is a bug!", ERROR );
        return false;
    }

    emit infoMessage( i18n("Generating fingerprint for track %1.",
                           d->track->trackNumber()), INFO );

    d->track->seek(0);
    d->trm.start( d->track->length() );

    char buffer[2352*10];
    int len = 0;
    long long dataRead = 0;
    while( !canceled() &&
           (len = d->track->read( buffer, 2352*10 )) > 0 ) {

        dataRead += len;

        // swap data
        char buf;
        for( int i = 0; i < len-1; i+=2 ) {
            buf = buffer[i];
            buffer[i] = buffer[i+1];
            buffer[i+1] = buf;
        }

        if( d->trm.generate( buffer, len ) ) {
            len = 0;
            break;
        }

        // FIXME: useless since libmusicbrainz does never need all the data
        emit percent( 100*dataRead/d->track->length().audioBytes() );
    }

    if( canceled() ) {
        return false;
    }
    else if( len != 0 ||
             !d->trm.finalize() ) {
        return false;
    }

    emit infoMessage( i18n("Querying MusicBrainz for track %1.",
                           d->track->trackNumber()), INFO );

    d->results = d->mb.query( d->trm.signature() );
    return( d->results > 0 );
}

#include "k3bmusicbrainztrackloopupjob.moc"
