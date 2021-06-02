/*

    SPDX-FileCopyrightText: 2003-2009 Sebastian Trueg <trueg@k3b.org>

    This file is part of the K3b project.
    SPDX-FileCopyrightText: 1998-2009 Sebastian Trueg <trueg@k3b.org>

    SPDX-License-Identifier: GPL-2.0-or-later
    See the file "COPYING" for the exact licensing terms.
*/


#ifndef K3BFILEVIEW_H
#define K3BFILEVIEW_H


#include "k3bcontentsview.h"


class QUrl;
class KActionCollection;
class KConfigGroup;

/**
 *@author Sebastian Trueg
 */
namespace K3b {
class FileView : public ContentsView
{
    Q_OBJECT

public:
    explicit FileView(QWidget *parent=0);
    ~FileView() override;

    void setUrl( const QUrl &url, bool forward = true );
    QUrl url();

    KActionCollection* actionCollection() const;

    void reload();

 Q_SIGNALS:
    void urlEntered( const QUrl& url );

public Q_SLOTS:
    void saveConfig( KConfigGroup grp );
    void readConfig( const KConfigGroup &grp );

private Q_SLOTS:
    void slotFilterChanged();

private:
    class Private;
    Private* d;
};
}


#endif
