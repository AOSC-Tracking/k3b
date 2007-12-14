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

#ifndef _K3B_PLUGIN_MANAGER_H_
#define _K3B_PLUGIN_MANAGER_H_

#include <qobject.h>
#include <qlist.h>
#include <qstringlist.h>
#include "k3b_export.h"


class K3bPlugin;
class QWidget;


/**
 * Use this class to access all K3b plugins (this does not include the
 * KParts Plugins!).
 * Like the K3bCore the single instance (which has to be created manually)
 * can be obtained with the k3bpluginmanager macro.
 */
class LIBK3B_EXPORT K3bPluginManager : public QObject
{
    Q_OBJECT

public:
    K3bPluginManager( QObject* parent = 0 );
    ~K3bPluginManager();

    /**
     * if group is empty all plugins are returned
     */
    QList<K3bPlugin*> plugins( const QString& category = QString() ) const;

    /**
     * Returnes a list of the available categories.
     */
    QStringList categories() const;

    int pluginSystemVersion() const;

public slots:
    void loadAll();

    int execPluginDialog( K3bPlugin*, QWidget* parent = 0, const char* name = 0 );

private:
    class Private;
    Private* d;
};

#endif
