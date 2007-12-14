/*
 *
 * Copyright (C) 2003-2007 Sebastian Trueg <trueg@k3b.org>
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

#include <config-k3b.h>

#include "k3bcore.h"
#include "k3bjob.h"

#include <k3bdevicemanager.h>
#include <k3bexternalbinmanager.h>
#include <k3bdefaultexternalprograms.h>
#include <k3bglobals.h>
#include <k3bversion.h>
#include <k3bjob.h>
#include <k3bthreadwidget.h>
#include <k3bglobalsettings.h>
#include <k3bpluginmanager.h>

#include <klocale.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kaboutdata.h>
#include <kstandarddirs.h>
#include <kapplication.h>

#include <q3ptrlist.h>
#include <qthread.h>
#include <qmutex.h>
//Added by qt3to4:
#include <QEvent>


static QThread* s_guiThreadHandle = 0;

// We cannot use QWaitCondition here since the event might be handled faster
// than the thread starts the waiting
class DeviceBlockingEventDoneCondition {
public:
    DeviceBlockingEventDoneCondition()
        : m_done(false) {
    }

    void done() {
        m_doneMutex.lock();
        m_done = true;
        m_doneMutex.unlock();
    }

    void wait() {
        while( true ) {
            m_doneMutex.lock();
            bool done = m_done;
            m_doneMutex.unlock();
            if( done )
                return;
        }
    }

private:
    QMutex m_doneMutex;
    bool m_done;
};

#ifdef __GNUC__
#warning FIXME: how do we define a user event type? Or just replace the whole event stuff with signals
#endif
class DeviceBlockingEvent : public QEvent
{
public:
    DeviceBlockingEvent( bool block_, K3bDevice::Device* dev, DeviceBlockingEventDoneCondition* cond_, bool* success_ )
        : QEvent( QEvent::User ),
          block(block_),
          device(dev),
          cond(cond_),
          success(success_) {
    }

    bool block;
    K3bDevice::Device* device;
    DeviceBlockingEventDoneCondition* cond;
    bool* success;
};


class K3bCore::Private {
public:
    Private()
        : version( LIBK3B_VERSION ),
          config(0),
          deleteConfig(false),
          deviceManager(0),
          externalBinManager(0),
          pluginManager(0),
          globalSettings(0) {
    }

    K3bVersion version;
    KConfig* config;
    bool deleteConfig;
    K3bDevice::DeviceManager* deviceManager;
    K3bExternalBinManager* externalBinManager;
    K3bPluginManager* pluginManager;
    K3bGlobalSettings* globalSettings;

    QList<K3bJob*> runningJobs;
    QList<K3bDevice::Device*> blockedDevices;
};



K3bCore* K3bCore::s_k3bCore = 0;



K3bCore::K3bCore( QObject* parent )
    : QObject( parent )
{
    d = new Private();

    if( s_k3bCore )
        qFatal("ONLY ONE INSTANCE OF K3BCORE ALLOWED!");
    s_k3bCore = this;

    // ew are probably constructed in the GUi thread :(
    s_guiThreadHandle = QThread::currentThread();

    // create the thread widget instance in the GUI thread
    K3bThreadWidget::instance();
}


K3bCore::~K3bCore()
{
    s_k3bCore = 0;

    delete d->globalSettings;
    delete d;
}


K3bDevice::DeviceManager* K3bCore::deviceManager() const
{
    const_cast<K3bCore*>(this)->initDeviceManager();
    return d->deviceManager;
}


K3bExternalBinManager* K3bCore::externalBinManager() const
{
    const_cast<K3bCore*>(this)->initExternalBinManager();
    return d->externalBinManager;
}


K3bPluginManager* K3bCore::pluginManager() const
{
    const_cast<K3bCore*>(this)->initPluginManager();
    return d->pluginManager;
}


K3bGlobalSettings* K3bCore::globalSettings() const
{
    const_cast<K3bCore*>(this)->initGlobalSettings();
    return d->globalSettings;
}


const K3bVersion& K3bCore::version() const
{
    return d->version;
}


KConfig* K3bCore::config() const
{
    if( !d->config ) {
        d->deleteConfig = true;
        d->config = new KConfig( "k3brc" );
    }

    return d->config;
}


void K3bCore::init()
{
    initGlobalSettings();
    initExternalBinManager();
    initDeviceManager();
    initPluginManager();

    // load the plugins before doing anything else
    // they might add external bins
    pluginManager()->loadAll();

    externalBinManager()->search();

    deviceManager()->scanBus();
}


void K3bCore::initGlobalSettings()
{
    if( !d->globalSettings )
        d->globalSettings = new K3bGlobalSettings();
}


void K3bCore::initExternalBinManager()
{
    if( !d->externalBinManager ) {
        d->externalBinManager = new K3bExternalBinManager( this );
        K3b::addDefaultPrograms( d->externalBinManager );
    }
}


void K3bCore::initDeviceManager()
{
    if( !d->deviceManager )
        d->deviceManager = new K3bDevice::DeviceManager( this );
}


void K3bCore::initPluginManager()
{
    if( !d->pluginManager )
        d->pluginManager = new K3bPluginManager( this );
}


void K3bCore::readSettings( KConfig* cnf )
{
    KConfig* c = cnf;
    if( !c )
        c = config();

    globalSettings()->readSettings( c );
    deviceManager()->readConfig( c );
    externalBinManager()->readConfig( c );
}


void K3bCore::saveSettings( KConfig* cnf )
{
    KConfig* c = cnf;
    if( !c )
        c = config();

    c->group( "General Options" ).writeEntry( "config version", version().toString() );

    deviceManager()->saveConfig( c );
    externalBinManager()->saveConfig( c );
    d->globalSettings->saveSettings( c );
}


void K3bCore::registerJob( K3bJob* job )
{
    d->runningJobs.append( job );
    emit jobStarted( job );
    if( K3bBurnJob* bj = dynamic_cast<K3bBurnJob*>( job ) )
        emit burnJobStarted( bj );
}


void K3bCore::unregisterJob( K3bJob* job )
{
    d->runningJobs.removeAll( job );
    emit jobFinished( job );
    if( K3bBurnJob* bj = dynamic_cast<K3bBurnJob*>( job ) )
        emit burnJobFinished( bj );
}


bool K3bCore::jobsRunning() const
{
    return !d->runningJobs.isEmpty();
}


const QList<K3bJob*>& K3bCore::runningJobs() const
{
    return d->runningJobs;
}


bool K3bCore::blockDevice( K3bDevice::Device* dev )
{
    if( QThread::currentThread() == s_guiThreadHandle ) {
        return internalBlockDevice( dev );
    }
    else {
        bool success = false;
        DeviceBlockingEventDoneCondition w;
        QApplication::postEvent( this, new DeviceBlockingEvent( true, dev, &w, &success ) );
        w.wait();
        return success;
    }
}


void K3bCore::unblockDevice( K3bDevice::Device* dev )
{
    if( QThread::currentThread() == s_guiThreadHandle ) {
        internalUnblockDevice( dev );
    }
    else {
        DeviceBlockingEventDoneCondition w;
        QApplication::postEvent( this, new DeviceBlockingEvent( false, dev, &w, 0 ) );
        w.wait();
    }
}


bool K3bCore::internalBlockDevice( K3bDevice::Device* dev )
{
    if( !d->blockedDevices.contains( dev ) ) {
        d->blockedDevices.append( dev );
        return true;
    }
    else
        return false;
}


void K3bCore::internalUnblockDevice( K3bDevice::Device* dev )
{
    d->blockedDevices.removeAll( dev );
}


void K3bCore::customEvent( QEvent* e )
{
    if( DeviceBlockingEvent* de = dynamic_cast<DeviceBlockingEvent*>(e) ) {
        if( de->block )
            *de->success = internalBlockDevice( de->device );
        else
            internalUnblockDevice( de->device );
        de->cond->done();
    }
}

#include "k3bcore.moc"
