//////////////////////////////////////////////////////////////////////
//
// BeeBEEP Copyright (C) 2010-2021 Marco Mastroddi
//
// BeeBEEP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// BeeBEEP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with BeeBEEP. If not, see <http://www.gnu.org/licenses/>.
//
// Author: Marco Mastroddi <marco.mastroddi(AT)gmail.com>
//
// $Id: GuiWizard.h 1455 2020-12-23 10:17:53Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#ifndef BEEBEEP_GUIWIZARD_H
#define BEEBEEP_GUIWIZARD_H

#include "Config.h"
#include "ui_GuiWizard.h"


class GuiWizard : public QDialog, private Ui::GuiWizardDialog
{
  Q_OBJECT

public:
  GuiWizard( QWidget* parent = Q_NULLPTR );
  void loadSettings();

  inline const QString& userName() const;
  inline const QPixmap& avatar() const;
  inline bool checkForUpdates() const;

protected slots:
  void saveSettings();
  void chooseAvatar();

private:
  QString m_userName;
  QPixmap m_avatar;
  QCheckBox* mp_cbCheckForUpdates;
  QPushButton* mp_pbChooseAvatar;

};

// Inline Functions
inline const QString& GuiWizard::userName() const { return m_userName; }
inline const QPixmap& GuiWizard::avatar() const { return m_avatar; }
inline bool GuiWizard::checkForUpdates() const { return mp_cbCheckForUpdates && mp_cbCheckForUpdates->isChecked(); }

#endif // BEEBEEP_GUIWIZARD_H
