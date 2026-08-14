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
// $Id: GuiChat.cpp 1540 2022-03-30 10:10:17Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "Avatar.h"
#include "BeeUtils.h"
#include "ChatManager.h"
#include "ChatMessage.h"
#include "Core.h"
#include "FileDialog.h"
#include "GuiChat.h"
#include "GuiChatMessage.h"
#include "GuiCreateTextFile.h"
#ifdef BEEBEEP_USE_VOICE_CHAT
#include "GuiVoicePlayer.h"
#endif
#include "IconManager.h"
#include "Protocol.h"
#include "Settings.h"
#include "ShadowStyle.h"
#include "ShortcutManager.h"
#ifdef BEEBEEP_USE_HUNSPELL
  #include "SpellChecker.h"
#endif
#include "UserManager.h"


GuiChat::GuiChat( QWidget *parent )
 : QWidget( parent )
{
  setupUi( this );
  setObjectName( "GuiChat" );
  setAcceptDrops( true );

#ifdef BEEBEEP_USE_VOICE_CHAT
  mp_guiVoicePlayer = new GuiVoicePlayer( mp_frameVoicePlayer );
  QGridLayout* voice_grid_layout = new QGridLayout( mp_frameVoicePlayer );
  voice_grid_layout->setSpacing( 0 );
  voice_grid_layout->setObjectName( QString::fromUtf8( "voice_grid_layout" ) );
  voice_grid_layout->setContentsMargins( 4, 4, 4, 4 );
  voice_grid_layout->addWidget( mp_guiVoicePlayer );
#endif
  mp_frameVoicePlayer->hide();

  QGridLayout* grid_layout = new QGridLayout( this );
  grid_layout->setSpacing( 0 );
  grid_layout->setObjectName( QString::fromUtf8( "grid_layout" ) );
  grid_layout->setContentsMargins( 4, 4, 4, 4 );

  mp_splitter = new QSplitter( this );
  mp_splitter->setOrientation( Qt::Vertical );
  mp_splitter->setChildrenCollapsible( false );
  mp_splitter->addWidget( mp_frameChat );
  mp_splitter->addWidget( mp_frameMessage );

  grid_layout->addWidget( mp_splitter, 1, 0, 1, 1 );
  QList<int> widget_sizes;
  widget_sizes.append( 200 );
  widget_sizes.append( 80 );
  mp_splitter->setSizes( widget_sizes );
  mp_modernAttachButton = Q_NULLPTR;
  mp_modernComposerHint = Q_NULLPTR;
  m_modernMode = false;

  mp_teMessage->setFocusPolicy( Qt::StrongFocus );
  mp_teMessage->setAcceptRichText( false );
#ifdef BEEBEEP_USE_HUNSPELL
  mp_teMessage->setCompleter( SpellChecker::instance().completer() );
#endif
  mp_teMessage->setObjectName( "GuiMessageEdit" );

  mp_teChat->setObjectName( "GuiChatViewer" );
  m_defaultChatPalette = mp_teChat->palette();
  m_defaultChatPalette.setColor( QPalette::Highlight, Qt::yellow );
  m_defaultChatPalette.setColor( QPalette::HighlightedText, Qt::black );
  mp_teChat->setPalette( m_defaultChatPalette );
  mp_teChat->setFocusPolicy( Qt::StrongFocus ); // need focus for keyboard events like CTRL+c
  mp_teChat->setReadOnly( true );
  mp_teChat->setUndoRedoEnabled( false );
  mp_teChat->setContextMenuPolicy( Qt::CustomContextMenu );
  mp_teChat->setOpenExternalLinks( false );
  mp_teChat->setOpenLinks( false );
  mp_teChat->setAcceptRichText( false );

#ifdef BEEBEEP_USE_VOICE_CHAT
  connect( mp_pbRecordVoiceMessage, SIGNAL( clicked() ), this, SIGNAL( showVoiceMessageDialogRequest() ) );
#else
  mp_pbRecordVoiceMessage->setDisabled( true );
  mp_pbRecordVoiceMessage->setToolTip( tr( "Voice messages are not available with this version of BeeBEEP.\nDownload for free and install the version with this feature from the %1 website." ).arg( Settings::instance().organizationDomain() ) );
#endif

  m_chatId = ID_DEFAULT_CHAT;
  m_lastMessageUserId = ID_SYSTEM_MESSAGE;
  m_lastTextFound = "";

  mp_menuContext = new QMenu( this );
  mp_menuFilters = new QMenu( this );
  mp_actFilterMessages = Q_NULLPTR;

  mp_scFocusInChat = new QShortcut( this );
  mp_scFocusInChat->setContext( Qt::WindowShortcut );
  connect( mp_scFocusInChat, SIGNAL( activated() ), this, SLOT( ensureFocusInChat() ) );

  mp_scPrintChat = new QShortcut( this );
  mp_scPrintChat->setKey( QKeySequence::Print );
  mp_scPrintChat->setContext( Qt::WindowShortcut );
  connect( mp_scPrintChat, SIGNAL( activated() ), this, SLOT( printChat() ) );

  mp_scFindTextInChat = new QShortcut( this );
  mp_scFindTextInChat->setKey( QKeySequence::Find );
  mp_scFindTextInChat->setContext( Qt::WindowShortcut );
  connect( mp_scFindTextInChat, SIGNAL( activated() ), this, SLOT( showFindTextInChatDialog() ) );

  mp_scFindNextTextInChat = new QShortcut( this );
  mp_scFindNextTextInChat->setKey( QKeySequence::FindNext );
  mp_scFindNextTextInChat->setContext( Qt::WindowShortcut );
  connect( mp_scFindNextTextInChat, SIGNAL( activated() ), this, SLOT( findNextTextInChat() ) );

  mp_scFindPreviousTextInChat = new QShortcut( this );
  mp_scFindPreviousTextInChat->setKey( QKeySequence::FindPrevious );
  mp_scFindPreviousTextInChat->setContext( Qt::WindowShortcut );
  connect( mp_scFindPreviousTextInChat, SIGNAL( activated() ), this, SLOT( findPreviousTextInChat() ) );

  mp_scViewEmoticons = new QShortcut( this );
  mp_scViewEmoticons->setContext( Qt::WindowShortcut );
  connect( mp_scViewEmoticons, SIGNAL( activated() ), this, SIGNAL( toggleVisibilityEmoticonsPanelRequest() ) );

  mp_actSelectBackgroundColor = new QAction( IconManager::instance().icon( "background-color.png" ), tr( "Change background color" ), this );
  connect( mp_actSelectBackgroundColor, SIGNAL( triggered() ), this, SLOT( selectBackgroundColor() ) );

  mp_actSelectForegroundColor = new QAction( IconManager::instance().icon( "font-color.png" ), tr( "Select chat default text color" ), this );
  connect( mp_actSelectForegroundColor, SIGNAL( triggered() ), this, SLOT( selectForegroundColor() ) );

  mp_actSelectSystemColor = new QAction( IconManager::instance().icon( "log.png" ), tr( "Select chat system text color" ), this );
  connect( mp_actSelectSystemColor, SIGNAL( triggered() ), this, SLOT( selectSystemColor() ) );

  mp_actSaveAs = new QAction( IconManager::instance().icon( "save-as.png" ), tr( "Save chat" ), this );
  connect( mp_actSaveAs, SIGNAL( triggered() ), this, SLOT( saveChat() ) );

  mp_actPrint = new QAction( IconManager::instance().icon( "printer.png" ), tr( "Print..." ), this );
  mp_actPrint->setShortcut( QKeySequence::Print );
  connect( mp_actPrint, SIGNAL( triggered() ), this, SLOT( printChat() ) );

  mp_actClear = new QAction( IconManager::instance().icon( "clear.png" ), tr( "Clear messages" ), this );
  connect( mp_actClear, SIGNAL( triggered() ), this, SLOT( clearChat() ) );

  mp_actClearSystemMessages = new QAction( IconManager::instance().icon( "log.png" ), tr( "Clear system messages" ), this );
  connect( mp_actClearSystemMessages, SIGNAL( triggered() ), this, SLOT( clearSystemMessages() ) );

  mp_actFindTextInChat = new QAction( IconManager::instance().icon( "search.png" ), tr( "Find text in chat" ), this );
  connect( mp_actFindTextInChat, SIGNAL( triggered() ), this, SLOT( showFindTextInChatDialog() ) );

  mp_actRestoreDefaultFont = new QAction( IconManager::instance().icon( "font.png" ), tr( "Restore the default font" ), this );
  connect( mp_actRestoreDefaultFont, SIGNAL( triggered() ), this, SLOT( resetChatFontToDefault() ) );

  setChatFont( Settings::instance().chatFont() );
  updateChatColors();
  updateOnSendingMessage();

  mp_pbSend->setIcon( IconManager::instance().icon( "send.png" ) );

#ifdef BEEBEEP_USE_SHAREDESKTOP
  mp_actShareDesktop = new QAction( IconManager::instance().icon( "desktop-share.png" ), tr( "Share your desktop" ), this );
  mp_actShareDesktop->setCheckable( true );
  mp_actShareDesktop->setEnabled( Settings::instance().enableShareDesktop() );
  connect( mp_actShareDesktop, SIGNAL( triggered() ), this, SLOT( shareDesktopToChat() ) );
  mp_actScreenshot = new QAction( IconManager::instance().icon( "screenshot.png" ), tr( "Send a screenshot" ), this );
  connect( mp_actScreenshot, SIGNAL( triggered() ), this, SLOT( sendScreenshotToChat() ) );
#endif

  connect( mp_teChat, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( customContextMenu( const QPoint& ) ) );
  connect( mp_teChat, SIGNAL( anchorClicked( const QUrl& ) ), this, SLOT( checkAnchorClicked( const QUrl&  ) ) );
  connect( mp_teMessage, SIGNAL( returnPressed() ), this, SLOT( sendMessage() ) );
  connect( mp_teMessage, SIGNAL( writing() ), this, SLOT( checkWriting() ) );
  connect( mp_teMessage, SIGNAL( urlsToCheck( const QMimeData* ) ), this, SLOT( checkAndSendUrls( const QMimeData* ) ) );
  connect( mp_teMessage, SIGNAL( imageToCheck( const QMimeData* ) ), this, SLOT( checkAndSendImage( const QMimeData* ) ) );
  connect( mp_pbSend, SIGNAL( clicked() ), this, SLOT( sendMessage() ) );
}

void GuiChat::setModernMode( bool enabled )
{
  if( !enabled || m_modernMode )
    return;

  m_modernMode = true;
  mp_frameChat->setObjectName( "ShadowTranscriptFrame" );
  mp_frameMessage->setObjectName( "ShadowComposer" );
  mp_splitter->setHandleWidth( 1 );
  mp_splitter->setContentsMargins( 0, 0, 0, 0 );

  QGridLayout* composer_layout = qobject_cast<QGridLayout*>( mp_frameMessage->layout() );
  if( composer_layout )
  {
    composer_layout->removeWidget( mp_teMessage );
    composer_layout->removeWidget( mp_pbSend );
    composer_layout->removeWidget( mp_pbRecordVoiceMessage );
    composer_layout->removeWidget( mp_cbSendAndClose );
    composer_layout->setContentsMargins( 16, 10, 16, 12 );
    composer_layout->setHorizontalSpacing( 8 );
    composer_layout->setVerticalSpacing( 8 );

    mp_teMessage->setMinimumHeight( 74 );
    mp_teMessage->setMaximumHeight( 160 );
    mp_teMessage->setAccessibleName( QStringLiteral( "消息输入框" ) );
    composer_layout->addWidget( mp_teMessage, 0, 0, 1, 4 );

    mp_modernAttachButton = new QToolButton( mp_frameMessage );
    mp_modernAttachButton->setObjectName( "ShadowComposerAction" );
    mp_modernAttachButton->setText( QStringLiteral( "图片 / 文件" ) );
    mp_modernAttachButton->setIcon( ShadowStyle::actionIcon( QStringLiteral( "paperclip" ) ) );
    mp_modernAttachButton->setIconSize( QSize( 18, 18 ) );
    mp_modernAttachButton->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    mp_modernAttachButton->setToolTip( QStringLiteral( "选择一个或多个文件；也可把图片拖到聊天窗口" ) );
    mp_modernAttachButton->setAccessibleName( QStringLiteral( "发送图片或文件" ) );
    connect( mp_modernAttachButton, SIGNAL( clicked() ), this, SLOT( sendFile() ) );
    composer_layout->addWidget( mp_modernAttachButton, 1, 0 );

    mp_modernComposerHint = new QLabel( mp_frameMessage );
    mp_modernComposerHint->setObjectName( "ShadowComposerHint" );
    composer_layout->addWidget( mp_modernComposerHint, 1, 1 );
    composer_layout->setColumnStretch( 2, 1 );

    mp_pbSend->setObjectName( "ShadowSendButton" );
    mp_pbSend->setText( QStringLiteral( "发送" ) );
    mp_pbSend->setIcon( QIcon() );
    mp_pbSend->setFlat( false );
    mp_pbSend->setMinimumSize( 72, 34 );
    mp_pbSend->setMaximumSize( 96, 36 );
    mp_pbSend->setAccessibleName( QStringLiteral( "发送消息" ) );
    mp_pbSend->setStyleSheet( QStringLiteral(
      "QPushButton { color: #FFFFFF; background: #2563EB; border: 1px solid #2563EB; border-radius: 8px; padding: 6px 16px; font-weight: 600; }"
      "QPushButton:hover { background: #1D4ED8; border-color: #1D4ED8; }"
      "QPushButton:disabled { color: #8A94A3; background: #E7EBF0; border-color: #E7EBF0; }"
      "QPushButton:focus { border: 2px solid #8CB4FF; padding: 5px 15px; }" ) );
    composer_layout->addWidget( mp_pbSend, 1, 3 );
  }

  mp_pbRecordVoiceMessage->hide();
  mp_cbSendAndClose->hide();
  mp_frameVoicePlayer->hide();
  updateUseReturnKeyToSendMessageToolTip();
  setChatFont( ShadowStyle::applicationFont() );
  updateChatColors();
  QList<int> modern_sizes;
  modern_sizes << 520 << 126;
  mp_splitter->setSizes( modern_sizes );
}

void GuiChat::setupToolBar( QToolBar* chat_bar )
{
  // Chat toolbar
  chat_bar->addAction( IconManager::instance().icon( "font.png" ), tr( "Change font style" ), this, SLOT( selectFont() ) );
  chat_bar->addAction( IconManager::instance().icon( "font-color.png" ), tr( "Change font color" ), this, SLOT( selectFontColor() ) );
  mp_actFilterMessages = chat_bar->addAction( IconManager::instance().icon( "filter.png" ), tr( "Filter message" ), this, SLOT( showChatMessageFilterMenu() ) );
  mp_actFilterMessages->setCheckable( true );
  updateFilterMessagesButton();
  chat_bar->addAction( IconManager::instance().icon( "settings.png" ), tr( "Chat settings" ), this, SIGNAL( showChatMenuRequest() ) );
  mp_actSpellChecker = chat_bar->addAction( IconManager::instance().icon( "spellchecker.png" ), tr( "Spell checking" ), this, SLOT( onSpellCheckerActionClicked() ) );
  mp_actSpellChecker->setCheckable( true );
  mp_actCompleter = chat_bar->addAction( IconManager::instance().icon( "dictionary.png" ), tr( "Word completer" ), this, SLOT( onCompleterActionClicked() ) );
  mp_actCompleter->setCheckable( true );
  mp_actUseReturnToSendMessage = chat_bar->addAction( IconManager::instance().icon( "key-return.png" ), tr( "Use Return key to send message" ), this, SLOT( onUseReturnToSendMessageClicked() ) );
  mp_actUseReturnToSendMessage->setCheckable( true );
  updateActionsOnFocusChanged();
  mp_actSendFile = chat_bar->addAction( IconManager::instance().icon( "send-file.png" ), tr( "Send file" ), this, SLOT( sendFile() ) );
  mp_actSendFolder = chat_bar->addAction( IconManager::instance().icon( "send-folder.png" ), tr( "Send folder" ), this, SLOT( sendFolder() ) );
  mp_actSendTextCode = chat_bar->addAction( IconManager::instance().icon( "send-code.png" ), tr( "Send code" ), this, SLOT( sendTextCode() ) );
#ifdef BEEBEEP_USE_SHAREDESKTOP
  chat_bar->addAction( mp_actScreenshot );
  chat_bar->addAction( mp_actShareDesktop );
#endif
  chat_bar->addAction( mp_actSaveAs );

  mp_teMessage->addActionToContextMenu( mp_actSendFile );
  mp_teMessage->addActionToContextMenu( mp_actSendFolder );
  mp_teMessage->addActionToContextMenu( mp_actSendTextCode );
  QAction* act = new QAction( mp_teMessage );
  act->setSeparator( true );
  mp_teMessage->addActionToContextMenu( act );
  mp_teMessage->addActionToContextMenu( mp_actRestoreDefaultFont );
}

void GuiChat::updateOnSendingMessage()
{
  if( Settings::instance().chatOnSendingMessage() == Settings::CloseChatOnSendingMessage )
  {
    mp_cbSendAndClose->setText( tr( "Close" ) );
    mp_cbSendAndClose->setToolTip( tr( "Close chat window on sending message" ) );
  }
  else
  {
    mp_cbSendAndClose->setText( tr( "Minimize" ) );
    mp_cbSendAndClose->setToolTip( tr( "Minimize chat window on sending message" ) );
  }
  mp_cbSendAndClose->setChecked( Settings::instance().chatOnSendingMessage() > Settings::SkipOnSendingMessage );
}

void GuiChat::updateActions( const Chat& c, bool is_connected, int connected_users, bool file_transfer_is_active )
{
  if( c.id() != m_chatId )
    return;

  mp_teMessage->setEnabled( c.isValid() );
  mp_teChat->setEnabled( c.isValid() );

  bool local_user_is_member = c.hasUser( Settings::instance().localUser().id() );
  bool chat_is_empty = ChatManager::instance().isChatEmpty( c, true );
  bool can_send_files = false;
  UserList chat_members;
#ifdef BEEBEEP_USE_SHAREDESKTOP
  bool desktop_is_shared = false;
  QStringList share_desktop_users;
#endif
  if( connected_users > 0 )
  {
    chat_members = UserManager::instance().userList().fromUsersId( c.usersId() );
    foreach( User u, chat_members.toList() )
    {
      if( u.isLocal() )
        continue;
      if( u.isStatusConnected() )
        can_send_files = true;
#ifdef BEEBEEP_USE_SHAREDESKTOP
      if( Settings::instance().enableShareDesktop() )
      {
        if( beeCore->shareDesktopIsActive( u.id() ) )
        {
          desktop_is_shared = true;
          share_desktop_users.append( u.name() );
        }
      }
#endif
    }
  }

  mp_actClear->setDisabled( chat_is_empty );
  mp_actClearSystemMessages->setEnabled( c.hasSystemMessages() );
  mp_actSendFile->setEnabled( file_transfer_is_active && local_user_is_member && is_connected && can_send_files );
  mp_actSendFolder->setEnabled( file_transfer_is_active && local_user_is_member && is_connected && can_send_files );
  if( mp_modernAttachButton )
    mp_modernAttachButton->setEnabled( mp_actSendFile->isEnabled() );

  if( !is_connected )
  {
    mp_teMessage->setEnabled( false );
    mp_pbSend->setEnabled( false );
    mp_actSendTextCode->setEnabled( false );
    mp_teMessage->setToolTip( tr( "You are not connected" ) );
  }
  else
  {
    if( Settings::instance().disableSendMessage() )
    {
      mp_teMessage->setEnabled( false );
      mp_pbSend->setEnabled( false );
      mp_actSendTextCode->setEnabled( false );
      mp_teMessage->setToolTip( tr( "Send messages is disabled" ) );
    }
    else if( c.isDefault() && !Settings::instance().chatWithAllUsersIsEnabled() )
    {
      mp_teMessage->setEnabled( false );
      mp_pbSend->setEnabled( false );
      mp_actSendTextCode->setEnabled( false );
      mp_teMessage->setToolTip( tr( "Chat with all users is disabled" ) );
    }
    else if( c.isPrivate() && Settings::instance().disablePrivateChats() )
    {
      mp_teMessage->setEnabled( false );
      mp_pbSend->setEnabled( false );
      mp_actSendTextCode->setEnabled( false );
      mp_teMessage->setToolTip( tr( "Private chat is disabled" ) );
    }
    else
    {
      mp_teMessage->setEnabled( local_user_is_member );
      mp_pbSend->setEnabled( local_user_is_member );
      mp_actSendTextCode->setEnabled( local_user_is_member );
      if( local_user_is_member )
        mp_teMessage->setToolTip( "" );
      else
        mp_teMessage->setToolTip( tr( "You have left this chat" ) );
    }
  }

#if QT_VERSION >= 0x050200
  if( mp_teMessage->isEnabled() )
  {
    if( c.isDefault() )
      mp_teMessage->setPlaceholderText( tr( "Write a message to all user" ) );
    else
      mp_teMessage->setPlaceholderText( tr( "Write a message to %1" ).arg( c.name() ) );
  }
  else
    mp_teMessage->setPlaceholderText( mp_teMessage->toolTip() );
#endif

#ifdef BEEBEEP_USE_SHAREDESKTOP
  mp_actShareDesktop->setEnabled( Settings::instance().enableShareDesktop() && m_chatId != ID_DEFAULT_CHAT && local_user_is_member && is_connected && can_send_files );
  if( Settings::instance().enableShareDesktop() )
    mp_actShareDesktop->setChecked( desktop_is_shared );
  else
    mp_actShareDesktop->setChecked( false );

  if( mp_actShareDesktop->isChecked() )
  {
    mp_actShareDesktop->setToolTip( tr( "Your desktop is shared with %1" ).arg( Bee::stringListToTextString( share_desktop_users, true, 5 ) ) );
  }
  else
  {
    mp_actShareDesktop->setToolTip( tr( "Share your desktop" ) );
    mp_actShareDesktop->setIcon( IconManager::instance().icon( "desktop-share.png" ) );
  }

  mp_actScreenshot->setEnabled( m_chatId != ID_DEFAULT_CHAT && local_user_is_member && is_connected && can_send_files );
#endif

#ifdef BEEBEEP_USE_VOICE_CHAT
  mp_pbRecordVoiceMessage->setEnabled( mp_pbSend->isEnabled() && file_transfer_is_active && !Settings::instance().disableVoiceMessages() );
  if( !file_transfer_is_active )
    mp_pbRecordVoiceMessage->setToolTip( tr( "File transfer must be enabled to send voice messages" ) );
  else
    mp_pbRecordVoiceMessage->setToolTip( tr( "Record voice message" ) );
#endif
}

void GuiChat::customContextMenu( const QPoint& p )
{
  mp_menuContext->clear();
  mp_menuContext->addAction( mp_actSelectBackgroundColor );
  mp_menuContext->addAction(  mp_actSelectForegroundColor );
  mp_menuContext->addAction(  mp_actSelectSystemColor );
  mp_menuContext->addSeparator();
  mp_menuContext->addAction( mp_actFindTextInChat );
  mp_menuContext->addSeparator();
  mp_menuContext->addAction( IconManager::instance().icon( "select-all.png" ), tr( "Select all" ), mp_teChat, SLOT( selectAll() ), QKeySequence::SelectAll );
  mp_menuContext->addSeparator();
  QAction* act = mp_menuContext->addAction( IconManager::instance().icon( "copy.png" ), tr( "Copy to clipboard" ), mp_teChat, SLOT( copy() ), QKeySequence::Copy );
  act->setEnabled( !mp_teChat->textCursor().selectedText().isEmpty() );
  act = mp_menuContext->addAction( IconManager::instance().icon( "anchor.png" ), tr( "Copy URL to clipboard" ), this, SLOT( copyUrlToClipboard() ) );
  QString url_clicked = mp_teChat->anchorAt( p );
  if( !url_clicked.isEmpty() )
    act->setData( url_clicked );
  else
    act->setEnabled( false );
  act = mp_menuContext->addAction( IconManager::instance().icon( "quote.png" ), tr( "Quote selected text" ), this, SLOT( quoteSelectedText() ) );
  act->setEnabled( !mp_teChat->textCursor().selectedText().isEmpty() );
  act = mp_menuContext->addAction( IconManager::instance().icon( "network.png" ), tr( "Open selected text as url" ), this, SLOT( openSelectedTextAsUrl() ) );
  act->setEnabled( !mp_teChat->textCursor().selectedText().isEmpty() );
  //act = mp_menuContext->addAction( IconManager::instance().icon( "font-color.png" ), tr( "Change the background color of the selected text" ), this, SLOT( changeBackgroundColorInSelectedText() ) );
  //act->setEnabled( !mp_teChat->textCursor().selectedText().isEmpty() );

  mp_menuContext->addSeparator();
  mp_menuContext->addAction( mp_actClear );
  mp_menuContext->addAction( mp_actClearSystemMessages );
  mp_menuContext->addSeparator();
  mp_menuContext->addAction( mp_actPrint );
  mp_menuContext->addSeparator();
  mp_menuContext->addAction( mp_actRestoreDefaultFont );
  mp_menuContext->exec( QCursor::pos() );
}

bool GuiChat::messageCanBeShowed( const ChatMessage& cm )
{
  if( m_chatId == ID_DEFAULT_CHAT && Settings::instance().showOnlyMessagesInDefaultChat() )
    return cm.isChatActivity() && !Settings::instance().chatMessageFilter().testBit( static_cast<int>( cm.type() ) );
  else
    return !Settings::instance().chatMessageFilter().testBit( static_cast<int>( cm.type() ) );
}

bool GuiChat::historyCanBeShowed()
{
  return !Settings::instance().chatMessageFilter().testBit( static_cast<int>( ChatMessage::History ) );
}

void GuiChat::updateFilterMessagesButton()
{
  if( !mp_actFilterMessages )
    return;
  QBitArray filter_array = Settings::instance().chatMessageFilter();
  bool filters_active = false;
  for( int i = 0; i < filter_array.size(); i++ )
  {
    if( filter_array.testBit( i ) )
    {
      filters_active = true;
      break;
    }
  }
  mp_actFilterMessages->setChecked( filters_active );
}

void GuiChat::showChatMessageFilterMenu()
{
  mp_menuFilters->clear();
  QAction* act;

  act = mp_menuFilters->addAction( tr( "Show only messages in default chat" ), this, SLOT( changeChatMessageFilter() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showOnlyMessagesInDefaultChat() );
  act->setData( static_cast<int>( ChatMessage::NumTypes ) );
  mp_menuFilters->addSeparator();

  for( int i = ChatMessage::System; i < ChatMessage::NumTypes; i++ )
  {
    act = mp_menuFilters->addAction( Bee::chatMessageTypeToString( i ), this, SLOT( changeChatMessageFilter() ) );
    act->setCheckable( true );
    act->setChecked( !Settings::instance().chatMessageFilter().testBit( i ) );
    act->setData( i );
  }
  mp_menuFilters->exec( QCursor::pos() );
  updateFilterMessagesButton();
}

void GuiChat::changeChatMessageFilter()
{
  QAction* act = qobject_cast<QAction*>( sender() );
  if( !act )
    return;

  if( act->data().toInt() == static_cast<int>( ChatMessage::NumTypes ) )
  {
    Settings::instance().setShowOnlyMessagesInDefaultChat( act->isChecked() );
    if( m_chatId != ID_DEFAULT_CHAT )
      return;
  }
  else
  {
    QBitArray filter_array = Settings::instance().chatMessageFilter();
    filter_array.setBit( act->data().toInt(), !act->isChecked() );
    Settings::instance().setChatMessageFilter( filter_array );
  }

  updateFilterMessagesButton();
  updateChat();
}

void GuiChat::setLastMessageTimestamp( const QDateTime& dt )
{
  if( dt.isValid() && !Settings::instance().chatShowMessageTimestamp() )
  {
    QString last_msg_timestamp = tr( "Last message %1" );
    if( dt.date() == QDate::currentDate() )
      last_msg_timestamp = last_msg_timestamp.arg( dt.toString( "hh:mm" ) );
    else
      last_msg_timestamp = last_msg_timestamp.arg( dt.toString( Qt::SystemLocaleLongDate ) );
    mp_teChat->setToolTip( last_msg_timestamp );
  }
  else
    mp_teChat->setToolTip( "" );
}

void GuiChat::sendMessage()
{
  if( Settings::instance().disableSendMessage() )
    return;
  QString msg = mp_teMessage->message();
  if( msg.isEmpty() )
    return;
  mp_teMessage->addMessageToHistory();
  emit newMessage( m_chatId, msg, false );
  mp_teMessage->clearMessage();
  if( mp_cbSendAndClose->isChecked() )
  {
    if( Settings::instance().chatOnSendingMessage() == Settings::CloseChatOnSendingMessage )
      QMetaObject::invokeMethod( this, "closeRequest", Qt::QueuedConnection );
    else
      QMetaObject::invokeMethod( this, "hideRequest", Qt::QueuedConnection );
  }
  else
    ensureFocusInChat();
}

void GuiChat::checkWriting()
{
  emit writing( m_chatId );
}

void GuiChat::checkAnchorClicked( const QUrl& url )
{
  if( url.scheme() == QLatin1String( "shadowexpand" ) )
  {
    const QString message_id = url.path();
    if( !message_id.isEmpty() )
    {
      m_expandedMessageIds.insert( message_id );
      updateChat();
    }
    return;
  }

  if( url.scheme() == FileInfo::urlSchemeVoiceMessage() )
    emit showStatusMessageRequest( tr( "Opening voice message" ) + QString( "..." ), 3000 );

  emit openUrl( url, m_chatId );
}

QString GuiChat::chatMessageToText( const ChatMessage& cm )
{
  QString s = "";

  if( !messageCanBeShowed( cm ) )
    return s;

  if( m_modernMode && !cm.isFromSystem() && !cm.isFileTransfer() && !cm.isImagePreview() )
    return modernChatMessageToText( cm );

  if( !cm.isFromSystem() )
  {
    User u = UserManager::instance().findUser( cm.userId() );
    if( !u.isValid() )
    {
      qWarning() << "User" << cm.userId() << "not found for message:" << qPrintable( cm.message() );
      return "";
    }
    s = GuiChatMessage::formatMessage( u, cm, m_lastMessageUserId, Settings::instance().chatShowMessageTimestamp(), false, false,
                                       Settings::instance().showMessagesGroupByUser(), m_chatId == ID_DEFAULT_CHAT ? true : Settings::instance().chatUseYourNameInsteadOfYou(),
                                       Settings::instance().chatCompact() );
  }
  else
    s = GuiChatMessage::formatSystemMessage( cm, m_lastMessageUserId, Settings::instance().chatShowMessageTimestamp(), false, Settings::instance().chatCompact() );

  m_lastMessageUserId = cm.isImportant() ? ID_IMPORTANT_MESSAGE : cm.userId();
  return s;
}

void GuiChat::updateChat()
{
  Chat c = ChatManager::instance().chat( m_chatId );
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << m_chatId << "found in GuiChat::updateChat(...)";
    return;
  }
  setChat( c );
}

void GuiChat::loadSavedMessages()
{
  emit showStatusMessageRequest( tr( "Loading of saved messages..." ), 3000 );
  QApplication::processEvents();

  if( !ChatManager::instance().isLoadHistoryCompleted() )
  {
    emit showStatusMessageRequest( tr( "The loading of saved messages has not yet been completed."), 2000 );
    QTimer::singleShot( 2000, this, SLOT( operationCompleted() ) );
    return;
  }

  if( !historyCanBeShowed() )
  {
    QTimer::singleShot( 0, this, SLOT( operationCompleted() ) );
    return;
  }

  Chat c = ChatManager::instance().chat( m_chatId );
  if( !c.isValid() )
  {
    QTimer::singleShot( 0, this, SLOT( operationCompleted() ) );
    return;
  }

  setChat( c );
}

void GuiChat::setChatStylesheet( QTextDocument* text_document )
{
  // Valid only for NEW INSERTED HTML
  QString default_chat_css = text_document->defaultStyleSheet();
  default_chat_css.append( QString( " .bee-quote { background-color: %1; color: %2; }" ).arg( Settings::instance().chatQuoteBackgroundColor(), Settings::instance().chatQuoteTextColor() ) );
  if( m_modernMode )
    default_chat_css.append( QStringLiteral(
      " .shadow-image-album { color: #243044; }"
      " a { color: #2457B8; text-decoration: none; }" ) );
  text_document->setDefaultStyleSheet( default_chat_css );
}

QString GuiChat::modernMessageId( const ChatMessage& cm ) const
{
  const QString key = QStringLiteral( "%1|%2|%3" )
                      .arg( cm.userId() )
                      .arg( cm.timestamp().toMSecsSinceEpoch() )
                      .arg( cm.message() );
  return QString::number( qHash( key ) );
}

QString GuiChat::modernChatMessageToText( const ChatMessage& cm )
{
  User user = UserManager::instance().findUser( cm.userId() );
  const bool from_local_user = cm.isFromLocalUser() || (user.isValid() && user.isLocal());
  const QString user_name = from_local_user ? QStringLiteral( "我" )
                                             : (user.isValid() ? Bee::userNameToShow( user, true ) : QStringLiteral( "未知用户" ));
  QString message_body = cm.message();

  if( cm.isSourceCode() )
    message_body = QStringLiteral( "<code>%1</code>" ).arg( message_body.toHtmlEscaped().replace( QLatin1Char( '\n' ), QLatin1String( "<br>" ) ) );

  QTextDocument plain_document;
  plain_document.setHtml( message_body );
  const QString plain_text = plain_document.toPlainText();
  const QString message_id = modernMessageId( cm );
  if( plain_text.size() > 700 && !m_expandedMessageIds.contains( message_id ) )
  {
    QString teaser = plain_text.left( 520 ).trimmed().toHtmlEscaped();
    teaser.replace( QLatin1Char( '\n' ), QLatin1String( "<br>" ) );
    message_body = QStringLiteral( "%1…<br><br><a href=\"shadowexpand:%2\"><b>展开全文</b></a>" )
                   .arg( teaser, message_id );
  }

  const QString alignment = from_local_user ? QStringLiteral( "right" ) : QStringLiteral( "left" );
  const QString bubble_color = from_local_user ? QStringLiteral( "#DDEBFF" ) : QStringLiteral( "#FFFFFF" );
  const QString timestamp = cm.timestamp().isValid() ? cm.timestamp().toString( QStringLiteral( "HH:mm" ) ) : QString();
  return QStringLiteral(
    "<br><table align=\"%1\" width=\"72%\" cellspacing=\"0\" cellpadding=\"0\">"
    "<tr><td align=\"%1\"><font color=\"#6B7686\">%2&nbsp;&nbsp;%3</font></td></tr>"
    "<tr><td><table width=\"100%\" cellspacing=\"0\" cellpadding=\"11\" bgcolor=\"%4\"><tr><td>%5</td></tr></table></td></tr>"
    "</table><br><br>" )
    .arg( alignment, user_name.toHtmlEscaped(), timestamp, bubble_color, message_body );
}

QString GuiChat::modernImageAlbumHtml( const QStringList& preview_messages, bool from_local_user ) const
{
  QString combined = preview_messages.join( QStringLiteral( " " ) );
  QStringList image_links;
  QRegExp image_link_expression( QStringLiteral( "(<a[^>]*>\\s*<img[^>]*>\\s*</a>)" ) );
  image_link_expression.setMinimal( true );
  int offset = 0;
  while( (offset = image_link_expression.indexIn( combined, offset )) >= 0 )
  {
    image_links.append( image_link_expression.cap( 1 ) );
    offset += qMax( 1, image_link_expression.matchedLength() );
  }

  if( image_links.isEmpty() )
  {
    combined.replace( QStringLiteral( "<br><div align=center>" ), QString() );
    combined.replace( QStringLiteral( "</div>" ), QString() );
    image_links.append( combined );
  }

  QString visible_html;
  foreach( QString image_link, image_links )
  {
    image_link.replace( QRegExp( QStringLiteral( "\\s+(width|height)=\"[^\"]*\"" ) ), QString() );
    image_link.replace( QStringLiteral( "<img" ), QStringLiteral( "<img width=\"440\"" ) );
    visible_html += QStringLiteral( "<div align=\"left\">%1</div><br>" ).arg( image_link );
  }

  const QString alignment = from_local_user ? QStringLiteral( "right" ) : QStringLiteral( "left" );
  const QString background = from_local_user ? QStringLiteral( "#DDEBFF" ) : QStringLiteral( "#FFFFFF" );
  return QStringLiteral(
    "<br><table align=\"%1\" width=\"74%\" cellspacing=\"0\" cellpadding=\"14\" bgcolor=\"%2\">"
    "<tr><td><font color=\"#5F6B7A\"><b>%3 张图片</b> · 点击图片查看原图</font>"
    "<br><br><div class=\"shadow-image-album\" align=\"left\">%4</div></td></tr></table><br>" )
    .arg( alignment )
    .arg( background )
    .arg( image_links.size() )
    .arg( visible_html );
}

#ifdef BEEBEEP_UX_PREVIEW
void GuiChat::showUxPreviewConversation( const QStringList& image_paths )
{
  QStringList preview_messages;
  foreach( const QString& image_path, image_paths )
  {
    const QString image_url = QUrl::fromLocalFile( image_path ).toString();
    preview_messages.append( QStringLiteral( "<a href=\"%1\"><img src=\"%1\" height=\"160\"></a>" ).arg( image_url ) );
  }

  QString html = QStringLiteral(
    "<p align=\"center\"><font color=\"#8A94A3\">今天 12:52</font></p>"
    "<br><table align=\"right\" width=\"72%\" cellspacing=\"0\" cellpadding=\"0\">"
    "<tr><td align=\"right\"><font color=\"#6B7686\">我&nbsp;&nbsp;12:52</font></td></tr>"
    "<tr><td><table width=\"100%\" cellspacing=\"0\" cellpadding=\"11\" bgcolor=\"#DDEBFF\"><tr><td>活动物料已整理好，请帮忙确认图片。</td></tr></table></td></tr>"
    "</table><br><br>" );
  html += modernImageAlbumHtml( preview_messages, true );
  html += QStringLiteral(
    "<br><table align=\"left\" width=\"72%\" cellspacing=\"0\" cellpadding=\"0\">"
    "<tr><td><font color=\"#6B7686\">林晓&nbsp;&nbsp;12:54</font></td></tr>"
    "<tr><td><table width=\"100%\" cellspacing=\"0\" cellpadding=\"11\" bgcolor=\"#FFFFFF\"><tr><td>收到，这样一次查看和保存方便多了。</td></tr></table></td></tr>"
    "</table><br>" );

  QTextDocument* text_document = mp_teChat->document();
  setChatStylesheet( text_document );
  mp_teChat->setHtml( html );
  mp_teChat->verticalScrollBar()->setValue( 0 );
}
#endif

bool GuiChat::setChat( const Chat& c )
{
  if( !c.isValid() )
    return false;

  m_chatId = c.id();

  int missed_lines = -1;
  int num_messages = c.messages().size();
  int msg_lines = Settings::instance().chatMessagesToShow() >= 0 ? qMin( num_messages, Settings::instance().chatMessagesToShow() ) : num_messages;
  int history_lines = Settings::instance().chatMessagesToShow() >= 0 ? qMax( 0, Settings::instance().chatMessagesToShow() - msg_lines ) : -1;
  m_lastMessageUserId = ID_SYSTEM_MESSAGE;

  QString html_text = "";
  if( ChatManager::instance().isLoadHistoryCompleted() && historyCanBeShowed() && history_lines != 0 )
  {
    if( !ChatManager::instance().chatHasSavedText( c.name() ) )
    {
      if( c.isPrivate() && c.name().contains( "@" ) && ChatManager::instance().chatHasSavedText( User::nameFromPath( c.name() ) ) )
        html_text += ChatManager::instance().chatSavedText( User::nameFromPath( c.name() ), history_lines, &missed_lines );
    }
    else
      html_text += ChatManager::instance().chatSavedText( c.name(), history_lines, &missed_lines );
  }

  if( missed_lines < 0 )
    missed_lines = qMax( 0, num_messages - msg_lines );

  if( Settings::instance().chatMessagesToShow() >= 0 && (missed_lines > 0 || num_messages > msg_lines) )
  {
    QUrl saved_chat_url;
    saved_chat_url.setUrl( QString( "#%1").arg( m_chatId ) );
    saved_chat_url.setScheme( "beeshowsavedchat" );
    QString limit_reached_text = "";
    limit_reached_text += QString( "<p><font color=%1><i>... %2 ...<br>... %3, %4 ...</i></font></p>" )
                            .arg( Settings::instance().chatSystemTextColor(),
                                  tr( "only the last %1 messages are shown" ).arg( Settings::instance().chatMessagesToShow() ),
                                  tr( "maximum number of messages to show reached" ),
                                  tr( "open %1 to read them all" ).arg( QString( "<a href='%1'>%2</a>" ).arg( saved_chat_url.toString(), tr( "the saved chat" ) ) ) );
    if( !html_text.isEmpty() )
      html_text.prepend( limit_reached_text );
    else
      html_text += limit_reached_text;
  }

  QStringList pending_image_previews;
  VNumber pending_image_user_id = ID_INVALID;
  foreach( ChatMessage cm, c.messages() )
  {
    if( num_messages > msg_lines )
    {
      num_messages--;
      continue;
    }

    if( m_modernMode && cm.isImagePreview() )
    {
      if( !pending_image_previews.isEmpty() && pending_image_user_id != cm.userId() )
      {
        html_text += modernImageAlbumHtml( pending_image_previews, pending_image_user_id == ID_LOCAL_USER );
        pending_image_previews.clear();
      }
      pending_image_user_id = cm.userId();
      pending_image_previews.append( cm.message() );
      m_lastMessageUserId = ID_SYSTEM_MESSAGE;
      continue;
    }

    if( m_modernMode && !pending_image_previews.isEmpty() && cm.isFileTransfer() )
      continue;

    if( !pending_image_previews.isEmpty() )
    {
      html_text += modernImageAlbumHtml( pending_image_previews, pending_image_user_id == ID_LOCAL_USER );
      pending_image_previews.clear();
      pending_image_user_id = ID_INVALID;
    }

    html_text += chatMessageToText( cm );
  }
  if( !pending_image_previews.isEmpty() )
    html_text += modernImageAlbumHtml( pending_image_previews, pending_image_user_id == ID_LOCAL_USER );

  QTextDocument *text_document = mp_teMessage->document();
  QTextOption text_option = text_document->defaultTextOption();
  text_option.setTextDirection( Settings::instance().showTextInModeRTL() ? Qt::RightToLeft : Qt::LeftToRight );
  text_document->setDefaultTextOption( text_option );
  mp_teMessage->setDocument( text_document );
  bool updates_is_enabled = mp_teChat->updatesEnabled();
  mp_teChat->setUpdatesEnabled( false );
  mp_teChat->clear();
  text_document = mp_teChat->document();
  text_option = text_document->defaultTextOption();
  text_option.setTextDirection( Settings::instance().showTextInModeRTL() ? Qt::RightToLeft : Qt::LeftToRight );
  text_document->setDefaultTextOption( text_option );
  setChatStylesheet( text_document );
  mp_teChat->setDocument( text_document );
  mp_teChat->setHtml( html_text );
  mp_teChat->setUpdatesEnabled( updates_is_enabled );
  setLastMessageTimestamp( c.lastMessageTimestamp() );
  ensureLastMessageVisible();
  updateChat( c );
  QTimer::singleShot( 0, this, SLOT( operationCompleted() ) );
  return true;
}

bool GuiChat::updateChat( const Chat& c )
{
  if( m_chatId != c.id() )
    return false;
  updateChatColors();
  return true;
}

void GuiChat::ensureFocusInChat()
{
  if( mp_teMessage->isEnabled() )
    mp_teMessage->setFocus();

  if( !mp_teMessage->hasFocus() )
    mp_teMessage->setCursor( Qt::WaitCursor );
  else
    mp_teMessage->setCursor( Qt::ArrowCursor );
}

void GuiChat::ensureLastMessageVisible()
{
  QScrollBar *bar = mp_teChat->verticalScrollBar();
  if( bar )
  {
    if( !bar->isSliderDown() )
      bar->setValue( bar->maximum() );
  }
  else
    mp_teChat->ensureCursorVisible();
}

bool GuiChat::appendChatMessage( const Chat& c, const ChatMessage& cm )
{
  if( m_chatId != c.id() )
  {
    qWarning() << "Trying to append chat message of chat id" << c.id() << "in chat shown with id" << m_chatId << "... skip it";
    return false;
  }

  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << m_chatId << "found in GuiChat::appendChatMessage(...)";
    return false;
  }

  if( m_modernMode && (cm.isImagePreview() || cm.isFileTransfer()) )
    return setChat( c );

  bool show_timestamp_last_message = !cm.isFromLocalUser() && !cm.isFromSystem();
  mp_actClear->setDisabled( c.isEmpty() && !ChatManager::instance().chatHasSavedText( c.name() ) );
  mp_actClearSystemMessages->setEnabled( c.hasSystemMessages() );
  QString text_message = chatMessageToText( cm );

  if( !text_message.isEmpty() )
  {
#ifdef BEEBEEP_DEBUG
    QTime time_to_insert;
    time_to_insert.start();
#endif
    bool updates_enabled = mp_teChat->updatesEnabled();
    mp_teChat->setUpdatesEnabled( false );
    QTextCursor cursor( mp_teChat->textCursor() );
    cursor.movePosition( QTextCursor::End );
    cursor.insertHtml( text_message );
    mp_teChat->setUpdatesEnabled( updates_enabled );
#ifdef BEEBEEP_DEBUG
    qDebug() << "Elapsed time to insert HTML text in chat:" << time_to_insert.elapsed() << "ms";
#endif
    ensureLastMessageVisible();
  }

  if( show_timestamp_last_message )
    setLastMessageTimestamp( cm.timestamp() );

  return true;
}

void GuiChat::setChatFont( const QFont& f )
{
  mp_teChat->setFont( f );
  mp_teMessage->setFont( f );
  mp_teMessage->update();
  mp_actRestoreDefaultFont->setEnabled( f != QApplication::font() );
}

void GuiChat::selectFont()
{
  bool ok = false;
  QFont f = QFontDialog::getFont( &ok, Settings::instance().chatFont(), this );
  if( ok )
  {
    Settings::instance().setChatFont( f, false );
    Settings::instance().save();
    setChatFont( f );
    mp_teChat->ensureCursorVisible();
    emit updateChatFontRequest();
  }
}

void GuiChat::updateChatColors()
{
  if( m_modernMode )
  {
    mp_teMessage->setTextColor( QColor( "#172033" ) );
    mp_teMessage->setStyleSheet( QStringLiteral(
      "#GuiMessageEdit { background: #FFFFFF; color: #172033; border: 1px solid #DCE3EC; border-radius: 10px; padding: 9px 10px; selection-background-color: #CFE0FF; }"
      "#GuiMessageEdit:focus { border: 2px solid #2563EB; padding: 8px 9px; }" ) );
    mp_teChat->setStyleSheet( QStringLiteral(
      "#GuiChatViewer { background: #F5F7FB; color: #243044; border: 0; padding: 18px 22px; selection-background-color: #CFE0FF; }" ) );
    return;
  }

  QString background_color = m_chatId == ID_DEFAULT_CHAT ? Settings::instance().defaultChatBackgroundColor() : Settings::instance().chatBackgroundColor();
  QString text_color = Settings::instance().chatFontColor() != Settings::instance().chatDefaultTextColor() ? Settings::instance().chatFontColor(): Settings::instance().chatDefaultTextColor();
  mp_teMessage->setTextColor( text_color );
  mp_teMessage->setStyleSheet( QString( "#GuiMessageEdit { background-color: %1; color: %2; }" ).arg( background_color, text_color ) );
  mp_teChat->setStyleSheet( QString( "#GuiChatViewer { background-color: %1; color: %2; }" ).arg( background_color, Settings::instance().chatDefaultTextColor() ) );
  // No css for bee-quote because stylesheet is only applied to new inserted HTML
}

void GuiChat::selectFontColor()
{
  QColor c = QColorDialog::getColor( QColor( Settings::instance().chatFontColor() ), this );
  if( c.isValid() && c.name() != Settings::instance().chatFontColor() )
  {
    Settings::instance().setChatFontColor( c.name() );
    Settings::instance().save();
    emit updateChatColorsRequest();
  }
}

void GuiChat::selectBackgroundColor()
{
  QColor c = QColorDialog::getColor( QColor( (m_chatId == ID_DEFAULT_CHAT ? Settings::instance().defaultChatBackgroundColor() : Settings::instance().chatBackgroundColor() ) ), this );
  if( c.isValid() )
  {
    if( m_chatId == ID_DEFAULT_CHAT )
    {
      Settings::instance().setDefaultChatBackgroundColor( c.name() );
      updateChatColors();
    }
    else
    {
      Settings::instance().setChatBackgroundColor( c.name() );
      emit updateChatColorsRequest();
    }
  }
}

void GuiChat::selectForegroundColor()
{
  QColor c = QColorDialog::getColor( QColor( Settings::instance().chatDefaultTextColor() ), this );
  if( c.isValid() && c.name() != Settings::instance().chatDefaultTextColor() )
  {
    Settings::instance().setChatDefaultTextColor( c.name() );
    emit updateChatColorsRequest();
  }
}

void GuiChat::selectSystemColor()
{
  QColor c = QColorDialog::getColor( QColor( Settings::instance().chatSystemTextColor() ), this );
  if( c.isValid() && c.name() != Settings::instance().chatSystemTextColor() )
  {
    Settings::instance().setChatSystemTextColor( c.name() );
    QMessageBox::information( this, Settings::instance().programName(), tr( "You must close and reopen this window to see the changes applied." ), tr( "Ok" ) );
    emit updateChatColorsRequest();
  }
}

void GuiChat::addEmoticon( const Emoticon& e )
{
  mp_teMessage->addEmoticon( e );
  ensureFocusInChat();
}

void GuiChat::addText( const QString& txt )
{
  mp_teMessage->addText( txt );
  ensureFocusInChat();
}

void GuiChat::saveChat()
{
  Chat c = ChatManager::instance().chat( m_chatId );
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat id" << m_chatId << "found in GuiChat::saveChat()";
    return;
  }

  QString file_name = FileDialog::getSaveFileName( this,
                        tr( "Please select a file to save all the messages of the chat." ),
                        Settings::instance().dataFolder(), "PDF Chat Files (*.pdf)" );
  if( file_name.isEmpty() )
    return;

  if( !file_name.endsWith( QLatin1String( ".pdf" ), Qt::CaseInsensitive ) )
    file_name.append( QLatin1String( ".pdf" ) );

  QPrinter printer;
  printer.setOutputFormat( QPrinter::PdfFormat );
  printer.setOutputFileName( file_name );
  QTextDocument *doc = new QTextDocument( this );
  QString html_text = "";
  if( ChatManager::instance().chatHasSavedText( c.name() ) )
    html_text.append( ChatManager::instance().chatSavedText( c.name() ) );
  html_text.append( GuiChatMessage::chatToHtml( c, !Settings::instance().chatSaveFileTransfers(),
                                                  !Settings::instance().chatSaveSystemMessages(), true, true, Settings::instance().chatCompact(), Settings::instance().useCompactDataSaving() ) );
  html_text.replace( FileInfo::urlSchemeVoiceMessage() + QLatin1String(":"), "file://" );
  html_text.replace( FileInfo::urlSchemeShowFileInFolder() + QLatin1String(":"), "file://" );
  doc->setHtml( html_text );
  doc->print( &printer );
  QMessageBox::information( this, Settings::instance().programName(), tr( "%1: save completed." ).arg( file_name ), tr( "Ok" ) );
  doc->deleteLater();
}

void GuiChat::clearChat()
{
  emit chatToClear( m_chatId );
}

void GuiChat::sendFile()
{
  emit sendFileFromChatRequest( m_chatId, QString( "" ) );
}

void GuiChat::sendFolder()
{
  QString folder_selected = FileDialog::getExistingDirectory( this, Settings::instance().programName(),
                                                              Settings::instance().lastDirectorySelected() );
  if( folder_selected.isEmpty() )
    return;

  emit sendFileFromChatRequest( m_chatId, folder_selected );
}

void GuiChat::dragEnterEvent( QDragEnterEvent *event )
{
  if( event->mimeData()->hasUrls() )
  {
    event->acceptProposedAction();
    if( m_modernMode )
    {
      mp_frameMessage->setProperty( "dropActive", true );
      mp_frameMessage->style()->unpolish( mp_frameMessage );
      mp_frameMessage->style()->polish( mp_frameMessage );
    }
  }
}

void GuiChat::dragLeaveEvent( QDragLeaveEvent *event )
{
  Q_UNUSED( event );
  if( m_modernMode )
  {
    mp_frameMessage->setProperty( "dropActive", false );
    mp_frameMessage->style()->unpolish( mp_frameMessage );
    mp_frameMessage->style()->polish( mp_frameMessage );
  }
}

void GuiChat::checkAndSendImage( const QMimeData* source )
{
  if( !source->hasImage() )
    return;

  QString image_format = "png";
  QString image_initial_path = Settings::instance().cacheFolder() +
                                    QString( "/beeimgtmp-%1." ).arg( Bee::dateTimeStringSuffix( QDateTime::currentDateTime() ) )
                                    + image_format;
  QString file_path = Bee::uniqueFilePath( image_initial_path, false );
  qDebug() << "Sending the dropped image as a file:" << file_path;

  QImage image = qvariant_cast<QImage>( source->imageData() );

  if( !image.save( file_path, image_format.toLatin1() ) )
  {
    QMessageBox::warning( this, Settings::instance().programName(),
      tr( "Unable to save temporary file: %1" ).arg( file_path ) );
    return;
  }

  Settings::instance().addTemporaryFilePath( file_path );

  emit sendFileFromChatRequest( m_chatId, file_path );
}


void GuiChat::checkAndSendUrls( const QMimeData* source )
{
  if( !source->hasUrls() )
    return;

  QStringList file_path_list;
  QString file_path;
  int num_files = 0;

  foreach( QUrl url, source->urls() )
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "Checking pasted url:" << qPrintable( url.toString() );
#endif
    if( Bee::isLocalFile( url ) )
    {
      file_path = url.toLocalFile();
      file_path_list.append( Bee::convertToNativeFolderSeparator( file_path ) );
      num_files++;
      if( num_files > Settings::instance().maxQueuedDownloads() )
        break;
    }
  }

  if( num_files <= 0 )
  {
    mp_teMessage->addPasted( source );
    return;
  }

  if( file_path_list.isEmpty() )
  {
    if( num_files > 0 )
    {
      QMessageBox::warning( this, Settings::instance().programName(), tr( "You are trying to send %1 files simultaneously but the maximum allowed is %2." )
                                                                      .arg( num_files ).arg( Settings::instance().maxQueuedDownloads() ), tr( "Ok" ) );
    }
    return;
  }

  num_files = qMin( num_files, Settings::instance().maxQueuedDownloads() );

  bool is_image_batch = num_files > 1;
  if( is_image_batch )
  {
    foreach( const QString& path, file_path_list )
    {
      if( !Bee::isFileTypeImage( QFileInfo( path ).suffix() ) )
      {
        is_image_batch = false;
        break;
      }
    }
  }

  if( (!m_modernMode || !is_image_batch) &&
      QMessageBox::question( this, Settings::instance().programName(),
                             tr( "Do you want to send %1 %2 to the members of this chat?" ).arg( num_files )
                             .arg( num_files == 1 ? tr( "file" ) : tr( "files" ) ),
                             tr( "Yes" ), tr( "No" ), QString(), 0, 1 ) == 1 )
  {
    return;
  }

  emit sendFilesFromChatRequest( m_chatId, file_path_list );
}

void GuiChat::dropEvent( QDropEvent *event )
{
  if( m_modernMode )
  {
    mp_frameMessage->setProperty( "dropActive", false );
    mp_frameMessage->style()->unpolish( mp_frameMessage );
    mp_frameMessage->style()->polish( mp_frameMessage );
  }
  if( event->mimeData()->hasUrls() )
    checkAndSendUrls( event->mimeData() );
}

void GuiChat::editChatMembers()
{
  emit editGroupRequest( m_chatId );
}

void GuiChat::updateShortcuts()
{
  QKeySequence ks = ShortcutManager::instance().shortcut( ShortcutManager::SetFocusInMessageBox );
  if( !ks.isEmpty() )
  {
    mp_scFocusInChat->setKey( ks );
    mp_scFocusInChat->setEnabled( Settings::instance().useCustomShortcuts() );
  }
  else
    mp_scFocusInChat->setEnabled( false );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::FindTextInChat );
  if( Settings::instance().useCustomShortcuts() )
  {
    if( ks.isEmpty() )
    {
      mp_actFindTextInChat->setShortcut( QKeySequence() );
      mp_scFindTextInChat->setKey( QKeySequence::Find );
      mp_scFindTextInChat->setEnabled( false );
    }
    else
    {
      mp_actFindTextInChat->setShortcut( ks );
      mp_scFindTextInChat->setKey( ks );
      mp_scFindTextInChat->setEnabled( true );
    }
  }
  else
  {
    mp_actFindTextInChat->setShortcut( QKeySequence::Find );
    mp_scFindTextInChat->setKey( QKeySequence::Find );
    mp_scFindTextInChat->setEnabled( true );
  }

  ks = ShortcutManager::instance().shortcut( ShortcutManager::FindNextTextInChat );
  if( Settings::instance().useCustomShortcuts() )
  {
    if( ks.isEmpty() )
    {
      mp_scFindNextTextInChat->setKey( QKeySequence::FindNext );
      mp_scFindNextTextInChat->setEnabled( false );
    }
    else
    {
      mp_scFindNextTextInChat->setKey( ks );
      mp_scFindNextTextInChat->setEnabled( true );
    }
  }
  else
  {
    mp_scFindNextTextInChat->setKey( QKeySequence::FindNext );
    mp_scFindNextTextInChat->setEnabled( true );
  }

  ks = ShortcutManager::instance().shortcut( ShortcutManager::FindPreviousTextInChat );
  if( Settings::instance().useCustomShortcuts() )
  {
    if( ks.isEmpty() )
    {
      mp_scFindPreviousTextInChat->setKey( QKeySequence::FindPrevious );
      mp_scFindPreviousTextInChat->setEnabled( false );
    }
    else
    {
      mp_scFindPreviousTextInChat->setKey( ks );
      mp_scFindPreviousTextInChat->setEnabled( true );
    }
  }
  else
  {
    mp_scFindPreviousTextInChat->setKey( QKeySequence::FindPrevious );
    mp_scFindPreviousTextInChat->setEnabled( true );
  }

  ks = ShortcutManager::instance().shortcut( ShortcutManager::SendFile );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    mp_actSendFile->setShortcut( ks );
  else
    mp_actSendFile->setShortcut( QKeySequence() );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::SendFolder );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    mp_actSendFolder->setShortcut( ks );
  else
    mp_actSendFolder->setShortcut( QKeySequence() );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::SendChatMessage );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    mp_pbSend->setShortcut( ks );
  else
    mp_pbSend->setShortcut( QKeySequence() );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::Print );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    mp_actPrint->setShortcut( ks );
  else
    mp_actPrint->setShortcut( QKeySequence() );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::Print );
  if( Settings::instance().useCustomShortcuts() )
  {
    if( ks.isEmpty() )
    {
      mp_actPrint->setShortcut( QKeySequence() );
      mp_scPrintChat->setKey( QKeySequence::Print );
      mp_scPrintChat->setEnabled( false );
    }
    else
    {
      mp_actPrint->setShortcut( ks );
      mp_scPrintChat->setKey( ks );
      mp_scPrintChat->setEnabled( true );
    }
  }
  else
  {
    mp_actPrint->setShortcut( QKeySequence::Print );
    mp_scPrintChat->setKey( QKeySequence::Print );
    mp_scPrintChat->setEnabled( true );
  }

  ks = ShortcutManager::instance().shortcut( ShortcutManager::ShowEmoticons );
  if( !ks.isEmpty() )
  {
    mp_scViewEmoticons->setKey( ks );
    mp_scViewEmoticons->setEnabled( Settings::instance().useCustomShortcuts() );
  }
  else
    mp_scViewEmoticons->setEnabled( false );

}

void GuiChat::updateUseReturnKeyToSendMessageToolTip()
{
  if( Settings::instance().useReturnToSendMessage() )
  {
    mp_actUseReturnToSendMessage->setToolTip( tr( "Use key Return to send message" ) );
    if( mp_modernComposerHint )
      mp_modernComposerHint->setText( QStringLiteral( "Enter 发送 · Shift+Enter 换行" ) );
  }
  else
  {
    mp_actUseReturnToSendMessage->setToolTip( tr( "Use key Return to make a carriage return" ) );
    if( mp_modernComposerHint )
      mp_modernComposerHint->setText( QStringLiteral( "Ctrl+Enter 发送 · Enter 换行" ) );
  }
}

void GuiChat::onUseReturnToSendMessageClicked()
{
  Settings::instance().setUseReturnToSendMessage( mp_actUseReturnToSendMessage->isChecked() );
  updateUseReturnKeyToSendMessageToolTip();
  ensureFocusInChat();
}

void GuiChat::updateSpellCheckerToolTip()
{
  QString tool_tip = "";
  if( Settings::instance().useSpellChecker() )
    tool_tip = tr( "Spell checking is enabled" );
  else
    tool_tip = tr( "Spell checking is disabled" );

#ifdef BEEBEEP_USE_HUNSPELL
  if( !SpellChecker::instance().isValid() )
  {
    tool_tip.append( QString( " (%1)" ).arg( tr( "There is not a valid dictionary" ) ) );
    mp_actSpellChecker->setEnabled( false );
  }
  else
    mp_actSpellChecker->setEnabled( true );
#else
  mp_actSpellChecker->setEnabled( false );
#endif

  mp_actSpellChecker->setToolTip( tool_tip );
}

void GuiChat::updateCompleterToolTip()
{
  QString tool_tip = "";

  if( Settings::instance().useWordCompleter() )
    tool_tip = tr( "Word completer is enabled" );
  else
    tool_tip = tr( "Word completer is disabled" );

#ifdef BEEBEEP_USE_HUNSPELL
  if( !SpellChecker::instance().isValid() )
  {
    tool_tip.append( QString( " (%1)" ).arg( tr( "There is not a valid dictionary" ) ) );
    mp_actCompleter->setEnabled( false );
  }
  else
    mp_actCompleter->setEnabled( true );
#else
  mp_actCompleter->setEnabled( false );
#endif

  mp_actCompleter->setToolTip( tool_tip );
}

void GuiChat::onSpellCheckerActionClicked()
{
#ifdef BEEBEEP_USE_HUNSPELL
  if( SpellChecker::instance().isValid() )
    Settings::instance().setUseSpellChecker( mp_actSpellChecker->isChecked() );
  else
    Settings::instance().setUseSpellChecker( false );
#else
  Settings::instance().setUseSpellChecker( mp_actSpellChecker->isChecked() );
#endif
  updateSpellCheckerToolTip();
  mp_teMessage->rehighlightMessage();
  ensureFocusInChat();
}

void GuiChat::onCompleterActionClicked()
{
#ifdef BEEBEEP_USE_HUNSPELL
  if( SpellChecker::instance().isValid() )
    Settings::instance().setUseWordCompleter( mp_actCompleter->isChecked() );
  else
    Settings::instance().setUseWordCompleter( false );
#else
  Settings::instance().setUseWordCompleter( mp_actCompleter->isChecked() );
#endif
  updateCompleterToolTip();
  ensureFocusInChat();
}

void GuiChat::updateActionsOnFocusChanged()
{
  mp_actUseReturnToSendMessage->setChecked( Settings::instance().useReturnToSendMessage() );
  updateUseReturnKeyToSendMessageToolTip();
  mp_actSpellChecker->setChecked( Settings::instance().useSpellChecker() );
  updateSpellCheckerToolTip();
  mp_actCompleter->setChecked( Settings::instance().useWordCompleter() );
  updateCompleterToolTip();
}

void GuiChat::printChat()
{
  QPrinter printer( QPrinter::HighResolution );
  printer.setFullPage( true );
  QPrintDialog *dlg = new QPrintDialog( &printer, this );
  dlg->setOptions( QAbstractPrintDialog::PrintSelection | QAbstractPrintDialog::PrintPageRange |
                   QAbstractPrintDialog::PrintShowPageSize | QAbstractPrintDialog::PrintCollateCopies |
#if QT_VERSION >= 0x040700
                   QAbstractPrintDialog::PrintCurrentPage |
#endif
                   QAbstractPrintDialog::PrintToFile );

  if( dlg->exec() == QDialog::Accepted)
    mp_teChat->print( dlg->printer() );

  dlg->deleteLater();
}

void GuiChat::showFindTextInChatDialog()
{
  QString label = tr( "Find text in chat" );
  bool ok = false;
  QString text_to_search = QInputDialog::getText( this, Settings::instance().programName(), label,
                                                  QLineEdit::Normal, m_lastTextFound, &ok );
  if( ok && !text_to_search.isEmpty() )
  {
    if( !text_to_search.trimmed().isEmpty() )
      findTextInChat( text_to_search.trimmed() );
    else
      findTextInChat( text_to_search );
  }
}

void GuiChat::findNextTextInChat()
{
  findTextInChat( m_lastTextFound );
}

void GuiChat::findPreviousTextInChat()
{
  findTextInChat( m_lastTextFound, false );
}


void GuiChat::findTextInChat( const QString& txt, bool search_forward )
{
  if( txt.isEmpty() )
    return;

  QTextDocument::FindFlags find_flags;
  if( !search_forward )
    find_flags = QTextDocument::FindBackward;

  bool search_from_start = false;
  if( txt != m_lastTextFound )
  {
    mp_teChat->moveCursor( QTextCursor::Start );
    search_from_start = true;
  }

  if( !mp_teChat->find( txt, find_flags ) )
  {
    if( !search_from_start )
    {
      mp_teChat->moveCursor( QTextCursor::Start );
      if( mp_teChat->find( txt, find_flags ) )
      {
        m_lastTextFound = txt;
        return;
      }
    }

    QMessageBox::information( this, Settings::instance().programName(), tr( "%1 not found in chat." ).arg( QString( "\"%1\"" ).arg( txt ) ) );
  }
  else
    m_lastTextFound = txt;
}

void GuiChat::openSelectedTextAsUrl()
{
  QString selected_text = mp_teChat->textCursor().selectedText();
  if( !selected_text.isEmpty() )
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "Try to open selected text as url:" << selected_text;
#endif
    QUrl url = QUrl::fromUserInput( selected_text );
    emit openUrl( url, m_chatId );
  }
}

void GuiChat::quoteSelectedText()
{
  QTextCursor tc = mp_teChat->textCursor();
  if( !tc.hasSelection() )
    return;
  QString selected_text = tc.selection().toHtml( "UTF-8" );
  selected_text.replace( "</p>", "</p>\n" );
  selected_text.replace( "<br>", "\n" );
  selected_text = Bee::removeHtmlTags( selected_text );
  selected_text.replace( "\n", "&nbsp;&nbsp;<br>&nbsp;&nbsp;" );
  QString text_to_add = QString( " [quote]%1 [/quote]<br>" ).arg( selected_text.trimmed() );
  mp_teMessage->addText( text_to_add );
  mp_teMessage->setFocus();
}

void GuiChat::changeBackgroundColorInSelectedText()
{
// FIXME: TODO (to save also in chat history and messages)

  QTextCursor tc = mp_teChat->textCursor();
  if( !tc.hasSelection() )
    return;

  QString selected_text = tc.selection().toHtml( "UTF-8" );
  selected_text.replace( "</p>", "</p>\n" );
  selected_text.replace( "<br>", "\n" );
  selected_text = Bee::removeHtmlTags( selected_text );
  selected_text.replace( "\n", "<br>" );
  int pos = tc.selectionStart();
  mp_teChat->textCursor().removeSelectedText();
  tc.setPosition( pos );
  mp_teChat->setTextCursor( tc );
  selected_text.prepend( "<span style=\"background-color:'#000'; color:'#fff'\">" );
  selected_text.append( "</span>" );
  mp_teChat->textCursor().insertHtml( selected_text );
}

void GuiChat::resetChatFontToDefault()
{
  if( QMessageBox::question( this, Settings::instance().programName(), tr( "Do you want to restore the default font?" ), tr( "Yes" ), tr( "No" ), QString(), 0, 1 ) == 1 )
    return;
  Settings::instance().setChatFont( QApplication::font(), false );
  setChatFont( Settings::instance().chatFont() );
}

void GuiChat::operationCompleted()
{
  emit showStatusMessageRequest( tr( "Ready." ), -1 );
}

void GuiChat::clearSystemMessages()
{
  emit clearSystemMessagesRequestFromChat( m_chatId );
}

#ifdef BEEBEEP_USE_SHAREDESKTOP
void GuiChat::shareDesktopToChat()
{
  emit shareDesktopToChatRequest( m_chatId, mp_actShareDesktop->isChecked() );
}

void GuiChat::sendScreenshotToChat()
{
  int answer_id = QMessageBox::question( this, Settings::instance().programName(), tr( "Do you want to send a screenshot?" ), tr( "Yes and hide this chat" ), tr( "Yes" ), tr( "No" ), 0, 2 );
  if( answer_id == 2 )
    return;

  mp_actScreenshot->setEnabled( false );
  if( answer_id == 0 )
    emit hideRequest();
  QTimer::singleShot( 1000, this, SLOT( sendScreenshotToChat_Private() ) );
}

void GuiChat::sendScreenshotToChat_Private()
{
  // To avoid chat window captured
  QTimer::singleShot( 3000, this, SLOT( enableScreenshotAction() ) );
  emit screenshotToChatRequest( m_chatId );
}

void GuiChat::enableScreenshotAction()
{
  mp_actScreenshot->setEnabled( beeCore->isConnected() );
}

void GuiChat::onTickEvent( int ticks )
{
  if( mp_actShareDesktop->isChecked() )
  {
    if( ticks % 2 == 0 )
      mp_actShareDesktop->setIcon( IconManager::instance().icon( "desktop-share.png" ) );
    else
      mp_actShareDesktop->setIcon( IconManager::instance().icon( "desktop-share-alternate.png" ) );
  }
}
#else
void GuiChat::onTickEvent( int )
{}
#endif

void GuiChat::copyUrlToClipboard()
{
  QAction* act = qobject_cast<QAction*>( sender() );
  if( act )
  {
    QString s_url = act->data().toString();
    if( !s_url.isEmpty() )
    {
      QClipboard* cb = qApp->clipboard();
      if( cb )
      {
        cb->setText( s_url);
        emit showStatusMessageRequest( tr( "Url copied to clipboard" ), 3000 );
      }
    }
  }
}

void GuiChat::sendTextCode()
{
  if( Settings::instance().disableSendMessage() )
    return;

  GuiCreateTextFile gct( this );
  gct.show();
  gct.setModal( true );
  gct.setSizeGripEnabled( true );
  if( gct.exec() == QDialog::Accepted )
  {
    QString text_code = gct.text().trimmed();
    if( text_code.isEmpty() )
      return;
    text_code.append( '\n' );
    if( gct.sendAsFile() )
    {
      QString folder_path = Settings::instance().createTextCodeAsTemporaryFile() ? Settings::instance().cacheFolder() : Settings::instance().downloadDirectory();
      QString file_initial_path = folder_path +
                                  QString( "/beecode-%1." ).arg( Bee::dateTimeStringSuffix( QDateTime::currentDateTime() ) ) +
                                  Settings::instance().createTextCodeFileSuffix();
      QString file_path = Bee::uniqueFilePath( file_initial_path, false );
      QFile f( file_path );
      if( f.open( QIODevice::WriteOnly ) )
      {
        QTextStream ts( &f );
        ts << text_code << endl;
        ts.flush();
        f.close();
        if( Settings::instance().createTextCodeAsTemporaryFile() )
          Settings::instance().addTemporaryFilePath( file_path );
        ensureFocusInChat();
        emit sendFileFromChatRequest( m_chatId, file_path );
      }
      else
      {
        QMessageBox::warning( this, Settings::instance().programName(), tr( "Unable to save temporary file: %1" ).arg( file_path ) );
        ensureFocusInChat();
        return;
      }
    }
    else
    {
      ensureFocusInChat();
      emit newMessage( m_chatId, text_code, true );
    }
  }
  else
  {
    ensureFocusInChat();
  }
}
