/*


    SPDX-FileCopyrightText: 2007 Sebastian Trueg <trueg@k3b.org>

    This file is part of the K3b project.
    SPDX-FileCopyrightText: 1998-2007 Sebastian Trueg <trueg@k3b.org>

    SPDX-License-Identifier: GPL-2.0-or-later
    See the file "COPYING" for the exact licensing terms.
*/

#ifndef _K3B_LAME_MANUAL_SETTINGS_DIALOG_H_
#define _K3B_LAME_MANUAL_SETTINGS_DIALOG_H_

#include <QDialog>

#include "ui_base_k3bmanualbitratesettingsdialog.h"

class K3bLameManualSettingsDialog : public QDialog, public Ui::K3bManualBitrateSettingsDialog
{
public:
    explicit K3bLameManualSettingsDialog( QWidget* parent = 0 );
    ~K3bLameManualSettingsDialog() override;
};

#endif
