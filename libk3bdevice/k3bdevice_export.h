/*  This file is part of the KDE project
    Copyright (C) 2007 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef K3BDEVICE_EXPORT_H
#define K3BDEVICE_EXPORT_H

/* needed for KDE_EXPORT and KDE_IMPORT macros */
#include <kdemacros.h>

#ifndef LIBK3BDEVICE_EXPORT
# if defined(MAKE_K3BDEVICE_LIB)
   /* We are building this library */ 
#  define LIBK3BDEVICE_EXPORT KDE_EXPORT
# else
   /* We are using this library */ 
#  define LIBK3BDEVICE_EXPORT KDE_IMPORT
# endif
#endif

# ifndef LIBK3BDEVICE_EXPORT_DEPRECATED
#  define LIBK3BDEVICE_EXPORT_DEPRECATED KDE_DEPRECATED LIBK3BDEVICE_EXPORT
# endif

#endif
