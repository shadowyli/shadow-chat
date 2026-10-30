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
// $Id: GuiSavedChat.h 1455 2020-12-23 10:17:53Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#ifndef BEEBEEP_GUISAVEDCHAT_H
#define BEEBEEP_GUISAVEDCHAT_H

#include "Config.h"
#include "HistoryRecord.h"

class QStackedWidget;

class GuiSavedChat : public QMainWindow
{
  Q_OBJECT

public:
  explicit GuiSavedChat( QWidget *parent = Q_NULLPTR, bool embedded = false );
  inline const QString& savedChatName() const;
  void setSearchText( const QString& text );

signals:
  void openUrl( const QUrl& );
  void deleteSavedChatRequest( const QString& );

public slots:
  void showSavedChat( const QString& );

protected slots:
  void printChat();
  void customContextMenu( const QPoint& );
  void openSelectedTextAsUrl();
  void deleteSavedChat();
  void saveHistoryAs();
  void findTextInHistory();
  void checkAnchorClicked( const QUrl& );
  void filterHistory();
  void locateHistoryResult();
  void showHistoryContext();

protected:
  void setupToolBar( QToolBar* );

private:
  void rebuildHistoryResults( bool resetReadingState );

  QTextBrowser* mp_teSavedChat;
  QLineEdit* mp_leFilter;
  QCheckBox* mp_cbCaseSensitive;
  QCheckBox* mp_cbWholeWordOnly;
  QToolBar* mp_barHistory;
  QMenu* mp_menuContext;
  QString m_savedChatName;
  QTabBar* mp_categories;
  QComboBox* mp_senderFilter;
  QComboBox* mp_dateFilter;
  QComboBox* mp_sortOrder;
  QListWidget* mp_results;
  QLabel* mp_resultCount;
  QLabel* mp_emptyState;
  QStackedWidget* mp_resultsStack;
  QPushButton* mp_contextButton;
  bool m_embedded;
  QList<HistoryRecord> m_records;
  QStringList m_recordKeys;

};

// Inline Functions
inline const QString& GuiSavedChat::savedChatName() const { return m_savedChatName; }

#endif // BEEBEEP_GUISAVEDCHAT_H
