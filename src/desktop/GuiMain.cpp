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
// $Id: GuiMain.cpp 1546 2022-09-29 10:45:33Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "AudioManager.h"
#include "Core.h"
#include "BeeApplication.h"
#include "BeeUtils.h"
#include "ChatManager.h"
#include "EmoticonManager.h"
#include "FileDialog.h"
#include "FileShare.h"
#include "GuiAddUser.h"
#include "GuiAskPassword.h"
#include "GuiChat.h"
#include "GuiChatList.h"
#include "GuiCreateGroup.h"
#include "GuiCreateMessage.h"
#include "GuiConfig.h"
#include "GuiEditVCard.h"
#include "GuiFileSharing.h"
#include "GuiFloatingChat.h"
#include "GuiGroupList.h"
#include "GuiHome.h"
#include "GuiImageBatchPreview.h"
#include "GuiLanguage.h"
#include "GuiLog.h"
#include "GuiNetworkTest.h"
#include "GuiPluginManager.h"
#include "GuiRefusedChat.h"
#include "GuiSavedChat.h"
#include "GuiSavedChatList.h"
#include "GuiScreenShot.h"
#include "GuiNetwork.h"
#include "GuiShareBox.h"
#ifdef BEEBEEP_USE_SHAREDESKTOP
  #include "GuiShareDesktop.h"
  #include "ImageOptimizer.h"
#endif
#include "GuiShareLocal.h"
#include "GuiShareNetwork.h"
#include "GuiShortcut.h"
#include "GuiSystemTray.h"
#include "GuiFileTransfer.h"
#include "GuiUserList.h"
#include "GuiMain.h"
#include "GuiVCard.h"
#ifdef BEEBEEP_USE_WEBENGINE
  #include "GuiWebView.h"
#endif
#include "GuiWizard.h"
#include "GuiWorkgroups.h"
#include "IconManager.h"
#include "NetworkManager.h"
#include "PluginManager.h"
#include "Protocol.h"
#include "MessageManager.h"
#include "Settings.h"
#include "ShortcutManager.h"
#include "SpellChecker.h"
#include "ShadowStyle.h"
#include "ShadowDialogStyle.h"
#include "ShadowWorkspaceArchive.h"
#include "ShadowLocalAccounts.h"
#include "Updater.h"
#include "ShadowUpdateGate.h"
#include "ShadowMention.h"
#include "ShadowConversationPreferences.h"
#include "UserManager.h"
#ifdef Q_OS_WIN
  #include <qt_windows.h>
#endif
#ifdef BEEBEEP_USE_VOICE_CHAT
  #include "VoicePlayer.h"
  #include "GuiRecordVoiceMessageSettings.h"
#endif
#ifdef Q_OS_WIN
  #include <Windows.h>
#endif


GuiMain::GuiMain( QWidget *parent )
 : QMainWindow( parent ), m_floatingChats()
{
  setObjectName( "GuiMainWindow" );
  setWindowIcon( QIcon( QStringLiteral( ":/images/shadowchat/shadowchat-mark-source.png" ) ) );

  // Create a status bar before the actions and the menu
  (void) statusBar();

  mp_tabMain = new QTabWidget( this );
  mp_tabMain->setObjectName( "GuiTabMain" );
  mp_tabMain->setTabPosition( QTabWidget::South );
  setCentralWidget( mp_tabMain );

  mp_fileSharing = Q_NULLPTR;
  mp_screenShot = Q_NULLPTR;
  mp_log = Q_NULLPTR;
  mp_networkTest = Q_NULLPTR;
  mp_modernRoot = Q_NULLPTR;
  mp_modernTitleBar = Q_NULLPTR;
  mp_modernNavigationRail = Q_NULLPTR;
  mp_modernListPanel = Q_NULLPTR;
  mp_modernConversationPanel = Q_NULLPTR;
  mp_modernDirectoryTabs = Q_NULLPTR;
  mp_modernChatPlaceholder = Q_NULLPTR;
  mp_modernChatPage = Q_NULLPTR;
  mp_modernChatBody = Q_NULLPTR;
  mp_modernListStack = Q_NULLPTR;
  mp_modernConversationStack = Q_NULLPTR;
  mp_modernContentSplitter = Q_NULLPTR;
  mp_modernSearch = Q_NULLPTR;
  mp_modernSearchResults = Q_NULLPTR;
  mp_modernNetworkStrip = Q_NULLPTR;
  mp_modernNetworkDot = Q_NULLPTR;
  mp_modernNetworkText = Q_NULLPTR;
  mp_modernNetworkAction = Q_NULLPTR;
  mp_modernEmptyState = Q_NULLPTR;
  mp_modernEmptyTitle = Q_NULLPTR;
  mp_modernEmptyHint = Q_NULLPTR;
  mp_modernEmptyAction = Q_NULLPTR;
  mp_modernSectionTitle = Q_NULLPTR;
  mp_modernSectionHint = Q_NULLPTR;
  mp_modernChatTitle = Q_NULLPTR;
  mp_modernChatSubtitle = Q_NULLPTR;
  mp_modernMessagesButton = Q_NULLPTR;
  mp_modernContactsButton = Q_NULLPTR;
  mp_modernPeopleButton = Q_NULLPTR;
  mp_modernGroupsButton = Q_NULLPTR;
  mp_modernProfileButton = Q_NULLPTR;
  mp_modernMoreButton = Q_NULLPTR;
  mp_modernBackButton = Q_NULLPTR;
  mp_modernMinimizeButton = Q_NULLPTR;
  mp_modernMaximizeButton = Q_NULLPTR;
  mp_modernCloseButton = Q_NULLPTR;
  mp_modernChatBodyLayout = Q_NULLPTR;
  mp_embeddedChat = Q_NULLPTR;
  m_modernPrimaryPage = 0;
  m_unreadActivities = 0;

  mp_barMain = addToolBar( tr( "Show the main tool bar" ) );
  mp_barMain->setObjectName( "GuiMainToolBar" );
  mp_barMain->setIconSize( Settings::instance().mainBarIconSize() );
  mp_barMain->toggleViewAction()->setVisible( false );

  mp_trayIcon = new GuiSystemTray( this );

  m_lastUserStatus = User::Online;
  m_forceShutdown = false;
  m_userSelectOffline = false;
  m_prevActivatedState = true;
  m_coreIsConnecting = false;
  m_changeTabToUserListOnFirstConnected = false;

  m_useFusionStyle = false;

  createActions();
  createMainWidgets();
  createMenus();
  createToolAndMenuBars();
  createModernShell();
  updadePluginMenu();

  connect( mp_tabMain, SIGNAL( currentChanged( int ) ), this, SLOT( onMainTabChanged( int ) ) );

  connect( beeCore, SIGNAL( connected() ), this, SLOT( onCoreConnected() ) );
  connect( beeCore, SIGNAL( disconnected() ), this, SLOT( onCoreDisconnected() ) );
  connect( beeCore, SIGNAL( newChatMessage( const Chat&, const ChatMessage& ) ), this, SLOT( onNewChatMessage( const Chat&, const ChatMessage& ) ) );
  connect( beeCore, SIGNAL( fileDownloadRequest( const User&, const FileInfo& ) ), this, SLOT( downloadFile( const User&, const FileInfo& ) ) );
  connect( beeCore, SIGNAL( folderDownloadRequest( const User&, const QString&, const QList<FileInfo>& ) ), this, SLOT( downloadFolder( const User&, const QString&, const QList<FileInfo>& ) ) );
  connect( beeCore, SIGNAL( userChanged( const User& ) ), this, SLOT( onUserChanged( const User& ) ) );
  connect( beeCore, SIGNAL( userRemoved( const User& ) ), this, SLOT( onUserRemoved( const User& ) ) );
  connect( beeCore, SIGNAL( userIsWriting( const User&, VNumber ) ), this, SLOT( showWritingUser( const User&, VNumber ) ) );
  connect( beeCore, SIGNAL( fileTransferProgress( VNumber, const User&, const FileInfo&, FileSizeType, qint64 ) ), this, SLOT( onFileTransferProgress( VNumber, const User&, const FileInfo&, FileSizeType, qint64 ) ) );
  connect( beeCore, SIGNAL( fileTransferMessage( VNumber, const User&, const FileInfo&, const QString&, FileTransferPeer::TransferState ) ), this, SLOT( onFileTransferMessage( VNumber, const User&, const FileInfo&, const QString&, FileTransferPeer::TransferState ) ) );
  connect( beeCore, SIGNAL( fileShareAvailable( const User& ) ), this, SLOT( showSharesForUser( const User& ) ) );
  connect( beeCore, SIGNAL( chatChanged( const Chat& ) ), this, SLOT( onChatChanged( const Chat& ) ) );
  connect( beeCore, SIGNAL( chatRemoved( const Chat& ) ), this, SLOT( onChatRemoved( const Chat& ) ) );
  connect( beeCore, SIGNAL( savedChatListAvailable() ), this, SLOT( loadSavedChatsCompleted() ) );
  connect( beeCore, SIGNAL( userConnectionStatusChanged( const User& ) ), this, SLOT( showConnectionStatusChanged( const User& ) ) );
  connect( beeCore, SIGNAL( networkInterfaceIsDown() ), this, SLOT( onNetworkInterfaceDown() ) );
  connect( beeCore, SIGNAL( networkInterfaceIsUp() ), this, SLOT( onNetworkInterfaceUp() ) );
  connect( beeCore, SIGNAL( chatReadByUser( const Chat&, const User& ) ), this, SLOT( onChatReadByUser( const Chat&, const User& ) ) );
  connect( beeCore, SIGNAL( localUserIsBuzzedBy( const User&, VNumber ) ), this, SLOT( showBuzzFromUser( const User&, VNumber ) ) );
  connect( beeCore, SIGNAL( helpRequestFrom( const User&, VNumber ) ), this, SLOT( showHelpRequestFromUser( const User&, VNumber ) ) );
  connect( beeCore, SIGNAL( helpAnswerFrom( const User&, VNumber ) ), this, SLOT( showHelpAnswerFromUser( const User&, VNumber ) ) );
  connect( beeCore, SIGNAL( newSystemStatusMessage( const QString&, int ) ), this, SLOT( showMessage( const QString&, int ) ) );
  connect( beeCore, SIGNAL( newsAvailable( const QString& ) ), this, SLOT( onNewsAvailable( const QString& ) ) );
  connect( beeCore, SIGNAL( offlineMessageSentToUser( const User& ) ), this, SLOT( updateUser( const User& ) ) );
#ifdef BEEBEEP_USE_SHAREDESKTOP
  connect( beeCore, SIGNAL( shareDesktopImageAvailable( const User&, const QImage&, const QString&, QRgb ) ), this, SLOT( onShareDesktopImageAvailable( const User&, const QImage&, const QString&, QRgb ) ) );
  connect( beeCore, SIGNAL( shareDesktopUpdate( const User& ) ), this, SLOT( onShareDesktopUpdate( const User& ) ) );
#endif
#ifdef BEEBEEP_USE_MULTICAST_DNS
  connect( beeCore, SIGNAL( multicastDnsChanged() ), this, SLOT( showDefaultServerPortInMenu() ) );
#endif
#ifdef BEEBEEP_USE_VOICE_CHAT
  connect( beeCore->voicePlayer(), SIGNAL( openWithExternalPlayer( const QUrl&, VNumber ) ), this, SLOT( openUrlFromChat( const QUrl&, VNumber ) ) );
#endif

  connect( mp_fileTransfer, SIGNAL( transferCanceled( VNumber ) ), beeCore, SLOT( cancelFileTransfer( VNumber ) ) );
  connect( mp_fileTransfer, SIGNAL( transferPaused( VNumber ) ), beeCore, SLOT( pauseFileTransfer( VNumber ) ) );
  connect( mp_fileTransfer, SIGNAL( openFileCompleted( const QUrl& ) ), this, SLOT( openUrl( const QUrl& ) ) );
  connect( mp_fileTransfer, SIGNAL( resumeTransfer( VNumber, const FileInfo& ) ), this, SLOT( resumeFileTransfer( VNumber, const FileInfo& ) ) );

  connect( mp_userList, SIGNAL( chatSelected( VNumber ) ), this, SLOT( showChat( VNumber ) ) );
  connect( mp_userList, SIGNAL( userSelected( VNumber ) ), this, SLOT( checkUserSelected( VNumber ) ) );
  connect( mp_userList, SIGNAL( showVCardRequest( VNumber ) ), this, SLOT( showVCard( VNumber ) ) );
  connect( mp_userList, SIGNAL( sendFilesToChatRequest( VNumber, const QStringList& ) ), this, SLOT( sendFilesFromChat( VNumber, const QStringList& ) ) );

  connect( mp_groupList, SIGNAL( openChatForGroupRequest( VNumber ) ), this, SLOT( showChat( VNumber ) ) );
  connect( mp_groupList, SIGNAL( createGroupRequest() ), this, SLOT( createGroupChat() ) );
  connect( mp_groupList, SIGNAL( editGroupRequest( VNumber ) ), this, SLOT( editGroupChat( VNumber ) ) );
  connect( mp_groupList, SIGNAL( showVCardRequest( VNumber ) ), this, SLOT( showVCard( VNumber ) ) );

  connect( mp_chatList, SIGNAL( chatSelected( VNumber ) ), this, SLOT( showChat( VNumber ) ) );
  connect( mp_chatList, SIGNAL( chatToClear( VNumber ) ), this, SLOT( clearChat( VNumber ) ) );
  connect( mp_chatList, SIGNAL( chatToEdit( VNumber ) ), this, SLOT( editGroupChat( VNumber ) ) );
  connect( mp_chatList, SIGNAL( createNewChatRequest() ), this, SLOT( createGroupChat() ) );
  connect( mp_chatList, SIGNAL( createNewMessageRequest() ), this, SLOT( createMessage() ) );
  connect( mp_chatList, SIGNAL( hideEmptyChatsRequest() ), this, SLOT( onHideEmptyChatsRequest() ) );
  connect( mp_chatList, SIGNAL( removeUnsentMessagesInChat( VNumber ) ), this, SLOT( removeUnsentMessageInChat( VNumber ) ) );

  connect( mp_trayIcon, SIGNAL( activated( QSystemTrayIcon::ActivationReason ) ), this, SLOT( trayIconClicked( QSystemTrayIcon::ActivationReason ) ) );
  connect( mp_trayIcon, SIGNAL( messageClicked() ), this, SLOT( trayMessageClicked() ) );

  connect( mp_savedChatList, SIGNAL( savedChatSelected( const QString& ) ), this, SLOT( showSavedChatSelected( const QString& ) ) );
  connect( mp_savedChatList, SIGNAL( savedChatRemoved( const QString& ) ), this, SLOT( removeSavedChat( const QString& ) ) );
  connect( mp_savedChatList, SIGNAL( savedChatLinkRequest( const QString& ) ), this, SLOT( linkSavedChat( const QString& ) ) );

  connect( statusBar(), SIGNAL( messageChanged( const QString& ) ), this, SLOT( onStatusBarMessageChanged( const QString& ) ) );

  initShortcuts();
  initGuiItems();
  updateShortcuts();
}

void GuiMain::initShortcuts()
{
  mp_scMinimizeAllChats = new QShortcut( this );
  mp_scMinimizeAllChats->setContext( Qt::ApplicationShortcut );
  connect( mp_scMinimizeAllChats, SIGNAL( activated() ), this, SLOT( minimizeAllChats() ) );
#ifdef BEEBEEP_USE_QXT
  mp_scShowAllChats = new QxtGlobalShortcut( this );
  connect( mp_scShowAllChats, SIGNAL( activated() ), this, SLOT( showAllChats() ) );
  mp_scSendHelpMessage = new QxtGlobalShortcut( this );
  connect( mp_scSendHelpMessage, SIGNAL( activated() ), this, SLOT( sendHelpMessage() ) );
  mp_scSelectFirstChat = new QxtGlobalShortcut( this );
  connect( mp_scSelectFirstChat, SIGNAL( activated() ), this, SLOT( selectFirstChat() ) );
#endif
  mp_scShowNextUnreadMessage = new QShortcut( this );
  mp_scShowNextUnreadMessage->setContext( Qt::ApplicationShortcut );
  connect( mp_scShowNextUnreadMessage, SIGNAL( activated() ), this, SLOT( showNextChat() ) );
}

void GuiMain::setupChatConnections( GuiChat* gui_chat )
{
  connect( gui_chat, &GuiChat::openSavedChatRequest, this, [this]( VNumber chat_id ) {
    MessageManager::instance().saveMessages( true );
    showSavedChatSelected( ChatManager::instance().chatName( chat_id ) );
  });
  connect( gui_chat, SIGNAL( newMessage( VNumber, const QString&, bool ) ), this, SLOT( sendMessage( VNumber, const QString&, bool ) ) );
  connect( gui_chat, SIGNAL( writing( VNumber ) ), beeCore, SLOT( sendWritingMessage( VNumber ) ) );
  connect( gui_chat, SIGNAL( nextChat() ), this, SLOT( showNextChat() ) );
  connect( gui_chat, SIGNAL( openUrl( const QUrl&, VNumber ) ), this, SLOT( openUrlFromChat( const QUrl&, VNumber ) ) );
  connect( gui_chat, SIGNAL( sendFileFromChatRequest( VNumber, const QString& ) ), this, SLOT( sendFileFromChat( VNumber, const QString& ) ) );
  connect( gui_chat, SIGNAL( sendFilesFromChatRequest( VNumber, const QStringList& ) ), this, SLOT( sendFilesFromChat( VNumber, const QStringList& ) ) );
  connect( gui_chat, SIGNAL( editGroupRequest( VNumber ) ), this, SLOT( editGroupChat( VNumber ) ) );
  connect( gui_chat, SIGNAL( chatToClear( VNumber ) ), this, SLOT( clearChat( VNumber ) ) );
  connect( gui_chat, SIGNAL( clearSystemMessagesRequestFromChat( VNumber ) ), this, SLOT( clearSystemMessagesInChat( VNumber ) ) );
  connect( gui_chat, SIGNAL( showChatMenuRequest() ), this, SLOT( showChatSettingsMenu() ) );
#ifdef BEEBEEP_USE_SHAREDESKTOP
  connect( gui_chat, SIGNAL( shareDesktopToChatRequest( VNumber, bool ) ), this, SLOT( onShareDesktopRequestFromChat( VNumber, bool ) ) );
  connect( gui_chat, SIGNAL( screenshotToChatRequest( VNumber ) ), this, SLOT( sendScreenshotToChat( VNumber ) ) );
#endif
}

void GuiMain::checkWindowFlagsAndShow()
{
  if( !mp_trayIcon->isVisible() )
  {
    mp_trayIcon->show();
#ifdef Q_OS_LINUX
    qApp->processEvents();
    mp_trayIcon->hide();
    qApp->processEvents();
    mp_trayIcon->show();
#endif
  }

  if( mp_tabMain->currentWidget() != mp_chatList )
    mp_tabMain->setCurrentWidget( mp_chatList );

  if( !Settings::instance().enableMaximizeButton() )
    setWindowFlags( windowFlags() & ~Qt::WindowMaximizeButtonHint );

  Bee::setWindowStaysOnTop( this, Settings::instance().stayOnTop() );

  if( !isVisible() )
    show();

  if( Settings::instance().resetGeometryAtStartup() || Settings::instance().guiGeometry().isEmpty() )
    resetGeometryAndState();
  else
    restoreGeometryAndState();

  checkViewActions();

#ifdef Q_OS_WIN
  beeApp->setSessionNotificationForWindow( reinterpret_cast<HWND>( winId() ) );
#endif

  if( Settings::instance().loadOnTrayAtStartup() && QSystemTrayIcon::isSystemTrayAvailable() )
  {
    QMetaObject::invokeMethod( this, "hideToTrayIcon", Qt::QueuedConnection );
    return;
  }

  if( Settings::instance().showMinimizedAtStartup() )
    QMetaObject::invokeMethod( this, "showMinimized", Qt::QueuedConnection );
}

void GuiMain::showUp()
{
  Bee::showUp( this );
}

void GuiMain::raiseOnTop()
{
  Bee::raiseOnTop( this );
}

void GuiMain::updateWindowTitle()
{
  setWindowTitle( QString( "%1 - %2" ).arg( Bee::userNameToShow( Settings::instance().localUser(), false ), Settings::instance().programName() ) );
}

static QString RemoveMenuStringFromTooltip( const QString& s )
{
  return s.isEmpty() ? QString( "" ) : s.split( QString( "\n" ), QString::KeepEmptyParts ).first();
}

void GuiMain::updateTabTitles()
{
  int tab_index = mp_tabMain->indexOf( mp_home );
  int current_value = m_unreadActivities;
  mp_tabMain->setTabText( tab_index, current_value > 0 ? QString::number( current_value ) : "" );
  if( current_value > 0 )
    mp_tabMain->setTabToolTip( tab_index, QString( "%1: %2 %3" ).arg( RemoveMenuStringFromTooltip( mp_home->mainToolTip() ) ).arg( current_value ).arg( tr( "news" ) ) );
  else
    mp_tabMain->setTabToolTip( tab_index, RemoveMenuStringFromTooltip( mp_home->mainToolTip() ) );

  tab_index = mp_tabMain->indexOf( mp_userList );
  current_value = beeCore->connectedUsers();
  int other_value = UserManager::instance().userList().size();
  mp_tabMain->setTabText( tab_index, current_value > 0 ? QString::number( current_value ) : (other_value > 0 ? QString::number( other_value ) : "" ) );
  if( current_value > 0 )
    mp_tabMain->setTabToolTip( tab_index, QString( "%1: %2 %3" ).arg( RemoveMenuStringFromTooltip( mp_userList->mainToolTip() ) ).arg( current_value ).arg( tr( "connected" ) ) );
  else
    mp_tabMain->setTabToolTip( tab_index, RemoveMenuStringFromTooltip( mp_userList->mainToolTip() ) );

  tab_index = mp_tabMain->indexOf( mp_chatList );
  current_value = ChatManager::instance().countNotEmptyChats( false );
  mp_tabMain->setTabText( tab_index, current_value > 0 ? QString::number( current_value ) : "" );

  tab_index = mp_tabMain->indexOf( mp_groupList );
  current_value = ChatManager::instance().countGroupChats();
  mp_tabMain->setTabText( tab_index, current_value > 0 ? QString::number( current_value ) : "" );

  tab_index = mp_tabMain->indexOf( mp_savedChatList );
  current_value = ChatManager::instance().constHistoryMap().size();
  mp_tabMain->setTabText( tab_index, current_value > 0 ? QString::number( current_value ) : "" );
}

void GuiMain::keyPressEvent( QKeyEvent* e )
{
  if( e->modifiers().testFlag( Qt::ControlModifier ) && e->key() == Qt::Key_K )
  {
    if( mp_modernSearch )
    {
      mp_modernSearch->setFocus();
      mp_modernSearch->selectAll();
    }
    e->accept();
    return;
  }

  if( e->modifiers().testFlag( Qt::ControlModifier ) && e->key() == Qt::Key_1 )
  {
    showModernMessages();
    e->accept();
    return;
  }

  if( e->modifiers().testFlag( Qt::ControlModifier ) && e->key() == Qt::Key_2 )
  {
    showModernContacts();
    e->accept();
    return;
  }

  if( e->key() == Qt::Key_Escape )
  {
    if( mp_modernSearch && !mp_modernSearch->text().isEmpty() )
    {
      mp_modernSearch->clear();
      e->accept();
      return;
    }
    if( width() < 900 && mp_modernConversationPanel && mp_modernConversationPanel->isVisible() )
    {
      showModernListPanel();
      e->accept();
      return;
    }
    if( Settings::instance().keyEscapeMinimizeInTray() )
      QTimer::singleShot( 0, this, SLOT( hideToTrayIcon() ) );
    else
      QTimer::singleShot( 0, this, SLOT( showMinimized() ) );
    e->accept();
    return;
  }

  QMainWindow::keyPressEvent( e );
}

void GuiMain::resizeEvent( QResizeEvent* e )
{
  QMainWindow::resizeEvent( e );
  updateModernResponsiveLayout();
}

void GuiMain::changeEvent( QEvent* e )
{
  QMainWindow::changeEvent( e );
  if( e->type() == QEvent::WindowStateChange && mp_modernMaximizeButton )
  {
    mp_modernMaximizeButton->setIcon( style()->standardIcon( isMaximized()
      ? QStyle::SP_TitleBarNormalButton : QStyle::SP_TitleBarMaxButton ) );
    mp_modernMaximizeButton->setToolTip( isMaximized() ? QStringLiteral( "还原" ) : QStringLiteral( "最大化" ) );
  }
}

bool GuiMain::nativeEvent( const QByteArray& event_type, void* message, long* result )
{
  Q_UNUSED( event_type );
#ifdef Q_OS_WIN
  MSG* native_message = static_cast<MSG*>( message );
  if( native_message && native_message->message == WM_NCHITTEST && mp_modernTitleBar )
  {
    const QPoint local_position = mapFromGlobal( QCursor::pos() );
    const int resize_border = 7;
    if( !isMaximized() )
    {
      const bool on_left = local_position.x() < resize_border;
      const bool on_right = local_position.x() >= width() - resize_border;
      const bool on_top = local_position.y() < resize_border;
      const bool on_bottom = local_position.y() >= height() - resize_border;
      if( on_top && on_left ) { *result = HTTOPLEFT; return true; }
      if( on_top && on_right ) { *result = HTTOPRIGHT; return true; }
      if( on_bottom && on_left ) { *result = HTBOTTOMLEFT; return true; }
      if( on_bottom && on_right ) { *result = HTBOTTOMRIGHT; return true; }
      if( on_left ) { *result = HTLEFT; return true; }
      if( on_right ) { *result = HTRIGHT; return true; }
      if( on_top ) { *result = HTTOP; return true; }
      if( on_bottom ) { *result = HTBOTTOM; return true; }
    }

    const QPoint title_position = mp_modernTitleBar->mapFrom( this, local_position );
    if( mp_modernTitleBar->rect().contains( title_position ) )
    {
      QWidget* title_child = mp_modernTitleBar->childAt( title_position );
      if( title_child != mp_modernMinimizeButton && title_child != mp_modernMaximizeButton &&
          title_child != mp_modernCloseButton )
      {
        *result = HTCAPTION;
        return true;
      }
    }
  }
#endif
  return QMainWindow::nativeEvent( event_type, message, result );
}

void GuiMain::closeEvent( QCloseEvent* e )
{
  if( !m_forceShutdown )
  {
    if( Settings::instance().closeMinimizeInTray() && QSystemTrayIcon::isSystemTrayAvailable() )
    {
      QTimer::singleShot( 0, this, SLOT( hideToTrayIcon() ) );
      e->ignore();
      return;
    }

    if( Settings::instance().promptOnCloseEvent() )
    {
      if( QMessageBox::question( this, Settings::instance().programName(), tr( "Do you want to quit %1?" ).arg( Settings::instance().programName() ),
                                 tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 1 )
      {
        e->ignore();
        return;
      }
    }
  }

  if( beeCore->isConnected() )
    beeCore->stop();

  if( Settings::instance().saveGeometryOnExit() )
#if QT_VERSION == 0x050906
  {
    if( isVisible() )
    {
      Settings::instance().setGuiGeometry( saveGeometry() );
      Settings::instance().save();
    }
  }
#else
    saveGeometryAndState();
#endif

  QSettings* sets = Settings::instance().objectSettings();
  sets->deleteLater();

  if( !m_forceShutdown )
  {
    if( !sets->isWritable() )
    {
      if( QMessageBox::warning( this, Settings::instance().programName(),
                              QString( "%1<br>%2<br>%3<br>%4<br>%5" ).arg( tr( "<b>Settings can not be saved</b>. Path:" ) )
                                                                     .arg( sets->fileName() )
                                                                     .arg( tr( "<b>is not writable</b> by user:" ) )
                                                                     .arg( Settings::instance().localUser().accountName() )
                                                                     .arg( tr( "Do you want to close anyway?" ) ),
                              tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 1 )
      {
        e->ignore();
        return;
      }
    }

    if( Settings::instance().chatAutoSave() )
    {
      if( !MessageManager::instance().chatMessageCanBeSaved() )
      {
        if( QMessageBox::warning( this, Settings::instance().programName(),
                              QString( "%1<br>%2<br>%3<br>%4<br>%5" ).arg( tr( "<b>Chat messages can not be saved</b>. Path:" ) )
                                                                     .arg( Settings::instance().savedChatsFilePath() )
                                                                     .arg( tr( "<b>is not writable</b> by user:" ) )
                                                                     .arg( Settings::instance().localUser().accountName() )
                                                                     .arg( tr( "Do you want to close anyway?" ) ),
                              tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 1 )
        {
          e->ignore();
          return;
        }
      }
    }
  }

  if( mp_fileSharing )
    mp_fileSharing->close();

  if( mp_screenShot )
    mp_screenShot->close();

  if( mp_networkTest )
    mp_networkTest->close();

#ifdef BEEBEEP_USE_WEBENGINE
  if( mp_webView )
    mp_webView->close();
#endif

#ifdef BEEBEEP_USE_VOICE_CHAT
  if( !beeCore->voicePlayer()->isStopped() )
    beeCore->voicePlayer()->stop();
#endif

  if( mp_log )
    mp_log->close();

  foreach( QWidget* w, qApp->allWidgets() )
  {
    GuiSavedChat* gsv = qobject_cast<GuiSavedChat*>( w );
    if( gsv )
      gsv->close();
  }

  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->close();

  if( mp_menuMain && mp_menuMain->isVisible() )
    mp_menuMain->close();
  if( mp_menuSettings && mp_menuSettings->isVisible() )
    mp_menuSettings->close();
  if( mp_menuInfo && mp_menuInfo->isVisible() )
    mp_menuInfo->close();
  if( mp_menuStatus && mp_menuStatus->isVisible() )
    mp_menuStatus->close();
  if( mp_menuUsersSettings && mp_menuUsersSettings->isVisible() )
    mp_menuUsersSettings->close();

  mp_trayIcon->hide();

  if( mp_dockFileTransfers->isFloating() && mp_dockFileTransfers->isVisible() )
    mp_dockFileTransfers->hide();

#ifdef Q_OS_WIN
  beeApp->resetSessionNotificationForWindow();
#endif
  // quit now on last window closed
  qApp->setQuitOnLastWindowClosed( true );

  e->accept();
}

void GuiMain::showNextChat()
{
#ifdef BEEBEEP_DEBUG
  qDebug() << "Show next chat in list with unread messages";
#endif
  Chat c = ChatManager::instance().firstChatWithUnreadMessages();
  if( c.isValid() )
  {
    showChat( c.id() );
    GuiFloatingChat* fl_chat = floatingChat( c.id() );
    if( fl_chat && !fl_chat->isActiveWindow() )
      fl_chat->setFocusInChat();
  }
  else
    showMessage( tr( "No new message available" ), 5000 );
}

void GuiMain::forceShutdown()
{
  qDebug() << "Shutdown...";
  m_forceShutdown = true;
  if( beeCore->isConnected() )
    beeCore->stop();
  close();
}

void GuiMain::onWakeUpRequest()
{
#ifdef BEEBEEP_DEBUG
  qDebug() << "Main window wakes up from sleep";
#endif
  initGuiItems();
}

void GuiMain::onSleepRequest()
{
#ifdef BEEBEEP_DEBUG
  qDebug() << "Main window goes to sleep";
#endif
  if( beeCore->isConnected() )
    stopCore();
}

void GuiMain::onCoreConnected()
{
  m_coreIsConnecting = false;
  initGuiItems();
  updateModernNetworkState();
  updateModernListState();
#ifdef BEEBEEP_USE_WEBENGINE
  QTimer::singleShot( 0, mp_webView, SLOT( loadNews() ) );
#endif
}

void GuiMain::onCoreDisconnected()
{
  if( m_coreIsConnecting )
    m_coreIsConnecting = false;
  initGuiItems();
  updateModernNetworkState();
  updateModernListState();
}

void GuiMain::startCore()
{
  if( beeCore->isConnected() || m_coreIsConnecting )
    return;

  mp_home->resetNews();
  m_coreIsConnecting = true;
  m_userSelectOffline = false;
  updateModernNetworkState();
  updateModernListState();

  if( Settings::instance().askChangeUserAtStartup() )
  {
    if( !showWizard() )
    {
      m_coreIsConnecting = false;
      return;
    }
  }

  if( Settings::instance().firstTime() )
  {
    Settings::instance().setFirstTime( false );
    Settings::instance().setAskChangeUserAtStartup( false );
  }

  if( Settings::instance().askPassword() )
  {
    if( !promptConnectionPassword() )
    {
      m_coreIsConnecting = false;
      return;
    }
  }

  if( beeCore->start() )
  {
    if( Settings::instance().showChatsOnConnection() )
    {
      mp_tabMain->setCurrentWidget( mp_chatList );
      m_changeTabToUserListOnFirstConnected = false;
    }
    else if( Settings::instance().showUsersOnConnection() )
    {
      mp_tabMain->setCurrentWidget( mp_userList );
      m_changeTabToUserListOnFirstConnected = false;
    }
    else
      m_changeTabToUserListOnFirstConnected = true;
  }
  else
  {
    m_coreIsConnecting = false;
    updateModernNetworkState();
    updateModernListState();
    QMetaObject::invokeMethod( beeCore, "checkNetworkInterface", Qt::QueuedConnection );
  }
}

bool GuiMain::promptConnectionPassword()
{
  GuiAskPassword gap( this );
  gap.setModal( true );
  gap.loadData();
  gap.show();
  gap.setFixedSize( gap.size() );
  if( gap.exec() == QDialog::Rejected )
    return false;
  mp_actPromptPassword->setChecked( Settings::instance().askPasswordAtStartup() );
  if( beeCore->isConnected() )
    showRestartConnectionAlertMessage();
  return true;
}

void GuiMain::disconnectFromNetwork()
{
  if( isFileTransferInProgress() )
    return;

  m_userSelectOffline = true;

  if( mp_tabMain->currentWidget() != mp_home )
    mp_tabMain->setCurrentWidget( mp_home );

#ifdef BEEBEEP_USE_SHAREDESKTOP
  foreach( GuiShareDesktop* gsd, m_desktops )
    gsd->close();
#endif

  beeCore->stop();
}

void GuiMain::stopCore()
{
  if( isFileTransferInProgress() )
    return;

  if( mp_tabMain->currentWidget() != mp_home )
    mp_tabMain->setCurrentWidget( mp_home );

#ifdef BEEBEEP_USE_SHAREDESKTOP
  foreach( GuiShareDesktop* gsd, m_desktops )
    gsd->close();
#endif

  beeCore->stop();
}

void GuiMain::restartCore()
{
  if( !beeCore->isConnected() && !m_coreIsConnecting )
  {
    startCore();
    return;
  }

  if( isFileTransferInProgress() )
    return;

#ifdef BEEBEEP_USE_SHAREDESKTOP
  foreach( GuiShareDesktop* gsd, m_desktops )
    gsd->close();
#endif

  mp_home->resetNews();
  m_coreIsConnecting = true;
  m_userSelectOffline = false;
  beeCore->restart();
}

bool GuiMain::isFileTransferInProgress()
{
  if( m_forceShutdown )
    return false;

  if( !NetworkManager::instance().isMainInterfaceUp() )
    return false;

  if( beeApp->isInSleepMode() )
    return false;

  if( beeCore->hasFileTransferInProgress() )
  {
    if( QMessageBox::warning( this, Settings::instance().programName(),
        tr( "There are still files that have not been transferred and will be interrupted. Do you want to disconnect anyway?" ),
        tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 1 )
      return true;
  }
  return false;
}

void GuiMain::initGuiItems()
{
  mp_userList->updateUsers();
  mp_chatList->updateChats();
  mp_groupList->updateGroups();
  mp_savedChatList->updateSavedChats();
  updateStatusIcon();
  updateNewMessageAction();
  checkViewActions();

#ifdef BEEBEEP_UX_PREVIEW
  QTimer::singleShot( 900, this, [this]() {
    QDir().mkpath( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review" ) );
    grab().save( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review/01-modern-shell.png" ) );
  } );
  QTimer::singleShot( 1200, this, [this]() {
    showChat( ID_DEFAULT_CHAT );
  } );
  QTimer::singleShot( 1900, this, [this]() {
#ifdef BEEBEEP_UX_PREVIEW
    if( mp_embeddedChat && mp_embeddedChat->guiChat() )
    {
      QStringList preview_images;
      preview_images << QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/src/images/shadowchat/shadowchat-mark-source.png" )
                     << QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/src/images/shadowchat/shadowchat-mark-source.png" );
      mp_embeddedChat->guiChat()->showUxPreviewConversation( preview_images );
    }
#endif
    grab().save( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review/04-chat-shell.png" ) );
  } );
  QTimer::singleShot( 2200, this, [this]() {
    showNormal();
    resize( 760, 620 );
  } );
  QTimer::singleShot( 2800, this, [this]() {
    grab().save( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review/05-compact-chat.png" ) );
  } );
  QTimer::singleShot( 3200, this, [this]() {
    QStringList preview_images;
    preview_images << QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/src/images/shadowchat/shadowchat-mark-source.png" )
                   << QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/src/images/shadowchat/shadowchat-mark-source.png" )
                   << QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/src/images/shadowchat/shadowchat-mark-source.png" )
                   << QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/src/images/shadowchat/shadowchat-mark-source.png" );
    GuiImageBatchPreview* preview = new GuiImageBatchPreview( preview_images, this );
    preview->setAttribute( Qt::WA_DeleteOnClose );
    preview->show();
    QTimer::singleShot( 650, preview, [preview]() {
      preview->grab().save( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review/06-image-batch-preview.png" ) );
      preview->close();
    } );
  } );
  QTimer::singleShot( 4300, this, [this]() {
    GuiWizard* wizard = new GuiWizard( this );
    wizard->setAttribute( Qt::WA_DeleteOnClose );
    wizard->loadSettings();
    wizard->show();
    wizard->raise();
    QTimer::singleShot( 650, wizard, [wizard]() {
      wizard->grab().save( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review/07-onboarding.png" ) );
      wizard->close();
    } );
  } );
  QTimer::singleShot( 5500, this, [this]() {
    QTimer::singleShot( 650, this, []() {
      QDialog* settings_dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
      if( settings_dialog )
      {
        settings_dialog->grab().save( QStringLiteral( "E:/ShadowChat/beebeep-code-r1557/.ux-review/08-settings.png" ) );
        settings_dialog->reject();
      }
    } );
    showShadowSettings();
  } );
#endif
}

void GuiMain::checkViewActions()
{
  bool is_connected = beeCore->isConnected();
  int connected_users = beeCore->connectedUsers();
  bool file_transfer_is_active = beeCore->isFileTransferActive();
  bool helper_found = beeCore->hasHelper();

  mp_actConnect->setEnabled( !m_coreIsConnecting && !is_connected );
  mp_actDisconnect->setEnabled( is_connected );
  mp_actBroadcast->setEnabled( is_connected );
  mp_actCreateMessage->setEnabled( is_connected && !Settings::instance().disableCreateMessage() );
  mp_actCreateGroupChat->setEnabled( UserManager::instance().userList().size() > 1 );
  mp_actViewFileTransfer->setEnabled( Settings::instance().enableFileTransfer() );
  mp_actViewFileSharing->setEnabled( file_transfer_is_active && Settings::instance().enableFileSharing() && !Settings::instance().disableFileSharing() );
  if( mp_menuFileTransferSettings->isEnabled() )
  {
    foreach( QAction* act, mp_menuFileTransferSettings->actions() )
    {
      if( act->data().toInt() == 12 )
        continue;
      act->setEnabled( Settings::instance().enableFileTransfer() );
    }
  }
  mp_actEnableFileSharing->setEnabled( Settings::instance().enableFileTransfer() && !Settings::instance().disableFileSharing() );

#ifdef BEEBEEP_USE_VOICE_CHAT
  mp_menuVoiceMessage->setEnabled( Settings::instance().enableFileTransfer() );
#endif

  showDefaultServerPortInMenu();

  if( !m_floatingChats.isEmpty() )
  {
    foreach( GuiFloatingChat* fl_chat, m_floatingChats )
      fl_chat->updateActions( is_connected, connected_users, file_transfer_is_active );
  }

  if( mp_fileSharing )
  {
    if( mp_actViewFileSharing->isEnabled() )
      mp_fileSharing->checkViewActions();
    else
      mp_fileSharing->close();
  }

  mp_actHelpRequest->setEnabled( is_connected && connected_users > 0 && helper_found );

  updateWindowTitle();
  updateTabTitles();
}

void GuiMain::showAbout()
{
#ifdef Q_OS_MAC
  QMessageBox::about( Q_NULLPTR, Settings::instance().programName(),
#else
  QMessageBox::about( this, Settings::instance().programName(),
#endif
                      QString( "<b>%1</b> - Free Office Messenger<br><br>%2<br>%3 %4 %5<br><br>%6 %7<br>%8<br>" )
                        .arg( Settings::instance().programName() )
                        .arg( Settings::instance().isDevelopmentVersion() ? tr( "Development version") : tr( "Version" ) )
                        .arg( Settings::instance().version( true, true, true ) )
                        .arg( tr( "for" ) )
                        .arg( Settings::instance().operatingSystem( true ) )
                        .arg( tr( "developed by" ) )
                        .arg( QString( "<a href='%1'>Marco Mastroddi</a>" ).arg( Settings::instance().developerWebSite() ) )
                        .arg( QString( "e-mail: <a href='mailto://marco.mastroddi@gmail.com'>marco.mastroddi@gmail.com</a><br>web: <a href='%1'>www.beebeep.net</a>" ).arg( Settings::instance().officialWebSite() ) )
                      );
}

void GuiMain::showLicense()
{
  QString license_txt = QLatin1String(
  "BeeBEEP Copyright (C) 2010-2021 Marco Mastroddi <br><br>"
  "BeeBEEP is free software: you can redistribute it and/or modify "
  "it under the terms of the GNU General Public License as published "
  "by the Free Software Foundation, either version 3 of the License "
  "or (at your option) any later version. <br><br>"
  "BeeBEEP is distributed in the hope that it will be useful, "
  "but WITHOUT ANY WARRANTY; without even the implied warranty "
  "of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. <br><br>"
  "See the GNU General Public License for more details. <br><br>" );
#ifdef Q_OS_MAC
  QMessageBox::about( Q_NULLPTR, Settings::instance().programName(), license_txt );
#else
  QMessageBox::about( this, Settings::instance().programName(), license_txt );
#endif
}

void GuiMain::showAboutQt()
{
#ifdef Q_OS_MAC
  QMessageBox::aboutQt( Q_NULLPTR, Settings::instance().programName() );
#else
  QMessageBox::aboutQt( this, Settings::instance().programName() );
#endif
}

void GuiMain::createActions()
{
  mp_actConnect = new QAction( IconManager::instance().icon( "connection.png" ), tr( "Connect" ), this );
  connect( mp_actConnect, SIGNAL( triggered() ), this, SLOT( startCore() ) );

  mp_actDisconnect = new QAction( IconManager::instance().icon( "disconnection.png" ), tr( "Disconnect" ), this );
  connect( mp_actDisconnect, SIGNAL( triggered() ), this, SLOT( disconnectFromNetwork() ) );

  mp_actBroadcast = new QAction( IconManager::instance().icon( "broadcast.png" ), tr( "Search users" ), this );
  connect( mp_actBroadcast, SIGNAL( triggered() ), this, SLOT( sendBroadcastMessage() ) );

  mp_actConfigureNetwork = new QAction( IconManager::instance().icon( "network.png"), tr( "Configure network..."), this );
  connect( mp_actConfigureNetwork, SIGNAL( triggered() ), this, SLOT( searchUsers() ) );

  mp_actQuit = new QAction( IconManager::instance().icon( "quit.png" ), tr( "Quit" ), this );
  mp_actQuit->setShortcuts( QKeySequence::Quit );
  mp_actQuit->setMenuRole( QAction::QuitRole );
  connect( mp_actQuit, SIGNAL( triggered() ), this, SLOT( forceShutdown() ) );

  mp_actVCard = new QAction( IconManager::instance().icon( "profile-edit.png"), tr( "Edit your profile..." ), this );
  connect( mp_actVCard, SIGNAL( triggered() ), this, SLOT( changeVCard() ) );

  mp_actAbout = new QAction( IconManager::instance().icon( "beebeep.png" ), tr( "About %1..." ).arg( Settings::instance().programName() ), this );
  mp_actAbout->setMenuRole( QAction::AboutRole );
  connect( mp_actAbout, SIGNAL( triggered() ), this, SLOT( showAbout() ) );

  mp_actCreateGroupChat = new QAction( IconManager::instance().icon( "group-create.png" ), tr( "Create new group chat" ), this );
  connect( mp_actCreateGroupChat, SIGNAL( triggered() ), this, SLOT( createGroupChat() ) );

  mp_actViewNewMessage = new QAction( IconManager::instance().icon( "beebeep-message.png" ), tr( "Show new message" ), this );
  connect( mp_actViewNewMessage, SIGNAL( triggered() ), this, SLOT( showNextChat() ) );

  mp_actViewFileSharing = new QAction( IconManager::instance().icon( "file-sharing.png" ), tr( "Show file sharing window" ), this );
  connect( mp_actViewFileSharing, SIGNAL( triggered() ), this, SLOT( showFileSharingWindow() ) );

  mp_actViewLog = new QAction( IconManager::instance().icon( "log.png" ), tr( "Show the %1 log" ).arg( Settings::instance().programName() )+QString("..."), this );
  connect( mp_actViewLog, SIGNAL( triggered() ), this, SLOT( showLogWindow() ) );

  mp_actViewScreenShot = new QAction( IconManager::instance().icon( "screenshot.png" ), tr( "Make a screenshot" ), this );
  connect( mp_actViewScreenShot, SIGNAL( triggered() ), this, SLOT( showScreenShotWindow() ) );

  mp_actCreateMessage = new QAction( IconManager::instance().icon( "message-create.png" ), tr( "Write a message" ), this );
  connect( mp_actCreateMessage, SIGNAL( triggered() ), this, SLOT( createMessage() ) );
  if( Settings::instance().disableCreateMessage() )
    mp_actCreateMessage->setToolTip( tr( "The option has been disabled by your system administrator.") );

  mp_actHelpRequest = new QAction( IconManager::instance().icon( "help.png" ), tr( "Ask for help" ), this );
  connect( mp_actHelpRequest, SIGNAL( triggered() ), this, SLOT( sendHelpMessage() ) );

#ifdef BEEBEEP_USE_WEBENGINE
  mp_actWebView = new QAction( IconManager::instance().icon( "network.png" ), tr( "News" ), this );
  connect( mp_actWebView, SIGNAL( triggered() ), this, SLOT( showWebView() ) );
  mp_actWebView->setDisabled( true );
#endif
}

void GuiMain::createMenus()
{
  /* Plugins Menu */
  mp_menuPlugins = new QMenu( tr( "Plugins" ) + QString( "..." ), this );
  mp_menuPlugins->setIcon( IconManager::instance().icon( "plugin.png" ) );

  /* Main Menu */
  mp_menuMain = new QMenu( tr( "Main" ), this );
  mp_menuMain->addAction( mp_actConnect );
  mp_menuMain->addAction( mp_actDisconnect );
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( mp_actBroadcast );
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( mp_actHelpRequest );
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( mp_actVCard );
  mp_menuMain->addSeparator();
  if( Settings::instance().resourceFolder() != Settings::instance().dataFolder() )
    mp_menuMain->addAction( IconManager::instance().icon( "resource-folder.png" ), tr( "Open your resource folder" )+QString("..."), this, SLOT( openResourceFolder() ) );
  mp_menuMain->addAction( IconManager::instance().icon( "data-folder.png" ), tr( "Open your data folder" )+QString("..."), this, SLOT( openDataFolder() ) );
  mp_menuMain->addAction( IconManager::instance().icon( "download-folder.png" ), tr( "Open your download folder" )+QString("..."), this, SLOT( openDownloadFolder() ) );
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( IconManager::instance().icon( "network-test.png" ), tr( "Test your network" )+QString("..."), this, SLOT( showNetworkTest() ) );
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( mp_actViewLog );
#ifdef BEEBEEP_USE_WEBENGINE
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( mp_actWebView );
#endif
  mp_menuMain->addSeparator();
  mp_menuMain->addMenu( mp_menuPlugins );
  mp_menuMain->addSeparator();
  mp_menuMain->addAction( mp_actQuit );

  QAction* act;
  /* Chat Menu */
  mp_menuChat = new QMenu( tr( "Chat" ), this );

  /* System Menu */
  mp_menuSettings = new QMenu( tr( "Settings" ), this );
  mp_menuInterfaceSettings = new QMenu( tr( "Interface" ), this );
  mp_menuInterfaceSettings->setIcon( IconManager::instance().icon( "interface.png" ) );
  mp_menuSettings->addMenu( mp_menuInterfaceSettings );
  act = mp_menuInterfaceSettings->addAction( tr( "Use the dark theme" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().useDarkStyle() );
  act->setData( 77 );
#if QT_VERSION < 0x050000
  act->setEnabled( false );
#endif
  act = mp_menuInterfaceSettings->addAction( tr( "Reset minimum width for applied style" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().resetMinimumWidthForStyle() );
  act->setData( 103 );
  mp_menuInterfaceSettings->addSeparator();
  act = mp_menuInterfaceSettings->addAction( tr( "Escape key minimize to tray icon" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().keyEscapeMinimizeInTray() );
  act->setData( 29 );
  act = mp_menuInterfaceSettings->addAction( tr( "Close button minimize to tray icon" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().closeMinimizeInTray() );
  act->setData( 11 );
#ifdef Q_OS_MAC
  // Close button on MacOSX must quit the app
  act->setDisabled( true );
#endif
  act = mp_menuInterfaceSettings->addAction( tr( "Enable maximize button" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().enableMaximizeButton() );
  act->setData( 28 );
  mp_menuInterfaceSettings->addSeparator();
  mp_menuInterfaceSettings->addAction( IconManager::instance().icon( "shortcut.png" ), tr( "Shortcuts" ) + QString( "..." ), this, SLOT( editShortcuts() ) );
  mp_menuInterfaceSettings->addAction( IconManager::instance().icon( "language.png" ), tr( "Select language" ) + QString( "..." ), this, SLOT( selectLanguage() ) );
  mp_menuInterfaceSettings->addAction( IconManager::instance().icon( "theme.png" ), tr( "Select icon theme" ) + QString( "..." ), this, SLOT( selectIconSourcePath() ) );
  mp_menuInterfaceSettings->addSeparator();
  mp_menuInterfaceSettings->addAction( IconManager::instance().icon( "update.png" ), tr( "Restore the colors to the default ones" ), this, SLOT( resetAllColors() ) );
  mp_menuInterfaceSettings->addAction( IconManager::instance().icon( "reset-window.png" ), tr( "Reset geometry of all windows" ), this, SLOT( askResetGeometryAndState() ) );

  mp_menuStartupSettings = new QMenu( tr( "On start" ), this );
  mp_menuStartupSettings->setIcon( IconManager::instance().icon( "settings-start.png" ) );
  mp_menuSettings->addMenu( mp_menuStartupSettings );
  act = mp_menuStartupSettings->addAction( tr( "Show minimized" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showMinimizedAtStartup() );
  act->setData( 35 );
  act = mp_menuStartupSettings->addAction( tr( "Show only on system tray" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().loadOnTrayAtStartup() );
  act->setData( 24 );
  act = mp_menuStartupSettings->addAction( tr( "Reset window geometry" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().resetGeometryAtStartup() );
  act->setData( 26 );
  act = mp_menuStartupSettings->addAction( tr( "Check for new version" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( true );
  act->setEnabled( false );
  act->setToolTip( QStringLiteral( "启动时及运行中每 5 分钟必须校验版本，无法关闭。" ) );
  act->setData( 43 );
  mp_menuStartupSettings->addSeparator();
  act = mp_menuStartupSettings->addAction( IconManager::instance().icon( "timer.png" ), tr( "Delay first connection" ) + QString( "..." ), this, SLOT( settingsChanged() ) );
  act->setData( 65 );

  mp_menuCloseSettings = new QMenu( tr( "On close" ), this );
  mp_menuCloseSettings->setIcon( IconManager::instance().icon( "settings-close.png" ) );
  mp_menuSettings->addMenu( mp_menuCloseSettings );
  act = mp_menuCloseSettings->addAction( tr( "Prompt on quit when connected" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().promptOnCloseEvent() );
  act->setData( 36 );
  act = mp_menuCloseSettings->addAction( tr( "Save window's geometry" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().saveGeometryOnExit() );
  act->setData( 68 );

  mp_menuConnectionSettings = new QMenu( tr( "On connection" ), this );
  mp_menuConnectionSettings->setIcon( IconManager::instance().icon( "connection.png" ) );
  mp_menuSettings->addMenu( mp_menuConnectionSettings );
  act = mp_menuConnectionSettings->addAction( tr( "Show the user list" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showUsersOnConnection() );
  act->setData( 69 );
  act = mp_menuConnectionSettings->addAction( tr( "Show the chat list" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showChatsOnConnection() );
  act->setData( 79 );
  mp_menuConnectionSettings->addSeparator();
  act = mp_menuConnectionSettings->addAction( tr( "Prompts to change user" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().askChangeUserAtStartup() );
  act->setData( 45 );
  mp_actPromptPassword = mp_menuConnectionSettings->addAction( tr( "Prompts to ask network password" ), this, SLOT( settingsChanged() ) );
  mp_actPromptPassword->setCheckable( true );
  mp_actPromptPassword->setChecked( Settings::instance().askPasswordAtStartup() );
  mp_actPromptPassword->setData( 17 );

  mp_menuNetworkStatus = new QMenu( tr( "Network" ), this );
  mp_menuNetworkStatus->setIcon( IconManager::instance().icon( "network.png" ) );
  mp_menuSettings->addMenu( mp_menuNetworkStatus );
  mp_menuNetworkStatus->addAction( mp_actConfigureNetwork );
  mp_menuNetworkStatus->addSeparator();
  mp_actEditWorkgroups = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "workgroup.png" ), tr( "Your workgroups" ) + QString( "..." ), this, SLOT( showWorkgroups() ) );
  mp_menuNetworkStatus->addSeparator();
  mp_actHostAddress = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "network.png" ), QString( "ip" ) );
  mp_actPortBroadcast = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "broadcast.png" ), QString( "udp1" ) );
  mp_actMulticastGroupAddress = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "multicast-group.png" ), QString( "multicast" ) );
  mp_actPortListener = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "default-chat-online.png" ), QString( "tcp1" ) );
  mp_actPortFileTransfer = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "network-scan.png" ), QString( "tcp2" ) );
#ifdef BEEBEEP_USE_MULTICAST_DNS
  mp_actMulticastDns = mp_menuNetworkStatus->addAction( IconManager::instance().icon( "mdns.png" ), QString( "mdns" ) );
#endif
  mp_menuNetworkStatus->addSeparator();
  mp_actEncryptedConnectionByDefault = mp_menuNetworkStatus->addAction( Settings::instance().disableConnectionSocketEncryption() ? IconManager::instance().icon( "encryption-disabled.png" ) : IconManager::instance().icon( "encryption-enabled.png" ), QString( "End-to-end encryption" ) );
  mp_actCompressedDataByDefault = mp_menuNetworkStatus->addAction( Settings::instance().disableConnectionSocketDataCompression() ? IconManager::instance().icon( "folder.png" ) : IconManager::instance().icon( "file-compressed.png" ), QString( "Data compression" ) );

  mp_menuUsersSettings = new QMenu( tr( "Users" ), this );
  mp_menuUsersSettings->setIcon( IconManager::instance().icon( "user-list.png" ) );
  mp_menuSettings->addMenu( mp_menuUsersSettings );

  QMenu* menu_recognize_users = mp_menuUsersSettings->addMenu( tr( "Recognize users" ) + QString( "..." ) );
  menu_recognize_users->setIcon( IconManager::instance().icon( "user-list.png" ) );
  mp_actGroupRecognizeUsers = new QActionGroup( this );
  mp_actGroupRecognizeUsers->setExclusive( true );
  act = menu_recognize_users->addAction( tr( "By nickname" ) + QString( " (%1)" ).arg( tr( "default" ) ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userRecognitionMethod() == Settings::RecognizeByNickname );
  act->setData( 59 );
  mp_actGroupRecognizeUsers->addAction( act );
  act = menu_recognize_users->addAction( tr( "By account name and domain name" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userRecognitionMethod() == Settings::RecognizeByAccountAndDomain );
  act->setData( 57 );
  mp_actGroupRecognizeUsers->addAction( act );
  act = menu_recognize_users->addAction( tr( "By account name" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userRecognitionMethod() == Settings::RecognizeByAccount );
  act->setData( 58 );
  mp_actGroupRecognizeUsers->addAction( act );
  menu_recognize_users->addSeparator();
  connect( mp_actGroupRecognizeUsers, SIGNAL( triggered( QAction* ) ), this, SLOT( settingsChanged( QAction* ) ) );
  mp_menuUsersSettings->addSeparator();
  mp_actAddUsers = mp_menuUsersSettings->addAction( IconManager::instance().icon( "user-add.png" ), tr( "Add users" ) + QString( "..." ), this, SLOT( showAddUser() ) );
  mp_menuUsersSettings->addSeparator();
  mp_actSaveUserList = mp_menuUsersSettings->addAction( tr( "Save users" ), this, SLOT( settingsChanged() ) );
  mp_actSaveUserList->setCheckable( true );
  mp_actSaveUserList->setChecked( Settings::instance().saveUserList() );
  mp_actSaveUserList->setData( 32 );
  mp_actSaveGroupList = mp_menuUsersSettings->addAction( tr( "Save groups" ), this, SLOT( settingsChanged() ) );
  mp_actSaveGroupList->setCheckable( true );
  mp_actSaveGroupList->setChecked( Settings::instance().saveGroupList() );
  mp_actSaveGroupList->setData( 2 );
  mp_menuUsersSettings->addSeparator();
  mp_actShowUserFullName = mp_menuUsersSettings->addAction( tr( "Show the name and surname of the users" ) , this, SLOT( settingsChanged() ) );
  mp_actShowUserFullName->setCheckable( true );
  mp_actShowUserFullName->setChecked( Settings::instance().useUserFullName() );
  mp_actShowUserFullName->setDisabled( true );
  mp_actShowUserFirstNameFirstInFullName = mp_menuUsersSettings->addAction( tr( "Show surname before firstname of the users" ) , this, SLOT( settingsChanged() ) );
  mp_actShowUserFirstNameFirstInFullName->setCheckable( true );
  mp_actShowUserFirstNameFirstInFullName->setChecked( !Settings::instance().useUserFirstNameFirstInFullName() );
  mp_actShowUserFirstNameFirstInFullName->setData( 102 );
  mp_menuUsersSettings->addSeparator();
  mp_actRemoveInactiveUsers = mp_menuUsersSettings->addAction( "", this, SLOT( settingsChanged() ) );
  mp_actRemoveInactiveUsers->setCheckable( true );
  mp_actRemoveInactiveUsers->setChecked( Settings::instance().removeInactiveUsers() );
  mp_actRemoveInactiveUsers->setData( 33 );
  setMaxInactivityDaysInAction( mp_actRemoveInactiveUsers );
  mp_menuChatSettings = new QMenu( tr( "Chat" ), this );
  mp_menuChatSettings->setIcon( IconManager::instance().icon( "chat.png" ) );
  mp_menuSettings->addMenu( mp_menuChatSettings );
  mp_menuChatSaveSettings = new QMenu( tr( "Save messages" ) + QString( "..." ), this );
  mp_menuChatSaveSettings->setIcon( IconManager::instance().icon( "save-as.png" ) );
  mp_menuChatSettings->addMenu( mp_menuChatSaveSettings );
  act = mp_menuChatSaveSettings->addAction( tr( "Enable message saving" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatAutoSave() );
  act->setData( 18 );
  mp_menuChatSaveSettings->addSeparator();
  act = mp_menuChatSaveSettings->addAction( tr( "Save unsent messages" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatSaveUnsentMessages() );
  act->setData( 81 );
  mp_actSaveFileTransferMessages = mp_menuChatSaveSettings->addAction( tr( "Save file transfer messages" ), this, SLOT( settingsChanged() ) );
  mp_actSaveFileTransferMessages->setCheckable( true );
  mp_actSaveFileTransferMessages->setChecked( Settings::instance().chatSaveFileTransfers() );
  mp_actSaveFileTransferMessages->setData( 82 );
  mp_actSaveSystemMessages = mp_menuChatSaveSettings->addAction( tr( "Save system messages" ), this, SLOT( settingsChanged() ) );
  mp_actSaveSystemMessages->setCheckable( true );
  mp_actSaveSystemMessages->setChecked( Settings::instance().chatSaveSystemMessages() );
  mp_actSaveSystemMessages->setData( 83 );
  mp_menuChatSettings->addSeparator();
  act = mp_menuChatSaveSettings->addAction( "", this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setData( 84 );
  setChatMaxLinesToSaveInAction( act );
  mp_menuChatSaveSettings->addSeparator();
  act = mp_menuChatSaveSettings->addAction( "", this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setData( 85 );
  setClearCacheAfterDaysInAction( act );
  mp_menuChatSettings->addSeparator();
  act = mp_menuChatSettings->addAction( tr( "Open chats in a single window" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showChatsInOneWindow() );
  act->setData( 7 );
  act = mp_menuChatSettings->addAction( tr( "Clear all read messages on closing window" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatClearAllReadMessages() );
  act->setData( 47 );
  act = mp_menuChatSettings->addAction( tr( "Use high resolution emoticons" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().useHiResEmoticons() );
  act->setData( 105 );
#if QT_VERSION < 0x050000 || defined( BEEBEEP_FOR_RASPBERRY_PI ) || defined( Q_OS_OS2 )
  act->setEnabled( false );
#endif
  act = mp_menuChatSettings->addAction( tr( "Use font emoticons" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().useFontEmoticons() );
  act->setData( 31 );
  act = mp_menuChatSettings->addAction( tr( "Show chat toolbar" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showChatToolbar() );
  act->setData( 42 );
  mp_menuChatSettings->addSeparator();
  act = mp_menuChatSettings->addAction( tr( "Send offline messages also to chat with all users" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().sendOfflineMessagesToDefaultChat() );
  act->setData( 66 );
  act = mp_menuChatSettings->addAction( "", this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  setChatMessagesToShowInAction( act );
  act->setData( 27 );
  act = mp_menuChatSettings->addAction( "", this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setData( 71 );
  setChatInactiveWindowOpacityLevelInAction( act );
  mp_menuChatSettings->addSeparator();
  QMenu* menu_on_sending_message = mp_menuChatSettings->addMenu( tr( "On sending message" ) + QString( "..." ) );
  menu_on_sending_message->setIcon( IconManager::instance().icon( "send.png" ) );
  mp_actGroupOnSendingMessage = new QActionGroup( this );
  mp_actGroupOnSendingMessage->setExclusive( true );
  act = menu_on_sending_message->addAction( tr( "Do nothing" ) + QString( " (%1)" ).arg( tr( "default" ) ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatOnSendingMessage() == Settings::SkipOnSendingMessage );
  act->setData( 91 );
  mp_actGroupOnSendingMessage->addAction( act );
  act = menu_on_sending_message->addAction( tr( "Minimize chat window" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatOnSendingMessage() == Settings::MinimizeChatOnSendingMessage );
  act->setData( 92 );
  mp_actGroupOnSendingMessage->addAction( act );
  act = menu_on_sending_message->addAction( tr( "Close chat window" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatOnSendingMessage() == Settings::CloseChatOnSendingMessage );
  act->setData( 93 );
  mp_actGroupOnSendingMessage->addAction( act );
  connect( mp_actGroupOnSendingMessage, SIGNAL( triggered( QAction* ) ), this, SLOT( settingsChanged( QAction* ) ) );
  mp_menuChatSettings->addSeparator();
  mp_menuChatColorSettings = new QMenu( tr( "Colors" ) + QString( "..." ), this );
  mp_menuChatColorSettings->setIcon( IconManager::instance().icon( "colors.png" ) );
  mp_menuChatSettings->addMenu( mp_menuChatColorSettings );
  act = mp_menuChatColorSettings->addAction( IconManager::instance().icon( "background-color.png" ), tr( "Select chat background color" ), this, SLOT( settingsChanged() ) );
  act->setData( 72 );
  act = mp_menuChatColorSettings->addAction( IconManager::instance().icon( "font-color.png" ), tr( "Select chat default text color" ), this, SLOT( settingsChanged() ) );
  act->setData( 73 );
  act = mp_menuChatColorSettings->addAction( IconManager::instance().icon( "log.png" ), tr( "Select chat system text color" ), this, SLOT( settingsChanged() ) );
  act->setData( 74 );
  act = mp_menuChatColorSettings->addAction( IconManager::instance().icon( "quote-text.png" ), tr( "Select quote text color" ), this, SLOT( settingsChanged() ) );
  act->setData( 89 );
  act = mp_menuChatColorSettings->addAction( IconManager::instance().icon( "quote-background.png" ), tr( "Select quote background color" ), this, SLOT( settingsChanged() ) );
  act->setData( 90 );
  mp_actSelectEmoticonSourcePath = mp_menuChatSettings->addAction( IconManager::instance().icon( "emoticon.png" ), tr( "Select emoticon theme" ) + QString( "..." ), this, SLOT( selectEmoticonSourcePath() ) );
  mp_actSelectEmoticonSourcePath->setEnabled( !Settings::instance().useFontEmoticons() );
  act = mp_menuChatSettings->addAction( IconManager::instance().icon( "icon-size.png" ), tr( "Change size of the emoticons" ) + QString( "..." ), this, SLOT( changeEmoticonSizeInChat() ) );
  act->setEnabled( !Settings::instance().useFontEmoticons() );
  mp_menuChatSettings->addSeparator();
  mp_menuChatSettings->addAction( IconManager::instance().icon( "dictionary.png" ), tr( "Dictionary" ) + QString( "..." ), this, SLOT( selectDictionatyPath() ) );
  mp_menuChatSettings->addSeparator();
  mp_menuChatSettings->addAction( IconManager::instance().icon( "refused-chat.png" ), tr( "Blocked chats" ) + QString( "..." ), this, SLOT( showRefusedChats() ) );

  mp_menuFileTransferSettings = new QMenu( tr( "File transfer" ), this );
  mp_menuFileTransferSettings->setIcon( IconManager::instance().icon( "file-transfer.png" ) );
  mp_menuFileTransferSettings->setDisabled( Settings::instance().disableFileTransfer() );
  mp_menuSettings->addMenu( mp_menuFileTransferSettings );
  act = mp_menuFileTransferSettings->addAction( tr( "Enable file transfer" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().enableFileTransfer() );
  act->setData( 12 );
  mp_actEnableFileSharing = mp_menuFileTransferSettings->addAction( tr( "Enable file sharing" ), this, SLOT( settingsChanged() ) );
  mp_actEnableFileSharing->setCheckable( true );
  mp_actEnableFileSharing->setChecked( Settings::instance().enableFileSharing() );
  mp_actEnableFileSharing->setData( 5 );
  mp_actEnableFileSharing->setDisabled( Settings::instance().disableFileSharing() );
  mp_menuFileTransferSettings->addSeparator();
  mp_actConfirmDownload = mp_menuFileTransferSettings->addAction( tr( "Prompt before downloading file" ), this, SLOT( settingsChanged() ) );
  mp_actConfirmDownload->setCheckable( true );
  mp_actConfirmDownload->setChecked( Settings::instance().confirmOnDownloadFile() );
  mp_actConfirmDownload->setData( 30 );
  act = mp_menuFileTransferSettings->addAction( tr( "Always download files into the folder with the user's name" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().downloadInUserFolder() );
  act->setData( 86 );
  mp_menuExistingFile = mp_menuFileTransferSettings->addMenu( tr( "If a file already exists" ) + QString( "..." ) );
  mp_actGroupExistingFile = new QActionGroup( this );
  mp_actGroupExistingFile->setExclusive( true );
  act = mp_menuExistingFile->addAction( tr( "Generate new file name" ) );
  act->setCheckable( true );
  act->setChecked( true );
  act->setData( (int)Settings::GenerateNewFileName );
  mp_actGroupExistingFile->addAction( act );
  act = mp_menuExistingFile->addAction( tr( "Skip" ) );
  act->setCheckable( true );
  act->setData( (int)Settings::SkipExistingFile );
  mp_actGroupExistingFile->addAction( act );
  act = mp_menuExistingFile->addAction( tr( "Overwrite" ) );
  act->setCheckable( true );
  act->setData( (int)Settings::OverwriteExistingFile );
  mp_actGroupExistingFile->addAction( act );
  act = mp_menuExistingFile->addAction( tr( "Overwrite older" ) );
  act->setCheckable( true );
  act->setData( (int)Settings::OverwriteOlderExistingFile );
  mp_actGroupExistingFile->addAction( act );
  foreach( QAction* act_to_select, mp_actGroupExistingFile->actions() )
  {
    if( act_to_select->data().toInt() == Settings::instance().onExistingFileAction() )
    {
      act_to_select->setChecked( true );
      break;
    }
  }
  connect( mp_actGroupExistingFile, SIGNAL( triggered( QAction* ) ), this, SLOT( onChangeSettingOnExistingFile( QAction* ) ) );

  act = mp_menuFileTransferSettings->addAction( tr( "Keep the original modification date of the transferred file" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().keepModificationDateOnFileTransferred() );
  act->setData( 101 );
  mp_actResumeFileTransfer = mp_menuFileTransferSettings->addAction( tr( "Resume file transfer" ) + QString( " (%1)" ).arg( tr( "when possible" ) ), this, SLOT( settingsChanged() ) );
  mp_actResumeFileTransfer->setCheckable( true );
  mp_actResumeFileTransfer->setChecked( Settings::instance().resumeFileTransfer() );
  mp_actResumeFileTransfer->setData( 97 );
#ifdef SHADOWCHAT_SAFE_BUILD
  // This release always restarts failed tasks and retains both file versions.
  mp_actResumeFileTransfer->setVisible( false );
  mp_menuExistingFile->menuAction()->setVisible( false );
#endif
  act = mp_menuFileTransferSettings->addAction( "", this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setData( 100 );
  setClearPartiallyDownloadedFilesAfterDaysInAction( act );
  mp_menuFileTransferSettings->addSeparator();
  act = mp_menuFileTransferSettings->addAction( "", this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  setMaxQueuedDownloadsInAction( act );
  act->setData( 88 );
  act = mp_menuFileTransferSettings->addAction( tr( "Always shows the progress of file transfer" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().alwaysShowFileTransferProgress() );
  act->setData( 78 );
  mp_menuFileTransferSettings->addSeparator();
  act = mp_menuFileTransferSettings->addAction( tr( "Use native file dialogs" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().useNativeDialogs() );
  act->setData( 4 );
  mp_menuFileTransferSettings->addSeparator();
  mp_actSelectDownloadFolder = mp_menuFileTransferSettings->addAction( IconManager::instance().icon( "download-folder.png" ), tr( "Select download folder" ) + QString( "..." ), this, SLOT( selectDownloadDirectory() ) );

#ifdef BEEBEEP_USE_VOICE_CHAT
  mp_menuVoiceMessage = new QMenu( tr( "Voice message" ), this );
  mp_menuVoiceMessage->setIcon( IconManager::instance().icon( "microphone.png" ) );
  mp_menuVoiceMessage->setDisabled( Settings::instance().disableVoiceMessages() );
  mp_menuSettings->addMenu( mp_menuVoiceMessage );
  mp_menuVoiceMessage->addAction( IconManager::instance().icon( "audio-settings.png" ), tr( "Voice encoder" ) + QString( "..." ), this, SLOT( showVoiceEncoderSettings() ) );
  mp_menuVoiceMessage->addSeparator();
  act = mp_menuVoiceMessage->addAction( IconManager::instance().icon( "timer.png" ), tr( "Maximum duration" ) + QString( "..." ), this, SLOT( settingsChanged() ) );
  act->setData( 94 );
  mp_menuVoiceMessage->addSeparator();
  act = mp_menuVoiceMessage->addAction( tr( "Use the integrated voice message player" ) + QString( " (beta)" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().useVoicePlayer() );
  act->setData( 95 );
#endif

  mp_menuNotificationSettings = new QMenu( tr( "Notifications" ), this );
  mp_menuNotificationSettings->setIcon( IconManager::instance().icon( "bell.png" ) );
  mp_menuSettings->addMenu( mp_menuNotificationSettings );
  mp_actBeepOnNewMessage = mp_menuNotificationSettings->addAction( tr( "Enable BEEP alert" ), this, SLOT( settingsChanged() ) );
  mp_actBeepOnNewMessage->setCheckable( true );
  mp_actBeepOnNewMessage->setChecked( Settings::instance().beepOnNewMessageArrived() );
  mp_actBeepOnNewMessage->setData( 34 );
  act = mp_menuNotificationSettings->addAction( tr( "Enable Buzz sound" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().playBuzzSound() );
  act->setData( 56 );
  act = mp_menuNotificationSettings->addAction( tr( "Enable visual notifications for chat windows" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().enableVisualNotificationsInChatWindow() );
  act->setData( 107 );
  act = mp_menuNotificationSettings->addAction( tr( "Enable notifications also for chat with all users" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().enableDefaultChatNotifications() );
  act->setData( 75 );
  act = mp_menuNotificationSettings->addAction( tr( "Enable BEEP alert also for active chat windows" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().beepInActiveWindowAlso() );
  act->setData( 98 );
  act = mp_menuNotificationSettings->addAction( tr( "Disable BEEP alert if your status is busy" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().disableBeepInUserStatusBusy() );
  act->setData( 96 );
  mp_menuNotificationSettings->addSeparator();
  act = mp_menuNotificationSettings->addAction( tr( "Raise main window on new message" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().raiseMainWindowOnNewMessageArrived() );
  act->setData( 80 );
  act = mp_menuNotificationSettings->addAction( tr( "Always open chat on new message" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().alwaysOpenChatOnNewMessageArrived() );
  act->setData( 70 );
  act = mp_menuNotificationSettings->addAction( tr( "Raise previously opened chat on new message" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().raiseOnNewMessageArrived() );
  act->setData( 15 );
  mp_menuNotificationSettings->addSeparator();
  mp_menuTrayIconSettings = new QMenu( tr( "System tray icon" ), this );
  mp_menuTrayIconSettings->setIcon( IconManager::instance().icon( "settings-tray-icon.png" ) );
  mp_menuNotificationSettings->addMenu( mp_menuTrayIconSettings );
  act = mp_menuTrayIconSettings->addAction( tr( "Enable tray icon notifications" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showNotificationOnTray()  );
  act->setData( 19 );
  act = mp_menuTrayIconSettings->addAction( tr( "Show only message notifications" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showOnlyMessageNotificationOnTray()  );
  act->setData( 40 );
  act = mp_menuTrayIconSettings->addAction( tr( "Show chat message preview" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showChatMessageOnTray() );
  act->setData( 46 );
  act = mp_menuTrayIconSettings->addAction( tr( "Show file notifications" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showFileTransferCompletedOnTray() );
  act->setData( 48 );
  mp_menuNotificationSettings->addSeparator();
  mp_menuNotificationSettings->addAction( IconManager::instance().icon( "file-beep.png" ), tr( "Select beep file..." ), this, SLOT( selectBeepFile() ) );
  mp_menuNotificationSettings->addAction( IconManager::instance().icon( "play.png" ), tr( "Play beep" ), this, SLOT( testBeepFile() ) );

#ifdef BEEBEEP_USE_SHAREDESKTOP
  QMenu* menu_share_desktop = new QMenu( tr( "Desktop sharing" ), this );
  menu_share_desktop->setIcon( IconManager::instance().icon( "desktop-share.png" ) );
  menu_share_desktop->setDisabled( Settings::instance().disableDesktopSharing() );
  mp_menuSettings->addMenu( menu_share_desktop );
  act = menu_share_desktop->addAction( tr( "Enable desktop sharing" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().enableShareDesktop() );
  act->setData( 60 );
  menu_share_desktop->addSeparator();
  act = menu_share_desktop->addAction( IconManager::instance().icon( "timer.png" ), tr( "Select screen capture interval" ) + QString( "..." ), this, SLOT( settingsChanged() ) );
  act->setData( 61 );
  act = menu_share_desktop->addAction( IconManager::instance().icon( "image-type.png" ), tr( "Select image type" ) + QString( "..." ), this, SLOT( settingsChanged() ) );
  act->setData( 62 );
  act = menu_share_desktop->addAction( IconManager::instance().icon( "image-quality.png" ), tr( "Select image quality" ) + QString( "..." ), this, SLOT( settingsChanged() ) );
  act->setData( 63 );
  menu_share_desktop->addSeparator();
  act = menu_share_desktop->addAction( tr( "Fit image to screen" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().shareDesktopFitToScreen() );
  act->setData( 64 );
#endif

  mp_menuSettings->addSeparator();
  act = mp_menuSettings->addAction( tr( "Always stay on top" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().stayOnTop() );
  act->setData( 14 );
#ifdef Q_OS_WIN
  act = mp_menuSettings->addAction( tr( "Start %1 automatically" ).arg( Settings::instance().programName() ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().hasStartOnSystemBoot() );
  act->setData( 16 );
#endif
  mp_actSaveWindowGeometry = mp_menuSettings->addAction( IconManager::instance().icon( "save-window.png" ), tr( "Save window's geometry" ), this, SLOT( askSaveGeometryAndState() ) );
  mp_actSaveWindowGeometry->setDisabled( Settings::instance().resetGeometryAtStartup() );

  /* User List Menu */
  mp_menuUserList = new QMenu( tr( "Options" ), this );
  act = mp_menuUserList->addAction( tr( "Sort users in ascending order" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().sortUsersAscending() );
  act->setData( 49 );
  QMenu* sorting_users_menu = mp_menuUserList->addMenu( tr( "Sorting mode" ) + QString( "..." ) );
  QActionGroup* sorting_users_action_group = new QActionGroup( this );
  sorting_users_action_group->setExclusive( true );
  act = sorting_users_menu->addAction( tr( "Default mode" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userSortingMode() < 1 || Settings::instance().userSortingMode() > 3 );
  act->setData( 50 );
  sorting_users_action_group->addAction( act );
  act = sorting_users_menu->addAction( tr( "By user name" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userSortingMode() == 1 );
  act->setData( 51 );
  sorting_users_action_group->addAction( act );
  act = sorting_users_menu->addAction( tr( "By user status" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userSortingMode() == 2 );
  act->setData( 52 );
  sorting_users_action_group->addAction( act );
  act = sorting_users_menu->addAction( tr( "By unread messages" ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().userSortingMode() == 3 );
  act->setData( 53 );
  sorting_users_action_group->addAction( act );
  connect( sorting_users_action_group, SIGNAL( triggered( QAction* ) ), this, SLOT( settingsChanged( QAction* ) ) );
  mp_menuUserList->addSeparator();
  mp_actShowOnlineUsersOnly = mp_menuUserList->addAction( tr( "Show online users only" ), this, SLOT( settingsChanged() ) );
  mp_actShowOnlineUsersOnly->setCheckable( true );
  mp_actShowOnlineUsersOnly->setChecked( Settings::instance().showOnlyOnlineUsers() );
  mp_actShowOnlineUsersOnly->setData( 6 );
  act = mp_menuUserList->addAction( tr( "Show users in their workgroups" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showUsersInWorkgroups() );
  act->setData( 87 );
  if( mp_actShowUserFullName->isChecked() )
    mp_menuUserList->addAction( mp_actShowUserFullName );
  mp_menuUserList->addAction( mp_actShowUserFirstNameFirstInFullName );
  mp_menuUserList->addSeparator();
  act = mp_menuUserList->addAction( tr( "Show the user's picture" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showUserPhoto() );
  act->setData( 21 );
  act = mp_menuUserList->addAction( tr( "Show the user's vCard on right click" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showVCardOnRightClick() );
  act->setData( 25 );
  act = mp_menuUserList->addAction( tr( "Show status color in background" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showUserStatusBackgroundColor() );
  act->setData( 38 );
  act = mp_menuUserList->addAction( tr( "Show the status description" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showUserStatusDescription() );
  act->setData( 37 );
  mp_menuUserList->addSeparator();
  mp_actEditAvatarIconSize = mp_menuUserList->addAction( IconManager::instance().icon( "icon-size.png" ), tr( "Change size of the user's picture" ), this, SLOT( changeAvatarSizeInList() ) );
  mp_userList->setMenuSettings( mp_menuUserList );

  /* Status Menu */
  mp_menuStatus = new QMenu( tr( "Status" ), this );
  mp_menuStatus->setIcon( IconManager::instance().icon( "user-status.png" ) );
  for( int i = User::Online; i < User::NumStatus; i++ )
  {
    act = mp_menuStatus->addAction( QIcon( Bee::menuUserStatusIconFileName( i ) ), Bee::userStatusToString( i ), this, SLOT( statusSelected() ) );
    act->setData( i );
    act->setIconVisibleInMenu( true );
  }
  mp_menuStatus->addSeparator();
  mp_actSetAutoAway = mp_menuStatus->addAction( tr( "Set your status to away automatically" ), this, SLOT( settingsChanged() ) );
  mp_actSetAutoAway->setCheckable( true );
  mp_actSetAutoAway->setChecked( Settings::instance().autoUserAway() );
  mp_actSetAutoAway->setData( 20 );
  mp_menuStatus->addSeparator();
  mp_menuUserStatusList = new QMenu( tr( "Recently used" ), this );
  act = mp_menuStatus->addMenu( mp_menuUserStatusList );
  act->setIcon( IconManager::instance().icon( "recent.png" ) );
  loadUserStatusRecentlyUsed();
  mp_actChangeStatusDescription = mp_menuStatus->addAction( IconManager::instance().icon( "user-status.png" ), tr( "Change your status description..." ), this, SLOT( changeStatusDescription() ) );
  mp_menuStatus->addAction( IconManager::instance().icon( "clear.png" ), tr( "Clear all status descriptions" ), this, SLOT( clearRecentlyUsedUserStatus() ) );
  mp_menuStatus->addSeparator();
  mp_menuStatus->addAction( mp_actEditWorkgroups );
  mp_menuStatus->addSeparator();
  act = mp_menuStatus->addAction( QIcon( Bee::menuUserStatusIconFileName( User::Offline ) ), Bee::userStatusToString( User::Offline ), this, SLOT( statusSelected() ) );
  act->setData( User::Offline );
  act->setIconVisibleInMenu( true );
  act = mp_menuStatus->menuAction();
  connect( act, SIGNAL( triggered() ), this, SLOT( showLocalUserVCard() ) );

  /* Context Menu for user list view */
  QMenu* context_menu_users = new QMenu( "Menu", this );
  if( mp_actShowUserFullName->isChecked() )
    context_menu_users->addAction( mp_actShowUserFullName );
  context_menu_users->addAction( mp_actShowUserFirstNameFirstInFullName );
  context_menu_users->addSeparator();
  context_menu_users->addAction( mp_actVCard );
  context_menu_users->addAction( mp_actChangeStatusDescription );
  context_menu_users->addSeparator();
  context_menu_users->addAction( mp_actSetAutoAway );
  context_menu_users->addSeparator();
  context_menu_users->addAction( mp_actConfigureNetwork );
  context_menu_users->addAction( mp_actAddUsers );
  context_menu_users->addSeparator();
  context_menu_users->addAction( mp_actEditAvatarIconSize );
  context_menu_users->addSeparator();
  context_menu_users->addAction( mp_actShowOnlineUsersOnly );
  mp_userList->setContextMenuUsers( context_menu_users );

  /* Help Menu */
  mp_menuInfo = new QMenu( tr("?" ), this );
  mp_menuInfo->addAction( IconManager::instance().icon( "donate.png" ), tr( "Donate for %1" ).arg( Settings::instance().programName() ) + QString( "..." ), this, SLOT( openDonationPage() ) );
  mp_menuInfo->addSeparator();
  mp_menuInfo->addAction( mp_actAbout );
  mp_menuInfo->addAction( IconManager::instance().icon( "license.png" ), tr( "Show %1's license..." ).arg( Settings::instance().programName() ), this, SLOT( showLicense() ) );
  act = mp_menuInfo->addAction( IconManager::instance().icon( "qt.png" ), tr( "Qt Library..." ), this, SLOT( showAboutQt() ) );
  act->setMenuRole( QAction::AboutQtRole );
  mp_menuInfo->addSeparator();
  mp_menuInfo->addAction( IconManager::instance().icon( "beebeep.png" ), tr( "Open %1 official website..." ).arg( Settings::instance().programName() ), this, SLOT( openWebSite() ) );
  mp_menuInfo->addAction( IconManager::instance().icon( "update.png" ), tr( "Check for new version..." ), this, SLOT( checkNewVersion() ) );
  mp_menuInfo->addAction( IconManager::instance().icon( "plugin.png" ), tr( "Download plugins..." ), this, SLOT( openDownloadPluginPage() ) );
  mp_menuInfo->addSeparator();
  mp_menuInfo->addAction( IconManager::instance().icon( "info.png" ), tr( "Help online..." ), this, SLOT( openHelpPage() ) );
  mp_menuInfo->addAction( IconManager::instance().icon( "tip.png" ), tr( "Show tips..." ), this, SLOT( showTipOfTheDay() ) );
  mp_menuInfo->addAction( IconManager::instance().icon( "chat-small.png" ), tr( "Read FAQ..." ), this, SLOT( showFaq() ) );
  mp_menuInfo->addAction( IconManager::instance().icon( "fact.png" ), tr( "Discover the fact of the day..." ), this, SLOT( showFactOfTheDay() ) );
  mp_menuInfo->addSeparator();
  mp_menuInfo->addAction( IconManager::instance().icon( "star.png" ), tr( "Information about %1..." ).arg( "Marco Mastroddi" ), this, SLOT( openDeveloperWebSite() ) );
#ifdef BEEBEEP_DEBUG
  mp_menuInfo->addSeparator();
  act = mp_menuInfo->addAction( tr( "Add +1 user to anonymous usage statistics" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().postUsageStatistics() );
  act->setData( 44 );
#endif

  /* Tray icon menu */
  mp_menuTrayIcon = new QMenu( this );
  act = mp_menuTrayIcon->addAction( IconManager::instance().icon( "beebeep.png" ), tr( "Show" ), this, SLOT( showUp() ) );
  mp_menuTrayIcon->setDefaultAction( act );
  mp_menuTrayIcon->addSeparator();
  mp_menuTrayIcon->addAction( mp_menuStatus->menuAction() );
  mp_menuTrayIcon->addSeparator();
  mp_menuTrayIcon->addAction( mp_actViewNewMessage );
  mp_menuTrayIcon->addSeparator();
  mp_menuTrayIcon->addAction( IconManager::instance().icon( "quit.png" ), tr( "Quit" ), this, SLOT( forceShutdown() ) );

  mp_trayIcon->setContextMenu( mp_menuTrayIcon );
}

void GuiMain::createToolAndMenuBars()
{
  menuBar()->addMenu( mp_menuMain );
  if( Settings::instance().disableMenuSettings() )
    mp_menuSettings->setDisabled( true );
  menuBar()->addMenu( mp_menuSettings );
  menuBar()->addMenu( mp_menuInfo );
  QLabel *label_version = new QLabel( this );
  label_version->setTextFormat( Qt::RichText );
  label_version->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
  QString label_version_text = QString( "&nbsp;&nbsp;<b>%1%2</b> %3&nbsp;" )
                                .arg( Settings::instance().version( Settings::instance().isDevelopmentVersion(), false, false ),
                                      Settings::instance().isDevelopmentVersion() ? QString( "-dev" ) : "",
                                      IconManager::instance().toHtml( Settings::instance().operatingSystemIconPath(), "*", 12, 12 ) );
  QString label_tooltip = QString( "BeeBEEP %1 %2%3" ).arg( Settings::instance().version( true, true, true ), Settings::instance().operatingSystem( true ) );
  if( Settings::instance().isDevelopmentVersion() )
    label_tooltip.append( QString( " (%1)" ).arg( tr("Development version") ) );

  if( Settings::instance().enableReceivingHelpMessages() )
  {
    label_version_text.append( QString( "&nbsp;%1&nbsp;" ).arg( IconManager::instance().toHtml( "help.png", "H", 12, 12 ) ) );
    label_tooltip.append( QString( " [%1]" ).arg( tr("Help station") ) );
  }

  label_version->setText( label_version_text );
  label_version->setToolTip( label_tooltip );
  menuBar()->setCornerWidget( label_version );

  mp_barMain->addAction( mp_menuStatus->menuAction() );
  mp_barMain->addAction( mp_actVCard );
  mp_barMain->addSeparator();
  mp_barMain->addAction( mp_actBroadcast );
  mp_barMain->addSeparator();
  mp_barMain->addAction( mp_actViewNewMessage );
  mp_barMain->addAction( mp_actCreateMessage );
  mp_barMain->addAction( mp_actCreateGroupChat );
  mp_barMain->addSeparator();
  mp_barMain->addAction( mp_actViewFileTransfer );
  mp_barMain->addAction( mp_actViewFileSharing );
}

void GuiMain::createModernShell()
{
  QWidget* legacy_central_widget = takeCentralWidget();

  setWindowFlag( Qt::FramelessWindowHint, true );

  mp_modernRoot = new QWidget( this );
  mp_modernRoot->setObjectName( "ShadowRoot" );
  QVBoxLayout* root_layout = new QVBoxLayout( mp_modernRoot );
  root_layout->setContentsMargins( 0, 0, 0, 0 );
  root_layout->setSpacing( 0 );

  mp_modernTitleBar = new QWidget( mp_modernRoot );
  mp_modernTitleBar->setObjectName( "ShadowTitleBar" );
  mp_modernTitleBar->setFixedHeight( 38 );
  QHBoxLayout* title_layout = new QHBoxLayout( mp_modernTitleBar );
  title_layout->setContentsMargins( 12, 0, 0, 0 );
  title_layout->setSpacing( 8 );
  QLabel* title_icon = new QLabel( mp_modernTitleBar );
  title_icon->setPixmap( QPixmap( QStringLiteral( ":/images/shadowchat/shadowchat-mark-source.png" ) )
                         .scaled( 22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
  QLabel* title_text = new QLabel( QStringLiteral( "影聊" ), mp_modernTitleBar );
  title_text->setObjectName( "ShadowWindowTitle" );
  title_layout->addWidget( title_icon );
  title_layout->addWidget( title_text );
  title_layout->addStretch( 1 );

  mp_modernMinimizeButton = new QToolButton( mp_modernTitleBar );
  mp_modernMinimizeButton->setObjectName( "ShadowWindowButton" );
  mp_modernMinimizeButton->setIcon( style()->standardIcon( QStyle::SP_TitleBarMinButton ) );
  mp_modernMinimizeButton->setToolTip( QStringLiteral( "最小化" ) );
  mp_modernMinimizeButton->setAccessibleName( QStringLiteral( "最小化窗口" ) );
  mp_modernMinimizeButton->setFocusPolicy( Qt::NoFocus );
  connect( mp_modernMinimizeButton, SIGNAL( clicked() ), this, SLOT( showMinimized() ) );
  title_layout->addWidget( mp_modernMinimizeButton );

  mp_modernMaximizeButton = new QToolButton( mp_modernTitleBar );
  mp_modernMaximizeButton->setObjectName( "ShadowWindowButton" );
  mp_modernMaximizeButton->setIcon( style()->standardIcon( QStyle::SP_TitleBarMaxButton ) );
  mp_modernMaximizeButton->setToolTip( QStringLiteral( "最大化" ) );
  mp_modernMaximizeButton->setAccessibleName( QStringLiteral( "最大化或还原窗口" ) );
  mp_modernMaximizeButton->setFocusPolicy( Qt::NoFocus );
  connect( mp_modernMaximizeButton, &QToolButton::clicked, this, [this]() {
    isMaximized() ? showNormal() : showMaximized();
  } );
  title_layout->addWidget( mp_modernMaximizeButton );

  mp_modernCloseButton = new QToolButton( mp_modernTitleBar );
  mp_modernCloseButton->setObjectName( "ShadowCloseButton" );
  mp_modernCloseButton->setIcon( style()->standardIcon( QStyle::SP_TitleBarCloseButton ) );
  mp_modernCloseButton->setToolTip( QStringLiteral( "关闭" ) );
  mp_modernCloseButton->setAccessibleName( QStringLiteral( "关闭窗口" ) );
  mp_modernCloseButton->setFocusPolicy( Qt::NoFocus );
  connect( mp_modernCloseButton, SIGNAL( clicked() ), this, SLOT( close() ) );
  title_layout->addWidget( mp_modernCloseButton );
  root_layout->addWidget( mp_modernTitleBar );

  QWidget* shell_content = new QWidget( mp_modernRoot );
  shell_content->setObjectName( "ShadowShellContent" );
  QHBoxLayout* content_layout = new QHBoxLayout( shell_content );
  content_layout->setContentsMargins( 0, 0, 0, 0 );
  content_layout->setSpacing( 0 );

  mp_modernNavigationRail = new QWidget( mp_modernRoot );
  mp_modernNavigationRail->setObjectName( "ShadowNavRail" );
  mp_modernNavigationRail->setFixedWidth( 80 );
  QVBoxLayout* navigation_layout = new QVBoxLayout( mp_modernNavigationRail );
  navigation_layout->setContentsMargins( 9, 16, 9, 12 );
  navigation_layout->setSpacing( 8 );

  QLabel* brand_mark = new QLabel( mp_modernNavigationRail );
  brand_mark->setObjectName( "ShadowBrandMark" );
  brand_mark->setAlignment( Qt::AlignCenter );
  brand_mark->setPixmap( QPixmap( QStringLiteral( ":/images/shadowchat/shadowchat-mark-source.png" ) )
                         .scaled( 42, 42, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
  QLabel* brand_name = new QLabel( QStringLiteral( "影聊" ), mp_modernNavigationRail );
  brand_name->setObjectName( "ShadowBrandName" );
  brand_name->setAlignment( Qt::AlignCenter );
  navigation_layout->addWidget( brand_mark );
  navigation_layout->addWidget( brand_name );
  navigation_layout->addSpacing( 12 );

  mp_modernMessagesButton = new QToolButton( mp_modernNavigationRail );
  mp_modernMessagesButton->setObjectName( "ShadowNavButton" );
  mp_modernMessagesButton->setText( QStringLiteral( "消息" ) );
  mp_modernMessagesButton->setIcon( ShadowStyle::navigationIcon( QStringLiteral( "chats-circle" ) ) );
  mp_modernMessagesButton->setIconSize( QSize( 24, 24 ) );
  mp_modernMessagesButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
  mp_modernMessagesButton->setCheckable( true );
  mp_modernMessagesButton->setAccessibleName( QStringLiteral( "消息" ) );
  mp_modernMessagesButton->setToolTip( QStringLiteral( "消息 (Ctrl+1)" ) );
  connect( mp_modernMessagesButton, SIGNAL( clicked() ), this, SLOT( showModernMessages() ) );
  navigation_layout->addWidget( mp_modernMessagesButton );

  mp_modernContactsButton = new QToolButton( mp_modernNavigationRail );
  mp_modernContactsButton->setObjectName( "ShadowNavButton" );
  mp_modernContactsButton->setText( QStringLiteral( "通讯录" ) );
  mp_modernContactsButton->setIcon( ShadowStyle::navigationIcon( QStringLiteral( "users-three" ) ) );
  mp_modernContactsButton->setIconSize( QSize( 24, 24 ) );
  mp_modernContactsButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
  mp_modernContactsButton->setCheckable( true );
  mp_modernContactsButton->setAccessibleName( QStringLiteral( "通讯录" ) );
  mp_modernContactsButton->setToolTip( QStringLiteral( "通讯录 (Ctrl+2)" ) );
  connect( mp_modernContactsButton, SIGNAL( clicked() ), this, SLOT( showModernContacts() ) );
  navigation_layout->addWidget( mp_modernContactsButton );
  navigation_layout->addStretch( 1 );

  mp_modernMoreButton = new QToolButton( mp_modernNavigationRail );
  mp_modernMoreButton->setObjectName( "ShadowNavButton" );
  mp_modernMoreButton->setText( QStringLiteral( "更多" ) );
  mp_modernMoreButton->setIcon( ShadowStyle::navigationIcon( QStringLiteral( "dots-three-circle" ) ) );
  mp_modernMoreButton->setIconSize( QSize( 22, 22 ) );
  mp_modernMoreButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
  mp_modernMoreButton->setPopupMode( QToolButton::InstantPopup );
  mp_modernMoreButton->setAccessibleName( QStringLiteral( "更多工具" ) );
  QMenu* more_menu = new QMenu( mp_modernMoreButton );
  more_menu->setTitle( QStringLiteral( "更多工具" ) );
  more_menu->addAction( ShadowStyle::actionIcon( QStringLiteral( "gear-six" ) ), QStringLiteral( "设置" ), this, SLOT( showShadowSettings() ) );
  more_menu->addAction( ShadowStyle::actionIcon( QStringLiteral( "folder-open" ) ), QStringLiteral( "打开接收文件夹" ), this, SLOT( openDownloadFolder() ) );
  more_menu->addSeparator();
  more_menu->addAction( ShadowStyle::actionIcon( QStringLiteral( "shield-check" ) ), QStringLiteral( "连接与安全状态" ), this, SLOT( showModernActivities() ) );
  more_menu->addAction( ShadowStyle::actionIcon( QStringLiteral( "archive" ) ), QStringLiteral( "聊天存档" ), this, SLOT( showModernSavedChats() ) );
  more_menu->addAction( mp_actViewFileTransfer );
  more_menu->addSeparator();
  QMenu* workspace_menu = more_menu->addMenu( ShadowStyle::actionIcon( QStringLiteral( "folder-open" ) ), QStringLiteral( "工作区与换机" ) );
  workspace_menu->addAction( QStringLiteral( "导出工作区…" ), this, SLOT( exportShadowWorkspace() ) );
  workspace_menu->addAction( QStringLiteral( "导入工作区…" ), this, SLOT( importShadowWorkspace() ) );
  QAction* auto_login_action = more_menu->addAction( QStringLiteral( "启动时自动登录" ) );
  auto_login_action->setCheckable( true );
  auto_login_action->setChecked( ShadowLocalAccounts().autoLoginEnabled() );
  connect( auto_login_action, &QAction::toggled, this, []( bool enabled ) { ShadowLocalAccounts().setAutoLoginEnabled( enabled ); } );
  more_menu->addAction( QStringLiteral( "设置安全恢复密码…" ), this, [this]() {
    ShadowLocalAccounts accounts;
    bool ok = false;
    if( accounts.hasRecoveryPassword() )
    {
      const QString previous = QInputDialog::getText( this, QStringLiteral( "安全恢复密码" ), QStringLiteral( "请输入当前恢复密码" ), QLineEdit::Password, QString(), &ok );
      if( !ok ) return;
      if( !accounts.verifyRecoveryPassword( previous ) ) { QMessageBox::warning( this, QStringLiteral( "安全恢复密码" ), QStringLiteral( "当前恢复密码不正确。" ) ); return; }
    }
    const QString password = QInputDialog::getText( this, QStringLiteral( "负责人设置" ), QStringLiteral( "设置统一恢复密码（至少 8 位），请由负责人保管。\n设置后可导出团队恢复配置，随安装包统一部署。" ), QLineEdit::Password, QString(), &ok );
    if( !ok ) return;
    const QString confirm = QInputDialog::getText( this, QStringLiteral( "确认恢复密码" ), QStringLiteral( "再次输入恢复密码" ), QLineEdit::Password, QString(), &ok );
    if( !ok ) return;
    if( password != confirm || !accounts.setRecoveryPassword( password ) )
      QMessageBox::warning( this, QStringLiteral( "安全恢复密码" ), QStringLiteral( "两次输入不一致、密码不足 8 位或保存失败。" ) );
    else QMessageBox::information( this, QStringLiteral( "安全恢复密码" ), QStringLiteral( "本机恢复密码已配置。请负责人妥善保管。" ) );
  } );
  more_menu->addAction( QStringLiteral( "导出团队恢复配置…" ), this, [this]() {
    ShadowLocalAccounts accounts;
    if( !accounts.hasRecoveryPassword() ) { QMessageBox::information( this, QStringLiteral( "团队恢复配置" ), QStringLiteral( "请负责人先设置安全恢复密码。" ) ); return; }
    bool ok = false;
    const QString password = QInputDialog::getText( this, QStringLiteral( "负责人验证" ), QStringLiteral( "请输入当前恢复密码" ), QLineEdit::Password, QString(), &ok );
    if( !ok ) return;
    if( !accounts.verifyRecoveryPassword( password ) ) { QMessageBox::warning( this, QStringLiteral( "团队恢复配置" ), QStringLiteral( "恢复密码不正确。" ) ); return; }
    const QString path = QFileDialog::getSaveFileName( this, QStringLiteral( "导出团队恢复配置" ), QStringLiteral( "shadowchat-recovery.ini" ), QStringLiteral( "恢复配置 (*.ini)" ) );
    if( path.isEmpty() ) return;
    if( !accounts.exportRecoveryConfiguration( path ) ) QMessageBox::warning( this, QStringLiteral( "团队恢复配置" ), QStringLiteral( "导出失败，请检查保存位置。" ) );
    else QMessageBox::information( this, QStringLiteral( "团队恢复配置" ), QStringLiteral( "配置只包含加盐密码校验值。将它随安装包部署到程序目录，所有新电脑即可使用同一个恢复密码。" ) );
  } );
  more_menu->addAction( QStringLiteral( "退出账号 / 切换账号…" ), this, [this]() {
    if( QMessageBox::question( this, QStringLiteral( "退出账号" ), QStringLiteral( "退出将中断当前收发，未完成文件下次需重新传输。\n退出后将显示登录页，是否继续？" ) ) != QMessageBox::Yes ) return;
    ShadowLocalAccounts().logout();
    qApp->setProperty( "shadowRestartForLogin", true );
    forceShutdown();
  } );
  more_menu->addSeparator();
  mp_menuSettings->setTitle( QStringLiteral( "高级设置" ) );
  mp_menuMain->setTitle( QStringLiteral( "系统工具" ) );
  more_menu->addMenu( mp_menuSettings );
  more_menu->addMenu( mp_menuMain );
  mp_modernMoreButton->setMenu( more_menu );
  navigation_layout->addWidget( mp_modernMoreButton );

  mp_modernProfileButton = new QToolButton( mp_modernNavigationRail );
  mp_modernProfileButton->setObjectName( "ShadowProfileButton" );
  mp_modernProfileButton->setText( QStringLiteral( "我的" ) );
  mp_modernProfileButton->setIcon( ShadowStyle::navigationIcon( QStringLiteral( "user-circle" ) ) );
  mp_modernProfileButton->setIconSize( QSize( 26, 26 ) );
  mp_modernProfileButton->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
  mp_modernProfileButton->setToolTip( QStringLiteral( "修改名字、头像与状态" ) );
  mp_modernProfileButton->setAccessibleName( QStringLiteral( "我的资料" ) );
  connect( mp_modernProfileButton, SIGNAL( clicked() ), this, SLOT( changeVCard() ) );
  navigation_layout->addWidget( mp_modernProfileButton );

  mp_modernListPanel = new QWidget( mp_modernRoot );
  mp_modernListPanel->setObjectName( "ShadowListPanel" );
  mp_modernListPanel->setMinimumWidth( 272 );
  mp_modernListPanel->setMaximumWidth( 380 );
  QVBoxLayout* list_layout = new QVBoxLayout( mp_modernListPanel );
  list_layout->setContentsMargins( 20, 20, 18, 16 );
  list_layout->setSpacing( 9 );

  QLabel* section_eyebrow = new QLabel( QStringLiteral( "办公通讯" ), mp_modernListPanel );
  section_eyebrow->setObjectName( "ShadowEyebrow" );
  list_layout->addWidget( section_eyebrow );

  mp_modernSectionTitle = new QLabel( QStringLiteral( "最近消息" ), mp_modernListPanel );
  mp_modernSectionTitle->setObjectName( "ShadowSectionTitle" );
  mp_modernSectionHint = new QLabel( QStringLiteral( "你最近联系的同事和群聊" ), mp_modernListPanel );
  mp_modernSectionHint->setObjectName( "ShadowSectionHint" );
  list_layout->addWidget( mp_modernSectionTitle );
  list_layout->addWidget( mp_modernSectionHint );

  mp_modernSearch = new QLineEdit( mp_modernListPanel );
  mp_modernSearch->setObjectName( "ShadowGlobalSearch" );
  mp_modernSearch->setPlaceholderText( QStringLiteral( "搜索同事、群聊或会话" ) );
  mp_modernSearch->setClearButtonEnabled( true );
  mp_modernSearch->addAction( ShadowStyle::actionIcon( QStringLiteral( "magnifying-glass" ) ), QLineEdit::LeadingPosition );
  mp_modernSearch->setAccessibleName( QStringLiteral( "全局搜索" ) );
  mp_modernSearch->setToolTip( QStringLiteral( "搜索同事、群聊或会话 (Ctrl+K)" ) );
  connect( mp_modernSearch, SIGNAL( textChanged( const QString& ) ), this, SLOT( onModernSearchChanged( const QString& ) ) );
  list_layout->addWidget( mp_modernSearch );

  mp_modernDirectoryTabs = new QWidget( mp_modernListPanel );
  mp_modernDirectoryTabs->setObjectName( "ShadowDirectoryTabs" );
  QHBoxLayout* directory_tabs_layout = new QHBoxLayout( mp_modernDirectoryTabs );
  directory_tabs_layout->setContentsMargins( 0, 0, 0, 0 );
  directory_tabs_layout->setSpacing( 6 );
  mp_modernPeopleButton = new QToolButton( mp_modernDirectoryTabs );
  mp_modernPeopleButton->setObjectName( "ShadowSegmentButton" );
  mp_modernPeopleButton->setText( QStringLiteral( "同事" ) );
  mp_modernPeopleButton->setCheckable( true );
  connect( mp_modernPeopleButton, SIGNAL( clicked() ), this, SLOT( showModernContacts() ) );
  directory_tabs_layout->addWidget( mp_modernPeopleButton );
  mp_modernGroupsButton = new QToolButton( mp_modernDirectoryTabs );
  mp_modernGroupsButton->setObjectName( "ShadowSegmentButton" );
  mp_modernGroupsButton->setText( QStringLiteral( "群聊" ) );
  mp_modernGroupsButton->setCheckable( true );
  connect( mp_modernGroupsButton, SIGNAL( clicked() ), this, SLOT( showModernGroups() ) );
  directory_tabs_layout->addWidget( mp_modernGroupsButton );
  list_layout->addWidget( mp_modernDirectoryTabs );

  mp_tabMain->setParent( mp_modernListPanel );
  mp_tabMain->tabBar()->hide();
  mp_tabMain->setDocumentMode( true );
  mp_tabMain->setContentsMargins( 0, 0, 0, 0 );
  foreach( QWidget* legacy_filter, QList<QWidget*>()
           << mp_chatList->findChild<QWidget*>( "mp_leFilter" )
           << mp_chatList->findChild<QWidget*>( "mp_pbClearFilter" )
           << mp_chatList->findChild<QWidget*>( "mp_pbSettings" )
           << mp_userList->findChild<QWidget*>( "mp_leFilter" )
           << mp_userList->findChild<QWidget*>( "mp_pbClearFilter" )
           << mp_userList->findChild<QWidget*>( "mp_pbSettings" )
           << mp_groupList->findChild<QWidget*>( "mp_leFilter" )
           << mp_groupList->findChild<QWidget*>( "mp_pbClearFilter" ) )
  {
    if( legacy_filter )
      legacy_filter->hide();
  }

  mp_modernSearchResults = new QTreeWidget( mp_modernListPanel );
  mp_modernSearchResults->setObjectName( "ShadowSearchResults" );
  mp_modernSearchResults->setHeaderHidden( true );
  mp_modernSearchResults->setRootIsDecorated( true );
  mp_modernSearchResults->setIndentation( 14 );
  mp_modernSearchResults->setFrameShape( QFrame::NoFrame );
  mp_modernSearchResults->setUniformRowHeights( true );
  mp_modernSearchResults->setAccessibleName( QStringLiteral( "搜索结果" ) );
  connect( mp_modernSearchResults, SIGNAL( itemActivated( QTreeWidgetItem*, int ) ), this, SLOT( onModernSearchItemActivated( QTreeWidgetItem*, int ) ) );
  connect( mp_modernSearchResults, SIGNAL( itemDoubleClicked( QTreeWidgetItem*, int ) ), this, SLOT( onModernSearchItemActivated( QTreeWidgetItem*, int ) ) );

  mp_modernListStack = new QStackedWidget( mp_modernListPanel );
  mp_modernListStack->setObjectName( "ShadowListStack" );
  mp_modernListStack->addWidget( mp_tabMain );
  mp_modernListStack->addWidget( mp_modernSearchResults );
  list_layout->addWidget( mp_modernListStack, 1 );

  mp_modernEmptyState = new QWidget( mp_modernListPanel );
  mp_modernEmptyState->setObjectName( "ShadowEmptyState" );
  QVBoxLayout* empty_layout = new QVBoxLayout( mp_modernEmptyState );
  empty_layout->setContentsMargins( 24, 36, 24, 36 );
  empty_layout->setSpacing( 8 );
  empty_layout->setAlignment( Qt::AlignCenter );
  QLabel* empty_icon = new QLabel( mp_modernEmptyState );
  empty_icon->setObjectName( "ShadowEmptyIcon" );
  empty_icon->setAlignment( Qt::AlignCenter );
  empty_icon->setPixmap( ShadowStyle::tintedPixmap( QStringLiteral( ":/images/shadowchat/icons/users-three.svg" ),
                                                    QSize( 42, 42 ), QColor( "#8A94A3" ) ) );
  mp_modernEmptyTitle = new QLabel( mp_modernEmptyState );
  mp_modernEmptyTitle->setObjectName( "ShadowEmptyTitle" );
  mp_modernEmptyTitle->setAlignment( Qt::AlignCenter );
  mp_modernEmptyHint = new QLabel( mp_modernEmptyState );
  mp_modernEmptyHint->setObjectName( "ShadowEmptyHint" );
  mp_modernEmptyHint->setAlignment( Qt::AlignCenter );
  mp_modernEmptyHint->setWordWrap( true );
  mp_modernEmptyAction = new QPushButton( mp_modernEmptyState );
  mp_modernEmptyAction->setObjectName( "ShadowPrimaryButton" );
  connect( mp_modernEmptyAction, &QPushButton::clicked, this, [this]() {
    if( m_modernPrimaryPage == 0 )
      showModernContacts();
    else if( m_modernPrimaryPage == 2 )
      createGroupChat();
    else
      restartCore();
  } );
  empty_layout->addWidget( empty_icon, 0, Qt::AlignHCenter );
  empty_layout->addWidget( mp_modernEmptyTitle );
  empty_layout->addWidget( mp_modernEmptyHint );
  empty_layout->addSpacing( 6 );
  empty_layout->addWidget( mp_modernEmptyAction, 0, Qt::AlignHCenter );
  mp_modernListStack->addWidget( mp_modernEmptyState );

  mp_modernNetworkStrip = new QFrame( mp_modernListPanel );
  mp_modernNetworkStrip->setObjectName( "ShadowNetworkStrip" );
  QHBoxLayout* network_layout = new QHBoxLayout( mp_modernNetworkStrip );
  network_layout->setContentsMargins( 10, 7, 10, 7 );
  network_layout->setSpacing( 8 );
  mp_modernNetworkDot = new QLabel( mp_modernNetworkStrip );
  mp_modernNetworkDot->setObjectName( "ShadowNetworkDot" );
  mp_modernNetworkText = new QLabel( mp_modernNetworkStrip );
  mp_modernNetworkText->setObjectName( "ShadowNetworkText" );
  mp_modernNetworkText->setWordWrap( true );
  mp_modernNetworkAction = new QPushButton( QStringLiteral( "重试" ), mp_modernNetworkStrip );
  mp_modernNetworkAction->setObjectName( "ShadowInlineAction" );
  mp_modernNetworkAction->setAccessibleName( QStringLiteral( "重新连接局域网" ) );
  connect( mp_modernNetworkAction, SIGNAL( clicked() ), this, SLOT( restartCore() ) );
  network_layout->addWidget( mp_modernNetworkDot, 0, Qt::AlignTop );
  network_layout->addWidget( mp_modernNetworkText, 1 );
  network_layout->addWidget( mp_modernNetworkAction, 0, Qt::AlignVCenter );
  list_layout->addWidget( mp_modernNetworkStrip );

  mp_modernConversationPanel = new QWidget( mp_modernRoot );
  mp_modernConversationPanel->setObjectName( "ShadowConversationPanel" );
  QVBoxLayout* conversation_layout = new QVBoxLayout( mp_modernConversationPanel );
  conversation_layout->setContentsMargins( 0, 0, 0, 0 );
  conversation_layout->setSpacing( 0 );

  mp_modernConversationStack = new QStackedWidget( mp_modernConversationPanel );
  mp_modernConversationStack->setObjectName( "ShadowConversationStack" );

  mp_modernChatPlaceholder = new QWidget( mp_modernConversationStack );
  mp_modernChatPlaceholder->setObjectName( "ShadowChatPlaceholder" );
  QVBoxLayout* placeholder_layout = new QVBoxLayout( mp_modernChatPlaceholder );
  placeholder_layout->setContentsMargins( 28, 28, 28, 28 );
  placeholder_layout->setAlignment( Qt::AlignCenter );

  QFrame* welcome_card = new QFrame( mp_modernChatPlaceholder );
  welcome_card->setObjectName( "ShadowWelcomeCard" );
  welcome_card->setMaximumWidth( 480 );
  QVBoxLayout* welcome_layout = new QVBoxLayout( welcome_card );
  welcome_layout->setContentsMargins( 40, 34, 40, 34 );
  welcome_layout->setSpacing( 8 );

  QLabel* placeholder_icon = new QLabel( welcome_card );
  placeholder_icon->setAlignment( Qt::AlignCenter );
  placeholder_icon->setPixmap( QPixmap( QStringLiteral( ":/images/shadowchat/shadowchat-mark-source.png" ) )
                               .scaled( 94, 94, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
  QLabel* placeholder_title = new QLabel( QStringLiteral( "局域网里的沟通，从这里开始" ), welcome_card );
  placeholder_title->setObjectName( "ShadowPlaceholderTitle" );
  placeholder_title->setAlignment( Qt::AlignCenter );
  QLabel* placeholder_hint = new QLabel( QStringLiteral( "从通讯录找到同事，或搜索一个已有会话" ), welcome_card );
  placeholder_hint->setObjectName( "ShadowPlaceholderHint" );
  placeholder_hint->setAlignment( Qt::AlignCenter );
  QLabel* privacy_pill = new QLabel( QStringLiteral( "聊天和文件在局域网内直连" ), welcome_card );
  privacy_pill->setObjectName( "ShadowPrivacyPill" );
  privacy_pill->setAlignment( Qt::AlignCenter );
  QPushButton* open_directory = new QPushButton( QStringLiteral( "打开通讯录" ), welcome_card );
  open_directory->setObjectName( "ShadowPrimaryButton" );
  open_directory->setAccessibleName( QStringLiteral( "打开通讯录" ) );
  connect( open_directory, SIGNAL( clicked() ), this, SLOT( showModernContacts() ) );

  welcome_layout->addWidget( placeholder_icon );
  welcome_layout->addSpacing( 8 );
  welcome_layout->addWidget( placeholder_title );
  welcome_layout->addWidget( placeholder_hint );
  welcome_layout->addSpacing( 12 );
  welcome_layout->addWidget( privacy_pill, 0, Qt::AlignHCenter );
  welcome_layout->addSpacing( 6 );
  welcome_layout->addWidget( open_directory, 0, Qt::AlignHCenter );
  placeholder_layout->addWidget( welcome_card );

  mp_modernChatPage = new QWidget( mp_modernConversationStack );
  mp_modernChatPage->setObjectName( "ShadowChatPage" );
  QVBoxLayout* chat_page_layout = new QVBoxLayout( mp_modernChatPage );
  chat_page_layout->setContentsMargins( 0, 0, 0, 0 );
  chat_page_layout->setSpacing( 0 );
  QWidget* chat_header = new QWidget( mp_modernChatPage );
  chat_header->setObjectName( "ShadowChatHeader" );
  chat_header->setFixedHeight( 72 );
  QHBoxLayout* chat_header_layout = new QHBoxLayout( chat_header );
  chat_header_layout->setContentsMargins( 16, 10, 24, 10 );
  chat_header_layout->setSpacing( 10 );
  mp_modernBackButton = new QToolButton( chat_header );
  mp_modernBackButton->setObjectName( "ShadowBackButton" );
  mp_modernBackButton->setIcon( ShadowStyle::actionIcon( QStringLiteral( "arrow-left" ) ) );
  mp_modernBackButton->setIconSize( QSize( 20, 20 ) );
  mp_modernBackButton->setToolTip( QStringLiteral( "返回消息列表" ) );
  mp_modernBackButton->setAccessibleName( QStringLiteral( "返回消息列表" ) );
  mp_modernBackButton->hide();
  connect( mp_modernBackButton, SIGNAL( clicked() ), this, SLOT( showModernListPanel() ) );
  chat_header_layout->addWidget( mp_modernBackButton );
  QVBoxLayout* chat_title_layout = new QVBoxLayout;
  chat_title_layout->setContentsMargins( 0, 0, 0, 0 );
  chat_title_layout->setSpacing( 2 );
  mp_modernChatTitle = new QLabel( chat_header );
  mp_modernChatTitle->setObjectName( "ShadowChatTitle" );
  mp_modernChatSubtitle = new QLabel( chat_header );
  mp_modernChatSubtitle->setObjectName( "ShadowChatSubtitle" );
  chat_title_layout->addWidget( mp_modernChatTitle );
  chat_title_layout->addWidget( mp_modernChatSubtitle );
  chat_header_layout->addLayout( chat_title_layout, 1 );
  chat_page_layout->addWidget( chat_header );

  mp_modernChatBody = new QWidget( mp_modernChatPage );
  mp_modernChatBody->setObjectName( "ShadowChatBody" );
  mp_modernChatBodyLayout = new QVBoxLayout( mp_modernChatBody );
  mp_modernChatBodyLayout->setContentsMargins( 0, 0, 0, 0 );
  mp_modernChatBodyLayout->setSpacing( 0 );
  chat_page_layout->addWidget( mp_modernChatBody, 1 );

  mp_modernConversationStack->addWidget( mp_modernChatPlaceholder );
  mp_modernConversationStack->addWidget( mp_modernChatPage );
  conversation_layout->addWidget( mp_modernConversationStack );

  mp_modernContentSplitter = new QSplitter( Qt::Horizontal, mp_modernRoot );
  mp_modernContentSplitter->setObjectName( "ShadowContentSplitter" );
  mp_modernContentSplitter->setChildrenCollapsible( false );
  mp_modernContentSplitter->setHandleWidth( 1 );
  mp_modernContentSplitter->addWidget( mp_modernListPanel );
  mp_modernContentSplitter->addWidget( mp_modernConversationPanel );
  mp_modernContentSplitter->setStretchFactor( 0, 0 );
  mp_modernContentSplitter->setStretchFactor( 1, 1 );
  mp_modernContentSplitter->setSizes( QList<int>() << 320 << 820 );

  content_layout->addWidget( mp_modernNavigationRail );
  content_layout->addWidget( mp_modernContentSplitter, 1 );
  root_layout->addWidget( shell_content, 1 );
  setCentralWidget( mp_modernRoot );

  if( legacy_central_widget && legacy_central_widget != mp_tabMain )
    legacy_central_widget->deleteLater();

  menuBar()->hide();
  mp_barMain->hide();
  statusBar()->hide();
  mp_dockFileTransfers->hide();
  mp_modernConversationStack->setCurrentWidget( mp_modernChatPlaceholder );
  setTabOrder( mp_modernMessagesButton, mp_modernContactsButton );
  setTabOrder( mp_modernContactsButton, mp_modernMoreButton );
  setTabOrder( mp_modernMoreButton, mp_modernProfileButton );
  setTabOrder( mp_modernProfileButton, mp_modernSearch );
  setTabOrder( mp_modernSearch, mp_modernPeopleButton );
  setTabOrder( mp_modernPeopleButton, mp_modernGroupsButton );
  setTabOrder( mp_modernGroupsButton, mp_tabMain );
  showModernMessages();
  applyModernStyle();
  updateModernNetworkState();
  updateModernListState();
  updateModernResponsiveLayout();
}

void GuiMain::applyModernStyle()
{
  const QFont modern_font = ShadowStyle::applicationFont();
  QApplication::setFont( modern_font );
  setFont( modern_font );

  foreach( QTreeWidget* tree, findChildren<QTreeWidget*>( QStringLiteral( "GuiCustomList" ) ) )
  {
    tree->setStyleSheet( QString() );
    tree->setFrameShape( QFrame::NoFrame );
    tree->setUniformRowHeights( true );
  }

  setStyleSheet( ShadowStyle::applicationStyleSheet() );
}

void GuiMain::updateModernResponsiveLayout()
{
  if( !mp_modernContentSplitter || !mp_modernListPanel || !mp_modernConversationPanel )
    return;

  const bool compact = width() < 900;
  if( mp_modernNavigationRail )
    mp_modernNavigationRail->setFixedWidth( compact ? 68 : 80 );

  if( !compact )
  {
    mp_modernListPanel->show();
    mp_modernConversationPanel->show();
    mp_modernContentSplitter->setHandleWidth( 1 );
    if( mp_modernBackButton )
      mp_modernBackButton->hide();
    return;
  }

  mp_modernContentSplitter->setHandleWidth( 0 );
  const bool chat_is_open = mp_modernConversationStack &&
                            mp_modernConversationStack->currentWidget() == mp_modernChatPage &&
                            mp_modernConversationPanel->isVisible();
  mp_modernListPanel->setVisible( !chat_is_open );
  mp_modernConversationPanel->setVisible( chat_is_open );
  if( mp_modernBackButton )
    mp_modernBackButton->setVisible( chat_is_open );
}

void GuiMain::showModernListPanel()
{
  if( width() >= 900 || !mp_modernListPanel || !mp_modernConversationPanel )
    return;

  mp_modernConversationPanel->hide();
  mp_modernListPanel->show();
  if( mp_modernBackButton )
    mp_modernBackButton->hide();
  if( mp_modernSearch )
    mp_modernSearch->setFocus();
}

void GuiMain::showShadowSettings()
{
  QDialog dialog( this );
  dialog.setWindowTitle( QStringLiteral( "影聊设置" ) );
  dialog.setModal( true );
  dialog.setMinimumWidth( 620 );
  ShadowDialogStyle::apply(&dialog);
  dialog.setStyleSheet(ShadowStyle::applicationStyleSheet()+dialog.styleSheet());
  Bee::removeContextHelpButton( &dialog );

  QVBoxLayout* root_layout = new QVBoxLayout( &dialog );
  root_layout->setContentsMargins( 28, 24, 28, 22 );
  root_layout->setSpacing( 14 );
  QLabel* title = new QLabel( QStringLiteral( "设置" ), &dialog );
  title->setObjectName( "ShadowOnboardingTitle" );
  title->setProperty("role","title");
  QLabel* subtitle = new QLabel( QStringLiteral( "日常会用到的选项都在这里；网络端口等低频项目收在“高级设置”中。" ), &dialog );
  subtitle->setObjectName( "ShadowOnboardingSubtitle" );
  subtitle->setProperty("role","hint");
  subtitle->setWordWrap( true );
  root_layout->addWidget( title );
  root_layout->addWidget( subtitle );

  QFrame* message_card = new QFrame( &dialog );
  message_card->setObjectName( "ShadowOnboardingCard" );
  QVBoxLayout* message_layout = new QVBoxLayout( message_card );
  message_layout->setContentsMargins( 18, 14, 18, 14 );
  message_layout->setSpacing( 8 );
  QLabel* message_title = new QLabel( QStringLiteral( "消息与提醒" ), message_card );
  message_title->setObjectName( "ShadowOnboardingStep" );
  QCheckBox* sound_check = new QCheckBox( QStringLiteral( "收到新消息时播放提示音" ), message_card );
  sound_check->setChecked( Settings::instance().beepOnNewMessageArrived() );
  QCheckBox* preview_check = new QCheckBox( QStringLiteral( "系统通知中显示消息内容" ), message_card );
  preview_check->setChecked( Settings::instance().showChatMessageOnTray() );
  preview_check->setToolTip( QStringLiteral( "关闭后，锁屏或共享屏幕时不会在系统通知里显示正文。" ) );
  QCheckBox* enter_check = new QCheckBox( QStringLiteral( "按 Enter 发送消息（关闭时使用 Ctrl+Enter 发送）" ), message_card );
  enter_check->setChecked( Settings::instance().useReturnToSendMessage() );
  message_layout->addWidget( message_title );
  message_layout->addWidget( sound_check );
  message_layout->addWidget( preview_check );
  message_layout->addWidget( enter_check );
  root_layout->addWidget( message_card );

  QFrame* file_card = new QFrame( &dialog );
  file_card->setObjectName( "ShadowOnboardingCard" );
  QGridLayout* file_layout = new QGridLayout( file_card );
  file_layout->setContentsMargins( 18, 14, 18, 14 );
  file_layout->setHorizontalSpacing( 8 );
  file_layout->setVerticalSpacing( 8 );
  QLabel* file_title = new QLabel( QStringLiteral( "接收文件" ), file_card );
  file_title->setObjectName( "ShadowOnboardingStep" );
  file_layout->addWidget( file_title, 0, 0, 1, 2 );
  QCheckBox* confirm_download_check = new QCheckBox( QStringLiteral( "接收前先询问保存位置" ), file_card );
  confirm_download_check->setChecked( Settings::instance().confirmOnDownloadFile() );
  file_layout->addWidget( confirm_download_check, 1, 0, 1, 2 );
  QLineEdit* download_folder = new QLineEdit( Settings::instance().downloadDirectory(), file_card );
  download_folder->setReadOnly( true );
  download_folder->setAccessibleName( QStringLiteral( "默认接收文件夹" ) );
  QPushButton* choose_folder = new QPushButton( QStringLiteral( "更改…" ), file_card );
  connect( choose_folder, &QPushButton::clicked, &dialog, [&dialog, download_folder]() {
    const QString selected = QFileDialog::getExistingDirectory( &dialog, QStringLiteral( "选择默认接收文件夹" ), download_folder->text() );
    if( !selected.isEmpty() )
      download_folder->setText( QDir::toNativeSeparators( selected ) );
  } );
  file_layout->addWidget( download_folder, 2, 0 );
  file_layout->addWidget( choose_folder, 2, 1 );
  root_layout->addWidget( file_card );

  QFrame* security_card = new QFrame( &dialog );
  security_card->setObjectName( "ShadowOnboardingTrust" );
  QVBoxLayout* security_layout = new QVBoxLayout( security_card );
  security_layout->setContentsMargins( 18, 14, 18, 14 );
  security_layout->setSpacing( 7 );
  QLabel* security_title = new QLabel( QStringLiteral( "安全与更新" ), security_card );
  security_title->setObjectName( "ShadowOnboardingStep" );
  QCheckBox* update_check = new QCheckBox( QStringLiteral( "启动时及运行中每 5 分钟强制校验版本" ), security_card );
  update_check->setChecked( true );
  update_check->setEnabled( false );
  update_check->setToolTip( QStringLiteral( "只访问影聊的静态版本清单，不上传姓名、联系人、文件或使用数据。" ) );
  QLabel* security_text = new QLabel( QStringLiteral( "聊天、文件和通讯录留在局域网；第三方插件、云端转发和从聊天中运行程序已关闭。" ), security_card );
  security_text->setWordWrap( true );
  security_text->setObjectName( "ShadowOnboardingBody" );
  QPushButton* version_page = new QPushButton( QStringLiteral( "查看最新版本" ), security_card );
  version_page->setMaximumWidth( 120 );
  connect( version_page, SIGNAL( clicked() ), this, SLOT( checkNewVersion() ) );
  security_layout->addWidget( security_title );
  security_layout->addWidget( update_check );
  security_layout->addWidget( security_text );
  security_layout->addWidget( version_page, 0, Qt::AlignLeft );
  root_layout->addWidget( security_card );

  QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog );
  buttons->setLayoutDirection(Qt::RightToLeft);
  buttons->button( QDialogButtonBox::Save )->setText( QStringLiteral( "保存" ) );
  buttons->button( QDialogButtonBox::Save )->setObjectName( "ShadowPrimaryButton" );
  buttons->button( QDialogButtonBox::Save )->setProperty("role","primary");
  buttons->button( QDialogButtonBox::Save )->setStyleSheet(QStringLiteral("QPushButton { background:#2463EB; color:white; border:1px solid #2463EB; border-radius:7px; padding:6px 18px; } QPushButton:hover { background:#1D52CB; }"));
  buttons->button( QDialogButtonBox::Save )->setDefault(true);
  buttons->button( QDialogButtonBox::Cancel )->setText( QStringLiteral( "取消" ) );
  connect( buttons, SIGNAL( accepted() ), &dialog, SLOT( accept() ) );
  connect( buttons, SIGNAL( rejected() ), &dialog, SLOT( reject() ) );
  root_layout->addWidget( buttons, 0, Qt::AlignRight );

  if( dialog.exec() != QDialog::Accepted )
    return;

  Settings::instance().setBeepOnNewMessageArrived( sound_check->isChecked() );
  Settings::instance().setShowChatMessageOnTray( preview_check->isChecked() );
  Settings::instance().setUseReturnToSendMessage( enter_check->isChecked() );
  Settings::instance().setConfirmOnDownloadFile( confirm_download_check->isChecked() );
  Settings::instance().setDownloadDirectory( QDir::toNativeSeparators( download_folder->text() ) );
  Settings::instance().setCheckNewVersionAtStartup( update_check->isChecked() );
  Settings::instance().save();
  if( mp_actBeepOnNewMessage )
    mp_actBeepOnNewMessage->setChecked( sound_check->isChecked() );
  foreach( GuiFloatingChat* floating_chat, m_floatingChats )
    floating_chat->guiChat()->updateActionsOnFocusChanged();
}

void GuiMain::exportShadowWorkspace()
{
  QMetaObject::invokeMethod( beeCore, "saveUsersAndGroups", Qt::DirectConnection );
  MessageManager::instance().saveMessages( true );
  Settings::instance().save();

  const QString default_name = QStringLiteral( "影聊工作区-%1.%2" )
                               .arg( QDate::currentDate().toString( QStringLiteral( "yyyyMMdd" ) ), ShadowWorkspaceArchive::fileExtension() );
  const QString default_path = QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( default_name );
  const QString file_path = QFileDialog::getSaveFileName( this, QStringLiteral( "导出影聊工作区" ), default_path,
                                                          QStringLiteral( "影聊工作区 (*.%1)" ).arg( ShadowWorkspaceArchive::fileExtension() ) );
  if( file_path.isEmpty() )
    return;

  QString target_path = file_path;
  if( !target_path.endsWith( QLatin1Char( '.' ) + ShadowWorkspaceArchive::fileExtension(), Qt::CaseInsensitive ) )
    target_path += QLatin1Char( '.' ) + ShadowWorkspaceArchive::fileExtension();
  QString error_message;
  const qint64 attachment_bytes = ShadowWorkspaceArchive::attachmentSize( Settings::instance().downloadDirectory() );
  const QMessageBox::StandardButton choice = QMessageBox::question( this, QStringLiteral( "备份附件" ),
    QStringLiteral( "是否包含接收的图片和文件？预计附件大小：%1 GB。\n选择“否”仅备份账号资料和聊天记录。" ).arg( double( attachment_bytes ) / (1024.0*1024*1024), 0, 'f', 2 ),
    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No );
  if( choice == QMessageBox::Cancel ) return;
  QApplication::setOverrideCursor( Qt::WaitCursor );
  const bool exported = choice == QMessageBox::Yes
    ? ShadowWorkspaceArchive::exportWithAttachments( Settings::instance().dataFolder(), Settings::instance().downloadDirectory(), target_path, &error_message )
    : ShadowWorkspaceArchive::exportWorkspace( Settings::instance().dataFolder(), target_path, &error_message );
  QApplication::restoreOverrideCursor();
  if( !exported )
  {
    QMessageBox::critical( this, QStringLiteral( "导出失败" ), error_message );
    return;
  }

  QMessageBox::information( this, QStringLiteral( "工作区已导出" ),
                            QStringLiteral( "个人资料、联系人、群聊、设置和聊天记录已保存。\n\n下载的附件、缓存、日志和插件没有包含在工作区中。" ) );
}

void GuiMain::importShadowWorkspace()
{
  const QString file_path = QFileDialog::getOpenFileName( this, QStringLiteral( "导入影聊工作区" ),
                                                          QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ),
                                                          QStringLiteral( "影聊工作区 (*.%1)" ).arg( ShadowWorkspaceArchive::fileExtension() ) );
  if( file_path.isEmpty() )
    return;

  if( QMessageBox::question( this, QStringLiteral( "导入工作区" ),
                             QStringLiteral( "下次启动时将用这个工作区替换当前的个人资料、联系人、群聊、设置和聊天记录。\n\n影聊会先创建恢复点，附件不会被修改。继续吗？" ),
                             QStringLiteral( "继续导入" ), QStringLiteral( "取消" ), QString(), 1, 1 ) != 0 )
    return;

  QString error_message;
  if( !ShadowWorkspaceArchive::stageImport( Settings::instance().dataFolder(), file_path, &error_message ) )
  {
    QMessageBox::critical( this, QStringLiteral( "导入失败" ), error_message );
    return;
  }

  QMessageBox::information( this, QStringLiteral( "工作区已准备好" ),
                            QStringLiteral( "文件已通过完整性校验。关闭并重新打开影聊后生效；导入前会自动创建当前工作区恢复点。" ) );
}

void GuiMain::updateModernNavigation()
{
  if( !mp_modernMessagesButton )
    return;
  mp_modernMessagesButton->setChecked( m_modernPrimaryPage == 0 );
  mp_modernContactsButton->setChecked( m_modernPrimaryPage == 1 || m_modernPrimaryPage == 2 );
  mp_modernPeopleButton->setChecked( m_modernPrimaryPage == 1 );
  mp_modernGroupsButton->setChecked( m_modernPrimaryPage == 2 );
  mp_modernDirectoryTabs->setVisible( m_modernPrimaryPage == 1 || m_modernPrimaryPage == 2 );
  updateModernListState();
}

void GuiMain::updateModernNetworkState()
{
  if( !mp_modernNetworkStrip || !mp_modernNetworkText || !mp_modernNetworkAction )
    return;

  QString state;
  if( !NetworkManager::instance().isMainInterfaceUp() )
  {
    state = QStringLiteral( "offline" );
    mp_modernNetworkText->setText( QStringLiteral( "未检测到可用网络，请检查网线或 Wi-Fi" ) );
    mp_modernNetworkAction->show();
  }
  else if( beeCore->isConnected() )
  {
    state = QStringLiteral( "connected" );
    const int peers = beeCore->connectedUsers();
    mp_modernNetworkText->setText( peers > 0
      ? QStringLiteral( "已连接局域网 · %1 位同事在线" ).arg( peers )
      : QStringLiteral( "已连接局域网，正在发现同事" ) );
    mp_modernNetworkAction->hide();
  }
  else if( m_coreIsConnecting )
  {
    state = QStringLiteral( "connecting" );
    mp_modernNetworkText->setText( QStringLiteral( "正在连接局域网…" ) );
    mp_modernNetworkAction->hide();
  }
  else
  {
    state = QStringLiteral( "offline" );
    mp_modernNetworkText->setText( QStringLiteral( "局域网连接已断开" ) );
    mp_modernNetworkAction->show();
  }

  mp_modernNetworkStrip->setProperty( "state", state );
  mp_modernNetworkDot->setProperty( "state", state );
  mp_modernNetworkStrip->style()->unpolish( mp_modernNetworkStrip );
  mp_modernNetworkStrip->style()->polish( mp_modernNetworkStrip );
  mp_modernNetworkDot->style()->unpolish( mp_modernNetworkDot );
  mp_modernNetworkDot->style()->polish( mp_modernNetworkDot );
}

void GuiMain::updateModernListState()
{
  if( !mp_modernListStack || !mp_modernEmptyState || !mp_modernSearch )
    return;

  if( !mp_modernSearch->text().trimmed().isEmpty() )
  {
    mp_modernListStack->setCurrentWidget( mp_modernSearchResults );
    return;
  }

  bool show_empty_state = false;
  bool has_known_colleague = false;
  foreach(const User& user,UserManager::instance().userList().toList())
    if(user.isValid()&&!user.isLocal()){has_known_colleague=true;break;}
  if( m_modernPrimaryPage == 0 && ChatManager::instance().countNotEmptyChats( false ) == 0 )
  {
    show_empty_state = true;
    mp_modernEmptyTitle->setText( QStringLiteral( "还没有最近消息" ) );
    mp_modernEmptyHint->setText( QStringLiteral( "从通讯录找到同事，发出第一条消息。" ) );
    mp_modernEmptyAction->setText( QStringLiteral( "打开通讯录" ) );
  }
  else if( m_modernPrimaryPage == 1 && !has_known_colleague )
  {
    show_empty_state = true;
    if( m_coreIsConnecting )
    {
      mp_modernEmptyTitle->setText( QStringLiteral( "正在发现同事" ) );
      mp_modernEmptyHint->setText( QStringLiteral( "影聊正在扫描当前局域网，请稍候。" ) );
      mp_modernEmptyAction->hide();
    }
    else if( beeCore->isConnected() )
    {
      mp_modernEmptyTitle->setText( QStringLiteral( "还没有发现同事" ) );
      mp_modernEmptyHint->setText( QStringLiteral( "请确认双方在同一局域网，并允许影聊通过防火墙。" ) );
      mp_modernEmptyAction->setText( QStringLiteral( "重新扫描" ) );
      mp_modernEmptyAction->show();
    }
    else
    {
      mp_modernEmptyTitle->setText( QStringLiteral( "暂时无法查看通讯录" ) );
      mp_modernEmptyHint->setText( QStringLiteral( "恢复局域网连接后即可发现同事。" ) );
      mp_modernEmptyAction->setText( QStringLiteral( "重新连接" ) );
      mp_modernEmptyAction->show();
    }
  }
  else if( m_modernPrimaryPage == 2 && ChatManager::instance().countGroupChats() == 0 )
  {
    show_empty_state = true;
    mp_modernEmptyTitle->setText( QStringLiteral( "还没有群聊" ) );
    mp_modernEmptyHint->setText( QStringLiteral( "创建一个群聊，和多位同事一起沟通。" ) );
    mp_modernEmptyAction->setText( QStringLiteral( "创建群聊" ) );
    mp_modernEmptyAction->show();
  }

  if( m_modernPrimaryPage != 1 || !m_coreIsConnecting )
    mp_modernEmptyAction->show();
  mp_modernListStack->setCurrentWidget( show_empty_state ? mp_modernEmptyState : mp_tabMain );
}

void GuiMain::showModernMessages()
{
  m_modernPrimaryPage = 0;
  mp_tabMain->setCurrentWidget( mp_chatList );
  mp_modernSectionTitle->setText( QStringLiteral( "最近消息" ) );
  mp_modernSectionHint->setText( QStringLiteral( "你最近联系的同事和群聊" ) );
  updateModernNavigation();
  showModernListPanel();
}

void GuiMain::showModernContacts()
{
  m_modernPrimaryPage = 1;
  mp_tabMain->setCurrentWidget( mp_userList );
  mp_modernSectionTitle->setText( QStringLiteral( "通讯录" ) );
  mp_modernSectionHint->setText( QStringLiteral( "查找当前局域网中的同事" ) );
  updateModernNavigation();
  showModernListPanel();
}

void GuiMain::showModernGroups()
{
  m_modernPrimaryPage = 2;
  mp_tabMain->setCurrentWidget( mp_groupList );
  mp_modernSectionTitle->setText( QStringLiteral( "群聊" ) );
  mp_modernSectionHint->setText( QStringLiteral( "查看和创建局域网群聊" ) );
  updateModernNavigation();
  showModernListPanel();
}

void GuiMain::showModernActivities()
{
  m_modernPrimaryPage = 3;
  mp_tabMain->setCurrentWidget( mp_home );
  mp_modernSectionTitle->setText( QStringLiteral( "系统活动" ) );
  mp_modernSectionHint->setText( QStringLiteral( "连接状态与诊断信息" ) );
  updateModernNavigation();
}

void GuiMain::showModernSavedChats()
{
  m_modernPrimaryPage = 4;
  mp_tabMain->setCurrentWidget( mp_savedChatList );
  mp_modernSectionTitle->setText( QStringLiteral( "聊天存档" ) );
  mp_modernSectionHint->setText( QStringLiteral( "保存在本机的历史记录" ) );
  updateModernNavigation();
}

void GuiMain::onModernSearchChanged( const QString& text )
{
  if( text.trimmed().isEmpty() )
  {
    updateModernListState();
    return;
  }
  updateModernSearchResults();
  mp_modernListStack->setCurrentWidget( mp_modernSearchResults );
}

void GuiMain::updateModernSearchResults()
{
  if( !mp_modernSearchResults || !mp_modernSearch )
    return;

  const QString query = mp_modernSearch->text().trimmed();
  mp_modernSearchResults->clear();
  if( query.isEmpty() )
    return;

  QTreeWidgetItem* chats_section = new QTreeWidgetItem( mp_modernSearchResults, QStringList( QStringLiteral( "会话" ) ) );
  chats_section->setFlags( Qt::ItemIsEnabled );
  chats_section->setIcon( 0, ShadowStyle::actionIcon( QStringLiteral( "chats-circle" ) ) );
  int chats_found = 0;
  foreach( Chat c, ChatManager::instance().constChatList() )
  {
    if( !c.isValid() || !c.name().contains( query, Qt::CaseInsensitive ) )
      continue;
    QTreeWidgetItem* item = new QTreeWidgetItem( chats_section, QStringList( c.isDefault() ? QStringLiteral( "全员群" ) : c.name() ) );
    item->setIcon( 0, ShadowStyle::actionIcon( c.isGroup() || c.isDefault()
                                               ? QStringLiteral( "users-three" )
                                               : QStringLiteral( "chats-circle" ) ) );
    item->setData( 0, Qt::UserRole, 1 );
    item->setData( 0, Qt::UserRole + 1, QVariant::fromValue<qulonglong>( c.id() ) );
    ++chats_found;
  }
  chats_section->setText( 0, QStringLiteral( "会话 · %1" ).arg( chats_found ) );
  chats_section->setExpanded( true );

  QTreeWidgetItem* users_section = new QTreeWidgetItem( mp_modernSearchResults, QStringList( QStringLiteral( "同事" ) ) );
  users_section->setFlags( Qt::ItemIsEnabled );
  users_section->setIcon( 0, ShadowStyle::actionIcon( QStringLiteral( "users-three" ) ) );
  int users_found = 0;
  foreach( User u, UserManager::instance().userList().toList() )
  {
    if( !u.isValid() || u.isLocal() )
      continue;
    const bool matches = u.name().contains( query, Qt::CaseInsensitive ) ||
                         u.vCard().fullName( Settings::instance().useUserFirstNameFirstInFullName() ).contains( query, Qt::CaseInsensitive ) ||
                         u.accountName().contains( query, Qt::CaseInsensitive );
    if( !matches )
      continue;
    QString label = u.name();
    if( !u.isStatusConnected() )
      label.append( QStringLiteral( " · 离线" ) );
    QTreeWidgetItem* item = new QTreeWidgetItem( users_section, QStringList( label ) );
    item->setIcon( 0, IconManager::instance().icon( u.isStatusConnected() ? "user-online.png" : "user-offline.png" ) );
    item->setData( 0, Qt::UserRole, 2 );
    item->setData( 0, Qt::UserRole + 1, QVariant::fromValue<qulonglong>( u.id() ) );
    ++users_found;
  }
  users_section->setText( 0, QStringLiteral( "同事 · %1" ).arg( users_found ) );
  users_section->setExpanded( true );

  if( chats_found == 0 && users_found == 0 )
  {
    QTreeWidgetItem* no_results = new QTreeWidgetItem( mp_modernSearchResults,
                                                        QStringList( QStringLiteral( "没有找到“%1”，试试姓名或群聊名称" ).arg( query ) ) );
    no_results->setFlags( Qt::ItemIsEnabled );
  }
}

void GuiMain::onModernSearchItemActivated( QTreeWidgetItem* item, int )
{
  if( !item )
    return;
  const int item_type = item->data( 0, Qt::UserRole ).toInt();
  const VNumber item_id = static_cast<VNumber>( item->data( 0, Qt::UserRole + 1 ).toULongLong() );
  if( item_type == 1 )
    showChat( item_id );
  else if( item_type == 2 )
    checkUserSelected( item_id );
  else
    return;
  mp_modernSearch->clear();
  showModernMessages();
}

void GuiMain::updateModernChatHeader( const Chat& c )
{
  if( !mp_modernChatTitle || !c.isValid() )
    return;

  mp_modernChatTitle->setText( c.isDefault() ? QStringLiteral( "全员群" ) : c.name() );
  if( c.isPrivate() )
  {
    User u = UserManager::instance().findUser( c.privateUserId() );
    mp_modernChatSubtitle->setText( u.isValid() && u.isStatusConnected()
                                    ? QStringLiteral( "在线 · 局域网直连" )
                                    : QStringLiteral( "离线 · 仅保存在本机与局域网" ) );
  }
  else
  {
    mp_modernChatSubtitle->setText( QStringLiteral( "%1 位成员 · 局域网群聊" ).arg( c.usersId().size() ) );
  }
}

void GuiMain::createMainWidgets()
{
  int tab_index;
  QString tooltip_right_button = tr( "Right click to open menu" );

  mp_home = new GuiHome( this );
  connect( mp_home, SIGNAL( openUrlRequest( const QUrl& ) ), this, SLOT( openUrl( const QUrl& ) ) );
  connect( mp_home, SIGNAL( clearSystemMessagesRequest( VNumber ) ), this, SLOT( clearSystemMessagesInChat( VNumber ) ) );
  tab_index = mp_tabMain->addTab( mp_home, IconManager::instance().icon( "activities.png" ), "" );
  mp_tabMain->setTabToolTip( tab_index, tr( "Activities" ) );
  mp_home->setMainToolTip( QString( "%1\n(%2)" ).arg( mp_tabMain->tabToolTip( tab_index ), tooltip_right_button ) );

  mp_userList = new GuiUserList( this );
  tab_index = mp_tabMain->addTab( mp_userList, IconManager::instance().icon( "user-list.png" ), "" );
  mp_tabMain->setTabToolTip( tab_index, tr( "Users" ) );
  mp_userList->setMainToolTip( QString( "%1\n(%2)" ).arg( mp_tabMain->tabToolTip( tab_index ), tooltip_right_button ) );

  mp_chatList = new GuiChatList( this );
  tab_index = mp_tabMain->addTab( mp_chatList, IconManager::instance().icon( "chat-list.png" ), "" );
  mp_tabMain->setTabToolTip( tab_index, tr( "Chats" ) );
  mp_chatList->setMainToolTip( QString( "%1\n(%2)" ).arg( mp_tabMain->tabToolTip( tab_index ), tooltip_right_button ) );

  mp_groupList = new GuiGroupList( this );
  tab_index = mp_tabMain->addTab( mp_groupList, IconManager::instance().icon( "group.png" ), "" );
  mp_tabMain->setTabToolTip( tab_index, tr( "Groups" ) );
  mp_groupList->setMainToolTip( QString( "%1\n(%2)" ).arg( mp_tabMain->tabToolTip( tab_index ), tooltip_right_button ) );

  mp_savedChatList = new GuiSavedChatList( this );
  tab_index = mp_tabMain->addTab( mp_savedChatList, IconManager::instance().icon( "saved-chat-list.png" ), "" );
  mp_tabMain->setTabToolTip( tab_index, tr( "Saved chats" ) );
  mp_savedChatList->setMainToolTip( QString( "%1\n(%2)" ).arg( mp_tabMain->tabToolTip( tab_index ), tooltip_right_button ) );

#ifdef BEEBEEP_USE_WEBENGINE
  mp_webView = new GuiWebView();
  connect( mp_webView, SIGNAL( newsLoadFinished( bool ) ), this, SLOT( onNewsLoad( bool ) ) );
#endif

  mp_dockFileTransfers = new QDockWidget( QStringLiteral("文件传输"), this );
  mp_dockFileTransfers->setObjectName( "GuiFileTransferDock" );
  mp_fileTransfer = new GuiFileTransfer( this );
  mp_dockFileTransfers->setWidget( mp_fileTransfer );
  mp_dockFileTransfers->setAllowedAreas( Qt::AllDockWidgetAreas );
  addDockWidget( Qt::BottomDockWidgetArea, mp_dockFileTransfers );
  mp_actViewFileTransfer = mp_dockFileTransfers->toggleViewAction();
  mp_actViewFileTransfer->setIcon( IconManager::instance().icon( "file-transfer.png" ) );
  mp_actViewFileTransfer->setText( tr( "Show the file transfer panel" ) );
  mp_actViewFileTransfer->setData( 99 );
  mp_dockFileTransfers->hide();
}

void GuiMain::startExternalApplicationFromActionData()
{
#ifdef SHADOWCHAT_SAFE_BUILD
  QMessageBox::information( qApp->activeWindow(), QStringLiteral( "安全模式" ),
                            QStringLiteral( "影聊官方构建不允许从应用内启动外部程序。" ) );
  return;
#else
  QAction* act = qobject_cast<QAction*>( sender() );
  if( !act )
    return;

  QString application_path = act->data().toString();
  qDebug() << "Starting external application:" << qPrintable( application_path );
  if( !QDesktopServices::openUrl( QUrl::fromLocalFile( application_path ) ) )
    QMessageBox::information( qApp->activeWindow(), Settings::instance().programName(), tr( "Unable to open %1" ).arg( application_path ), tr( "Ok" ) );
#endif
}

void GuiMain::settingsChanged()
{
  QAction* act = qobject_cast<QAction*>( sender() );
  if( act )
    settingsChanged( act );
}

void GuiMain::settingsChanged( QAction* act )
{
  bool refresh_users = false;
  bool refresh_chat = false;
  int settings_data_id = act->data().toInt();
  bool ok = false;

#ifdef BEEBEEP_DEBUG
  if( act->isCheckable() )
    qDebug() << "Settings changed for action id" << settings_data_id << "to" << static_cast<int>(act->isChecked());
#endif

  switch( settings_data_id )
  {
  case 1:
    Settings::instance().setChatCompact( act->isChecked() );
    refresh_chat = true;
    break;
  case 2:
    Settings::instance().setSaveGroupList( act->isChecked() );
    break;
  case 3:
    Settings::instance().setChatShowMessageTimestamp( act->isChecked() );
    refresh_chat = true;
    break;
  case 4:
    Settings::instance().setUseNativeDialogs( act->isChecked() );
    break;
  case 5:
    setFileSharingEnabled( act->isChecked() );
    break;
  case 6:
    Settings::instance().setShowOnlyOnlineUsers( act->isChecked() );
    refresh_users = true;
    refresh_chat = true;
    break;
  case 7:
    {
      Settings::instance().setShowChatsInOneWindow( act->isChecked() );
      if( Settings::instance().showChatsInOneWindow() )
      {
        foreach( GuiFloatingChat* fl_chat, m_floatingChats )
          fl_chat->close();
      }
    }
    break;
  case 8:
    Settings::instance().setChatUseHtmlTags( act->isChecked() );
    refresh_chat = true;
    break;
  case 9:
    Settings::instance().setChatUseClickableLinks( act->isChecked() );
    refresh_chat = true;
    break;
  case 10:
    Settings::instance().setShowEmoticons( act->isChecked() );
    refresh_chat = true;
    break;
  case 11:
    Settings::instance().setCloseMinimizeInTray( act->isChecked() );
    break;
  case 12:
    setFileTransferEnabled( act->isChecked() );
    break;
  case 13:
    Settings::instance().setShowMessagesGroupByUser( act->isChecked() );
    refresh_chat = true;
    break;
  case 14:
    {
      Settings::instance().setStayOnTop( act->isChecked() );
      Bee::setWindowStaysOnTop( this, act->isChecked() );
      foreach( GuiFloatingChat* fl_chat, m_floatingChats )
        Bee::setWindowStaysOnTop( fl_chat, act->isChecked() );
      if( mp_fileSharing )
        Bee::setWindowStaysOnTop( mp_fileSharing, act->isChecked() );
      if( mp_log )
        Bee::setWindowStaysOnTop( mp_log, act->isChecked() );
      if( mp_screenShot )
        Bee::setWindowStaysOnTop( mp_screenShot, act->isChecked() );
      if( mp_networkTest )
        Bee::setWindowStaysOnTop( mp_networkTest, act->isChecked() );
    }
    break;
  case 15:
    Settings::instance().setRaiseOnNewMessageArrived( act->isChecked() );
    break;
  case 16:
    checkAutoStartOnBoot( act->isChecked() );
    break;
  case 17:
    {
      if( !act->isChecked() )
      {
        Settings::instance().setAskPasswordAtStartup( false );
        if( Settings::instance().askPassword() )
        {
          QMessageBox::information( this, Settings::instance().programName(), tr( "Please save the network password in the next dialog if you want to use password without prompt." ) );
          promptConnectionPassword();
          return;
        }
      }
      else
        Settings::instance().setAskPasswordAtStartup( true );
    }
    break;
  case 18:
    {
      Settings::instance().setChatAutoSave( act->isChecked() );
      if( !Settings::instance().chatAutoSave() && (Settings::instance().chatSaveFileTransfers() || !Settings::instance().chatSaveSystemMessages()) )
      {
        if( QMessageBox::warning( this, Settings::instance().programName(),
                               tr( "There will be no saving of 'system' and 'file transfer' messages if this option is disabled." ),
                               tr( "Ok" ), tr( "Cancel" ), QString(), 0, 1 ) )
        {
          Settings::instance().setChatSaveFileTransfers( false );
          Settings::instance().setChatSaveSystemMessages( false );
          mp_actSaveFileTransferMessages->setChecked( false );
          mp_actSaveSystemMessages->setChecked( false );
        }
      }
    }
    break;
  case 19:
    Settings::instance().setShowNotificationOnTray( act->isChecked() );
    break;
  case 20:
    {
      Settings::instance().setAutoUserAway( act->isChecked() );
      if( act->isChecked() )
      {
        int away_timeout = QInputDialog::getInt( this, Settings::instance().programName(),
                              tr( "How many minutes of idle %1 can wait before changing status to away?" ).arg( Settings::instance().programName() ),
                              Settings::instance().userAwayTimeout(), 1, 30, 1, &ok );
        if( ok && away_timeout > 0 )
          Settings::instance().setUserAwayTimeout( away_timeout );

        if( beeApp )
          beeApp->setIdleTimeout( Settings::instance().userAwayTimeout() );
      }
    }
    break;
  case 21:
    Settings::instance().setShowUserPhoto( act->isChecked() );
    refresh_users = true;
    break;
  case 23:
    {
      Settings::instance().setChatFont( QApplication::font(), false );
      updateChatFont();
    }
    break;
  case 24:
    Settings::instance().setLoadOnTrayAtStartup( act->isChecked() );
    break;
  case 25:
    Settings::instance().setShowVCardOnRightClick( act->isChecked() );
    break;
  case 26:
    {
      Settings::instance().setResetGeometryAtStartup( act->isChecked() );
      mp_actSaveWindowGeometry->setDisabled( Settings::instance().resetGeometryAtStartup() );
      foreach( GuiFloatingChat* fl_chat, m_floatingChats )
        fl_chat->setSaveGeometryDisabled( Settings::instance().resetGeometryAtStartup() );
    }
    break;
  case 27:
    {
#if QT_VERSION >= 0x050000
      int num_messages = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int num_messages = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                                   tr( "Please select the maximum number of messages to be showed" ) + QString( "\n" )
                                                   + tr( "(current: %1, default: %2, all: -1, none: 0)" )
                                                       .arg( Settings::instance().chatMessagesToShow() )
                                                       .arg( Settings::instance().defaultChatMessagesToShow() ),
                                                   Settings::instance().chatMessagesToShow(),
                                                   -1, 2000, 10, &ok );
      if( ok )
      {
        Settings::instance().setChatMessagesToShow( num_messages );
        refresh_chat = true;
      }
      setChatMessagesToShowInAction( act );
    }
    break;
  case 28:
    {
      Settings::instance().setEnableMaximizeButton( act->isChecked() );
      if( Settings::instance().enableMaximizeButton() )
        setWindowFlags( windowFlags() | Qt::WindowMaximizeButtonHint );
      else
        setWindowFlags( windowFlags() & ~Qt::WindowMaximizeButtonHint );
      if( !isVisible() )
        show();
    }
    break;
  case 29:
    Settings::instance().setKeyEscapeMinimizeInTray( act->isChecked() );
    break;
  case 30:
    Settings::instance().setConfirmOnDownloadFile( act->isChecked() );
    break;
  case 31:
    Settings::instance().setUseFontEmoticons( act->isChecked() );
    mp_actSelectEmoticonSourcePath->setEnabled( !act->isChecked() );
    updateEmoticons();
    refresh_chat = true;
    break;
  case 32:
    Settings::instance().setSaveUserList( act->isChecked() );
    mp_actRemoveInactiveUsers->setEnabled( act->isChecked() );
    break;
  case 33:
    {
      Settings::instance().setRemoveInactiveUsers( act->isChecked() );
      if( act->isChecked() )
      {
#if QT_VERSION >= 0x050000
        int num_days = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
        int num_days = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                                     tr( "Please select the number of inactive days before user is removed" ),
                                                     Settings::instance().maxDaysOfUserInactivity(),
                                                     2, 365, 5, &ok );
        if( ok )
        {
          Settings::instance().setMaxDaysOfUserInactivity( num_days );
          setMaxInactivityDaysInAction( act );
        }
      }
    }
    break;
  case 34:
    Settings::instance().setBeepOnNewMessageArrived( act->isChecked() );
    break;
  case 35:
    Settings::instance().setShowMinimizedAtStartup( act->isChecked() );
    break;
  case 36:
    Settings::instance().setPromptOnCloseEvent( act->isChecked() );
    break;
  case 37:
    Settings::instance().setShowUserStatusDescription( act->isChecked() );
    refresh_users = true;
    break;
  case 38:
    Settings::instance().setShowUserStatusBackgroundColor( act->isChecked() );
    refresh_users = true;
    break;
  case 39:
    // FREE
    break;
  case 40:
    Settings::instance().setShowOnlyMessageNotificationOnTray( act->isChecked() );
    break;
  case 41:
    Settings::instance().setChatUseYourNameInsteadOfYou( act->isChecked() );
    refresh_chat = true;
    break;
  case 42:
    {
      Settings::instance().setShowChatToolbar( act->isChecked() );
      foreach( GuiFloatingChat* fl_chat, m_floatingChats )
        fl_chat->setChatToolbarVisible( Settings::instance().showChatToolbar() );
    }
    break;
  case 43:
    Settings::instance().setCheckNewVersionAtStartup( act->isChecked() );
    break;
  case 44:
    Settings::instance().setPostUsageStatistics( act->isChecked() );
    break;
  case 45:
    Settings::instance().setAskChangeUserAtStartup( act->isChecked() );
    break;
  case 46:
    Settings::instance().setShowChatMessageOnTray( act->isChecked() );
    break;
  case 47:
    Settings::instance().setChatClearAllReadMessages( act->isChecked() );
    break;
  case 48:
    Settings::instance().setShowFileTransferCompletedOnTray( act->isChecked() );
    break;
  case 49:
    Settings::instance().setSortUsersAscending( act->isChecked() );
    refresh_users = true;
    break;
  case 50:
    Settings::instance().setUserSortingMode( 0 );
    refresh_users = true;
    break;
  case 51:
    Settings::instance().setUserSortingMode( 1 );
    refresh_users = true;
    break;
  case 52:
    Settings::instance().setUserSortingMode( 2 );
    refresh_users = true;
    break;
  case 53:
    Settings::instance().setUserSortingMode( 3 );
    refresh_users = true;
    break;
  case 54:
    Settings::instance().setShowPresetMessages( act->isChecked() );
    break;
  case 55:
    Settings::instance().setShowTextInModeRTL( act->isChecked() );
    refresh_chat = true;
    break;
  case 56:
    Settings::instance().setPlayBuzzSound( act->isChecked() );
    break;
  case 57:
    Settings::instance().setUserRecognitionMethod( Settings::RecognizeByAccountAndDomain );
    showRestartConnectionAlertMessage();
    break;
  case 58:
    Settings::instance().setUserRecognitionMethod( Settings::RecognizeByAccount );
    showRestartConnectionAlertMessage();

    break;
  case 59:
    Settings::instance().setUserRecognitionMethod( Settings::RecognizeByNickname );
    showRestartConnectionAlertMessage();
    break;
#ifdef BEEBEEP_USE_SHAREDESKTOP
  case 60:
    {
      Settings::instance().setEnableShareDesktop( act->isChecked() );
      if( !act->isChecked() )
      {
        if( beeCore->shareDesktopIsActive( ID_INVALID ) )
          beeCore->stopShareDesktop();
      }
    }
    break;
  case 61:
    {
      int capture_delay = QInputDialog::getInt( this, Settings::instance().programName(), act->text() + QString( " (ms)" ),
                                                Settings::instance().shareDesktopCaptureDelay(), 1100, 8000, 300, &ok );
      if( ok )
        Settings::instance().setShareDesktopCaptureDelay( capture_delay );
    }
    break;
  case 62:
    {
      QString image_type = QInputDialog::getItem( this, Settings::instance().programName(),
                                                  act->text() + QString( "\n(%1)" ).arg( tr( "jpg for photo, png for presentation" ) ),
                                                  ImageOptimizer::instance().imageTypes(),
                                                  qMax( 0, ImageOptimizer::instance().imageTypes().indexOf( Settings::instance().shareDesktopImageType() ) ),
                                                  false, &ok );
      if( ok )
        Settings::instance().setShareDesktopImageType( image_type );
    }
    break;
  case 63:
    {
      int image_quality = QInputDialog::getInt( this, Settings::instance().programName(),
                                                act->text() + QString( "\n(%1)" ).arg( tr( "-1 default, 10 low, 60 medium, 100 high" ) ),
                                                Settings::instance().shareDesktopImageQuality(), -1, 100, 10, &ok );
      if( ok )
        Settings::instance().setShareDesktopImageQuality( image_quality );
    }
    break;
  case 64:
    Settings::instance().setShareDesktopFitToScreen( act->isChecked() );
    break;
#endif
  case 65:
    {
      int delay_connection = QInputDialog::getInt( this, Settings::instance().programName(),
                                              act->text() + QString( "\n(%1)" ).arg( tr( "milliseconds, 5000 default" ) ),
                                              Settings::instance().delayConnectionAtStartup(), 3000, 60000, 1000, &ok );
      if( ok )
        Settings::instance().setDelayConnectionAtStartup( delay_connection );
    }
    break;
  case 66:
    Settings::instance().setSendOfflineMessagesToDefaultChat( act->isChecked() );
    break;
  case 67:
    Settings::instance().setChatUseColoredUserNames( act->isChecked() );
    refresh_chat = true;
    break;
  case 68:
    Settings::instance().setSaveGeometryOnExit( act->isChecked() );
    break;
  case 69:
    Settings::instance().setShowUsersOnConnection( act->isChecked() );
    break;
  case 70:
    Settings::instance().setAlwaysOpenChatOnNewMessageArrived( act->isChecked() );
    break;
  case 71:
    {
 #if QT_VERSION >= 0x050000
      int opacity_level = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int opacity_level = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                                 tr( "Please select the opacity percentage of inactive chat window (default: %1%)" ).arg( Settings::instance().chatInactiveWindowDefaultOpacityLevel() ),
                                                 Settings::instance().chatInactiveWindowOpacityLevel(),
                                                 10, 100, 5, &ok );
      if( ok )
        Settings::instance().setChatInactiveWindowOpacityLevel( opacity_level );
      setChatInactiveWindowOpacityLevelInAction( act );
    }
    break;
  case 72:
    {
      QColor c = QColorDialog::getColor( QColor( Settings::instance().chatBackgroundColor() ), this );
      if( c.isValid() )
      {
        Settings::instance().setChatBackgroundColor( c.name() );
        updateChatColors();
      }
    }
    break;
  case 73:
    {
      QColor c = QColorDialog::getColor( QColor( Settings::instance().chatDefaultTextColor() ), this );
      if( c.isValid() )
      {
        Settings::instance().setChatDefaultTextColor( c.name() );
        updateChatColors();
      }
    }
    break;
  case 74:
    {
      QColor c = QColorDialog::getColor( QColor( Settings::instance().chatSystemTextColor() ), this );
      if( c.isValid() )
      {
        Settings::instance().setChatSystemTextColor( c.name() );
        updateChatColors();
        QMessageBox::information( this, Settings::instance().programName(), tr( "You must close and reopen the chat windows to see the changes applied." ), tr( "Ok" ) );
      }
    }
    break;
  case 75:
    Settings::instance().setEnableDefaultChatNotifications( act->isChecked() );
    break;
  case 76:
    Settings::instance().setUseMessageTimestampWithAP( act->isChecked() );
    refresh_chat = true;
    break;
  case 77:
    {
      if( QMessageBox::question( this, Settings::instance().programName(), tr( "Do you really want to apply the new theme?" ), tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 0 )
      {
        Settings::instance().setUseDarkStyle( act->isChecked() );
        Settings::instance().resetAllColors();
        QTimer::singleShot( 0, this, SLOT( loadStyle() ) );
      }
    }
    break;
  case 78:
    Settings::instance().setalwaysShowFileTransferProgress( act->isChecked() );
    break;
  case 79:
    Settings::instance().setShowChatsOnConnection( act->isChecked() );
    break;
  case 80:
    Settings::instance().setRaiseMainWindowOnNewMessageArrived( act->isChecked() );
    break;
  case 81:
    {
      Settings::instance().setChatSaveUnsentMessages( act->isChecked() );
      if( Settings::instance().chatSaveUnsentMessages() && (!Settings::instance().saveUserList() || !Settings::instance().saveGroupList()) )
      {
        if( QMessageBox::question( this, Settings::instance().programName(),
                               tr( "Saving unsent messages may fail if 'Save users' and 'Save groups' options are not enabled. Do you want to enable them?" ),
                               tr( "Yes" ), tr( "No" ), QString(), 0, 1 ) == 0 )
        {
          Settings::instance().setSaveUserList( true );
          Settings::instance().setSaveGroupList( true );
          mp_actSaveUserList->setChecked( true );
          mp_actSaveGroupList->setChecked( true );
        }
      }
    }
    break;
  case 82:
    Settings::instance().setChatSaveFileTransfers( act->isChecked() );
    showCheckSaveChatMessages();
    break;
  case 83:
    Settings::instance().setChatSaveSystemMessages( act->isChecked() );
    showCheckSaveChatMessages();
    break;
  case 84:
    {
#if QT_VERSION >= 0x050000
      int save_max_lines = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int save_max_lines = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                                 tr( "Please select the maximum number of lines to be saved in the chat (current: %1)." ).arg( Settings::instance().chatMaxLineSaved() ),
                                                 Settings::instance().chatMaxLineSaved(),
                                                 100, 50000, 100, &ok );
      if( ok )
        Settings::instance().setChatMaxLineSaved( save_max_lines );
      setChatMaxLinesToSaveInAction( act );
    }
    break;
  case 85:
    {
#if QT_VERSION >= 0x050000
      int cc_days = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int cc_days = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                          tr( "Please select the number of days that items (such as images) can remain cached." ) +
                                          QString( "\n(%1)" ).arg( tr( "current: %1, never clear: -1, always clear: 0" ).arg( Settings::instance().clearCacheAfterDays() ) ),
                                          Settings::instance().clearCacheAfterDays(), -1, 999, 10, &ok );
      if( ok )
      {
        Settings::instance().setClearCacheAfterDays( cc_days );
        setClearCacheAfterDaysInAction( act );
      }
    }
    break;
  case 86:
    Settings::instance().setDownloadInUserFolder( act->isChecked() );
    break;
  case 87:
    Settings::instance().setShowUsersInWorkgroups( act->isChecked() );
    refresh_users = true;
    break;
  case 88:
    {
#if QT_VERSION >= 0x050000
      int max_files_in_queue = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int max_files_in_queue = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                  tr( "Please select the maximum number of files you can queue up for the transfer (current: %1)." ).arg( Settings::instance().maxQueuedDownloads() ),
                                  Settings::instance().maxQueuedDownloads(),
                                  1, 9999, 10, &ok );
      if( ok )
      {
        Settings::instance().setMaxQueuedDownloads( max_files_in_queue);
        setMaxQueuedDownloadsInAction( act );
      }
    }
    break;
  case 89:
    {
      QColor c = QColorDialog::getColor( QColor( Settings::instance().chatQuoteTextColor() ), this );
      if( c.isValid() )
      {
        Settings::instance().setChatQuoteTextColor( c.name() );
        refresh_chat = true;
      }
    }
    break;
  case 90:
    {
      QColor c = QColorDialog::getColor( QColor( Settings::instance().chatQuoteBackgroundColor() ), this );
      if( c.isValid() )
      {
        Settings::instance().setChatQuoteBackgroundColor( c.name() );
        refresh_chat = true;
      }
    }
    break;
  case 91:
    {
      Settings::instance().setChatOnSendingMessage( Settings::SkipOnSendingMessage );
      foreach( GuiFloatingChat* fl_chat, m_floatingChats )
        fl_chat->guiChat()->updateOnSendingMessage();
    }
    break;
  case 92:
    {
      Settings::instance().setChatOnSendingMessage( Settings::MinimizeChatOnSendingMessage  );
      foreach( GuiFloatingChat* fl_chat, m_floatingChats )
        fl_chat->guiChat()->updateOnSendingMessage();
    }
    break;
  case 93:
    {
      Settings::instance().setChatOnSendingMessage( Settings::CloseChatOnSendingMessage );
      foreach( GuiFloatingChat* fl_chat, m_floatingChats )
        fl_chat->guiChat()->updateOnSendingMessage();
    }
    break;
  case 94:
    {
#if QT_VERSION >= 0x050000
      int max_duration = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int max_duration = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                  tr( "Please select the maximum duration (in seconds) that a voice message can have (current: %1)." ).arg( Settings::instance().voiceMessageMaxDuration() ),
                                  Settings::instance().voiceMessageMaxDuration(),
                                  10, 900, 10, &ok );
      if( ok )
        Settings::instance().setVoiceMessageMaxDuration( max_duration );
    }
    break;
  case 95:
    Settings::instance().setUseVoicePlayer( act->isChecked() );
    break;
  case 96:
    Settings::instance().setDisableBeepInUserStatusBusy( act->isChecked() );
    break;
  case 97:
    Settings::instance().setResumeFileTransfer( act->isChecked() );
    break;
  case 98:
    Settings::instance().setBeepInActiveWindowAlso( act->isChecked() );
    break;
  case 99:
    break;
  case 100:
    {
#if QT_VERSION >= 0x050000
      int cc_days = QInputDialog::getInt( qApp->activeWindow(), Settings::instance().programName(),
#else
      int cc_days = QInputDialog::getInteger( qApp->activeWindow(), Settings::instance().programName(),
#endif
                                              tr( "Please select the number of days that partially downloaded files can remain cached." ) +
                                              QString( "\n(%1)" ).arg( tr( "current: %1, never clear: -1, always clear: 0" ).arg( Settings::instance().removePartiallyDownloadedFilesAfterDays() ) ),
                                              Settings::instance().removePartiallyDownloadedFilesAfterDays(), -1, 999, 10, &ok );
      if( ok )
      {
        Settings::instance().setRemovePartiallyDownloadedFilesAfterDays( cc_days );
        setClearPartiallyDownloadedFilesAfterDaysInAction( act );
      }
    }
    break;
  case 101:
    Settings::instance().setKeepModificationDateOnFileTransferred( act->isChecked() );
    break;
  case 102:
    Settings::instance().setUseUserFirstNameFirstInFullName( !act->isChecked() );
    break;
  case 103:
    Settings::instance().setResetMinimumWidthForStyle( act->isChecked() );
    setMinimumWidthForStyle();
    break;
  case 104:
    EmoticonManager::instance().clearRecentEmoticons();
    updateEmoticons();
    break;
  case 105:
    Settings::instance().setUseHiResEmoticons( act->isChecked() );
    updateEmoticons();
    break;
  case 106:
    EmoticonManager::instance().clearFavoriteEmoticons();
    updateEmoticons();
    break;
  case 107:
    Settings::instance().setEnableVisualNotificationsInChatWindow( act->isChecked() );
    break;
  default:
    qWarning() << "GuiMain::settingsChanged(): error in setting id" << act->data().toInt();
  }

  if( refresh_users )
    mp_userList->updateUsers();

  if( refresh_chat )
  {
    QApplication::setOverrideCursor( Qt::WaitCursor );
    QApplication::processEvents();
    mp_chatList->updateChats();
    foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    {
      Chat c = ChatManager::instance().chat( fl_chat->guiChat()->chatId() );
      if( c.isValid() )
        fl_chat->setChat( c );
    }
    QApplication::restoreOverrideCursor();
  }

  if( settings_data_id > 0 && settings_data_id != 99 )
  {
    if( act->isCheckable() )
    {
      QApplication::processEvents(); // Menu is closing and status bar will be reset -> force events to display next messages
      if( act->isChecked() )
        showMessage( tr( "Option enabled." ), 3000 );
      else
        showMessage( tr( "Option disabled." ), 3000 );
    }
    Settings::instance().save();
  }
}

void GuiMain::showCheckSaveChatMessages()
{
  if( !Settings::instance().chatAutoSave() && (Settings::instance().chatSaveFileTransfers() || !Settings::instance().chatSaveSystemMessages()) )
  {
    if( QMessageBox::warning( this, Settings::instance().programName(),
                              tr( "There will be no saving of 'system' and 'file transfer' messages if this option is disabled." ),
                              tr( "Ok" ), tr( "Cancel" ), QString(), 0, 1 ) )
    {
      Settings::instance().setChatSaveFileTransfers( false );
      Settings::instance().setChatSaveSystemMessages( false );
      mp_actSaveFileTransferMessages->setChecked( false );
      mp_actSaveSystemMessages->setChecked( false );
    }
  }
}

void GuiMain::setChatMessagesToShowInAction( QAction* act )
{
  act->setText( tr( "Show only last %1 messages" ).arg( Settings::instance().chatMessagesToShow() >= 0 ? Settings::instance().chatMessagesToShow() : 800 ) );
  act->setChecked( Settings::instance().chatMessagesToShow() >= 0 );
}

void GuiMain::setMaxInactivityDaysInAction( QAction* act )
{
  act->setText( tr( "Remove users after %1 days of inactivity" ).arg( Settings::instance().maxDaysOfUserInactivity() ) );
  act->setEnabled( Settings::instance().saveUserList() );
}

void GuiMain::setChatInactiveWindowOpacityLevelInAction( QAction* act )
{
  act->setChecked( Settings::instance().chatInactiveWindowOpacityLevel() < 100 );
  act->setText( tr( "Show inactive chat window with %1% opacity" ).arg( Settings::instance().chatInactiveWindowOpacityLevel() ) );
}

void GuiMain::setChatMaxLinesToSaveInAction( QAction* act )
{
#ifdef SHADOWCHAT_SAFE_BUILD
  act->setText( QStringLiteral( "保留全部聊天记录（手动清空）" ) );
  act->setEnabled( false );
  act->setChecked( false );
  return;
#endif
  act->setText( tr( "Save maximum %1 lines of chat" ).arg( Settings::instance().chatMaxLineSaved() ) );
  act->setEnabled( Settings::instance().chatAutoSave() );
  act->setChecked( Settings::instance().chatMaxLineSaved() > 0 );
}

void GuiMain::setClearCacheAfterDaysInAction( QAction* act )
{
  act->setText( tr( "Clean the cache from items older than %1 days" ).arg( Settings::instance().clearCacheAfterDays() >= 0 ? Settings::instance().clearCacheAfterDays() : 96 ) );
  act->setEnabled( Settings::instance().chatAutoSave() );
  act->setChecked( Settings::instance().clearCacheAfterDays() >= 0 );
}

void GuiMain::setClearPartiallyDownloadedFilesAfterDaysInAction( QAction* act )
{
  act->setText( tr( "Delete partially downloaded files after %1 days" ).arg( Settings::instance().removePartiallyDownloadedFilesAfterDays() >= 0 ? Settings::instance().removePartiallyDownloadedFilesAfterDays() : 5 ) );
  act->setChecked( Settings::instance().removePartiallyDownloadedFilesAfterDays() >= 0 );
}

void GuiMain::setMaxQueuedDownloadsInAction( QAction* act )
{
  act->setText( tr( "Add up to %1 files to the transfer queue" ).arg( Settings::instance().maxQueuedDownloads() ) );
  act->setChecked( Settings::instance().maxQueuedDownloads() > 0 );
}

void GuiMain::sendMessage( VNumber chat_id, const QString& msg, bool is_source_code )
{
#ifdef BEEBEEP_DEBUG
  int num_messages = beeCore->sendChatMessage( chat_id, msg, false, false, is_source_code );
  qDebug() << num_messages << "messages sent";
#else
  beeCore->sendChatMessage( chat_id, msg, false, false, is_source_code );
#endif
  mp_chatList->updateChat( ChatManager::instance().chat( chat_id ) ); // to sort the chats
}

void GuiMain::showAlertForMessage( const Chat& c, const ChatMessage& cm )
{
  if( cm.isImportant() )
  {
    playBuzz();
  }
  else
  {
    if( Settings::instance().beepOnNewMessageArrived() &&
        !(Settings::instance().localUser().status() == User::Busy && Settings::instance().disableBeepInUserStatusBusy()) )
    {
      playBeep();
    }
  }

  bool show_message_in_tray = true;

  GuiFloatingChat* fl_chat = floatingChat( c.id() );
  if( fl_chat )
  {
    fl_chat->setMainIcon( true );
    if( Settings::instance().enableVisualNotificationsInChatWindow() )
      QApplication::alert( fl_chat, 0 );
    if( Settings::instance().raiseOnNewMessageArrived() || cm.isImportant() )
    {
      fl_chat->raiseOnTop();
      show_message_in_tray = false;
    }
  }

  if( Settings::instance().raiseMainWindowOnNewMessageArrived() )
    raiseOnTop();

  if( show_message_in_tray )
  {
    User u = UserManager::instance().findUser( cm.userId() );
    QString msg;
    bool long_time_show = false;

    if( u.isValid() )
    {
      if( Settings::instance().showChatMessageOnTray() )
      {
        QString txt = Bee::removeHtmlTags( cm.message() );
        if( txt.size() > Settings::instance().textSizeInChatMessagePreviewOnTray() )
        {
          txt.truncate( Settings::instance().textSizeInChatMessagePreviewOnTray() );
          txt.append( "..." );
        }

        if( c.isDefault() )
          msg = QString( "%1 %2: %3" ).arg( u.name(), tr( "to all" ), txt );
        else if( c.isGroup() )
          msg = QString( "%1 %2 %3: %4" ).arg( u.name(), tr( "to" ), c.name(), txt );
        else
          msg = QString( "%1 %2: %4" ).arg( u.name(), tr( "to you" ), txt );

        long_time_show = true;
      }
      else
      {
        QString pre_msg = tr( "New message from" );
        if( c.isDefault() )
          msg = QString( "%1 %2 %3" ).arg( pre_msg, u.name(), tr( "to all" ) );
        else if( c.isGroup() )
          msg = QString( "%1 %2 %3 %4" ).arg( pre_msg, u.name(), tr( "to" ), c.name() );
        else
          msg = QString( "%1 %2 %3" ).arg( pre_msg, u.name(), tr( "to you" ) );
      }
    }
    else
      msg = tr( "New message arrived" );

    mp_trayIcon->showNewMessageArrived( c.id(), msg, long_time_show );
  }
}

void GuiMain::onNewChatMessage( const Chat& c, const ChatMessage& cm )
{
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << c.name() << "found in GuiMain::onNewChatMessage(...)";
    return;
  }

  if( c.isDefault() )
  {
    if( mp_home->addSystemMessage( cm ) && mp_tabMain->currentWidget() != mp_home )
    {
      m_unreadActivities++;
      updateTabTitles();
    }
  }

  bool floating_chat_created = false;
  bool alert_can_be_notified = false;

  const ShadowConversationPreferences conversationPreferences;
  bool incomingContent = !cm.isFromLocalUser() && !cm.isFromSystem() &&
    (cm.type() == ChatMessage::Chat || cm.isVoice() || cm.isFromAutoresponder());
  // Transfer progress also uses system messages. Only completed, saved material
  // with a remote sender should restore a conversation the user closed.
  if( cm.isFromSystem() && cm.canBeSaved() && (cm.isFileTransfer() || cm.isImagePreview()) )
  {
    const QRegularExpression senderPattern( QStringLiteral("<!--shadow-sender:([A-Za-z0-9+/=]+)-->") );
    const QRegularExpressionMatch senderMatch = senderPattern.match( cm.message() );
    if( senderMatch.hasMatch() )
    {
      const QString sender = QString::fromUtf8( QByteArray::fromBase64( senderMatch.captured( 1 ).toLatin1() ) );
      incomingContent = !sender.isEmpty() && sender != Settings::instance().localUser().name();
    }
  }
  if( incomingContent && conversationPreferences.hidden( c ) )
  {
    conversationPreferences.reveal( c );
    mp_chatList->updateChat( c );
  }
  const bool mentioned = c.isGroup() && !cm.isFromLocalUser() && !cm.isFromSystem() &&
    shadowMentionsUser( Bee::removeHtmlTags( cm.message() ), Settings::instance().localUser().name() );
  if( c.isDefault() )
    alert_can_be_notified = cm.alertCanBeSent() && Settings::instance().enableDefaultChatNotifications();
  else if( c.isGroup() )
    alert_can_be_notified = cm.alertCanBeSent() && (mentioned || !Settings::instance().isNotificationDisabledForGroup( c.privateId() ));
  else
    alert_can_be_notified = cm.alertCanBeSent();
  if( !mentioned && conversationPreferences.muted( c ) )
    alert_can_be_notified = false;

  GuiFloatingChat* fl_chat = floatingChat( c.id() );
  bool open_chat_window = Settings::instance().alwaysOpenChatOnNewMessageArrived() || (!cm.isFromLocalUser() && !cm.isFromSystem() && cm.isImportant() && c.isPrivate());

  if( !fl_chat && open_chat_window && alert_can_be_notified )
  {
    fl_chat = createFloatingChat( c );
    floating_chat_created = true;
    fl_chat->show();
    fl_chat->guiChat()->ensureLastMessageVisible();
    if( !Settings::instance().raiseOnNewMessageArrived() && !cm.isImportant() )
      fl_chat->showMinimized();
  }

  if( fl_chat && !floating_chat_created )
    fl_chat->showChatMessage( c, cm );

  if( fl_chat && !floating_chat_created && fl_chat->isActiveWindow() )
  {
    if( alert_can_be_notified && (cm.isImportant() || Settings::instance().beepInActiveWindowAlso()) )
    {
      if( cm.isImportant() )
        playBuzz();
      else
        playBeep();
    }
    readAllMessagesInChat( c.id() );
  }
  else
  {
    if( alert_can_be_notified )
      showAlertForMessage( c, cm );
  }
}

void GuiMain::updateUser( const User& u )
{
  mp_userList->setUser( u, false );
  mp_chatList->updateUser( u );
  mp_groupList->updateUser( u );
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->updateUser( u );
  mp_fileTransfer->updateUser( u );
}

void GuiMain::searchUsers()
{
  GuiNetwork gn( this );
  gn.setModal( true );
  gn.loadSettings();
  gn.setSizeGripEnabled( true );
  gn.show();

  if( gn.exec() != QDialog::Accepted )
    return;

  if( gn.restartConnection() )
  {
    showRestartConnectionAlertMessage();
    return;
  }

  if( !beeCore->isConnected() )
    return;

#ifdef BEEBEEP_USE_MULTICAST_DNS
  if( Settings::instance().useMulticastDns() )
    beeCore->startDnsMulticasting();
  else
    beeCore->stopDnsMulticasting();
#endif

  QMetaObject::invokeMethod( this, "sendBroadcastMessage", Qt::QueuedConnection );
}

void GuiMain::showWritingUser( const User& u, VNumber chat_id )
{
  QString msg = tr( "%1 is writing..." ).arg( u.name() );
  GuiFloatingChat* fl_chat = floatingChat( chat_id );
  if( fl_chat )
    fl_chat->showStatusMessage( msg, Settings::instance().writingTimeout() );
}

void GuiMain::setUserStatusSelected( int user_status )
{
  if( user_status == User::Offline )
  {
    if( beeCore->isConnected() )
    {
      if( !Settings::instance().promptOnCloseEvent() || QMessageBox::question( this, Settings::instance().programName(),
                                   tr( "Do you want to disconnect from %1 network?" ).arg( Settings::instance().programName() ),
                                   tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 0 )
      {
        disconnectFromNetwork();
        return;
      }
    }
    updateStatusIcon();
  }
  else
  {
    if( beeCore->isConnected() )
    {
      beeCore->setLocalUserStatus( user_status );
    }
    else
    {
      Settings::instance().setLocalUserStatus( static_cast<User::Status>(user_status) );
      startCore();
    }
  }
}

void GuiMain::statusSelected()
{
  QAction* act = qobject_cast<QAction*>( sender() );
  if( !act )
    return;

  int user_status = act->data().toInt();
  setUserStatusSelected( user_status );
}

void GuiMain::updateStatusIcon()
{
  int status_type;
  if( !beeCore->isConnected() )
    status_type = User::Offline;
  else
    status_type = Settings::instance().localUser().status();

  mp_menuStatus->setIcon( Bee::avatarForUser( Settings::instance().localUser(), Settings::instance().avatarIconSize(), true ) );
  QString tip = tr( "You are %1%2" ).arg( Bee::userStatusToString( status_type ) )
      .arg( (Settings::instance().localUser().statusDescription().isEmpty() ? QString( "" ) : QString( ": %1" ).arg( Settings::instance().localUser().statusDescription() ) ) );
  QAction* act = mp_menuStatus->menuAction();
  act->setToolTip( tip );
  act->setText( Bee::capitalizeFirstLetter( Bee::userStatusToString( status_type ), true ) );
  updateWindowTitle();
}

void GuiMain::changeStatusDescription()
{
  bool ok = false;
  QString status_description = QInputDialog::getText( this, Settings::instance().programName(),
                           tr( "Please insert the new status description" ), QLineEdit::Normal, Settings::instance().localUser().statusDescription(), &ok );
  if( !ok || status_description.isNull() )
    return;
  beeCore->setLocalUserStatusDescription( Settings::instance().localUser().status(), status_description, true );
  loadUserStatusRecentlyUsed();
  updateStatusIcon();
  updateLocalStatusMessage();
}

void GuiMain::sendFileFromChat( VNumber chat_id, const QString& file_path )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
    return;

  if( c.isDefault() && !Settings::instance().chatWithAllUsersIsEnabled() )
    return;

  QStringList files_path_selected = checkFilePath( file_path );
  if( files_path_selected.isEmpty() )
    return;

  QString caption;
  if( !GuiImageBatchPreview::confirm( files_path_selected, activeWindow(), &caption ) )
    return;
  if( !caption.isEmpty() )
    sendMessage( chat_id, caption, false );

  beeCore->sendFilesFromChat( c.id(), files_path_selected );
}

void GuiMain::sendFilesFromChat( VNumber chat_id, const QStringList& file_path_list )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
    return;

  if( c.isDefault() && !Settings::instance().chatWithAllUsersIsEnabled() )
    return;

  if( file_path_list.isEmpty() )
    return;

  QString caption;
  if( !GuiImageBatchPreview::confirm( file_path_list, activeWindow(), &caption ) )
    return;
  if( !caption.isEmpty() )
    sendMessage( chat_id, caption, false );

  beeCore->sendFilesFromChat( c.id(), file_path_list );
}

void GuiMain::sendFile( VNumber user_id )
{
  User u = UserManager::instance().findUser( user_id );
  QStringList files_path_selected = checkFilePath( "" );
  if( files_path_selected.isEmpty() )
    return;
  Chat c = ChatManager::instance().privateChatForUser( user_id );
  if( !c.isValid() )
    qWarning() << "Unable to send files to user" << user_id << "without a private chat in GuiMain::sendFile(...)";
  beeCore->sendFilesFromChat( c.id(), files_path_selected );
}

QStringList GuiMain::checkFilePath( const QString& file_path )
{
  QStringList files_path_selected;
  if( file_path.isEmpty() || !QFile::exists( file_path ) )
  {
    files_path_selected = FileDialog::getOpenFileNames( true, activeWindow(), tr( "%1 - Select a file" ).arg( Settings::instance().programName() ) + QString( " %1" ).arg( tr( "or more" ) ),
                                                       Settings::instance().lastDirectorySelected() );
    if( files_path_selected.isEmpty() )
      return files_path_selected;

    Settings::instance().setLastDirectorySelectedFromFile( files_path_selected.last() );
  }
  else
  {
    files_path_selected.append( file_path );
  }

  return files_path_selected;
}

bool GuiMain::sendFile( const User& u, const QString& file_path, VNumber chat_id )
{
  if( !Settings::instance().enableFileTransfer() )
  {
    QMessageBox::information( activeWindow(), Settings::instance().programName(), tr( "File transfer is not enabled." ) );
    return false;
  }

  if( !beeCore->isConnected() )
  {
    QMessageBox::information( activeWindow(), Settings::instance().programName(), tr( "You are not connected." ) );
    return false;
  }

  User user_selected;

  if( !u.isValid() )
  {
    QStringList user_string_list;
    foreach( User u, UserManager::instance().userList().toList() )
    {
      if( u.isStatusConnected() )
        user_string_list.append( u.path() );
    }

    if( user_string_list.isEmpty() )
    {
      QMessageBox::information( activeWindow(), Settings::instance().programName(), tr( "There is no user connected." ) );
      return false;
    }

    bool ok = false;
    QString user_path = QInputDialog::getItem( activeWindow(), Settings::instance().programName(),
                                        tr( "Please select the user to whom you would like to send a file."),
                                        user_string_list, 0, false, &ok );
    if( !ok )
      return false;

    user_selected = UserManager::instance().findUserByPath( user_path );

    if( !user_selected.isValid() )
    {
      QMessageBox::warning( activeWindow(), Settings::instance().programName(), tr( "User not found." ) );
      return false;
    }

    Chat c = ChatManager::instance().privateChatForUser( user_selected.id() );
    chat_id = c.id();
  }
  else
    user_selected = u;

  return beeCore->sendFile( user_selected.id(), file_path, "", false, chat_id );
}

void GuiMain::sendFile( const QString& file_path )
{
  sendFile( User(), file_path, ID_INVALID );
}

bool GuiMain::askToDownloadFile( const User& u, const FileInfo& fi, const QString& download_path, bool make_questions )
{
  if( !Settings::instance().enableFileTransfer() )
  {
    QMessageBox::warning( activeWindow(), Settings::instance().programName(), tr( "File transfer is disabled. You cannot download %1." ).arg( fi.name() ) );
    return false;
  }

  int msg_result = make_questions ? 0 : 1;

  if( msg_result == 0 )
  {
    if( Settings::instance().confirmOnDownloadFile() )
    {
      if( isMinimized() || !isActiveWindow() )
      {
        if( Settings::instance().raiseMainWindowOnNewMessageArrived() )
          raiseOnTop();
        mp_actViewNewMessage->setEnabled( true );
      }
      QString msg = tr( "Do you want to download %1 (%2) from %3?" ).arg( fi.name(), Bee::bytesToString( fi.size() ), Bee::userNameToShow( u, false ) );
      msg_result = QMessageBox::question( this, Settings::instance().programName(), msg, tr( "No" ), tr( "Yes" ), tr( "Yes, and don't ask anymore" ), 0, 0 );
    }
    else
      msg_result = 1;
  }

  if( msg_result == 2 )
  {
    qDebug() << "Prompt on download file disabled by user request";
    Settings::instance().setConfirmOnDownloadFile( false );
    mp_actConfirmDownload->setChecked( false );
  }

  if( msg_result > 0 )
  {
    QFileInfo qfile_info( download_path, fi.name() );
    qDebug() << "You accept to download" << fi.name() << "from" << qPrintable( u.path() );
    if( qfile_info.exists() && Settings::instance().onExistingFileAction() == Settings::GenerateNewFileName )
    {
      QString file_name = Bee::uniqueFilePath( qfile_info.absoluteFilePath(), false );
      qDebug() << "File" << qfile_info.absoluteFilePath() << "exists. Save it with new path (auto):" << qPrintable( file_name );
      qfile_info = QFileInfo( file_name );
    }
    FileInfo file_info = fi;
    file_info.setPath( qfile_info.absoluteFilePath() );
    return beeCore->downloadFile( u.id(), file_info, make_questions );
  }
  else
  {
    qDebug() << "You refuse to download" << fi.name() << "from" << u.path();
    return false;
  }
}

void GuiMain::downloadFile( const User& u, const FileInfo& fi )
{
  if( !askToDownloadFile( u, fi, Settings::instance().downloadDirectoryForUser( u ), true ) )
    beeCore->refuseToDownloadFile( u.id(), fi );
}

void GuiMain::downloadSharedFiles( const QList<SharedFileInfo>& share_file_info_list )
{
  if( share_file_info_list.isEmpty() )
    return;

  QString download_folder;
  User u;
  int files_to_download = 0;

  if( share_file_info_list.size() > Settings::instance().maxQueuedDownloads() )
  {
    if( QMessageBox::question( activeWindow(), Settings::instance().programName(),
                               tr( "You cannot download all these files at once. Do you want to download the first %1 files of the list?" )
                               .arg( Settings::instance().maxQueuedDownloads() ),
                               tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) != 0 )
      return;
  }
  else if( share_file_info_list.size() > 100 )
  {
    if( QMessageBox::question( activeWindow(), Settings::instance().programName(),
                           tr( "Downloading %1 files is a hard duty. Maybe you have to wait a lot of minutes. Do yo want to continue?" ).arg( share_file_info_list.size() ),
                           tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) != 0 )
      return;
  }

  foreach( SharedFileInfo sfi, share_file_info_list )
  {
    u = UserManager::instance().findUser( sfi.first );
    download_folder = Bee::convertToNativeFolderSeparator( QString( "%1/%2" ).arg( Settings::instance().downloadDirectoryForUser( u ), sfi.second.shareFolder() ) );
    if( !askToDownloadFile( u, sfi.second, download_folder, false ) )
      return;
    files_to_download++;
    if( files_to_download > Settings::instance().maxQueuedDownloads() )
      break;
  }

  showMessage( tr( "Downloading %1 files" ).arg( files_to_download ), 5000 );
}

void GuiMain::downloadSharedFile( VNumber user_id, VNumber file_id )
{
  User u = UserManager::instance().findUser( user_id );
  FileInfo file_info = FileShare::instance().networkFileInfo( user_id, file_id );

  if( u.isStatusConnected() && file_info.isValid() )
  {
    askToDownloadFile( u, file_info, Settings::instance().downloadDirectoryForUser( u ), true );
    return;
  }

  qWarning() << "Unable to download shared file" << file_id << "from user" << user_id;
  QString info_msg = tr( "File is not available for download." );
  if( u.isValid() && !u.isStatusConnected() )
    info_msg += QLatin1String( "\n" ) + tr( "%1 is not connected." ).arg( u.name() );
  info_msg += QLatin1String( "\n" ) + tr( "Please reload the list of shared files." );

  if( QMessageBox::information( activeWindow(), Settings::instance().programName(), info_msg,
                              tr( "Reload file list" ), tr( "Cancel" ), QString(), 1, 1 ) == 0 )
  {
    if( mp_fileSharing )
      mp_fileSharing->updateNetworkFileList();
  }
}

void GuiMain::downloadFolder( const User& u, const QString& folder_name, const QList<FileInfo>& file_info_list )
{
  if( !Settings::instance().enableFileTransfer() )
  {
    QMessageBox::warning( activeWindow(), Settings::instance().programName(), tr( "File transfer is disabled. You cannot download %1." ).arg( folder_name ) );
    return;
  }

  if( file_info_list.isEmpty() )
  {
    qWarning() << "Unable to download folder" << folder_name << "from user" << qPrintable( u.path() ) << "with empty file list";
    return;
  }

  int msg_result = Settings::instance().confirmOnDownloadFile() ? 0 : 1;

  if( msg_result == 0 )
  {
    QString msg = tr( "Do you want to download folder %1 (%2 files) from %3?" ).arg( folder_name ).arg( file_info_list.size() ).arg( u.name() );
    msg_result = QMessageBox::question( activeWindow(), Settings::instance().programName(), msg, tr( "No" ), tr( "Yes" ), tr( "Yes, and don't ask anymore" ), 0, 0 );
  }

  if( msg_result == 2 )
  {
    Settings::instance().setConfirmOnDownloadFile( false );
    mp_actConfirmDownload->setChecked( false );
  }

  if( msg_result > 0 )
  {
    // Accepted
    qDebug() << "You accept to download folder" << folder_name << "from" << u.path();
    const QString root_name = QDir::fromNativeSeparators( folder_name );
    if( root_name.isEmpty() || root_name == QLatin1String( "." ) || root_name == QLatin1String( ".." ) || root_name.contains( QLatin1Char( '/' ) ) )
    {
      QMessageBox::warning( this, QStringLiteral( "接收文件夹失败" ), QStringLiteral( "文件夹名称无效。" ) ); return;
    }
    const QString destination = Bee::uniqueFilePath( QDir( Settings::instance().downloadDirectoryForUser( u ) ).filePath( root_name ), false );
    if( !QDir().mkpath( destination ) )
    {
      QMessageBox::warning( this, QStringLiteral( "接收文件夹失败" ), QStringLiteral( "无法创建文件夹，请检查目录权限和剩余空间。" ) ); return;
    }
    foreach( FileInfo fi, file_info_list )
    {
      const QString parent = QDir::fromNativeSeparators( fi.shareFolder() );
      if( parent != root_name && !parent.startsWith( root_name + QLatin1Char( '/' ) ) )
      {
        QMessageBox::warning( this, QStringLiteral( "接收文件夹失败" ), QStringLiteral( "文件夹内容路径无效。" ) ); return;
      }
      const QString relative = parent.mid( root_name.size() ).remove( QRegularExpression( QStringLiteral( "^/+" ) ) );
      const QString download_folder = relative.isEmpty() ? destination : QDir( destination ).filePath( relative );
      if( fi.isFolder() )
      {
        const QString leaf = fi.name();
        if( leaf.isEmpty() || leaf == QLatin1String( "." ) || leaf == QLatin1String( ".." ) || leaf.contains( QLatin1Char( '/' ) ) || leaf.contains( QLatin1Char( '\\' ) ) ||
            !QDir().mkpath( QDir( download_folder ).filePath( leaf ) ) )
        {
          beeCore->refuseToDownloadFolder( u.id(), folder_name, file_info_list.first().chatPrivateId() );
          QMessageBox::warning( this, QStringLiteral( "接收文件夹失败" ), QStringLiteral( "无法创建项目目录，请检查路径、权限和剩余空间。" ) );
          return;
        }
        continue;
      }
      if( !askToDownloadFile( u, fi, download_folder, false ) )
        return;

      // A project folder is one accepted task. Limit active transfer concurrency,
      // never silently truncate its contents at the queue display limit.
    }
  }
  else
  {
    qDebug() << "You refuse to download folder" << folder_name << "from" << u.path();
    beeCore->refuseToDownloadFolder( u.id(), folder_name, file_info_list.first().chatPrivateId() );
  }
}

void GuiMain::selectDownloadDirectory()
{
  QString download_directory_path = FileDialog::getExistingDirectory( activeWindow(),
                                                                       tr( "%1 - Select the download folder" )
                                                                       .arg( Settings::instance().programName() ),
                                                                       Settings::instance().downloadDirectory() );
  if( download_directory_path.isEmpty() )
    return;

  Settings::instance().setDownloadDirectory( download_directory_path );
  QMessageBox::information( this, Settings::instance().programName(),
                            QString( "%1<br>%2%3%4" ).arg( tr( "The files will be downloaded to the folder:" ) )
                                                     .arg( download_directory_path )
                                                     .arg( Bee::nativeFolderSeparator() )
                                                     .arg( Settings::instance().downloadInUserFolder() ? Bee::convertToNativeFolderSeparator( QString( "<user's name>" ) ) : "" ),
                            tr( "Ok" ) );
}

void GuiMain::showTipOfTheDay()
{
  openWebUrl( Settings::instance().tipsWebSite() );
}

void GuiMain::showFactOfTheDay()
{
  openWebUrl( Settings::instance().factWebSite() );
}

void GuiMain::showFaq()
{
  openWebUrl( Settings::instance().faqWebSite() );
}

void GuiMain::showChat( VNumber chat_id )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << chat_id << "found in GuiMain::showChat(...)";
    return;
  }

  const ShadowConversationPreferences conversationPreferences;
  if( conversationPreferences.hidden( c ) && conversationPreferences.reveal( c ) )
    mp_chatList->updateChat( c );
  GuiFloatingChat* fl_chat = createFloatingChat( c );
  if( !fl_chat )
    return;

  updateModernChatHeader( c );
  if( mp_modernConversationStack && mp_modernChatPage )
    mp_modernConversationStack->setCurrentWidget( mp_modernChatPage );
  if( width() < 900 && mp_modernListPanel && mp_modernConversationPanel )
  {
    mp_modernListPanel->hide();
    mp_modernConversationPanel->show();
    if( mp_modernBackButton )
      mp_modernBackButton->show();
  }
  fl_chat->show();
  if( fl_chat != mp_embeddedChat )
    fl_chat->showUp();
  fl_chat->setFocusInChat();
}

void GuiMain::changeVCard()
{
  GuiEditVCard gvc;
  gvc.setModal( true );
  gvc.setUser( Settings::instance().localUser() );
  gvc.setSizeGripEnabled( true );
  gvc.show();
  if( gvc.exec() == QDialog::Accepted )
  {
    beeCore->setLocalUserVCard( gvc.userColor(), gvc.vCard() );
    if( gvc.regenerateUserHash() )
    {
      beeCore->regenerateLocalUserHash();
      QMessageBox::information( this, Settings::instance().programName(), tr( "You must restart %1 to apply these changes." ).arg( Settings::instance().programName() ) );
    }
  }
}

void GuiMain::showLocalUserVCard()
{
  showVCard( ID_LOCAL_USER );
}

void GuiMain::showVCard( VNumber user_id )
{
  User u = UserManager::instance().findUser( user_id );
  if( !u.isValid() )
    return;

  GuiVCard* gvc = new GuiVCard( this );
  connect( gvc, SIGNAL( showChat( VNumber ) ), this, SLOT( showChat( VNumber ) ) );
  connect( gvc, SIGNAL( sendFile( VNumber ) ), this, SLOT( sendFile( VNumber ) ) );
  connect( gvc, SIGNAL( changeUserColor( VNumber, const QString& ) ), this, SLOT( changeUserColor( VNumber, const QString& ) ) );
  connect( gvc, SIGNAL( toggleFavorite( VNumber ) ), this, SLOT( toggleUserFavorite( VNumber ) ) );
  connect( gvc, SIGNAL( removeUser( VNumber ) ), this, SLOT( removeUserFromList( VNumber ) ) );
  connect( gvc, SIGNAL( buzzUser( VNumber ) ), this, SLOT( sendBuzzToUser( VNumber ) ) );
  connect( gvc, SIGNAL( sendHelpRequestToUser( VNumber ) ), this, SLOT( sendHelpMessageToUser( VNumber ) ) );
  gvc->setVCard( u, ChatManager::instance().privateChatForUser( u.id() ).id(), beeCore->isConnected() );

  QPoint cursor_pos = QCursor::pos();
  QRect screen_rect = qApp->desktop()->availableGeometry( cursor_pos );
  int diff_margin = cursor_pos.x() + gvc->size().width() - screen_rect.width();
  if( diff_margin > 0 )
    cursor_pos.setX( cursor_pos.x() - gvc->size().width() - 16 );
  diff_margin = cursor_pos.y() + gvc->size().height() - screen_rect.height();
  if( diff_margin > 0 )
    cursor_pos.setY( cursor_pos.y() - gvc->size().height() + 24 );
  gvc->move( cursor_pos );
  gvc->show();
  gvc->adjustSize();
  gvc->setFixedSize( gvc->size() );
}

void GuiMain::updadePluginMenu()
{
  mp_menuPlugins->clear();
#ifdef SHADOWCHAT_SAFE_BUILD
  QAction* safe_mode_action = mp_menuPlugins->addAction( QStringLiteral( "第三方插件已在安全模式下关闭" ) );
  safe_mode_action->setEnabled( false );
  return;
#else
  QAction* act;
  mp_menuPlugins->addAction( IconManager::instance().icon( "plugin.png" ), tr( "Plugin Manager..." ), this, SLOT( showPluginManager() ) );

  QString help_data_ts = tr( "is a plugin developed by" );
  QString help_data_format = QString( "<p>%1 <b>%2</b> %3 <b>%4</b>.<br><i>%5</i></p><br>" );

  mp_menuPlugins->addAction( mp_actViewScreenShot );

  bool copymastro_available = false;
  QString copy_mastro_path = "";
#ifdef Q_OS_WIN
  copy_mastro_path = QString( "%1\\%2" ).arg( Settings::instance().pluginPath(), QString( "CopyMastro.exe" ) );
  copymastro_available = QFile::exists( copy_mastro_path );
  if( !copymastro_available )
  {
    copy_mastro_path = QString( "C:\\Program Files (x86)\\CopyMastro\\CopyMastro.exe" );
    copymastro_available = QFile::exists( copy_mastro_path );
  }
#endif

  if( copymastro_available )
  {
    qDebug() << "CopyMastro is found:" << qPrintable( copy_mastro_path );
    act = mp_menuPlugins->addAction( IconManager::instance().icon( "CopyMastro.png" ), "CopyMastro", this, SLOT( startExternalApplicationFromActionData() ) );
    act->setToolTip( tr( "Start the new application to copy file and folders by Marco Mastroddi" ) );
    act->setData( copy_mastro_path );
  }

  if( PluginManager::instance().textMarkers().size() > 0 )
  {
    mp_menuPlugins->addSeparator();

    foreach( TextMarkerInterface* text_marker, PluginManager::instance().textMarkers() )
    {
      act = mp_menuPlugins->addAction( text_marker->name(), this, SLOT( showPluginHelp() ) );

      act->setData( help_data_format
                  .arg( IconManager::instance().toHtml( (text_marker->icon().isNull() ? "images/plugin.png" : text_marker->iconFileName()), "*P*" ),
                        text_marker->name(), help_data_ts, text_marker->author(), text_marker->help() ) );
      act->setIcon( text_marker->icon() );
      act->setEnabled( text_marker->isEnabled() );
    }
  }
#endif
}

void GuiMain::showPluginHelp()
{
  QAction* act = qobject_cast<QAction*>(sender());
  if( !act )
    return;

  QMessageBox::information( this, act->text(), act->data().toString() );
}

void GuiMain::showPluginManager()
{
#ifdef SHADOWCHAT_SAFE_BUILD
  QMessageBox::information( this, QStringLiteral( "安全模式" ), QStringLiteral( "影聊官方构建不加载第三方插件。" ) );
#else
  GuiPluginManager gpm;
  gpm.setModal( true );
  gpm.setSizeGripEnabled( true );
  gpm.updatePlugins();
  gpm.show();
  gpm.exec();
  if( gpm.isChanged() )
    updadePluginMenu();
#endif
}

bool GuiMain::showWizard()
{
  GuiWizard gw( this );
  gw.setModal( true );
  gw.loadSettings();
  gw.show();
  gw.setFixedSize( gw.size() );
  if( gw.exec() == QDialog::Accepted )
  {
    if( !beeCore->changeLocalUser( gw.userName() ) )
    {
      if( QMessageBox::question( this, Settings::instance().programName(), tr( "Your name is not changed. Do you want to continue?" ),
                                 tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 1 )
      {
        return false;
      }
    }
    VCard profile = Settings::instance().localUser().vCard();
    profile.setNickName( gw.userName() );
    if( !gw.avatar().isNull() )
      profile.setPhoto( gw.avatar() );
    beeCore->setLocalUserVCard( Settings::instance().localUser().color(), profile );
    Settings::instance().setCheckNewVersionAtStartup( gw.checkForUpdates() );
    Settings::instance().save();
    return true;
  }
  else
    return false;
}

void GuiMain::hideToTrayIcon()
{
  Chat c = ChatManager::instance().firstChatWithUnreadMessages();
  if( c.isValid() )
    mp_trayIcon->setUnreadMessages( c.id(), c.unreadMessages() );
  else
    mp_trayIcon->setUnreadMessages( ID_INVALID, 0 );
  hide();
}

void GuiMain::trayIconClicked( QSystemTrayIcon::ActivationReason ar )
{
#ifdef Q_OS_MAC

  // In Mac that is the expected behavior, there is no distinction
  // between left and right buttons for the systray icons.
  // They will always show the context menu, that's the Mac behavior.
  Q_UNUSED( ar );

#else

  // Other OS

  if( ar == QSystemTrayIcon::Context )
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "TrayIcon is activated with context click and menu is showed";
#endif
    return;
  }

  if( ar == QSystemTrayIcon::Trigger )
  {
    if( !isActiveWindow() || isMinimized() )
    {
#ifdef BEEBEEP_DEBUG
      qDebug() << "TrayIcon is activated with trigger click and main window will be showed";
#endif
      QTimer::singleShot( 0, this, SLOT( raiseOnTop() ) );
    }
    else
    {
#ifdef BEEBEEP_DEBUG
      qDebug() << "TrayIcon is activated with trigger click and menu will be showed";
#endif
      mp_menuTrayIcon->popup( QCursor::pos() );
    }
  }
  else
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "TrayIcon is activated with unknown click";
#endif
    if( !mp_menuTrayIcon->isVisible() )
      mp_menuTrayIcon->popup( QCursor::pos() );
  }
#endif
}

void GuiMain::trayMessageClicked()
{
  // QT 2019-06-27: Currently this signal is not sent on macOS.
  if( mp_trayIcon->chatId() != ID_INVALID && ChatManager::instance().chat( mp_trayIcon->chatId() ).isValid() )
  {
    VNumber chat_id = mp_trayIcon->chatId();
    showChat( chat_id );
    GuiFloatingChat* fl_chat = floatingChat( chat_id );
    if( fl_chat && !fl_chat->chatIsVisible() )
      QTimer::singleShot( 0, fl_chat, SLOT( raiseOnTop() ) );
  }
  else
    QTimer::singleShot( 0, this, SLOT( raiseOnTop() ) );
}

void GuiMain::addToShare( const QString& share_path )
{
  beeCore->addPathToShare( share_path );
}

void GuiMain::removeFromShare( const QString& share_path )
{
  beeCore->removePathFromShare( share_path );
}

void GuiMain::openUrl( const QUrl& file_url )
{
  openUrlFromChat( file_url, ID_INVALID );
}

void GuiMain::openUrlFromChat( const QUrl& file_url, VNumber chat_id )
{
#ifdef BEEBEEP_DEBUG
  qDebug() << "Opening url (not encoded):" << qPrintable( file_url.toString() );
#endif

  if( file_url.scheme() == QLatin1String( "beeshowsavedchat" ) )
  {
    QStringList sl_chat_id = file_url.toString().split( "#" );
    if( !sl_chat_id.isEmpty() )
    {
      bool chat_id_is_integer = false;
      VNumber chat_id = sl_chat_id.last().toULongLong( &chat_id_is_integer );
      if( chat_id_is_integer && chat_id > 0 )
      {
        QString chat_name = ChatManager::instance().chatName( chat_id );
        if( ChatManager::instance().chatHasSavedText( chat_name ) )
          showSavedChatSelected( chat_name );
        else
          QMessageBox::information( QApplication::activeWindow(), Settings::instance().programName(),
                                    tr( "There are no messages saved in the chat with %1." ).arg( chat_name ), tr( "Ok" ) );
      }
      else
        qWarning() << "Invalid chat id" << chat_id << "found parsing open history request in GuiMain::openUrl(...)";
    }
  }
  else if( file_url.scheme() == FileInfo::urlSchemeShowFileInFolder() )
  {
    QUrl adj_file_url = file_url;
    adj_file_url.setScheme( QLatin1String( "file" ) );
    if( !Bee::showFileInGraphicalShell( Bee::convertToNativeFolderSeparator( adj_file_url.toLocalFile() ) ) )
    {
      QFileInfo file_info_url( adj_file_url.toLocalFile() );
      adj_file_url = QUrl::fromLocalFile( Bee::convertToNativeFolderSeparator( file_info_url.absoluteDir().absolutePath() ) );
      openUrlFromChat( adj_file_url, chat_id );
      return;
    }
  }
  else if( file_url.scheme() == FileInfo::urlSchemeVoiceMessage() )
  {
    QUrl adj_file_url = file_url;
    adj_file_url.setScheme( QLatin1String( "file" ) );
#ifdef BEEBEEP_USE_VOICE_CHAT
    if( Settings::instance().useVoicePlayer() )
    {
      GuiFloatingChat* fl_chat = floatingChat( chat_id );
      if( fl_chat )
        fl_chat->guiChat()->guiVoicePlayer()->setFilePath( Bee::convertToNativeFolderSeparator( adj_file_url.toLocalFile() ), chat_id );
    }
    else
#endif
      openUrlFromChat( adj_file_url, chat_id );
  }
  else if( Bee::isLocalFile( file_url ) )
  {
    QString file_path = Bee::convertToNativeFolderSeparator( file_url.toLocalFile() );
    if( file_path.isEmpty() )
    {
      qWarning() << "Unable to open an empty file path";
      return;
    }

    QFileInfo fi( file_path );
    if( !fi.exists() )
    {
      QMessageBox::information( qApp->activeWindow(), Settings::instance().programName(),
                                tr( "%1: no such file or directory." ).arg( file_path.isEmpty() ? file_url.toString() : file_path ), tr( "Ok" ) );
      return;
    }

#ifdef Q_OS_MAC
    bool is_exe_file = fi.isBundle();
#else
    bool is_exe_file = fi.isExecutable() && !fi.isDir();
#endif
#ifdef SHADOWCHAT_SAFE_BUILD
    if( is_exe_file )
    {
      QMessageBox::information( qApp->activeWindow(), QStringLiteral( "已阻止运行程序" ),
                                QStringLiteral( "影聊可以接收这个文件，但不会从聊天中启动可执行程序。请由公司 IT 确认后再处理。" ) );
      return;
    }
#else
    if( is_exe_file && QMessageBox::question( qApp->activeWindow(), Settings::instance().programName(),
                             tr( "Do you want to open the file %1?" ).arg( file_path ),
                             tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) != 0 )
      return;
#endif

    qDebug() << "Open file:" << file_path;
    if( !QDesktopServices::openUrl( QUrl::fromLocalFile( file_path ) ) )
      QMessageBox::information( qApp->activeWindow(), Settings::instance().programName(),
                              tr( "Unable to open %1" ).arg( file_path.isEmpty() ? file_url.toString() : file_path ), tr( "Ok" ) );
  }
  else
  {
    QString url_txt = file_url.toString();
#ifdef SHADOWCHAT_SAFE_BUILD
    if( file_url.scheme() != QLatin1String( "https" ) && file_url.scheme() != QLatin1String( "http" ) )
    {
      QMessageBox::information( qApp->activeWindow(), QStringLiteral( "已阻止未知链接" ),
                                QStringLiteral( "影聊只允许手动打开 HTTP 或 HTTPS 网页链接。" ) );
      return;
    }
    if( QMessageBox::question( qApp->activeWindow(), QStringLiteral( "打开外部网页" ),
                               QStringLiteral( "这个操作会离开影聊并使用系统浏览器访问：\n%1\n\n是否继续？" ).arg( url_txt ),
                               QStringLiteral( "继续打开" ), QStringLiteral( "取消" ), QString(), 1, 1 ) != 0 )
      return;
#endif
    qDebug() << "Open url:" << url_txt;
    if( !QDesktopServices::openUrl( file_url ) )
      qWarning() << "Unable to open link url:" << url_txt;
  }
}

void GuiMain::selectBeepFile()
{
  QString file_path = FileDialog::getOpenFileName( false, this, Settings::instance().programName(), Settings::instance().beepFilePath(), tr( "Sound files (*.wav)" ) );
  if( file_path.isNull() || file_path.isEmpty() )
    return;

  Settings::instance().setBeepFilePath( file_path );
  qDebug() << "New sound file selected:" << qPrintable( file_path );

  AudioManager::instance().clearBeep();
  AudioManager::instance().playBeep();

  if( !Settings::instance().beepOnNewMessageArrived() )
  {
    if( QMessageBox::question( this, Settings::instance().programName(), tr( "Sound is not enabled on a new message. Do you want to enable it?" ), tr( "Yes" ), tr( "No" ) ) == 0 )
    {
      Settings::instance().setBeepOnNewMessageArrived( true );
      mp_actBeepOnNewMessage->setChecked( true );
    }
  }
}

void GuiMain::testBeepFile()
{
  QString s_default_beep = tr( "The default BEEP will be used" );
  if( !AudioManager::instance().isAudioDeviceAvailable() )
  {
    qWarning() << "Sound device is not available";
    QMessageBox::warning( this, Settings::instance().programName(), QString( "%1. %2." ).arg( tr( "Sound module is not working" ), s_default_beep  ) );
  }
  else if( !QFile::exists( Settings::instance().beepFilePath() ) )
  {
    QString warn_text = QString( "%1\n%2. %3." ).arg( Settings::instance().beepFilePath() )
                                                  .arg( tr( "Sound file not found" ) )
                                                  .arg( s_default_beep );
    QMessageBox::warning( this, Settings::instance().programName(), warn_text );
  }

  playBeep();
}

void GuiMain::playBeep()
{
  AudioManager::instance().playBeep();
}

void GuiMain::playBuzz()
{
  AudioManager::instance().playBuzz();
}

void GuiMain::createGroupChat()
{
  if( !Settings::instance().canAddMembersToGroup() )
  {
    QMessageBox::information( this, Settings::instance().programName(),
                              tr( "You are not allowed create groups." ) + QString( " " ) + tr( "The option has been disabled by your system administrator." ),
                              tr( "Ok" ) );
    return;
  }

  GuiCreateGroup gcg( activeWindow() );
  gcg.loadData();
  gcg.setModal( true );
  gcg.show();
  gcg.setFixedSize( gcg.size() );
  if( gcg.exec() != QDialog::Accepted )
    return;

  if( gcg.leaveGroup() )
    return;

  Group g = gcg.group();

  Chat c = ChatManager::instance().findGroupChatByUsers( g.usersId() );
  if( c.isValid() )
  {
    if( QMessageBox::question( this, Settings::instance().programName(),
                                     QString( "%1\n%2" ).arg( tr( "There is a chat with the same members: %1." ).arg( c.name() ) )
                                                        .arg( "How do you want to continue?" ),
                                     tr( "Create new group chat" ), tr( "Cancel" ), QString(), 1, 1 ) == 1 )
    {
      return;
    }
  }

  c = beeCore->createGroupChat( Settings::instance().localUser(), g, true );
  if( c.isValid() )
    showChat( c.id() );
}

void GuiMain::editGroupChat( VNumber chat_id )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
    return;

  if( beeCore->isLocalGroupOwner( chat_id ) )
  {
    bool accepted = false;
    const QString manage = QStringLiteral( "群名称与成员" );
    const QString transfer = QStringLiteral( "转让群主" );
    const QString dissolve = QStringLiteral( "解散群" );
    const QString action = QInputDialog::getItem( this, QStringLiteral( "管理项目群" ), c.name(),
      QStringList() << manage << transfer << dissolve, 0, false, &accepted );
    if( !accepted ) return;
    if( action == transfer )
    {
      QStringList names; QList<VNumber> ids;
      foreach( VNumber id, c.usersId() )
      {
        if( id == ID_LOCAL_USER ) continue;
        const User member = UserManager::instance().findUser( id );
        if( !member.isValid() ) continue;
        names << QStringLiteral( "%1 (%2)" ).arg( member.name(), member.accountName() ); ids << id;
      }
      if( names.isEmpty() ) { QMessageBox::information( this, QStringLiteral( "转让群主" ), QStringLiteral( "群内还没有其他成员。" ) ); return; }
      const QString name = QInputDialog::getItem( this, transfer, QStringLiteral( "选择新群主" ), names, 0, false, &accepted );
      if( accepted && !beeCore->transferGroupOwnership( chat_id, ids.at( names.indexOf( name ) ) ) )
        QMessageBox::warning( this, transfer, QStringLiteral( "转让失败，请确认群和成员状态后重试。" ) );
      return;
    }
    if( action == dissolve )
    {
      if( QMessageBox::question( this, dissolve, QStringLiteral( "解散后成员不能继续在此群收发消息。本机历史和已接收文件会保留。确定解散？" ), QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) == QMessageBox::Yes &&
          !beeCore->dissolveGroup( chat_id ) )
        QMessageBox::warning( this, dissolve, QStringLiteral( "解散失败，请稍后重试。" ) );
      return;
    }
  }

  if( !Settings::instance().canAddMembersToGroup() && !Settings::instance().canRemoveMembersFromGroup() )
  {
    QMessageBox::information( this, Settings::instance().programName(),
                              tr( "You are not allowed modify groups." ) + QString( " " ) + tr( "The option has been disabled by your system administrator." ),
                              tr( "Ok" ) );
    return;
  }

  GuiCreateGroup gcg( activeWindow() );
  gcg.init( c.group() );
  gcg.loadData();
  gcg.setModal( true );
  gcg.show();
  gcg.setFixedSize( gcg.size() );
  if( gcg.exec() == QDialog::Accepted )
  {
    if( gcg.leaveGroup() )
      removeChat( chat_id );
    else
      beeCore->changeGroupChat( Settings::instance().localUser(), gcg.group() );
  }
}

void GuiMain::checkAutoStartOnBoot( bool add_service )
{
  if( add_service )
  {
    if( Settings::instance().addStartOnSystemBoot() )
    {
      QString alert_message = tr( "Now %1 will start on windows boot." ).arg( Settings::instance().programName() );
#ifdef Q_OS_WIN
      if( QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS8_1 )
        alert_message += QString( "\n%1" ).arg( "Note: you have to disable the alert window for unsigned software." );
#endif
      QMessageBox::information( this, Settings::instance().programName(), alert_message );
    }
    else
      QMessageBox::warning( this, Settings::instance().programName(), tr( "Unable to add this key in the registry: permission denied." ) );
  }
  else
  {
    if( Settings::instance().removeStartOnSystemBoot() )
      QMessageBox::information( this, Settings::instance().programName(), tr( "%1 will not start on windows boot." ).arg( Settings::instance().programName() ) );
    else
      QMessageBox::warning( this, Settings::instance().programName(), tr( "Unable to add this key in the registry: permission denied." ) );
  }
}

void GuiMain::loadSession()
{
  showMessage( tr( "Starting" ), 3000 );
  mp_tabMain->setCurrentWidget( mp_home );
  mp_home->loadSystemMessages();
  QTimer::singleShot( 100, beeCore, SLOT( buildSavedChatList() ) );

  QTimer::singleShot( Settings::instance().delayConnectionAtStartup(), this, SLOT( startCore() ) );
  if( Settings::instance().delayConnectionAtStartup() > 5000 )
    qDebug() << "Delay first connection for" << Settings::instance().delayConnectionAtStartup() << "ms";
}

void GuiMain::showSavedChatSelected( const QString& chat_name )
{
  if( chat_name.isEmpty() )
    return;

  foreach( QWidget* w, qApp->allWidgets() )
  {
    GuiSavedChat* gsv = qobject_cast<GuiSavedChat*>( w );
    if( gsv && gsv->savedChatName() == chat_name )
    {
      gsv->raise();
      return;
    }
  }

  GuiSavedChat* saved_chat = new GuiSavedChat( this );
  saved_chat->setAttribute( Qt::WA_DeleteOnClose, true );
  saved_chat->resize( qMin( QApplication::desktop()->availableGeometry().width()-80, 1000 ), qMin( QApplication::desktop()->availableGeometry().height()-80, 740 ) );
  connect( saved_chat, SIGNAL( deleteSavedChatRequest( const QString& ) ), this, SLOT( removeSavedChat( const QString& ) ) );
  connect( saved_chat, SIGNAL( openUrl( const QUrl& ) ), this, SLOT( openUrl( const QUrl& ) ) );
  saved_chat->showSavedChat( chat_name );
  saved_chat->show();
}

void GuiMain::removeSavedChat( const QString& chat_name )
{
  if( QMessageBox::question( this, Settings::instance().programName(), tr( "Do you want to delete saved chat with %1?" ).arg( chat_name ), tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 1 )
    return;

  foreach( QWidget* w, qApp->allWidgets() )
  {
    GuiSavedChat* gsv = qobject_cast<GuiSavedChat*>( w );
    if( gsv && gsv->savedChatName() == chat_name )
    {
      gsv->close();
      break;
    }
  }
  beeCore->removeSavedChat( chat_name );
  mp_savedChatList->updateSavedChats();
  updateTabTitles();
}

void GuiMain::linkSavedChat( const QString& chat_name )
{
  bool ok = false;
  QStringList chat_names_string_list = ChatManager::instance().chatNamesToStringList( true );
  chat_names_string_list.removeOne( chat_name );

  QString chat_name_selected = QInputDialog::getItem( this, Settings::instance().programName(),
                                        tr( "Please select a chat you would like to link the saved text."),
                                        chat_names_string_list, 0, false, &ok );
  if( !ok )
    return;

  bool add_to_existing_saved_text = false;
  if( ChatManager::instance().chatHasSavedText( chat_name_selected ) )
  {
     switch( QMessageBox::question( this, Settings::instance().programName(),
               tr( "The chat '%1' selected has already a saved text.<br>"
                   "What do you want to do with the selected saved text?" ).arg( chat_name_selected ),
                   tr( "Overwrite" ), tr( "Add in the head" ), tr( "Cancel" ), 2, 2 ) )
     {
     case 0:
       break;
     case 1:
       add_to_existing_saved_text = true;
       break;
     default:
       return;
     }

  }

  beeCore->linkSavedChat( chat_name, chat_name_selected, add_to_existing_saved_text );
}

bool GuiMain::openWebUrl( const QString& web_url )
{
  QUrl url( web_url );

  if( !QDesktopServices::openUrl( url ) )
  {
    QMessageBox::information( this, Settings::instance().programName(), tr( "Unable to open %1" ).arg( web_url ), tr( "Ok" ) );
    return false;
  }
  else
    return true;
}

void GuiMain::checkNewVersion()
{
  if( m_updateGate )
    m_updateGate->checkNow();
}

void GuiMain::openWebSite()
{
  openWebUrl( Settings::instance().officialWebSite() );
}

void GuiMain::openDownloadPluginPage()
{
  openWebUrl( Settings::instance().pluginWebSite() );
}

void GuiMain::openDonationPage()
{
  openWebUrl( Settings::instance().donationWebSite() );
}

void GuiMain::openHelpPage()
{
  openWebUrl( Settings::instance().helpWebSite() );
}

void GuiMain::openDeveloperWebSite()
{
  openWebUrl( Settings::instance().developerWebSite() );
}

void GuiMain::setInIdle()
{
  if( !beeCore->isConnected() )
    return;

  if( !Settings::instance().autoUserAway() )
    return;

  if( Settings::instance().localUser().status() == User::Away )
    return;

  m_lastUserStatus = Settings::instance().localUser().status();
  beeCore->setLocalUserStatus( User::Away );
  updateStatusIcon();
}

void GuiMain::exitFromIdle()
{
  if( !beeCore->isConnected() )
    return;

  if( !Settings::instance().autoUserAway() )
    return;

  if( Settings::instance().localUser().status() != User::Away )
    return;

  beeCore->setLocalUserStatus( m_lastUserStatus );
  updateStatusIcon();
}

void GuiMain::updateLocalStatusMessage()
{
  showMessage( Settings::instance().localUser().statusDescription(), 0 );
}

void GuiMain::onStatusBarMessageChanged( const QString& msg )
{
  if( msg.isNull() && !Settings::instance().localUser().statusDescription().isEmpty() )
    QTimer::singleShot( 0, this, SLOT( updateLocalStatusMessage() ) );
}

void GuiMain::showMessage( const QString& status_msg, int time_out )
{
  statusBar()->showMessage( status_msg, time_out );
  if( status_msg == Settings::instance().localUser().statusDescription() )
    statusBar()->setToolTip( tr( "Your status description" ) );
  else
    statusBar()->setToolTip( "" );
}

void GuiMain::changeUserColor( VNumber user_id, const QString& user_color )
{
  QColor c = QColorDialog::getColor( QColor( user_color ), qApp->activeWindow() );
  if( c.isValid() )
    beeCore->changeUserColor( user_id, c.name() );
}

void GuiMain::onUserChanged( const User& u )
{
  mp_userList->setUser( u, true );
  mp_groupList->updateUser( u );
  mp_chatList->updateUser( u );
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->updateUser( u );
  if( mp_embeddedChat && mp_embeddedChat->guiChat()->chatId() != ID_INVALID )
    updateModernChatHeader( ChatManager::instance().chat( mp_embeddedChat->guiChat()->chatId() ) );
  if( mp_modernSearch && !mp_modernSearch->text().trimmed().isEmpty() )
    updateModernSearchResults();
  updateModernNetworkState();
  updateModernListState();
  checkViewActions();
  if( u.isLocal() )
  {
    updateStatusIcon();
    updateWindowTitle();
  }
  else
  {
    if( m_changeTabToUserListOnFirstConnected && u.isStatusConnected() && mp_tabMain->currentWidget() == mp_home )
    {
      m_changeTabToUserListOnFirstConnected = false;
      mp_tabMain->setCurrentWidget( mp_userList );
    }
  }
#ifdef BEEBEEP_USE_SHAREDESKTOP
  foreach( GuiShareDesktop* gsd, m_desktops )
    gsd->onUserChanged( u );
#endif
}

void GuiMain::onUserRemoved( const User& u )
{
  showMessage( tr( "%1 removed" ).arg( u.name() ), 2000 );
  mp_userList->updateUsers();
  updateTabTitles();
  updateModernNetworkState();
  updateModernListState();
}

void GuiMain::onChatChanged( const Chat& c )
{
#ifdef BEEBEEP_DEBUG
  showMessage( tr( "%1 updated" ).arg( c.name() ), 2000 );
#endif
  mp_userList->updateChat( c );
  mp_chatList->updateChat( c );
  mp_groupList->updateChat( c );
  GuiFloatingChat* fl_chat = floatingChat( c.id() );
  if( fl_chat )
  {
    fl_chat->updateChat( c );
    if( fl_chat == mp_embeddedChat )
      updateModernChatHeader( c );
  }
  mp_savedChatList->updateSavedChats();
  updateTabTitles();
  updateNewMessageAction();
  if( mp_modernSearch && !mp_modernSearch->text().trimmed().isEmpty() )
    updateModernSearchResults();
  updateModernListState();
}

void GuiMain::onChatRemoved( const Chat& c )
{
  showMessage( tr( "%1 removed" ).arg( c.name() ), 2000 );
  GuiFloatingChat* fl_chat = floatingChat( c.id() );
  if( fl_chat )
    fl_chat->close();
  mp_chatList->updateChats();
  mp_groupList->updateGroups();
  mp_savedChatList->updateSavedChats();
  if( mp_trayIcon->chatId() == c.id() )
    mp_trayIcon->setUnreadMessages( c.id(), 0 );
  updateTabTitles();
  updateNewMessageAction();
  updateModernListState();
}

bool GuiMain::checkAllChatMembersAreConnected( const QList<VNumber>& users_id )
{
  if( !beeCore->areUsersConnected( users_id ) )
  {
    if( QMessageBox::question( activeWindow(), Settings::instance().programName(),
                               tr( "All the members of this chat are not online. The changes may not be permanent. Do you wish to continue?" ),
                               tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) != 0 )
       return false;
  }
  return true;
}

void GuiMain::clearChat( VNumber chat_id )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
    return;
  QString chat_name = c.isDefault() ? QObject::tr( "All users" ).toLower() : c.name();
  if( c.isEmpty() && !ChatManager::instance().chatHasSavedText( c.name() ) )
  {
    QMessageBox::information( activeWindow(), Settings::instance().programName(), tr( "Chat with %1 is empty." ).arg( chat_name ) );
    return;
  }

  QString question_txt = tr( "Do you want to clear messages with %1?" ).arg( chat_name );
  QString button_2_text;
  if( ChatManager::instance().chatHasSavedText( c.name() ) )
    button_2_text = QString( "  " ) + tr( "Yes and delete history" ) + QString( "  " );

  bool chat_cleared = false;
  switch( QMessageBox::information( activeWindow(), Settings::instance().programName(), question_txt, tr( "Yes" ), tr( "No" ), button_2_text, 1, 1 ) )
  {
  case 0:
    chat_cleared = beeCore->clearMessagesInChat( chat_id, false );
    break;
  case 2:
    chat_cleared = beeCore->clearMessagesInChat( chat_id, true );
    break;
  default:
    return;
  }

  if( chat_cleared )
  {
    GuiFloatingChat* fl_chat = floatingChat( chat_id );
    if( fl_chat )
    {
      Chat c = ChatManager::instance().chat( chat_id );
      if( c.isValid() )
        fl_chat->setChat( c );
    }

    if( chat_id == ID_DEFAULT_CHAT )
      mp_home->loadSystemMessages();
  }
}

void GuiMain::clearSystemMessagesInChat( VNumber chat_id )
{
  if( beeCore->clearSystemMessagesInChat( chat_id ) )
  {
    GuiFloatingChat* fl_chat = floatingChat( chat_id );
    if( fl_chat )
    {
      Chat c = ChatManager::instance().chat( chat_id );
      if( c.isValid() )
        fl_chat->setChat( c );
    }

    if( chat_id == ID_DEFAULT_CHAT )
      mp_home->loadSystemMessages();
  }
}

void GuiMain::removeChat( VNumber chat_id )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << chat_id << "found in GuiMain::removeChat(...)";
    return;
  }

  QString button_2_text;
  if( ChatManager::instance().chatHasSavedText( c.name() ) )
    button_2_text = QString( "  " ) + tr( "Yes and delete history" ) + QString( "  " );
  QString question_txt = tr( "Do you want to leave the chat with %1?" ).arg( c.name() );
  int answer_id = QMessageBox::question( activeWindow(), Settings::instance().programName(), question_txt, tr( "Yes" ), tr( "No" ), button_2_text, 1, 1 );
  if( answer_id == 1 )
    return;

  if( !beeCore->removeChat( chat_id, answer_id == 0 ) )
    QMessageBox::warning( activeWindow(), Settings::instance().programName(), tr( "Unable to leave the chat with %1." ).arg( c.name() ) );
}

void GuiMain::removeUnsentMessageInChat( VNumber chat_id )
{
  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << chat_id << "found in GuiMain::removeChat(...)";
    return;
  }

  QString question_txt = tr( "Do you want to delete all unsent messages in the chat with %1?" ).arg( c.name() );
  int answer_id = QMessageBox::question( activeWindow(), Settings::instance().programName(), question_txt, tr( "Yes" ), tr( "No" ), QString::null, 1, 1 );
  if( answer_id == 1 )
    return;

  beeCore->removeUnsentMessagesInChat( chat_id );
}

void GuiMain::showSharesForUser( const User& u )
{
  if( mp_fileSharing )
    mp_fileSharing->showUserFileList( u );
}

void GuiMain::selectLanguage()
{
  GuiLanguage gl;
  gl.setModal( true );
  gl.loadLanguages();
  gl.setSizeGripEnabled( true );
  gl.show();

  if( gl.exec() == QDialog::Rejected )
    return;

  QString old_language_path = Settings::instance().languageFilePath( Settings::instance().languagePath(), Settings::instance().language() );
  QString new_language_path = Settings::instance().languageFilePath( gl.folderSelected(), gl.languageSelected() );

  if( old_language_path != new_language_path )
  {
    QString language_message;
    if( gl.languageSelected().isEmpty() )
      language_message = tr( "Default language is restored." );
    else
      language_message = tr( "New language '%1' is selected." ).arg( gl.languageSelected() );

    QMessageBox::information( this, Settings::instance().programName(),
                              QString( "%1<br>%2" ).arg( language_message ).arg( tr( "You must restart %1 to apply these changes." )
                                                                                    .arg( Settings::instance().programName() ) ) );

    Settings::instance().setLanguage( gl.languageSelected() );
    Settings::instance().setLanguagePath( gl.folderSelected() );
  }
}

void GuiMain::showAddUser()
{
  GuiAddUser gad;
  gad.loadUsers();
  gad.setModal( true );
  gad.setSizeGripEnabled( true );
  gad.show();

  if( gad.exec() == QDialog::Accepted )
  {
    if( !Settings::instance().networkAddressList().isEmpty() )
    {
      beeCore->updateUsersAddedManually();
      QMetaObject::invokeMethod( this, "sendBroadcastMessage", Qt::QueuedConnection );
    }
  }
}

void GuiMain::showChatSettingsMenu()
{
  mp_menuChat->clear();
  QAction* act = mp_menuChat->addAction( tr( "Use RTL mode to show text" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showTextInModeRTL() );
  act->setData( 55 );
  mp_menuChat->addSeparator();
  act = mp_menuChat->addAction( tr( "Show the chat in compact view mode" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatCompact() );
  act->setData( 1 );
  act = mp_menuChat->addAction( tr( "Show the timestamp" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatShowMessageTimestamp() );
  act->setData( 3 );
  act = mp_menuChat->addAction( tr( "Show the time with the AM/PM notation" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().useMessageTimestampWithAP() );
  act->setData( 76 );
  act = mp_menuChat->addAction( tr( "Show messages grouped by user" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showMessagesGroupByUser() );
  act->setData( 13 );
  act = mp_menuChat->addAction( tr( "Show your name instead of 'You'" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatUseYourNameInsteadOfYou() );
  act->setData( 41 );
  if( mp_actShowUserFullName->isChecked() )
  {
    mp_menuChat->addAction( mp_actShowUserFullName );
    mp_menuChat->addAction( mp_actShowUserFirstNameFirstInFullName );
  }
  act = mp_menuChat->addAction( tr( "Show username's color" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatUseColoredUserNames() );
  act->setData( 67 );
  mp_menuChat->addSeparator();
  act = mp_menuChat->addAction( tr( "Show emoticons" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().showEmoticons() );
  act->setData( 10 );
  act = mp_menuChat->addAction( tr( "Use HTML tags" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatUseHtmlTags() );
  act->setData( 8 );
  act = mp_menuChat->addAction( tr( "Use clickable links" ), this, SLOT( settingsChanged() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().chatUseClickableLinks() );
  act->setData( 9 );
  mp_menuChat->addSeparator();
  act = mp_menuChat->addAction( tr( "Clear recent emoticons" ), this, SLOT( settingsChanged() ) );
  act->setIcon( IconManager::instance().icon( "clear.png" ) );
  act->setData( 104 );
  act = mp_menuChat->addAction( tr( "Clear favorite emoticons" ), this, SLOT( settingsChanged() ) );
  act->setIcon( IconManager::instance().icon( "clear.png" ) );
  act->setData( 106 );
  act = mp_menuChat->addAction( tr( "Restore default font" ), this, SLOT( settingsChanged() ) );
  act->setIcon( IconManager::instance().icon( "font.png" ) );
  act->setData( 23 );
  mp_menuChat->exec( QCursor::pos() );
}

void GuiMain::showDefaultServerPortInMenu()
{
  QString host_address = tr( "offline" );
  QString broadcast_port = tr( "offline" );
  QString listener_port = tr( "offline" );
  QString file_transfer_port = tr( "offline" );
#ifdef BEEBEEP_USE_MULTICAST_DNS
  QString multicast_dns = tr( "inactive" );
#endif
  QString multicast_group = tr( "none" );

  if( beeCore->isConnected() )
  {
    mp_menuNetworkStatus->setIcon( IconManager::instance().icon( "network-connected.png" ) );
    mp_actHostAddress->setIcon( IconManager::instance().icon( "network-connected.png" ) );
    mp_actHostAddress->setEnabled( true );
    mp_actPortBroadcast->setEnabled( true );
    mp_actPortListener->setEnabled( true );
    mp_actEncryptedConnectionByDefault->setEnabled( true );
    mp_actCompressedDataByDefault->setEnabled( true );

    host_address = Settings::instance().localUser().networkAddress().hostAddress().toString();
    broadcast_port = QString::number( Settings::instance().defaultBroadcastPort() );
    listener_port = QString::number( Settings::instance().localUser().networkAddress().hostPort() );

    QHostAddress multicast_host_address = beeCore->multicastGroupAddress();
    mp_actMulticastGroupAddress->setEnabled( !multicast_host_address.isNull() );
    if( !multicast_host_address.isNull() )
      multicast_group = multicast_host_address.toString();

    if( Settings::instance().enableFileTransfer() )
    {
      file_transfer_port = QString::number( beeCore->fileTransferPort() );
      mp_actPortFileTransfer->setEnabled( true );
      if( mp_networkTest )
        mp_networkTest->updateSettings( QString::number( beeCore->fileTransferPort() ) );
    }
    else
    {
      file_transfer_port = tr( "disabled" );
      mp_actPortFileTransfer->setEnabled( false );
      if( mp_networkTest )
        mp_networkTest->updateSettings( file_transfer_port );
    }

#ifdef BEEBEEP_USE_MULTICAST_DNS
    if( beeCore->dnsMulticastingIsActive() )
    {

      multicast_dns = tr( "active" );
      mp_actMulticastDns->setEnabled( true );
    }
    else
      mp_actMulticastDns->setEnabled( false );
#endif


  }
  else
  {
    mp_menuNetworkStatus->setIcon( IconManager::instance().icon( "network-disconnected.png" ) );
    mp_actHostAddress->setIcon( IconManager::instance().icon( "network-disconnected.png" ) );
    mp_actHostAddress->setEnabled( false );
    mp_actPortBroadcast->setEnabled( false );
    mp_actMulticastGroupAddress->setEnabled( false );
    mp_actPortListener->setEnabled( false );
    mp_actPortFileTransfer->setEnabled( false );
#ifdef BEEBEEP_USE_MULTICAST_DNS
    mp_actMulticastDns->setEnabled( false );
#endif
    mp_actEncryptedConnectionByDefault->setEnabled( false );
    mp_actCompressedDataByDefault->setEnabled( false );
  }

  mp_actHostAddress->setText( QString( "ip: %1 (%2)" ).arg( host_address ).arg( tr( "your IP address" ) ) );
  mp_actPortBroadcast->setText( QString( "udp: %1 (%2)" ).arg( broadcast_port ).arg( tr( "to search for users" ) ) );
  mp_actMulticastGroupAddress->setText( QString( "multicast: %1" ).arg( multicast_group ) );
  mp_actPortListener->setText( QString( "tcp1: %1 (%2)" ).arg( listener_port ).arg( tr( "chat messages" ) ) );
  mp_actPortFileTransfer->setText( QString( "tcp2: %1 (%2)" ).arg( file_transfer_port ).arg( tr( "file transfer" ) ) );
#ifdef BEEBEEP_USE_MULTICAST_DNS
  mp_actMulticastDns->setText( QString( "mdns: %1" ).arg( multicast_dns ) );
#endif
  if( Settings::instance().disableConnectionSocketEncryption() )
  {
    mp_actEncryptedConnectionByDefault->setIcon( IconManager::instance().icon( "warning.png" ) );
    mp_actEncryptedConnectionByDefault->setText( tr( "End-to-end encryption is disabled" ) );
  }
  else
  {
    mp_actEncryptedConnectionByDefault->setIcon( IconManager::instance().icon( "encryption-enabled.png" ) );
    mp_actEncryptedConnectionByDefault->setText( tr( "End-to-end encryption is enabled" ) );
  }

  if( Settings::instance().disableConnectionSocketEncryption() )
  {
    mp_actCompressedDataByDefault->setIcon( IconManager::instance().icon( "folder.png" ) );
    mp_actCompressedDataByDefault->setText( tr( "Data compression is disabled" ) );
  }
  else
  {
    mp_actCompressedDataByDefault->setIcon( IconManager::instance().icon( "file-compressed.png" ) );
    mp_actCompressedDataByDefault->setText( tr( "Data compression is enabled" ) );
  }
}

void GuiMain::sendBroadcastMessage()
{
  mp_actBroadcast->setDisabled( true );
  beeCore->sendBroadcastMessage();
#ifdef BEEBEEP_USE_MULTICAST_DNS
  beeCore->sendDnsMulticastingMessage();
#endif
  QTimer::singleShot( 61000, this, SLOT( enableBroadcastAction() ) );
}

void GuiMain::enableBroadcastAction()
{
  mp_actBroadcast->setEnabled( beeCore->isConnected() );
}

void GuiMain::checkUserSelected( VNumber user_id )
{
  User u = UserManager::instance().findUser( user_id );
  if( !u.isValid() )
  {
    qWarning() << "Invalid user id" << user_id << "found in check user selected";
    return;
  }

  Chat c = ChatManager::instance().privateChatForUser( user_id );
  if( !c.isValid() )
  {
    beeCore->createPrivateChat( u );
    c = ChatManager::instance().privateChatForUser( user_id );
    if( !c.isValid() )
    {
      qWarning() << "Unable to create private chat for user" << user_id;
      return;
    }
  }
  showChat( c.id() );
}

void GuiMain::showConnectionStatusChanged( const User& u )
{
  if( !beeCore->isConnected() )
    return;

  if( Settings::instance().showOnlyMessageNotificationOnTray() )
    return;

  QString msg;
  if( u.isStatusConnected() )
    msg = tr( "%1 is online" ).arg( u.name() );
  else
    msg = tr( "%1 is offline" ).arg( u.name() );

  Chat c = ChatManager::instance().privateChatForUser( u.id() );
  if( c.isValid() && u.isStatusConnected() )
    mp_trayIcon->showUserStatusChanged( c.id(), msg );
  else
    mp_trayIcon->showUserStatusChanged( ID_DEFAULT_CHAT, msg );
}

void GuiMain::changeAvatarSizeInList()
{
  bool ok = false;
  int avatar_size = QInputDialog::getInt( this, Settings::instance().programName(), tr( "Please select the new size of the user picture" ),
                                          Settings::instance().avatarIconSize().height(), 16, 96, 4, &ok );
  if( !ok )
    return;

  Settings::instance().setAvatarIconSize( QSize( avatar_size, avatar_size ) );
  mp_userList->updateUsers();
  mp_chatList->updateChats();
  mp_groupList->updateGroups();
  mp_savedChatList->updateSavedChats();
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
  {
    Chat c = ChatManager::instance().chat( fl_chat->guiChat()->chatId() );
    if( c.isValid() )
      fl_chat->updateChatMembers( c );
  }
}

void GuiMain::toggleUserFavorite( VNumber user_id )
{
  beeCore->toggleUserFavorite( user_id );
}

void GuiMain::removeUserFromList( VNumber user_id )
{
  QString question_txt = tr( "Do you want to delete user %1?" ).arg( UserManager::instance().findUser( user_id ).name() );
  if( QMessageBox::question( activeWindow(), Settings::instance().programName(), question_txt, tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) != 0 )
    return;
  beeCore->removeOfflineUser( user_id );
}

void GuiMain::openResourceFolder()
{
  QUrl folder_url = QUrl::fromLocalFile( Settings::instance().resourceFolder() );
  openUrl( folder_url );
}

void GuiMain::openDataFolder()
{
  QUrl folder_url = QUrl::fromLocalFile( Settings::instance().dataFolder() );
  openUrl( folder_url );
}

void GuiMain::openDownloadFolder()
{
  QUrl folder_url = QUrl::fromLocalFile( Settings::instance().downloadDirectory() );
  openUrl( folder_url );
}

GuiFloatingChat* GuiMain::floatingChat( VNumber chat_id ) const
{
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
  {
    if( fl_chat->guiChat()->chatId() == chat_id )
      return fl_chat;
  }
  return Q_NULLPTR;
}

void GuiMain::removeFloatingChatFromList( VNumber chat_id )
{
  GuiFloatingChat* fl_chat = floatingChat( chat_id );
  if( !fl_chat )
    return;

  if( mp_menuChat->isVisible() )
    mp_menuChat->close();

  m_floatingChats.removeOne( fl_chat );
  if( fl_chat == mp_embeddedChat )
  {
    mp_embeddedChat = Q_NULLPTR;
    if( mp_modernConversationStack && mp_modernChatPlaceholder )
      mp_modernConversationStack->setCurrentWidget( mp_modernChatPlaceholder );
  }
  fl_chat->deleteLater();
#ifdef BEEBEEP_DEBUG
  qDebug() << "Floating chat" << chat_id << "closed and deleted";
#endif

#ifdef BEEBEEP_USE_SHAREDESKTOP
  onShareDesktopRequestFromChat( chat_id, false );
#endif
}

GuiFloatingChat* GuiMain::createFloatingChat( const Chat& c )
{
  GuiFloatingChat* fl_chat = floatingChat( c.id() );
  if( fl_chat )
    return fl_chat;

  if( mp_embeddedChat )
    fl_chat = mp_embeddedChat;

  if( !fl_chat && Settings::instance().showChatsInOneWindow() && !m_floatingChats.isEmpty() )
    fl_chat = m_floatingChats.first();

  if( !fl_chat )
  {
    if( mp_modernChatBody && mp_modernChatBodyLayout )
    {
      fl_chat = new GuiFloatingChat( mp_modernChatBody );
      fl_chat->setWindowFlags( Qt::Widget );
      fl_chat->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
      fl_chat->setEmbeddedMode( true );
      fl_chat->statusBar()->hide();
      mp_modernChatBodyLayout->addWidget( fl_chat );
      mp_embeddedChat = fl_chat;
    }
    else
      fl_chat = new GuiFloatingChat;
    setupChatConnections( fl_chat->guiChat() );
    connect( fl_chat, SIGNAL( chatIsAboutToClose( VNumber ) ), this, SLOT( removeFloatingChatFromList( VNumber ) ) );
    connect( fl_chat, SIGNAL( readAllMessages( VNumber ) ), this, SLOT( readAllMessagesInChat( VNumber ) ) );
    connect( fl_chat, SIGNAL( showVCardRequest( VNumber ) ), this, SLOT( showVCard( VNumber ) ) );
    connect( fl_chat, SIGNAL( updateChatFontRequest() ), this, SLOT( updateChatFont() ) );
    connect( fl_chat, SIGNAL( updateChatColorsRequest() ), this, SLOT( updateChatColors() ) );
#ifdef BEEBEEP_USE_VOICE_CHAT
    connect( fl_chat, SIGNAL( sendVoiceMessageRequest( VNumber, const QString&, qint64 ) ), this, SLOT( sendVoiceMessageToChat( VNumber, const QString&, qint64 ) ) );
#endif

    m_floatingChats.append( fl_chat );
    if( fl_chat != mp_embeddedChat )
    {
      fl_chat->setWindowFlagsAndGeometry();
      fl_chat->setSaveGeometryDisabled( Settings::instance().resetGeometryAtStartup() );
    }
    else
      fl_chat->setSaveGeometryDisabled( true );
  }

  if( !fl_chat->setChat( c ) )
  {
    qWarning() << "Unable to create floating window for not existing chat" << c.id() << c.name();
    fl_chat->deleteLater();
    return Q_NULLPTR;
  }

  return fl_chat;
}

QWidget* GuiMain::activeWindow()
{
  QWidget* active_window = QApplication::activeWindow();
  if( active_window )
    return active_window;
  else
    return qobject_cast<QWidget*>( this );
}

void GuiMain::loadUserStatusRecentlyUsed()
{
  if( !mp_menuUserStatusList->isEmpty() )
    mp_menuUserStatusList->clear();

  QList<UserStatusRecord> user_status_list;
  foreach( QString s, Settings::instance().userStatusList() )
  {
    UserStatusRecord usr = Protocol::instance().loadUserStatusRecord( s );
    if( usr.isValid() )
      user_status_list.append( usr );
  }

  if( user_status_list.size() < Settings::instance().maxUserStatusDescriptionInList() )
  {
    UserStatusRecord usr1;
    usr1.setStatus( User::Away );
    usr1.setStatusDescription( tr( "at lunch" ) );
    if( !user_status_list.contains( usr1 ) )
      user_status_list.append( usr1 );
  }

  if( user_status_list.size() < Settings::instance().maxUserStatusDescriptionInList() )
  {
    UserStatusRecord usr2;
    usr2.setStatus( User::Busy );
    usr2.setStatusDescription( tr( "in a meeting" ) );
    if( !user_status_list.contains( usr2 ) )
      user_status_list.append( usr2 );
  }

  qSort( user_status_list );

  QAction* act;
  foreach( UserStatusRecord usr, user_status_list )
  {
    if( usr.isValid() )
    {
      act = mp_menuUserStatusList->addAction( Bee::userStatusIcon( usr.status() ), usr.statusDescription(), this, SLOT( recentlyUsedUserStatusSelected() ) );
      act->setData( Protocol::instance().saveUserStatusRecord( usr ) );
    }
  }
}

void GuiMain::recentlyUsedUserStatusSelected()
{
  QAction* act = qobject_cast<QAction*>( sender() );
  if( !act )
    return;

  UserStatusRecord usr = Protocol::instance().loadUserStatusRecord( act->data().toString() );
  if( usr.isValid() )
  {
    beeCore->setLocalUserStatusDescription( usr.status(), usr.statusDescription(), false );
    setUserStatusSelected( usr.status() );
    loadUserStatusRecentlyUsed();
    updateLocalStatusMessage();
  }
}

void GuiMain::clearRecentlyUsedUserStatus()
{
  if( QMessageBox::question( this, Settings::instance().programName(),
                             tr( "Do you want to clear all saved status descriptions?" ),
                             tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) != 0 )
    return;

  qDebug() << "User status description list is cleared";
  QStringList sl = Settings::instance().userStatusList();
  sl.clear();
  Settings::instance().setUserStatusList( sl );
  loadUserStatusRecentlyUsed();
}

void GuiMain::loadSavedChatsCompleted()
{
  mp_chatList->updateChats();
  mp_savedChatList->updateSavedChats();
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->setChat( ChatManager::instance().chat( fl_chat->guiChat()->chatId() ) );
  updateTabTitles();
}

void GuiMain::editShortcuts()
{
  GuiShortcut gs;
  gs.setModal( true );
  gs.loadShortcuts();
  gs.setSizeGripEnabled( true );
  gs.show();

  if( gs.exec() == QDialog::Rejected )
    return;

  Settings::instance().setCustomShortcuts( ShortcutManager::instance().saveToStringList() );
  Settings::instance().save();
  updateShortcuts();
}

void GuiMain::updateShortcuts()
{
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->guiChat()->updateShortcuts();

  QKeySequence ks = ShortcutManager::instance().shortcut( ShortcutManager::ShowFileTransfers );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    mp_actViewFileTransfer->setShortcut( ks );
  else
    mp_actViewFileTransfer->setShortcut( QKeySequence() );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::MinimizeAllChats );
  if( !ks.isEmpty() )
  {
    mp_scMinimizeAllChats->setKey( ks );
    mp_scMinimizeAllChats->setEnabled( Settings::instance().useCustomShortcuts() );
  }
  else
    mp_scMinimizeAllChats->setEnabled( false );

#ifdef BEEBEEP_USE_QXT
  ks = ShortcutManager::instance().shortcut( ShortcutManager::ShowAllChats );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
  {
    mp_scShowAllChats->setShortcut( ks );
    mp_scShowAllChats->setEnabled( true );
  }
  else
  {
    mp_scShowAllChats->unsetShortcut();
    mp_scShowAllChats->setEnabled( false );
  }

  ks = ShortcutManager::instance().shortcut( ShortcutManager::SendHelpMessage );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
  {
    mp_scSendHelpMessage->setShortcut( ks );
    mp_scSendHelpMessage->setEnabled( true );
  }
  else
  {
    mp_scSendHelpMessage->unsetShortcut();
    mp_scSendHelpMessage->setEnabled( false );
  }

  ks = ShortcutManager::instance().shortcut( ShortcutManager::SelectFirstChat );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
  {
    mp_scSelectFirstChat->setShortcut( ks );
    mp_scSelectFirstChat->setEnabled( true );
  }
  else
  {
    mp_scSelectFirstChat->unsetShortcut();
    mp_scSelectFirstChat->setEnabled( false );
  }
#endif

  ks = ShortcutManager::instance().shortcut( ShortcutManager::ShowNextUnreadMessage );
  if( !ks.isEmpty() )
  {
    mp_scShowNextUnreadMessage->setKey( ks );
    mp_scShowNextUnreadMessage->setEnabled( Settings::instance().useCustomShortcuts() );
  }
  else
    mp_scShowNextUnreadMessage->setEnabled( false );

  ks = ShortcutManager::instance().shortcut( ShortcutManager::Broadcast );
  if( !ks.isEmpty() && Settings::instance().useCustomShortcuts() )
    mp_actBroadcast->setShortcut( ks );
  else
    mp_actBroadcast->setShortcut( QKeySequence() );

}

void GuiMain::minimizeAllChats()
{
  QWidget* w = qApp->activeWindow();

  bool last_active_window_exists = w == this ? true : false;

  if( !m_floatingChats.isEmpty() )
  {
    foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    {
      if( !fl_chat->isMinimized() )
        fl_chat->showMinimized();
      if( fl_chat == mp_lastActiveWindow )
        last_active_window_exists = true;
    }
  }

  if( !isMinimized() )
    showMinimized();

  if( last_active_window_exists )
    mp_lastActiveWindow = w;
}

void GuiMain::showAllChats()
{
  bool last_active_window_exists = false;

  if( !m_floatingChats.isEmpty() )
  {
    foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    {
      if( fl_chat->isMinimized() )
        fl_chat->showNormal();
      else
        fl_chat->show();
      fl_chat->raiseOnTop();

      if( fl_chat == mp_lastActiveWindow )
        last_active_window_exists = true;
    }
  }

  if( isMinimized() )
    showNormal();
  else
    show();
  raiseOnTop();

  if( this == mp_lastActiveWindow )
    last_active_window_exists = true;

  if( last_active_window_exists )
  {
    mp_lastActiveWindow->raise();
    qApp->setActiveWindow( mp_lastActiveWindow );
  }
}

void GuiMain::selectDictionatyPath()
{
  QString dictionary_path = FileDialog::getOpenFileName( false, this, tr( "Select your dictionary path" ), Settings::instance().dictionaryPath(), QString( "*.dic" ) );

  if( dictionary_path.isEmpty() )
    return;

  Settings::instance().setDictionaryPath( dictionary_path );
#ifdef BEEBEEP_USE_HUNSPELL
  if( SpellChecker::instance().setDictionary( dictionary_path ) )
    QMessageBox::information( this, Settings::instance().programName(), tr( "Dictionary selected: %1" ).arg( dictionary_path ) );
  else
    QMessageBox::warning( this, Settings::instance().programName(), tr( "Unable to set dictionary: %1" ).arg( dictionary_path ) );
#endif
  Settings::instance().setUseSpellChecker( true );
  Settings::instance().setUseWordCompleter( true );

  // update spellchecker and wordcompleter actions
  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->guiChat()->updateActionsOnFocusChanged();
}

void GuiMain::onNetworkInterfaceDown()
{
  updateModernNetworkState();
  updateModernListState();
  if( beeCore->isConnected() )
    QMetaObject::invokeMethod( this, "stopCore", Qt::QueuedConnection );
}

void GuiMain::onNetworkInterfaceUp()
{
  if( m_userSelectOffline )
    qDebug() << "User selected offline mode and the connection to network is refused";
  else
    QMetaObject::invokeMethod( this, "startCore", Qt::QueuedConnection );
  updateModernNetworkState();
  updateModernListState();
}

void GuiMain::onTickEvent( int ticks )
{
  int chat_tab_index = mp_tabMain->indexOf( mp_chatList );
  if( mp_actViewNewMessage->isEnabled() )
  {
    QIcon new_message_blinking_icon = IconManager::instance().icon( "beebeep-message.png" );
    mp_actViewNewMessage->setIcon( ticks % 2 == 0 ? new_message_blinking_icon : Bee::convertToGrayScale( new_message_blinking_icon, Settings::instance().mainBarIconSize() ) );
    mp_tabMain->setTabIcon( chat_tab_index, ticks % 2 == 0 ? new_message_blinking_icon : IconManager::instance().icon( "chat-list.png" ) );
    setWindowIcon( ticks % 2 == 0 ? new_message_blinking_icon : IconManager::instance().icon( "beebeep.png" )  );
  }

  mp_trayIcon->onTickEvent( ticks );
  mp_chatList->onTickEvent( ticks );
  mp_userList->onTickEvent( ticks );
  mp_groupList->onTickEvent( ticks );
  mp_fileTransfer->onTickEvent( ticks );
  if( mp_fileSharing )
    mp_fileSharing->onTickEvent( ticks );
  if( mp_networkTest )
    mp_networkTest->onTickEvent( ticks );

  if( beeCore->hasFileTransferInProgress() )
    mp_actViewFileTransfer->setIcon( ticks % 2 == 0 ? IconManager::instance().icon( "file-transfer-progress.png" ) : IconManager::instance().icon( "file-transfer.png" ) );

  if( ticks % 2 == 0 && mp_actViewNewMessage->isEnabled() )
  {
    Chat c = ChatManager::instance().firstChatWithUnreadMessages();
    GuiFloatingChat* fl_chat = floatingChat( c.id() );
    if( !fl_chat )
      QApplication::alert( this, 1000 );
    showMessage( tr( "You have new message" ), 1000 );
  }

  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->onTickEvent( ticks );

#ifdef BEEBEEP_USE_SHAREDESKTOP
  foreach( GuiShareDesktop* gsd, m_desktops )
    gsd->onTickEvent( ticks );
#endif

  if( beeApp->isInSleepMode() )
    return;

  if( !m_coreIsConnecting )
  {
    if( Bee::isTimeToCheck( ticks, Settings::instance().tickIntervalCheckIdle() ) )
    {
      if( beeApp->idleTimeout() > 0 )
        QMetaObject::invokeMethod( beeApp, "checkIdle", Qt::QueuedConnection );
    }

    beeCore->onTickEvent( ticks );
  }
}

void GuiMain::onChatReadByUser( const Chat& c, const User& u )
{
  GuiFloatingChat* fl_chat = floatingChat( c.id() );
  if( fl_chat )
    fl_chat->setChatReadByUser( c, u );
}

void GuiMain::readAllMessagesInChat( VNumber chat_id )
{
  beeCore->readAllMessagesInChat( chat_id );
}

void GuiMain::saveSession( QSessionManager& )
{
  qDebug() << "Session manager ask to save and close session";
  forceShutdown();
}

void GuiMain::updateEmoticons()
{
  if( m_floatingChats.isEmpty() )
    return;

  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->updateEmoticons();
}

void GuiMain::updateNewMessageAction()
{
  Chat c = ChatManager::instance().firstChatWithUnreadMessages();
  mp_trayIcon->setNextChatToRead( c );
  mp_actViewNewMessage->setEnabled( c.isValid() );
  int chat_tab_index = mp_tabMain->indexOf( mp_chatList );
  if( mp_actViewNewMessage->isEnabled() )
  {
    mp_actViewNewMessage->setStatusTip( tr( "You have new message" ) );
    mp_tabMain->setTabIcon( chat_tab_index, IconManager::instance().icon( "beebeep-message.png" ) );
    setWindowIcon( IconManager::instance().icon( "beebeep-message.png" ) );
  }
  else
  {
    mp_actViewNewMessage->setStatusTip( "" );
    mp_tabMain->setTabIcon( chat_tab_index, IconManager::instance().icon( "chat-list.png" ) );
    setWindowIcon( IconManager::instance().icon( "beebeep.png" )  );
  }
}

void GuiMain::saveGeometryAndState()
{
  if( isVisible() )
  {
    Settings::instance().setGuiGeometry( saveGeometry() );
    Settings::instance().setGuiState( saveState() );
    Settings::instance().save();
  }
}

void GuiMain::askSaveGeometryAndState()
{
  if( isVisible() )
  {
    QByteArray ba_state = saveState();
#if QT_VERSION == 0x050906
    int default_button = mp_dockFileTransfers->isVisible() ? 0 : 1;
    int ret_code = QMessageBox::warning( this, Settings::instance().programName(),
                                         tr( "Qt libraries have a bug on saving the window's state." ) + QString( " " ) +
                                         tr( "If you have layout problem please save only geometry." ),
                                         tr( "Save all" ), tr( "Save only geometry" ), tr( "Cancel" ), default_button, 2 );
    switch( ret_code )
    {
    case 0:
      break;
    case 1:
      ba_state = QByteArray();
      break;
    default:
      return;
    }
 #endif
    Settings::instance().setGuiGeometry( saveGeometry() );
    Settings::instance().setGuiState( ba_state );
    Settings::instance().save();
    if( ba_state.isEmpty() )
      showMessage( tr( "Window geometry saved" ), 3000 );
    else
      showMessage( tr( "Window geometry and state saved" ), 3000 );
  }
}

void GuiMain::restoreGeometryAndState()
{
  restoreGeometry( Settings::instance().guiGeometry() );
  if( !Settings::instance().guiState().isEmpty() )
    restoreState( Settings::instance().guiState() );
}

void GuiMain::resetGeometryAndState()
{
  if( mp_dockFileTransfers->isVisible() )
    mp_dockFileTransfers->hide();
  const QRect available_geometry = QApplication::desktop()->availableGeometry();
  const int target_width = qMin( 1280, qMax( 980, available_geometry.width() - 140 ) );
  const int target_height = qMin( 820, qMax( 640, available_geometry.height() - 140 ) );
  resize( target_width, target_height );

#ifdef Q_OS_WIN
  move( available_geometry.x() + (available_geometry.width() - frameGeometry().width()) / 2,
        available_geometry.y() + (available_geometry.height() - frameGeometry().height()) / 2 );
#elif defined BEEBEEP_FOR_RASPBERRY_PI
  move( QApplication::desktop()->availableGeometry().width() - frameGeometry().width(), 70 );
#elif defined Q_OS_MAC
  #if QT_VERSION >= 0x050900
    move( QApplication::desktop()->availableGeometry().width() - frameGeometry().width(), 0 );
  #else
    move( QApplication::desktop()->availableGeometry().width() - frameGeometry().width() - 20, 0 );
  #endif
#else
  move( QApplication::desktop()->availableGeometry().width() - frameGeometry().width(), 0 );
#endif

  if( !Settings::instance().guiGeometry().isEmpty() )
    Settings::instance().setGuiGeometry( QByteArray() );
  if( !Settings::instance().guiState().isEmpty() )
    Settings::instance().setGuiState( QByteArray() );
}

void GuiMain::askResetGeometryAndState()
{
  if( QMessageBox::question( this, Settings::instance().programName(), tr( "Do you really want to reset window's geometry?" ), tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 0 )
  {
    resetGeometryAndState();
    setMinimumWidthForStyle();
    if( !Settings::instance().floatingChatGeometry().isEmpty() )
      Settings::instance().setFloatingChatGeometry( QByteArray() );
    if( !Settings::instance().floatingChatState().isEmpty() )
      Settings::instance().setFloatingChatState( QByteArray() );
    if( !Settings::instance().floatingChatSplitterState().isEmpty() )
      Settings::instance().setFloatingChatSplitterState( QByteArray() );
    if( !Settings::instance().createMessageGeometry().isEmpty() )
      Settings::instance().setCreateMessageGeometry( QByteArray() );
    if( !Settings::instance().fileSharingGeometry().isEmpty() )
      Settings::instance().setFileSharingGeometry( QByteArray() );
    Settings::instance().setShowEmoticonMenu( false );
    Settings::instance().save();
    showMessage( tr( "Geometry of all windows has been reset" ), 3000 );
  }
}

void GuiMain::onChangeSettingOnExistingFile( QAction* act )
{
  if( !act )
    return;

  Settings::instance().setOnExistingFileAction( act->data().toInt() );
  Settings::instance().save();
}

void GuiMain::onFileTransferProgress( VNumber peer_id, const User& u, const FileInfo& fi, FileSizeType bytes, qint64 elapsed_time )
{
  mp_fileTransfer->setProgress( peer_id, u, fi, bytes, elapsed_time );
  if( Settings::instance().alwaysShowFileTransferProgress() )
  {
    if( !mp_dockFileTransfers->isVisible() )
    {
      mp_dockFileTransfers->show();
      raiseOnTop();
    }
  }
}

void GuiMain::onFileTransferMessage( VNumber peer_id, const User& u, const FileInfo& fi, const QString& msg, FileTransferPeer::TransferState ft_state )
{
  mp_fileTransfer->setMessage( peer_id, u, fi, msg, ft_state );
  if( Settings::instance().alwaysShowFileTransferProgress() )
  {
    if( !mp_dockFileTransfers->isVisible() )
    {
      mp_dockFileTransfers->show();
      raiseOnTop();
    }
  }

  if( ft_state == FileTransferPeer::Completed )
  {
    if( fi.isDownload() )
    {
      Chat c = ChatManager::instance().findChatByPrivateId( fi.chatPrivateId(), false, u.id() );
      if( !c.isValid() )
      {
        qWarning() << "Unable to find chat by private id" << qPrintable( fi.chatPrivateId() ) << "for user" << qPrintable( u.name() ) << "in onFileTransferCompleted(...)";
        return;
      }

      if( Settings::instance().showFileTransferCompletedOnTray() &&
          !ShadowConversationPreferences().muted( c ) &&
          !(c.isGroup() && Settings::instance().isNotificationDisabledForGroup( c.privateId() )) )
        mp_trayIcon->showNewFileArrived( c.id(), tr( "New file from %1" ).arg( u.name() ), false );

      GuiFloatingChat* fl_chat = floatingChat( c.id() );
      if( fl_chat && fl_chat->isActiveWindow() )
        return;
      onChatChanged( c );
    }
  }
}

void GuiMain::sendBuzzToUser( VNumber user_id )
{
  if( beeCore->isConnected() )
    beeCore->sendBuzzToUser( user_id );
}

void GuiMain::showBuzzFromUser( const User& u, VNumber chat_id )
{
  if( Settings::instance().playBuzzSound() )
    playBuzz();

  mp_trayIcon->showNewMessageArrived( chat_id, tr( "%1 is buzzing you!" ).arg( Bee::userNameToShow( u, false ) ), true );
}

void GuiMain::sendHelpMessage()
{
  if( !beeCore->sendHelpMessage() )
  {
    // TODO: alert / success message
  }
}

void GuiMain::sendHelpMessageToUser( VNumber user_id )
{
  if( !beeCore->sendHelpMessageToUser( user_id ) )
  {
    // TODO: alert / success message
  }
}

void GuiMain::showHelpRequestFromUser( const User& u, VNumber chat_id )
{
  if( Settings::instance().playBuzzSound() )
    playBuzz();

  mp_trayIcon->showNewMessageArrived( chat_id, tr( "%1 is asking for your help!" ).arg( Bee::userNameToShow( u, false ) ), true );
}

void GuiMain::showHelpAnswerFromUser( const User& u, VNumber chat_id )
{
  if( Settings::instance().playBuzzSound() )
    playBuzz();

  mp_trayIcon->showNewMessageArrived( chat_id, tr( "%1 got your call for help." ).arg( Bee::userNameToShow( u, false ) ), true );
}

void GuiMain::showFileSharingWindow()
{
  if( Settings::instance().disableFileSharing() )
    return;

  if( !mp_fileSharing )
  {
    mp_fileSharing = new GuiFileSharing( Q_NULLPTR );
    mp_fileSharing->setAttribute( Qt::WA_DeleteOnClose, true );
    Bee::setWindowStaysOnTop( mp_fileSharing, Settings::instance().stayOnTop() );
    mp_fileSharing->resize( qMin( (QApplication::desktop()->availableGeometry().width()-20), 860 ), 460 );
    mp_fileSharing->updateLocalFileList();
    connect( mp_fileSharing, SIGNAL( destroyed() ), this, SLOT( onFileSharingWindowClosed() ) );
    connect( mp_fileSharing, SIGNAL( openUrlRequest( const QUrl& ) ), this, SLOT( openUrl( const QUrl& ) ) );
    connect( mp_fileSharing, SIGNAL( sendFileRequest( const QString& ) ), this, SLOT( sendFile( const QString& ) ) );
    connect( mp_fileSharing, SIGNAL( downloadSharedFileRequest( VNumber, VNumber ) ), this, SLOT( downloadSharedFile( VNumber, VNumber ) ) );
    connect( mp_fileSharing, SIGNAL( downloadSharedFilesRequest( const QList<SharedFileInfo>& ) ), this, SLOT( downloadSharedFiles( QList<SharedFileInfo> ) ) );
    connect( beeCore, SIGNAL( userChanged( const User& ) ), mp_fileSharing, SLOT( onUserChanged( const User& ) ) );
  }
  mp_fileSharing->showUp();
}

void GuiMain::onFileSharingWindowClosed()
{
  if( mp_fileSharing )
    mp_fileSharing = Q_NULLPTR;
}

void GuiMain::showScreenShotWindow()
{
  if( !mp_screenShot )
  {
    mp_screenShot = new GuiScreenShot;
    mp_screenShot->setAttribute( Qt::WA_DeleteOnClose, true );
    Bee::setWindowStaysOnTop( mp_screenShot, Settings::instance().stayOnTop() );
    mp_screenShot->resize( 620,  460 );
    connect( mp_screenShot, SIGNAL( screenShotToSend( const QString& ) ), this, SLOT( sendFile( const QString& ) ) );
    connect( mp_screenShot, SIGNAL( destroyed() ), this, SLOT( onScreenShotWindowClosed() ) );
  }
  mp_screenShot->showUp();
}

void GuiMain::onScreenShotWindowClosed()
{
  if( mp_screenShot )
    mp_screenShot = Q_NULLPTR;
}

void GuiMain::showLogWindow()
{
  if( !mp_log )
  {
    mp_log = new GuiLog;
    Bee::setWindowStaysOnTop( mp_log, Settings::instance().stayOnTop() );
    mp_log->resize( qMin( (QApplication::desktop()->availableGeometry().width()-20), 760 ), 460 );
    connect( mp_log, SIGNAL( destroyed() ), this, SLOT( onLogWindowClosed() ) );
  }
  mp_log->showUp();
}

void GuiMain::onLogWindowClosed()
{
  if( mp_log )
    mp_log = Q_NULLPTR;
}

void GuiMain::onMainTabChanged( int tab_index )
{
  if( mp_tabMain->widget( tab_index ) == mp_home )
  {
    m_unreadActivities = 0;
    updateTabTitles();
  }

  if( m_changeTabToUserListOnFirstConnected )
    m_changeTabToUserListOnFirstConnected = false;
}

void GuiMain::setFileTransferEnabled( bool enable )
{
  if( Settings::instance().disableFileTransfer() )
    return;

  Settings::instance().setEnableFileTransfer( enable );
  if( !enable )
  {
    Settings::instance().setEnableFileSharing( false );
    Settings::instance().setUseShareBox( false );
    mp_actEnableFileSharing->setChecked( false );
    beeCore->stopFileTransferServer();
    QMetaObject::invokeMethod( beeCore, "buildLocalShareList", Qt::QueuedConnection );
  }
  else
  {
    if( !Bee::folderIsWriteable( Settings::instance().downloadDirectory(), false ) )
    {
      QMessageBox::warning( this, Settings::instance().programName(), tr( "The download folder does not exists or is not writable. Please select a new one." ), tr( "Ok" ) );
      selectDownloadDirectory();
    }
    beeCore->startFileTransferServer();
  }
  checkViewActions();
}

void GuiMain::setFileSharingEnabled( bool enable )
{
  if( Settings::instance().disableFileTransfer() || Settings::instance().disableFileSharing() )
    return;

  Settings::instance().setEnableFileSharing( enable );
  QMetaObject::invokeMethod( beeCore, "buildLocalShareList", Qt::QueuedConnection );
  checkViewActions();
  if( !enable )
  {
    if( mp_fileSharing )
      mp_fileSharing->close();
  }
}

void GuiMain::showWorkgroups()
{
  GuiWorkgroups gw( this );
  gw.loadWorkgroups();
  gw.setModal( true );
  gw.setSizeGripEnabled( true );
  gw.show();
  if( gw.exec() != QDialog::Accepted )
    return;

  if( Settings::instance().acceptConnectionsOnlyFromWorkgroups() && !Settings::instance().localUser().workgroups().isEmpty() )
    qDebug() << "You have selected to accept connections only from these workgroups:" << qPrintable( Settings::instance().localUser().workgroups().join( ", " ) );

  if( gw.restartConnection() )
    showRestartConnectionAlertMessage();
}

void GuiMain::showRefusedChats()
{
  GuiRefusedChat grc( this );
  int previous_refused_chats = grc.loadRefusedChats();
  grc.setModal( true );
  grc.setSizeGripEnabled( true );
  grc.show();
  if( grc.exec() == QDialog::Accepted )
  {
    showMessage( tr( "%1 blocked chats" ).arg( Settings::instance().refusedChats().size() ), 5000 );
    if( previous_refused_chats != ChatManager::instance().refusedChats().size() )
      showRestartConnectionAlertMessage();
  }
}

void GuiMain::selectIconSourcePath()
{
  int ret_code = 1;
  if( Settings::instance().iconSourcePath().isEmpty() )
  {
    ret_code = QMessageBox::information( this, Settings::instance().programName(),
                                         tr( "You are using the default icons." ),
                                         tr( "Select the icon folder" ), tr( "Cancel" ), QString(), 1, 1 );
  }
  else
  {
    ret_code = QMessageBox::information( this, Settings::instance().programName(),
                                         tr( "You are using custom icons in folder %1." ).arg( Settings::instance().iconSourcePath() ),
                                         tr( "Select the icon folder" ), tr( "Cancel" ), tr( "Restore default icons" ), 1, 1 );
  }

  if( ret_code == 1 )
    return;

  QString icon_source_path = "";
  if( ret_code == 0 )
  {
    icon_source_path = QFileDialog::getExistingDirectory( this, tr( "Select the icon folder" ),
                                                                Settings::instance().iconSourcePath().isEmpty() ? Settings::instance().resourceFolder() : Settings::instance().iconSourcePath() );
    if( icon_source_path.isEmpty() )
      return;

    icon_source_path = Bee::convertToNativeFolderSeparator( icon_source_path );
  }

  if( icon_source_path != Settings::instance().iconSourcePath() )
  {
    showRestartApplicationAlertMessage();
    Settings::instance().setIconSourcePath( icon_source_path );
  }
}

void GuiMain::selectEmoticonSourcePath()
{
  int ret_code = 1;
  if( Settings::instance().emoticonSourcePath().isEmpty() )
  {
    ret_code = QMessageBox::information( this, Settings::instance().programName(),
                                         tr( "You are using the default emoticons." ),
                                         tr( "Select the emoticon folder" ), tr( "Cancel" ), QString(), 1, 1 );
  }
  else
  {
    ret_code = QMessageBox::information( this, Settings::instance().programName(),
                                         tr( "You are using custom emoticons in folder %1." ).arg( Settings::instance().emoticonSourcePath() ),
                                         tr( "Select the emoticon folder" ), tr( "Cancel" ), tr( "Restore default emoticons" ), 1, 1 );
  }

  if( ret_code == 1 )
    return;

  QString emoticon_source_path = "";
  if( ret_code == 0 )
  {
    emoticon_source_path = QFileDialog::getExistingDirectory( this, tr( "Select the emoticon folder" ),
                                                                  Settings::instance().emoticonSourcePath().isEmpty() ? Settings::instance().resourceFolder() : Settings::instance().emoticonSourcePath() );
    if( emoticon_source_path.isEmpty() )
      return;

    emoticon_source_path = Bee::convertToNativeFolderSeparator( emoticon_source_path );
  }

  if( emoticon_source_path != Settings::instance().emoticonSourcePath() )
  {
    Settings::instance().setEmoticonSourcePath( emoticon_source_path );
    updateEmoticons();
  }
}

void GuiMain::showRestartConnectionAlertMessage()
{
  if( QMessageBox::information( this, Settings::instance().programName(), tr( "You have to restart your connection to apply changes." ),
                            tr( "Restart connection now" ), tr( "Restart later manually" ), QString(), 0, 1 ) == 0 )
  {
    QTimer::singleShot( 0, this, SLOT( restartCore() ) );
  }
}

void GuiMain::showRestartApplicationAlertMessage()
{
  QMessageBox::information( this, Settings::instance().programName(), tr( "You must restart %1 to apply these changes." ).arg( Settings::instance().programName() ), tr( "Ok" ) );
}

void GuiMain::onNewsAvailable( const QString& news )
{
#ifdef BEEBEEP_DEBUG
  qDebug() << "News from the website available:" << qPrintable( news );
#endif
  mp_home->setNews( news );
}

void GuiMain::createMessage()
{
  if( Settings::instance().disableCreateMessage() )
  {
    QMessageBox::information( this, Settings::instance().programName(),
                              tr( "You are not allowed create new message." ) + QString( " " ) + tr( "The option has been disabled by your system administrator." ),
                              tr( "Ok" ) );
    return;
  }
  GuiCreateMessage gcm( this );
  gcm.setModal( true );
  gcm.setSizeGripEnabled( true );
  gcm.show();
  if( gcm.exec() == QDialog::Accepted )
  {
    if( !gcm.toChatIdList().isEmpty() )
    {
      int max_chat_to_open = 6;
      int num_chat_opened = 0;
      foreach( VNumber chat_id, gcm.toChatIdList() )
      {
        beeCore->sendChatMessage( chat_id, gcm.message(), gcm.messageIsImportant(), num_chat_opened >= max_chat_to_open, false );
        if( gcm.openChat() && num_chat_opened < max_chat_to_open )
        {
          showChat( chat_id );
          num_chat_opened++;
        }
      }
      updateTabTitles();
      mp_chatList->updateChats();
    }
  }
}

void GuiMain::updateChatFont()
{
  if( m_floatingChats.isEmpty() )
    return;

  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
    fl_chat->guiChat()->setChatFont( Settings::instance().chatFont() );
}

void GuiMain::updateChatColors()
{
  if( m_floatingChats.isEmpty() )
    return;

  foreach( GuiFloatingChat* fl_chat, m_floatingChats )
  {
    if( fl_chat->guiChat()->chatId() != ID_DEFAULT_CHAT )
      fl_chat->guiChat()->updateChatColors();
  }
}

void GuiMain::resetAllColors()
{
  if( QMessageBox::question( this, Settings::instance().programName(), tr( "Do you really want to restore the colors to the default ones?" ), tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 0 )
  {
    Settings::instance().resetAllColors();
    Settings::instance().save();
    loadStyle();
  }
}

void GuiMain::onHideEmptyChatsRequest()
{
  Settings::instance().setHideEmptyChatsInList( !Settings::instance().hideEmptyChatsInList() );
  mp_chatList->updateChats();
}

void GuiMain::onApplicationFocusChanged( QWidget* old, QWidget* now )
{
  if( old == Q_NULLPTR && isAncestorOf( now )  )
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "Main window has grabbed focus";
#endif
    m_prevActivatedState = true;
    return;
  }

  if( isAncestorOf( old ) && now == Q_NULLPTR )
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "Main window has lost focus";
#endif
    m_prevActivatedState = false;
    return;
  }

  bool current_state = isActiveWindow();
  if( current_state != m_prevActivatedState )
  {
    m_prevActivatedState = current_state;
#ifdef BEEBEEP_DEBUG
    if( current_state )
      qDebug() << "Main window has grabbed focus (active)";
    else
      qDebug() << "Main window has lost focus (active)";
#endif
  }
}

#ifdef BEEBEEP_USE_WEBENGINE
void GuiMain::showWebView()
{
  mp_webView->show();
}

void GuiMain::onNewsLoad( bool ok )
{
  mp_actWebView->setEnabled( ok );
}
#endif

#ifdef BEEBEEP_USE_SHAREDESKTOP
void GuiMain::onShareDesktopImageAvailable( const User& u, const QImage& img, const QString& image_type, QRgb diff_color )
{
  foreach( GuiShareDesktop* gsd, m_desktops )
  {
    if( gsd->userId() == u.id() )
    {
      if( img.isNull() )
        gsd->close();
      else
        gsd->updateImage( img, image_type, diff_color );
      return;
    }
  }

  if( img.isNull() )
    return;

  int desktop_h = qApp->desktop()->availableGeometry().height() - 50;
  int desktop_w = qApp->desktop()->availableGeometry().width() - 80;
  int frame_w = frameGeometry().width() - geometry().width();
  int frame_h = frameGeometry().height() - geometry().height();
  int max_img_w = img.width()+ frame_w;
  int max_img_h = img.height()+ frame_h;

  GuiShareDesktop* new_gui = new GuiShareDesktop;
  connect( new_gui, SIGNAL( shareDesktopClosed( VNumber ) ), this, SLOT( onShareDesktopCloseEvent( VNumber ) ) );
  connect( new_gui, SIGNAL( shareDesktopDeleteRequest( VNumber ) ), this, SLOT( onShareDesktopDeleteRequest( VNumber ) ) );
  new_gui->setUser( u );
  new_gui->setGeometry( 10, 40, qMin( max_img_w, qMax( 640, desktop_w ) ), qMin( max_img_h, qMax( 480, desktop_h ) ) );
  new_gui->setMaximumSize( max_img_w, max_img_h );
  new_gui->show();
  new_gui->updateImage( img, image_type, diff_color );

  m_desktops.append( new_gui );
}

void GuiMain::onShareDesktopCloseEvent( VNumber user_id )
{
#ifdef BEEBEEP_DEBUG
  qDebug() << "You close the desktop shared viewer for user" << user_id;
#endif
  beeCore->refuseToViewShareDesktop( ID_LOCAL_USER, user_id );
}

void GuiMain::onShareDesktopDeleteRequest( VNumber user_id )
{
  QList<GuiShareDesktop*>::iterator it = m_desktops.begin();
  while( it != m_desktops.end() )
  {
    if( (*it)->userId() == user_id )
    {
#ifdef BEEBEEP_DEBUG
      qDebug() << "Delete GuiShareDesktop for user" << user_id;
#endif
      (*it)->disconnect();
      (*it)->deleteLater();
      m_desktops.erase( it );
      return;
    }
  }
}

void GuiMain::onShareDesktopRequestFromChat( VNumber chat_id, bool enable_desktop_sharing )
{
  if( chat_id == ID_DEFAULT_CHAT )
    return;

  if( beeCore->shareDesktopIsActive( ID_INVALID ) == enable_desktop_sharing )
    return;

  Chat c = ChatManager::instance().chat( chat_id );
  if( !c.isValid() )
  {
    qWarning() << "Invalid chat" << chat_id << "found in GuiMain::onShareDesktopRequestFromChat(...)";
    return;
  }

  if( enable_desktop_sharing )
  {
    if( QMessageBox::question( activeWindow(), Settings::instance().programName(),
                               tr( "Do you really want to share your desktop with %1?" ).arg( c.name() ),
                               tr( "Yes" ), tr( "No" ), QString(), 0, 1 ) == 1 )
      return;

    foreach( VNumber user_id, c.usersId() )
    {
      if( user_id != ID_LOCAL_USER )
        beeCore->startShareDesktop( user_id );
    }
  }
  else
  {
    foreach( VNumber user_id, c.usersId() )
    {
      if( user_id != ID_LOCAL_USER )
        beeCore->stopShareDesktop( user_id );
    }
  }
}

void GuiMain::onShareDesktopUpdate( const User& u )
{
  bool core_is_connected = beeCore->isConnected();
  int connected_users = beeCore->connectedUsers();
  bool file_transfer_is_active = beeCore->isFileTransferActive();
  QList<Chat> chat_list = ChatManager::instance().chatsWithUser( u.id() );
  foreach( Chat c, chat_list )
  {
    GuiFloatingChat* fl_chat = floatingChat( c.id() );
    if( fl_chat )
      fl_chat->guiChat()->updateActions( c, core_is_connected, connected_users, file_transfer_is_active );
  }
}

void GuiMain::sendScreenshotToChat( VNumber chat_id )
{
  QMetaObject::invokeMethod( beeCore, "sendScreenshotToChat", Qt::QueuedConnection, Q_ARG(VNumber, chat_id) );
  GuiFloatingChat* fl_chat = floatingChat( chat_id );
  if( fl_chat && fl_chat->isMinimized() )
    QTimer::singleShot( 200, fl_chat, SLOT( showNormal() ) );
}
#endif

void GuiMain::showNetworkTest()
{
  if( !mp_networkTest )
  {
    mp_networkTest = new GuiNetworkTest( this );
    mp_networkTest->updateSettings( Settings::instance().enableFileTransfer() ? QString::number( beeCore->fileTransferPort() ) : tr( "disabled" ) );
    connect( mp_networkTest, SIGNAL( destroyed() ), this, SLOT( onScreenShotWindowClosed() ) );
  }
  mp_networkTest->showUp();
}

void GuiMain::onNetworkTestWindowClosed()
{
  if( mp_networkTest )
    mp_networkTest = Q_NULLPTR;
}

void GuiMain::setMinimumWidthForStyle()
{
  if( Settings::instance().resetMinimumWidthForStyle() )
  {
    int old_w = width();
    int wasted_w = 20;
#if defined( Q_OS_MAC )
    int icon_wasted_w = m_useFusionStyle ? 4 : 8;
    int min_w = qMax( 300, mp_barMain->actions().size() * (mp_barMain->iconSize().width() + icon_wasted_w) + wasted_w );
#elif defined( Q_OS_UNIX )
    int min_w = qMax( 320, mp_barMain->actions().size() * (mp_barMain->iconSize().width()+4) + wasted_w );
#elif defined( Q_OS_WIN )
    int min_w = qMax( 320, mp_barMain->actions().size() * (mp_barMain->iconSize().width()+2) + wasted_w );
#else
    int min_w = qMax( 320, mp_barMain->actions().size() * (mp_barMain->iconSize().width()+2) + wasted_w );
#endif
    setMinimumWidth( min_w );
    if( min_w > old_w )
    {
      int new_pos_x = qMax( 1, pos().x() - (min_w - old_w) );
      move( new_pos_x, pos().y() );
    }
  }
  else
    setMinimumWidth( 90 );
}

void GuiMain::loadStyle()
{
  QStyle* p_style = QStyleFactory::create( "Fusion" );
  if( p_style )
  {
    beeApp->setStyle( p_style );
    m_useFusionStyle = true;
  }
  else
  {
    beeApp->resetStyle();
    m_useFusionStyle = false;
  }

  if( Settings::instance().useDarkStyle() )
  {
    qDebug() << "Stylesheet:" << (m_useFusionStyle ? "Fusion" : "System Default") << "Dark";
    beeApp->setPalette( Bee::darkPalette() );
  }
  else
  {
    qDebug() << "Stylesheet:" << (m_useFusionStyle ? "Fusion" : "System Default" );
    beeApp->resetPalette();
  }

  setMinimumWidthForStyle();

  mp_home->updateBackground();
  mp_userList->updateBackground();
  mp_chatList->updateBackground();
  mp_groupList->updateBackground();
  mp_savedChatList->updateBackground();
  updateChatColors();
  updateEmoticons();
  applyModernStyle();
  setMinimumSize( 720, 520 );
}

#ifdef BEEBEEP_USE_VOICE_CHAT
void GuiMain::sendVoiceMessageToChat( VNumber chat_id, const QString& file_path, qint64 message_duration )
{
  beeCore->sendVoiceMessageToChat( chat_id, file_path, message_duration );
}

void GuiMain::showVoiceEncoderSettings()
{
  GuiRecordVoiceMessageSettings grvm( this );
  grvm.setModal( true );
  grvm.init();
  grvm.loadSettings();
  grvm.show();
  if( grvm.exec() == QDialog::Accepted )
  {
    Settings::instance().save();
    AudioManager::instance().checkDefaultAudioDevice();
  }
}
#endif

void GuiMain::resumeFileTransfer( VNumber user_id, const FileInfo& file_info )
{
  beeCore->resumeFileTransfer( user_id, file_info );
}

void GuiMain::changeEmoticonSizeInChat()
{
  bool ok = false;
  int max_size = Settings::instance().emoticonSourcePath().isEmpty() ? (Settings::instance().useHiResEmoticons() ? 190 : 24) : 248;
  int emoticon_size = QInputDialog::getInt( this, Settings::instance().programName(), tr( "Please select the new size of the emoticons in chat" ),
                                          Settings::instance().emoticonSizeInChat(), 16, max_size, 4, &ok );
  if( !ok )
    return;

  Settings::instance().setEmoticonSizeInChat( emoticon_size );
}

void GuiMain::selectFirstChat()
{
  if( isMinimized() )
    showNormal();
  else
    show();
  raiseOnTop();

  if( mp_tabMain->currentWidget() != mp_chatList )
    mp_tabMain->setCurrentWidget( mp_chatList );
  qApp->setActiveWindow( this );
  mp_chatList->selectFirstChat();

}
