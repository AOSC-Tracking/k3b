/*
 *
 * Copyright (C) 2003-2009 Sebastian Trueg <trueg@k3b.org>
 * Copyright (C) 2009 Michal Malek <michalm@jabster.pl>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2009 Sebastian Trueg <trueg@k3b.org>
 * Copyright (C) 2009 Michal Malek <michalm@jabster.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#ifndef K3BEXTERNALBINPERMISSIONMODEL_H
#define K3BEXTERNALBINPERMISSIONMODEL_H

#include "k3bhelperprogramitem.h"
#include "config-k3b.h"
#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QStringList>

namespace K3b {

    class ExternalBin;
    class ExternalBinManager;

    class ExternalBinPermissionModel : public QAbstractItemModel
    {
        Q_OBJECT

    public:
        enum Columns {
            ProgramColumn,
            PermissionsColumn,
            NewPermissionsColumn,
            NumColumns
        };

    public:
        ExternalBinPermissionModel(ExternalBinManager const& externalBinManager, QObject* parent = 0);
        ~ExternalBinPermissionModel();

        QList<HelperProgramItem> selectedPrograms() const;
        bool changesNeeded() const;
        QStringList searchPaths() const;
        const QString& burningGroup() const;

        const ExternalBin* programForIndex( const QModelIndex& index ) const;
        QModelIndex indexForProgram( const ExternalBin* program ) const;

        virtual QVariant data( const QModelIndex& index, int role ) const;
        virtual bool setData( const QModelIndex& index, const QVariant& value, int role );
        virtual Qt::ItemFlags flags( const QModelIndex& index ) const;
        virtual QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
        virtual QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
        virtual QModelIndex parent( const QModelIndex& index ) const;
        virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const;
        virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;
        virtual QModelIndex buddy( const QModelIndex& index ) const;

    public Q_SLOTS:
#ifdef ENABLE_PERMISSION_HELPER
        void setBurningGroup( const QString& burningGroup );
#endif
        void update();

    private:
        class Private;
        Private* d;
    };

} // namespace K3b

#endif // K3BEXTERNALBINPERMISSIONMODEL_H
