/*
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


#include "k3bglobals.h"
#include <k3bversion.h>
#include <k3bdevice.h>
#include <k3bdevicemanager.h>
#include <k3bdeviceglobals.h>
#include <k3bexternalbinmanager.h>
#include <k3bcore.h>

#include <kdeversion.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kurl.h>
#include <kprocess.h>

#include <kmountpoint.h>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/OpticalDrive>

#include <qdatastream.h>
#include <qdir.h>
#include <qfile.h>

#include <cmath>
#include <sys/utsname.h>
#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#  include <sys/param.h>
#  include <sys/mount.h>
#  include <sys/endian.h>
#  define bswap_16(x) bswap16(x)
#  define bswap_32(x) bswap32(x)
#  define bswap_64(x) bswap64(x)
#else
#  include <byteswap.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif


/*
  struct Sample {
  unsigned char msbLeft;
  unsigned char lsbLeft;
  unsigned char msbRight;
  unsigned char lsbRight;

  short left() const {
  return ( msbLeft << 8 ) | lsbLeft;
  }
  short right() const {
  return ( msbRight << 8 ) | lsbRight;
  }
  void left( short d ) {
  msbLeft = d >> 8;
  lsbLeft = d;
  }
  void right( short d ) {
  msbRight = d >> 8;
  lsbRight = d;
  }
  };
*/

QString K3b::framesToString( int h, bool showFrames )
{
    int m = h / 4500;
    int s = (h % 4500) / 75;
    int f = h % 75;

    QString str;

    if( showFrames ) {
        // cdrdao needs the MSF format where 1 second has 75 frames!
        str.sprintf( "%.2i:%.2i:%.2i", m, s, f );
    }
    else
        str.sprintf( "%.2i:%.2i", m, s );

    return str;
}

/*QString K3b::sizeToTime(long size)
  {
  int h = size / sizeof(Sample) / 588;
  return framesToString(h, false);
  }*/


qint16 K3b::swapByteOrder( const qint16& i )
{
    return bswap_16( i );
    //((i << 8) & 0xff00) | ((i >> 8 ) & 0xff);
}


qint32 K3b::swapByteOrder( const qint32& i )
{
    //return ((i << 24) & 0xff000000) | ((i << 8) & 0xff0000) | ((i >> 8) & 0xff00) | ((i >> 24) & 0xff );
    return bswap_32( i );
}


qint64 K3b::swapByteOrder( const qint64& i )
{
    return bswap_64( i );
}


int K3b::round( double d )
{
    return (int)( floor(d) + 0.5 <= d ? ceil(d) : floor(d) );
}


QString K3b::findUniqueFilePrefix( const QString& _prefix, const QString& path )
{
    QString url;
    if( path.isEmpty() || !QFile::exists(path) )
        url = defaultTempPath();
    else
        url = prepareDir( path );

    QString prefix = _prefix;
    if( prefix.isEmpty() )
        prefix = "k3b_";

    // now create the unique prefix
    QDir dir( url );
    QStringList entries = dir.entryList( QDir::NoFilter, QDir::Name );
    int i = 0;
    for( QStringList::iterator it = entries.begin();
         it != entries.end(); ++it ) {
        if( (*it).startsWith( prefix + QString::number(i) ) ) {
            i++;
            it = entries.begin();
        }
    }

    return url + prefix + QString::number(i);
}


QString K3b::findTempFile( const QString& ending, const QString& d )
{
    return findUniqueFilePrefix( "k3b_", d ) + ( ending.isEmpty() ? QString() : (QString::fromLatin1(".") + ending) );
}


QString K3b::defaultTempPath()
{
    KConfigGroup grp = KGlobal::config()->group( "General Options" );
    QString url = grp.readPathEntry( "Temp Dir", KGlobal::dirs()->resourceDirs( "tmp" ).first() );
    return prepareDir( url );
}


QString K3b::prepareDir( const QString& dir )
{
    if(dir.isEmpty())
        return QString();
    else
        return (dir + (dir[dir.length()-1] != '/' ? "/" : ""));
}


QString K3b::parentDir( const QString& path )
{
    QString parent = path;
    if( path.isEmpty())
        return QString();
    if( path[path.length()-1] == '/' )
        parent.truncate( parent.length()-1 );

    int pos = parent.lastIndexOf( '/' );
    if( pos >= 0 )
        parent.truncate( pos+1 );
    else // relative path, do anything...
        parent = "/";

    return parent;
}


QString K3b::fixupPath( const QString& path )
{
    QString s;
    bool lastWasSlash = false;
    for( int i = 0; i < path.length(); ++i ) {
        if( path[i] == '/' ) {
            if( !lastWasSlash ) {
                lastWasSlash = true;
                s.append( "/" );
            }
        }
        else {
            lastWasSlash = false;
            s.append( path[i] );
        }
    }

    return s;
}


K3bVersion K3b::kernelVersion()
{
    // initialize kernel version
    K3bVersion v;
    utsname unameinfo;
    if( ::uname(&unameinfo) == 0 ) {
        v = QString::fromLocal8Bit( unameinfo.release );
        kDebug() << "kernel version: " << v;
    }
    else
        kError() << "could not determine kernel version." ;
    return v;
}


K3bVersion K3b::simpleKernelVersion()
{
    return kernelVersion().simplify();
}


QString K3b::systemName()
{
    QString v;
    utsname unameinfo;
    if( ::uname(&unameinfo) == 0 ) {
        v = QString::fromLocal8Bit( unameinfo.sysname );
    }
    else
        kError() << "could not determine system name." ;
    return v;
}


bool K3b::kbFreeOnFs( const QString& path, unsigned long& size, unsigned long& avail )
{
#ifdef HAVE_SYS_STATVFS_H
    struct statvfs fs;
    if( ::statvfs( QFile::encodeName(path), &fs ) == 0 ) {
        unsigned long kBfak = fs.f_frsize/1024;

        size = fs.f_blocks*kBfak;
        avail = fs.f_bavail*kBfak;

        return true;
    }
    else
#endif
        return false;
}


KIO::filesize_t K3b::filesize( const KUrl& url )
{
    KIO::filesize_t fSize = 0;
    if( url.isLocalFile() ) {
        k3b_struct_stat buf;
        k3b_stat( QFile::encodeName( url.path() ), &buf );
        fSize = (KIO::filesize_t)buf.st_size;
    }
    else {
        KIO::UDSEntry uds;
        KIO::NetAccess::stat( url, uds, 0 );
        fSize = uds.numberValue( KIO::UDSEntry::UDS_SIZE );
    }

    return fSize;
}


KIO::filesize_t K3b::imageFilesize( const KUrl& url )
{
    KIO::filesize_t size = K3b::filesize( url );
    int cnt = 0;
    while( KIO::NetAccess::exists( url.url() + '.' + QString::number(cnt).rightJustified( 3, '0' ), KIO::NetAccess::SourceSide, 0 ), true )
        size += K3b::filesize( url.url() + '.' + QString::number(cnt++).rightJustified( 3, '0' ) );
    return size;
}


QString K3b::cutFilename( const QString& name, int len )
{
    if( name.length() > len ) {
        QString ret = name;

        // determine extension (we think of an extension to be at most 5 chars in length)
        int pos = name.indexOf( '.', -6 );
        if( pos > 0 )
            len -= (name.length() - pos);

        ret.truncate( len );

        if( pos > 0 )
            ret.append( name.mid( pos ) );

        return ret;
    }
    else
        return name;
}


QString K3b::removeFilenameExtension( const QString& name )
{
    QString v = name;
    int dotpos = v.lastIndexOf( '.' );
    if( dotpos > 0 )
        v.truncate( dotpos );
    return v;
}


QString K3b::appendNumberToFilename( const QString& name, int num, unsigned int maxlen )
{
    // determine extension (we think of an extension to be at most 5 chars in length)
    QString result = name;
    QString ext;
    int pos = name.indexOf( '.', -6 );
    if( pos > 0 ) {
        ext = name.mid(pos);
        result.truncate( pos );
    }

    ext.prepend( QString::number(num) );
    result.truncate( maxlen - ext.length() );

    return result + ext;
}


bool K3b::plainAtapiSupport()
{
    // FIXME: what about BSD?
    return ( K3b::simpleKernelVersion() >= K3bVersion( 2, 5, 40 ) );
}


bool K3b::hackedAtapiSupport()
{
    // IMPROVEME!!!
    // FIXME: since when does the kernel support this?
    return ( K3b::simpleKernelVersion() >= K3bVersion( 2, 4, 0 ) );
}


QString K3b::externalBinDeviceParameter( K3bDevice::Device* dev, const K3bExternalBin* bin )
{
    Q_UNUSED( bin );
    return dev->blockDeviceName();
}


int K3b::writingAppFromString( const QString& s )
{
    if( s.toLower() == "cdrdao" )
        return K3b::CDRDAO;
    else if( s.toLower() == "cdrecord" )
        return K3b::CDRECORD;
    else if( s.toLower() == "dvdrecord" )
        return K3b::DVDRECORD;
    else if( s.toLower() == "growisofs" )
        return K3b::GROWISOFS;
    else if( s.toLower() == "dvd+rw-format" )
        return K3b::DVD_RW_FORMAT;
    else
        return K3b::DEFAULT;
}


QString K3b::writingAppToString( int app )
{
    switch( app ) {
    case CDRECORD:
        return "cdrecord";
    case CDRDAO:
        return "cdrdao";
    case DVDRECORD:
        return "dvdrecord";
    case GROWISOFS:
        return "growisofs";
    case DVD_RW_FORMAT:
        return "dvd+rw-format";
    default:
        return "auto";
    }
}


QString K3b::writingModeString( int mode )
{
    if( mode == WRITING_MODE_AUTO )
        return i18n("Auto");
    else
        return K3bDevice::writingModeString( mode );
}


QString K3b::resolveLink( const QString& file )
{
    QFileInfo f( file );
    return f.canonicalFilePath();
//     QStringList steps( f.absoluteFilePath() );
//     while( f.isSymLink() ) {
//         QString p = f.readLink();
//         if( !p.startsWith( "/" ) )
//             p.prepend( f.absolutePath() + "/" );
//         f.setFile( p );
//         if( steps.contains( f.absoluteFilePath() ) ) {
//             kDebug() << "(K3b) symlink loop detected.";
//             break;
//         }
//         else
//             steps.append( f.absoluteFilePath() );
//     }
//     return f.absoluteFilePath();
}


K3bDevice::Device* K3b::urlToDevice( const KUrl& deviceUrl )
{
#ifdef __GNUC__
#warning kded mediamanager: is that now solid?
#endif
    return NULL;
#if 0
    if( deviceUrl.protocol() == "media" || deviceUrl.protocol() == "system" ) {
        kDebug() << "(K3b) Asking mediamanager for " << deviceUrl.fileName();
        DCOPRef mediamanager("kded", "mediamanager");
        DCOPReply reply = mediamanager.call("properties(QString)", deviceUrl.fileName());
        QStringList properties = reply;
        if( !reply.isValid() || properties.count() < 6 ) {
            kError() << "(K3b) Invalid reply from mediamanager" ;
            return 0;
        }
        else {
            kDebug() << "(K3b) Reply from mediamanager " << properties[5];
            return k3bcore->deviceManager()->findDevice( properties[5] );
        }
    }

    return k3bcore->deviceManager()->findDevice( deviceUrl.path() );
#endif
}


KUrl K3b::convertToLocalUrl( const KUrl& url )
{
    if( !url.isLocalFile() ) {
        return KIO::NetAccess::mostLocalUrl( url, 0 );
    }

    return url;
}


KUrl::List K3b::convertToLocalUrls( const KUrl::List& urls )
{
    KUrl::List r;
    for( KUrl::List::const_iterator it = urls.constBegin(); it != urls.constEnd(); ++it )
        r.append( convertToLocalUrl( *it ) );
    return r;
}


qint16 K3b::fromLe16( char* data )
{
#ifdef WORDS_BIGENDIAN // __BYTE_ORDER == __BIG_ENDIAN
    return swapByteOrder( *((qint16*)data) );
#else
    return *((qint16*)data);
#endif
}


qint32 K3b::fromLe32( char* data )
{
#ifdef WORDS_BIGENDIAN // __BYTE_ORDER == __BIG_ENDIAN
    return swapByteOrder( *((qint32*)data) );
#else
    return *((qint32*)data);
#endif
}


qint64 K3b::fromLe64( char* data )
{
#ifdef WORDS_BIGENDIAN // __BYTE_ORDER == __BIG_ENDIAN
    return swapByteOrder( *((qint64*)data) );
#else
    return *((qint64*)data);
#endif
}


QString K3b::findExe( const QString& name )
{
    // first we search the path
    QString bin = KStandardDirs::findExe( name );

    // then go on with our own little list
    if( bin.isEmpty() )
        bin = KStandardDirs::findExe( name, k3bcore->externalBinManager()->searchPath().join(":") );

    return bin;
}


bool K3b::isMounted( K3bDevice::Device* dev )
{
    if( !dev )
        return false;
    else
        return( KMountPoint::currentMountPoints().findByDevice( dev->blockDeviceName() ).data() != 0 );
}


bool K3b::unmount( K3bDevice::Device* dev )
{
    if( !dev )
        return false;

    QString mntDev = dev->blockDeviceName();

    // first try to unmount it the standard way
    if( KIO::NetAccess::synchronousRun( KIO::unmount( mntDev, false ), 0 ) )
        return true;

    QString mntPath = KMountPoint::currentMountPoints().findByDevice( dev->blockDeviceName() )->mountPoint();
    if ( mntPath.isEmpty() ) {
        mntPath = dev->blockDeviceName();
    }

    Solid::StorageAccess *sa = dev->solidDevice().as<Solid::StorageAccess>();
    if ( sa && sa->teardown() ){
	    return true;
    }

    QString umountBin = K3b::findExe( "umount" );
    if( !umountBin.isEmpty() ) {
        KProcess p;
        p << umountBin;
        p << "-l"; // lazy unmount
        p << mntPath;
        p.start();
        if (p.waitForFinished(-1))
          return true;
    }

    // now try pmount
    QString pumountBin = K3b::findExe( "pumount" );
    if( !pumountBin.isEmpty() ) {
        KProcess p;
        p << pumountBin;
        p << "-l"; // lazy unmount
        p << mntPath;
        p.start();
        return p.waitForFinished(-1);
    }
    else {
        return false;
    }
}


bool K3b::mount( K3bDevice::Device* dev )
{
    if( !dev )
        return false;

    QString mntDev = dev->blockDeviceName();

    // first try to mount it the standard way
    if( KIO::NetAccess::synchronousRun( KIO::mount( true, 0, mntDev, false ), 0 ) )
        return true;

    Solid::StorageAccess* sa = dev->solidDevice().as<Solid::StorageAccess>();
    if ( sa && sa->setup() ) {
        return true;
    }

    // now try pmount
    QString pmountBin = K3b::findExe( "pmount" );
    if( !pmountBin.isEmpty() ) {
        KProcess p;
        p << pmountBin;
        p << mntDev;
        p.start();
        return p.waitForFinished(-1);
    }

    // and the most simple one
    QString mountBin = K3b::findExe( "mount" );
    if( !mountBin.isEmpty() ) {
        KProcess p;
        p << mountBin;
        p << mntDev;
        p.start();
        return p.waitForFinished(-1);
    }

    return false;
}


bool K3b::eject( K3bDevice::Device* dev )
{
    if( K3b::isMounted( dev ) )
        K3b::unmount( dev );

    if ( dev->solidDevice().as<Solid::OpticalDrive>()->eject() ) {
        return true;
    }
    else {
        return dev->eject();
    }
}
