/*
 *
 * Copyright (C) 2008 Sebastian Trueg <trueg@k3b.org>
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

#include "k3baudiofileanalyzerjob.h"
#include "k3baudiodecoder.h"

class K3bAudioFileAnalyzerJob::Private
{
public:
    K3bAudioDecoder* decoder;
};


K3bAudioFileAnalyzerJob::K3bAudioFileAnalyzerJob( K3bJobHandler* hdl, QObject* parent )
    : K3bThreadJob( hdl, parent ),
      d( new Private() )
{
    d->decoder = 0;
}


K3bAudioFileAnalyzerJob::~K3bAudioFileAnalyzerJob()
{
    delete d;
}


void K3bAudioFileAnalyzerJob::setDecoder( K3bAudioDecoder* decoder )
{
    d->decoder = decoder;
}


K3bAudioDecoder* K3bAudioFileAnalyzerJob::decoder() const
{
    return d->decoder;
}


bool K3bAudioFileAnalyzerJob::run()
{
    if ( !d->decoder ) {
        emit infoMessage( "Internal error: no decoder set. This is a bug.", ERROR );
        return false;
    }

    return d->decoder->analyseFile();
}

#include "k3baudiofileanalyzerjob.moc"
