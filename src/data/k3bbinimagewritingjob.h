/*
 *
 * $Id$
 * Copyright (C) 2003 Klaus-Dieter Krannich <kd@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2003 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */


#ifndef K3BBINIMAGEWRITINGJOB_H
#define K3BBINIMAGEWRITINGJOB_H

#include <k3bjob.h>

class K3bAbstractWriter;
class K3bCdDevice::CdDevice;

/**
  *@author Klaus-Dieter Krannich
  */
class K3bBinImageWritingJob : public K3bBurnJob
{
  Q_OBJECT

 public: 
  K3bBinImageWritingJob( QObject* parent = 0 );
  ~K3bBinImageWritingJob();

  K3bDevice* writer() const { return m_device; };

  QString jobDescription() const;
  QString jobDetails() const;

 public slots:
  void start();
  void cancel();

  void setWriter( K3bDevice* dev ) { m_device = dev; }
  void setSimulate( bool b ) { m_simulate = b; }
  void setBurnproof( bool b ) { m_burnproof = b; }
  void setForce(bool b) { m_force = b; }
  void setMulti( bool b ) { m_noFix = b; }
  void setTocFile( const QString& s);
  void setCopies(int c) { m_copies = c; }
  void setSpeed( int s ) { m_speed = s; }

 private slots:
  void writerFinished(bool);
  void copyPercent(int p);
  void copySubPercent(int p);
  void slotNextTrack( int, int );

 private:
  void writerStart();
  bool prepareWriter();

  K3bDevice* m_device;
  bool m_simulate;
  bool m_burnproof;
  bool m_force;
  bool m_noFix;
  QString m_tocFile;
  int m_speed;
  int m_copies;
  int m_finishedCopies;

  bool m_canceled;

  K3bAbstractWriter* m_writer;

};

#endif
