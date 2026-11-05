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
// $Id: GuiEditVCard.h 1455 2020-12-23 10:17:53Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#ifndef BEEBEEP_GUIEDITVCARD_H
#define BEEBEEP_GUIEDITVCARD_H

#include <ui_GuiEditVCard.h>
#include "User.h"
#include <QPointer>


class GuiEditVCard : public QDialog, private Ui::GuiVCardDialog
{
  Q_OBJECT

public:
  GuiEditVCard( QWidget* parent = Q_NULLPTR );

  void setUser( const User& );
  void setProfileStatus( int );
  void reject() override;
  void showAt( QWidget* anchor );
  void editWorkgroupsInline();
  const QStringList& workgroups() const { return m_workgroups; }

  inline const VCard& vCard() const;
  inline const QString& userColor() const;
  inline bool regenerateUserHash() const;

signals:
  void userStatusSelected( int );

protected:
  void showEvent( QShowEvent* ) override;
  bool eventFilter( QObject*, QEvent* ) override;
  void loadVCard();
  void setUserColor( const QString& );
  bool checkLineEdit( QLineEdit*, const QString& );
  bool checkData();
  void saveVCard();
  QString currentAvatarName() const;
  void resizeProfilePage();
  void showProfileEditor( QWidget* );
  void beginInlineEdit( QWidget* );
  void finishInlineEdit( QWidget* );
  void updateInlineLabels();
  void refreshCardPhoto();
  bool validateInlineData();
  void updateCardClock();
  void placeCard();

private slots:
  void changePhoto();
  void onOkClicked();
  void changeUserColor();
  void regenerateHash();

private:
  VCard m_vCard;
  QPixmap m_photo;
  QString m_userColor;
  QString m_savedUserColor;
  bool m_regenerateUserHash;
  QString m_accountEmail;
  QStackedWidget* m_profilePages;
  bool m_resizingProfile = false;
  QLabel* m_cardNickname;
  QLabel* m_cardPhoto;
  QLabel* m_cardInfo;
  QLabel* m_cardEmail;
  QLabel* m_cardPhone;
  QLabel* m_cardGroups;
  QLabel* m_cardClock;
  QPushButton* m_cardStatus;
  QPointer<QWidget> m_cardAnchor;
  bool m_cardAnchored = false;
  QStringList m_workgroups;
  QStringList m_savedWorkgroups;
  QLineEdit* m_workgroupsEdit;
  QLabel* m_nicknameError;
  QLabel* m_workgroupsError;
  QPushButton* m_cardSave;
  QPushButton* m_cardCancel;
  bool m_inlineDirty = false;
  bool m_workgroupsEdited = false;

};


// Inline Functions
inline const VCard& GuiEditVCard::vCard() const { return m_vCard; }
inline const QString& GuiEditVCard::userColor() const { return m_userColor; }
inline bool GuiEditVCard::regenerateUserHash() const { return m_regenerateUserHash; }

#endif // BEEBEEP_GUIEDITVCARD_H
