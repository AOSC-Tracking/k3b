/***************************************************************************
                          k3bcdrecordwriter.cpp  -  description
                             -------------------
    begin                : Mon Mar 26 15:30:59 CEST 2001
    copyright            : (C) 2001 by Sebastian Trueg
    email                : trueg@informatik.uni-freiburg.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "k3bcdrecordwriter.h"

#include <k3b.h>
#include <k3bexternalbinmanager.h>
#include <k3bprocess.h>
#include <device/k3bdevice.h>

#include <qstring.h>
#include <qstringlist.h>
#include <qvaluelist.h>
#include <qregexp.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfig.h>

#include <errno.h>
#include <string.h>


K3bCdrecordWriter::K3bCdrecordWriter( K3bDevice* dev, QObject* parent, const char* name )
  : K3bAbstractWriter( dev, parent, name ),
    m_dao(false),
    m_rawWrite(false),
    m_stdin(false)
{
  m_process = 0;
}


K3bCdrecordWriter::~K3bCdrecordWriter()
{
  delete m_process;
}


void K3bCdrecordWriter::prepareArgumentList()
{
  if( m_process ) delete m_process;  // kdelibs want this!
  m_process = new K3bProcess();
  m_process->setSplitStdout(true);
  connect( m_process, SIGNAL(stdoutLine(const QString&)), this, SLOT(slotStdLine(const QString&)) );
  connect( m_process, SIGNAL(stderrLine(const QString&)), this, SLOT(slotStdLine(const QString&)) );
  connect( m_process, SIGNAL(processExited(KProcess*)), this, SLOT(slotProcessExited(KProcess*)) );
  connect( m_process, SIGNAL(wroteStdin(KProcess*)), this, SIGNAL(dataWritten()) );

  m_cdrecordBinObject = K3bExternalBinManager::self()->binObject("cdrecord");
  if( !m_cdrecordBinObject )
    return;

  *m_process << m_cdrecordBinObject->path;

  // display progress
  *m_process << "-v";
    
  if( m_cdrecordBinObject->hasFeature( "gracetime") )
    *m_process << "gracetime=2";  // 2 is the lowest allowed value (Joerg, why do you do this to us?)
    
  // Again we assume the device to be set!
  *m_process << QString("dev=%1").arg(burnDevice()->busTargetLun());
  *m_process << QString("speed=%1").arg(burnSpeed());
    
  if( m_dao ) {
    if( burnDevice()->dao() )
      *m_process << "-dao";
    else
      emit infoMessage( i18n("Writer does not support disk at once (DAO) recording"), INFO );
  }
    
  if( simulate() )
    *m_process << "-dummy";
    
  if( burnproof() ) {
    if( burnDevice()->burnproof() ) {
      // with cdrecord 1.11a02 burnproof was renamed to burnfree
      // we have to be aware that 1.9 is bigger than 1.11
      int dotpos = m_cdrecordBinObject->version.find(".");
      int alphapos = m_cdrecordBinObject->version.find( QRegExp("\\D"), dotpos+1 );
	
      int major = m_cdrecordBinObject->version.left( dotpos ).toInt();
      int minor = m_cdrecordBinObject->version.mid( dotpos + 1, (alphapos > 0 ? alphapos-dotpos-1 : -1 ) ).toInt();
      QString alpha = (alphapos > 0 ? m_cdrecordBinObject->version.mid( alphapos ) : "" );
	
      kdDebug() << "(K3bCdrecordWriter) dotpos: " << dotpos << "; alphapos: " << alphapos << endl;
      kdDebug() << "(K3bCdrecordWriter) major: " << major << "; minor: " << minor << "; alpha: " << alpha << endl;
	
      if( major > 1 || 
	  (major == 1 && 
	   (minor > 11 || minor == 11 && alpha > "a02") ) )
	*m_process << "driveropts=burnfree";
      else
	*m_process << "driveropts=burnproof";
    }
    else
      emit infoMessage( i18n("Writer does not support buffer underrun free recording (BURNPROOF)"), INFO );
  }
    
  if( k3bMain()->eject() )
    *m_process << "-eject";

  kapp->config()->setGroup("General Options");
    
  bool manualBufferSize = k3bMain()->config()->readBoolEntry( "Manual buffer size", false );
  if( manualBufferSize ) {
    *m_process << QString("fs=%1m").arg( k3bMain()->config()->readNumEntry( "Cdrecord buffer", 4 ) );
  }
    
  bool overburn = k3bMain()->config()->readBoolEntry( "Allow overburning", false );
  if( overburn )
    if( m_cdrecordBinObject->hasFeature("overburn") )
      *m_process << "-overburn";
    else
      emit infoMessage( i18n("Cdrecord version <= 1.10 does not support overburning!"), INFO );
    
  // additional user parameters from config
  QStringList params = m_cdrecordBinObject->userParameters();
  for( QStringList::Iterator it = params.begin(); it != params.end(); ++it )
    *m_process << *it;
}


K3bCdrecordWriter* K3bCdrecordWriter::addArgument( const QString& arg )
{
  *m_process << arg;
  return this;
}


void K3bCdrecordWriter::start()
{
  if( !m_cdrecordBinObject ) {
    emit infoMessage( i18n("Could not find cdrecord executable."), ERROR );
    emit finished(false);
    return;
  }

  kdDebug() << "***** cdrecord parameters:\n";
  const QValueList<QCString>& args = m_process->args();
  QString s;
  for( QValueList<QCString>::const_iterator it = args.begin(); it != args.end(); ++it ) {
    s += *it + " ";
  }
  kdDebug() << s << endl << flush;

  m_currentTrack = 0;
  m_cdrecordError = UNKNOWN;
  m_totalTracksParsed = false;
  m_alreadyWritten = 0;
  m_trackSize = 0;
  m_totalSize = 0;

  emit newSubTask( i18n("Preparing write process...") );

  if( !m_process->start( KProcess::NotifyOnExit, m_stdin ? KProcess::All : KProcess::AllOutput ) ) {
    // something went wrong when starting the program
    // it "should" be the executable
    kdDebug() << "(K3bCdrecordWriter) could not start cdrecord" << endl;
    emit infoMessage( i18n("Could not start cdrecord!"), K3bJob::ERROR );
  }
  else {
    if( simulate() ) {
      emit infoMessage( i18n("Starting simulation at %1x speed...").arg(burnSpeed()), K3bJob::STATUS );
      emit newTask( i18n("Simulating") );
    }
    else {
      emit infoMessage( i18n("Starting recording at %1x speed...").arg(burnSpeed()), K3bJob::STATUS );
      emit newTask( i18n("Writing") );
    }

    m_writeSpeedInitialized = false;
    emit started();
  }
}


void K3bCdrecordWriter::cancel()
{
  if( m_process ) {
    if( m_process->isRunning() ) {
      m_process->disconnect();
      m_process->kill();

      // we need to unlock the writer because cdrecord locked it while writing
      bool block = burnDevice()->block( false );
      if( !block )
	emit infoMessage( i18n("Could not unlock CD drive."), K3bJob::ERROR );
      else if( k3bMain()->eject() )
	burnDevice()->eject();
    }
    
    emit canceled();
    emit finished( false );
  }
}


bool K3bCdrecordWriter::write( const char* data, int len )
{
  return m_process->writeStdin( data, len );
}


void K3bCdrecordWriter::slotStdLine( const QString& line )
{
  emit debuggingOutput( "cdrecord", line );

  if( line.startsWith( "Track" ) )
    {
      if( !m_totalTracksParsed ) {
	// this is not the progress display but the list of tracks that will get written
	// we always extract the tracknumber to get the highest at last
	bool ok;
	int tt = line.mid( 6, 2 ).toInt(&ok);
	if( ok )
	  m_totalTracks = tt;

	int sizeStart = line.find( QRegExp("\\d"), 10 );
	int sizeEnd = line.find( "MB", sizeStart );
	int ts = line.mid( sizeStart, sizeEnd-sizeStart ).toInt();
	m_totalSize += ts;
      }

      else if( line.contains( "fifo", false ) > 0 )
	{
	  // parse progress
	  int num, made, size, fifo;
	  bool ok;

	  // --- parse number of track ---------------------------------------
	  // ----------------------------------------------------------------------
	  int pos1 = 5;
	  int pos2 = line.find(':');
	  if( pos1 == -1 ) {
	    kdDebug() << "parsing did not work" << endl;
	    return;
	  }
	  // now pos2 to the first colon :-)
	  num = line.mid(pos1,pos2-pos1).toInt(&ok);
	  if(!ok)
	    kdDebug() << "parsing did not work" << endl;

	  // --- parse already written Megs -----------------------------------
	  // ----------------------------------------------------------------------
	  pos1 = line.find(':');
	  if( pos1 == -1 ) {
	    kdDebug() << "parsing did not work" << endl;
	    return;
	  }
	  pos2 = line.find("of");
	  if( pos2 == -1 ) {
	    kdDebug() << "parsing did not work" << endl;
	    return;
	  }
	  // now pos1 point to the colon and pos2 to the 'o' of 'of' :-)
	  pos1++;
	  made = line.mid(pos1,pos2-pos1).toInt(&ok);
	  if(!ok)
	    kdDebug() << "parsing did not work" << endl;

	  // --- parse total size of track ---------------------------------------
	  // ------------------------------------------------------------------------
	  pos1 = line.find("MB");
	  if( pos1 == -1 ) {
	    kdDebug() << "parsing did not work" << endl;
	    return;
	  }
	  // now pos1 point to the 'M' of 'MB' and pos2 to the 'o' of 'of' :-)
	  pos2 += 2;
	  size = line.mid(pos2,pos1-pos2).toInt(&ok);
	  if(!ok)
	    kdDebug() << "parsing did not work" << endl;

	  // --- parse status of fifo --------------------------------------------
	  // ------------------------------------------------------------------------
	  pos1 = line.find("fifo");
	  if( pos1 == -1 ) {
	    kdDebug() << "parsing did not work" << endl;
	    return;
	  }
	  pos2 = line.find('%');
	  if( pos2 == -1 ) {
	    kdDebug() << "parsing did not work" << endl;
	    return;
	  }
	  // now pos1 point to the 'f' of 'fifo' and pos2 to the %o'  :-)
	  pos1+=4;
	  fifo = line.mid(pos1,pos2-pos1).toInt(&ok);
	  if(!ok)
	    kdDebug() << "parsing did not work" << endl;

	  // -------------------------------------------------------------------
	  // -------- parsing finished --------------------------------------

	  emit buffer( fifo );
	  emit processedSubSize( made, size );
	  emit subPercent( 100*made/size );

	  if( m_totalSize > 0 ) {
	    emit processedSize( m_alreadyWritten+made, m_totalSize );
	    emit percent( 100*(m_alreadyWritten+made)/m_totalSize );
	  }

	  createEstimatedWriteSpeed( m_alreadyWritten+made, !m_writeSpeedInitialized );
	  m_writeSpeedInitialized = true;

	  m_trackSize = size;
	}
    }
  else if( line.contains( "at speed" ) ) {
    // parse the speed and inform the user if cdrdao switched it down
    int pos = line.find( "at speed" );
    int po2 = line.find( QRegExp("\\D"), pos + 9 );
    int speed = line.mid( pos+9, po2-pos-9 ).toInt();
    if( speed < burnSpeed() ) {
      emit infoMessage( i18n("Medium does not support writing at %1x speed").arg(burnSpeed()), K3bJob::INFO );
      emit infoMessage( i18n("Switching down burn speed to %1x").arg(speed), K3bJob::PROCESS );
    }
  }
  else if( line.startsWith( "Starting new" ) )
    {
      m_totalTracksParsed = true;
      m_alreadyWritten += m_trackSize;
      kdDebug() << "(K3bCdrecordWriter) writing " << m_totalTracks << " tracks." << endl;
      m_currentTrack++;
      emit nextTrack( m_currentTrack, m_totalTracks );
      //      emit newSubTask( i18n("Writing track %1").arg(m_currentTrack) );
    }
  else if( line.startsWith( "Fixating" ) ) {
    emit newSubTask( i18n("Fixating") );
  }
  else if( line.contains("seconds.") ) {
    int pos2 = line.find("seconds.") - 2;
    int pos1 = line.findRev( QRegExp("\\D"), pos2 ) + 1;
    emit infoMessage( i18n("Starting in 1 second", 
			   "Starting in %n seconds", 
			   line.mid(pos1, pos2-pos1+1).toInt()), K3bJob::PROCESS );
  }
  else if( line.startsWith( "Writing pregap" ) ) {
    emit newSubTask( i18n("Writing pregap") );
  }
  else if( line.startsWith( "Performing OPC" ) ) {
    emit infoMessage( i18n("Performing OPC"), K3bJob::PROCESS );
  }
  else if( line.startsWith( "Sending" ) ) {
    emit infoMessage( i18n("Sending CUE sheet"), K3bJob::PROCESS );
  }
  else if( line.contains( "Turning BURN-Proof" ) ) {
    emit infoMessage( i18n("Enabled BURN-Proof"), K3bJob::PROCESS );
  }
  else if( line.contains( "Drive needs to reload the media" ) ) {
    emit infoMessage( i18n("Reloading of media required"), K3bJob::PROCESS );
  }
  else if( line.contains( "Drive does not support SAO" ) ) {
    emit infoMessage( i18n("SAO, DAO recording not supported with this writer"), K3bJob::ERROR );
    emit infoMessage( i18n("Please turn off DAO (disk at once) and try again"), K3bJob::ERROR );
  }
  else if( line.contains("Data may not fit") ) {
    bool overburn = k3bMain()->config()->readBoolEntry( "Allow overburning", false );
    if( overburn && m_cdrecordBinObject->hasFeature("overburn") )
      emit infoMessage( i18n("Trying to write more than the official disk capacity"), K3bJob::INFO );
    m_cdrecordError = OVERSIZE;
  }
  else if( line.contains("Bad Option") ) {
    m_cdrecordError = BAD_OPTION;
    // parse option
    int pos = line.find( "Bad Option" ) + 13;
    int len = line.length() - pos - 1;
    emit infoMessage( i18n("No valid cdrecord option: %1").arg(line.mid(pos, len)), ERROR );
  }
  else if( line.contains("shmget failed") ) {
    m_cdrecordError = SHMGET_FAILED;
  }
  else if( line.contains("OPC failed") ) {
    m_cdrecordError = OPC_FAILED;
  }
  else if( line.contains("Cannot set speed/dummy") ) {
    m_cdrecordError = CANNOT_SET_SPEED;
  }
  else {
    // debugging
    kdDebug() << "(cdrecord) " << line << endl;
  }
}


void K3bCdrecordWriter::slotProcessExited( KProcess* p )
{
  if( p->normalExit() ) {
    switch( p->exitStatus() ) {
    case 0:
      if( simulate() )
	emit infoMessage( i18n("Simulation successfully finished"), K3bJob::STATUS );
      else
	emit infoMessage( i18n("Writing successfully finished"), K3bJob::STATUS );

      createAverageWriteSpeedInfoMessage();
      emit finished( true );
      break;

    default:
      switch( m_cdrecordError ) {
      case OVERSIZE:  // It seems as if this is error code 254 but I'm not sure...
	if( k3bMain()->config()->readBoolEntry( "Allow overburning", false ) &&
	    m_cdrecordBinObject->hasFeature("overburn") )
	  emit infoMessage( i18n("Data did not fit on disk."), ERROR );
	else
	  emit infoMessage( i18n("Data does not fit on disk."), ERROR );
	break;
      case BAD_OPTION:
	// error message has already been emited earlier since we needed the actual line
	break;
      case SHMGET_FAILED:
	emit infoMessage( i18n("Cdrecord could not reserve shared memory segment of requested size."), ERROR );
	emit infoMessage( i18n("Probably you chose a too large buffer size."), ERROR );
	break;
      case OPC_FAILED:
	emit infoMessage( i18n("OPC failed. Probably the writer does not like the disk."), ERROR );
	break;
      case CANNOT_SET_SPEED:
	emit infoMessage( i18n("Unable to set write speed to %1.").arg(burnSpeed()), ERROR );
	emit infoMessage( i18n("Probably this is lower than your writer's lowest writing speed."), ERROR );
	break;
      case UNKNOWN:
	// no recording device and also other errors!! :-(
	emit infoMessage( i18n("Cdrecord returned an unknown error! (code %1)").arg(p->exitStatus()), 
			  K3bJob::ERROR );
	emit infoMessage( strerror(p->exitStatus()), K3bJob::ERROR );
	emit infoMessage( i18n("Please send me an email with the last output."), K3bJob::ERROR );
	break;
      }
      emit finished( false );
    }
  }
  else {
    emit infoMessage( i18n("Cdrecord did not exit cleanly."), K3bJob::ERROR );
    emit finished( false );
  }
}


#include "k3bcdrecordwriter.moc"
