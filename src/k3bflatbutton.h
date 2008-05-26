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

#ifndef FLATBUTTON_H
#define FLATBUTTON_H

#include <QFrame>
#include <qcolor.h>
#include <qpixmap.h>
//Added by qt3to4:
#include <QMouseEvent>
#include <QEvent>

class QEvent;
class QMouseEvent;
class QAction;
class QPaintEvent;

/**
   @author Sebastian Trueg
*/
class K3bFlatButton : public QFrame
{
    Q_OBJECT

public:
    K3bFlatButton( QWidget *parent = 0);
    K3bFlatButton( const QString& text, QWidget *parent = 0 );
    K3bFlatButton( QAction*, QWidget *parent = 0);
  
    ~K3bFlatButton();

    QSize sizeHint() const;

public Q_SLOTS:
    void setColors( const QColor& fore, const QColor& back );
    void setText( const QString& );
    void setPixmap( const QPixmap& );
    void setMargin( int margin );

 Q_SIGNALS:
    void pressed();
    void clicked();

    private Q_SLOTS:
    void slotThemeChanged();

private:
    void init();

    void mousePressEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    void enterEvent( QEvent* );
    void leaveEvent( QEvent* );
    void paintEvent ( QPaintEvent * event );
    void setHover( bool );

    bool m_pressed;
    QColor m_backColor;
    QColor m_foreColor;
    QString m_text;
    QPixmap m_pixmap;
    int m_margin;

    bool m_hover;
};

#endif
