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
// $Id: GuiHome.cpp 1540 2022-03-30 10:10:17Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "BeeUtils.h"
#include "GuiHome.h"
#include "GuiChatMessage.h"
#include "ChatManager.h"
#include "ChatMessage.h"
#include "IconManager.h"
#include "Settings.h"
#include "ShortcutManager.h"


GuiHome::GuiHome( QWidget* parent )
  : QWidget( parent )
{
  setObjectName( "GuiHome" );
  setupUi( this );
  mp_lNews->hide();
  QGridLayout* content = qobject_cast<QGridLayout*>( layout() );
  if( content )
  {
    content->removeWidget( mp_teSystem );
    content->removeWidget( mp_lNews );
    content->setContentsMargins( 4, 8, 4, 0 );
    content->setSpacing( 14 );
    QLabel* title = new QLabel( QStringLiteral( "连接与安全" ), this );
    title->setStyleSheet( QStringLiteral( "font-size:16px;font-weight:600;color:#172033;" ) );
    content->addWidget( title, 0, 0 );
    content->addWidget( mp_teSystem, 1, 0 );
    content->setRowStretch( 1, 1 );
  }
  m_prev_sys_mess = "";

  mp_teSystem->setObjectName( "GuiSystemViewer" );
  mp_teSystem->setFocusPolicy( Qt::ClickFocus );
  mp_teSystem->setReadOnly( true );
  mp_teSystem->setContextMenuPolicy( Qt::CustomContextMenu );
  mp_teSystem->setOpenExternalLinks( false );
  mp_teSystem->setOpenLinks( false );
  updateBackground();

  mp_lNews->setOpenExternalLinks( true );

  mp_menuContext = new QMenu( this );

  connect( mp_teSystem, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( customContextMenu( const QPoint& ) ) );
  connect( mp_teSystem, SIGNAL( anchorClicked( const QUrl& ) ), this, SLOT( checkAnchorClicked( const QUrl&  ) ) );
}

bool GuiHome::addSystemMessage( const ChatMessage& cm )
{
  if( !cm.isSystemActivity() )
    return false;

  QString body = cm.message();
  body.remove( QRegularExpression( QStringLiteral( "<img\\b[^>]*>" ), QRegularExpression::CaseInsensitiveOption ) );
  QString sys_message = QStringLiteral( "<p style=\"margin-top:0;margin-bottom:14px\"><font color=\"#96A0AC\" style=\"font-size:10px\">%1</font><br>%2</p>" )
    .arg( cm.timestamp().toString( QStringLiteral( "HH:mm" ) ), body );

  if( sys_message.isEmpty() )
    return false;

  if( sys_message == m_prev_sys_mess )
    return false;

  m_prev_sys_mess = sys_message;

  QTextCursor cursor( mp_teSystem->textCursor() );
  cursor.movePosition( QTextCursor::End );
  cursor.insertHtml( sys_message );
  QScrollBar *bar = mp_teSystem->verticalScrollBar();
  if( bar )
    bar->setValue( bar->maximum() );
  return true;
}

void GuiHome::checkAnchorClicked( const QUrl& url )
{
  emit openUrlRequest( url );
}

void GuiHome::customContextMenu( const QPoint& )
{
  mp_menuContext->clear();
  mp_menuContext->addAction( QStringLiteral( "全选" ), mp_teSystem, SLOT( selectAll() ), QKeySequence::SelectAll );
  mp_menuContext->addSeparator();
  QAction* act = mp_menuContext->addAction( QStringLiteral( "复制" ), mp_teSystem, SLOT( copy() ), QKeySequence::Copy );
  act->setEnabled( !mp_teSystem->textCursor().selectedText().isEmpty() );
  mp_menuContext->addSeparator();
  act = mp_menuContext->addAction( QStringLiteral( "打印记录" ), this, SLOT( printActivities() ) );
  QKeySequence ks = ShortcutManager::instance().shortcut( ShortcutManager::Print );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    act->setShortcut( ks );
  else
    act->setShortcut( QKeySequence() );
  mp_menuContext->addSeparator();
  act = mp_menuContext->addAction( QStringLiteral( "清空活动记录" ), this, SLOT( clearSystemMessages() ) );
  mp_menuContext->exec( QCursor::pos() );
}

int GuiHome::loadSystemMessages()
{
  int num_sys_msg = 0;
  mp_teSystem->clear();
  Chat c = ChatManager::instance().defaultChat();
  foreach( ChatMessage cm, c.messages() )
  {
    if( addSystemMessage( cm ) )
      num_sys_msg++;
  }
  QTimer::singleShot( 0, this, SLOT( resetNews() ) );
  return num_sys_msg;
}

void GuiHome::clearSystemMessages()
{
  emit clearSystemMessagesRequest( ID_DEFAULT_CHAT );
}

void GuiHome::resetNews()
{
  setNews( "" );
}

void GuiHome::setNews( const QString& news )
{
  Q_UNUSED( news );
  mp_lNews->clear();
  mp_lNews->hide();
}

void GuiHome::reloadMessages()
{
  QApplication::setOverrideCursor( Qt::WaitCursor );
  mp_teSystem->clear();
  loadSystemMessages();
  QApplication::restoreOverrideCursor();
}

void GuiHome::onAddTimestampClicked()
{
  QAction* act = qobject_cast<QAction*>( sender() );
  if( !act )
    return;

  Settings::instance().setHomeShowMessageTimestamp( act->isChecked() );
  QTimer::singleShot( 100, this, SLOT( reloadMessages() ) );
}

void GuiHome::printActivities()
{
  QPrinter printer( QPrinter::HighResolution );
  printer.setFullPage( true );
  QPrintDialog *dlg = new QPrintDialog( &printer, this );
  dlg->setOptions( QAbstractPrintDialog::PrintSelection | QAbstractPrintDialog::PrintPageRange |
                   QAbstractPrintDialog::PrintShowPageSize |  QAbstractPrintDialog::PrintCollateCopies |
#if QT_VERSION >= 0x040700
                   QAbstractPrintDialog::PrintCurrentPage |
#endif
                   QAbstractPrintDialog::PrintToFile );

  if( dlg->exec() == QDialog::Accepted)
    mp_teSystem->print( dlg->printer() );

  dlg->deleteLater();
}

void GuiHome::selectBackgroundColor()
{
  QColor c = Bee::selectColor( this, Settings::instance().homeBackgroundColor() );
  if( c.isValid() )
  {
    Settings::instance().setHomeBackgroundColor( c.name() );
    updateBackground();
  }
}

void GuiHome::updateBackground()
{
  mp_teSystem->setStyleSheet( QStringLiteral(
    "#GuiSystemViewer { background:#FFFFFF;color:#5F6B7A;border:0;padding:4px;font-size:12px; }" ) );
}
