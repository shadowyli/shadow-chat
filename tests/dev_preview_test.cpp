#include "BeeApplication.h"
#include "ShadowDevPreview.h"
#include "ShadowLocalAccounts.h"
#include "Settings.h"
#include "Core.h"
#include "Connection.h"
#include "ChatManager.h"
#include "GuiMain.h"
#include "GuiSystemTray.h"
#include "IconManager.h"
#include "ShadowChatTranscript.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include "ShadowDialogStyle.h"
#include "ShadowScreenshotPreview.h"
#include "GuiChat.h"
#include "MessageManager.h"
#include "UserManager.h"
#include "ShadowDirectoryPage.h"
#include "ShadowSearchPanel.h"
#include "GuiSavedChat.h"
#include "GuiWizard.h"
#include "GuiChatList.h"
#include "ShadowConversationPreferences.h"
#include "Random.h"
#include "Protocol.h"
#include "ShadowWindowShape.h"
#include <QTcpServer>
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

class PreviewCoreProbe : public Core
{
public:
  using Core::newPeerFound;
  using Core::sendFileToUser;
  using Core::parseMessage;
  using Core::dispatchChatMessageReceived;

  void showTestSendFailure( VNumber chatId )
  {
    dispatchSystemMessage( chatId, ID_LOCAL_USER, QStringLiteral( "未发送：合成失败原因" ),
      DispatchToChat, ChatMessage::UserInfo, false );
  }
};

static int verifyQuietProfileUpdates( PreviewCoreProbe& core )
{
  Settings& settings = Settings::instance();
  const User originalLocal = settings.localUser();
  const Chat originalChat = ChatManager::instance().chat( 88001 );
  User originalPeer;
  for( VNumber userId : originalChat.usersId() )
    if( userId != ID_LOCAL_USER )
      originalPeer = UserManager::instance().findUser( userId );
  CHECK( originalPeer.isValid() );
  QMap<VNumber, int> messageCounts;
  QMap<VNumber, int> unreadCounts;
  QMap<VNumber, QDateTime> messageTimes;
  for( const Chat& chat : ChatManager::instance().constChatList() )
  {
    messageCounts.insert( chat.id(), chat.messages().size() );
    unreadCounts.insert( chat.id(), chat.unreadMessages() );
    messageTimes.insert( chat.id(), chat.lastMessageTimestamp() );
  }
  QObject observer;
  int notifications = 0;
  int localChanges = 0;
  int peerChanges = 0;
  QObject::connect( &core, &Core::newChatMessage, &observer,
    [&notifications]( const Chat&, const ChatMessage& ) { ++notifications; } );
  QObject::connect( &core, &Core::userChanged, &observer,
    [&]( const User& user ) {
      if( user.isLocal() ) ++localChanges;
      if( user.id() == originalPeer.id() ) ++peerChanges;
    } );
  VCard localProfile = originalLocal.vCard();
  localProfile.setNickName( QStringLiteral( "静默昵称验证" ) );
  CHECK( core.setLocalUserVCard( originalLocal.color(), localProfile ) );
  CHECK( settings.localUser().name() == localProfile.nickName() && localChanges == 1 );
  QPixmap avatar( 24, 24 );
  avatar.fill( QColor( "#4381ca" ) );
  localProfile.setPhoto( avatar );
  localProfile.setInfo( QStringLiteral( "静默资料更新" ) );
  CHECK( core.setLocalUserVCard( originalLocal.color(), localProfile ) );
  CHECK( settings.localUser().vCard().info() == localProfile.info() &&
    settings.localUser().vCard().photo().toImage() == avatar.toImage() && localChanges == 2 );
  CHECK( core.setLocalUserVCard( "#2468ac", localProfile ) );
  CHECK( settings.localUser().color() == "#2468ac" && localChanges == 3 );
  CHECK( !core.setLocalUserVCard( "#2468ac", localProfile ) && localChanges == 3 );
  auto applyPeerProfile = [&]( const User& peer ) {
    const User local = settings.localUser();
    settings.setLocalUser( peer );
    const Message update = Protocol::instance().localVCardMessage( peer.protocolVersion() );
    settings.setLocalUser( local );
    core.parseMessage( peer.id(), update );
  };
  User peer = originalPeer;
  peer.setName( QStringLiteral( "联系人静默改名" ) );
  applyPeerProfile( peer );
  CHECK( UserManager::instance().findUser( peer.id() ).name() == peer.name() && peerChanges == 1 );
  CHECK( ChatManager::instance().chat( originalChat.id() ).name() == peer.name() );
  VCard peerProfile = peer.vCard();
  peerProfile.setInfo( QStringLiteral( "联系人静默更新资料和头像" ) );
  peerProfile.setPhoto( avatar );
  peer.setVCard( peerProfile );
  applyPeerProfile( peer );
  const User updatedPeer = UserManager::instance().findUser( peer.id() );
  CHECK( updatedPeer.vCard().info() == peerProfile.info() && peerChanges == 2 );
  CHECK( !updatedPeer.vCard().photo().isNull() &&
    updatedPeer.vCard().photo().toImage().pixelColor( 0, 0 ) == QColor( "#4381ca" ) );
  applyPeerProfile( peer );
  CHECK( peerChanges == 2 );
  CHECK( core.setLocalUserVCard( originalLocal.color(), originalLocal.vCard() ) );
  applyPeerProfile( originalPeer );
  CHECK( notifications == 0 );
  for( const Chat& chat : ChatManager::instance().constChatList() )
  {
    CHECK( chat.messages().size() == messageCounts.value( chat.id() ) );
    CHECK( chat.unreadMessages() == unreadCounts.value( chat.id() ) );
    CHECK( chat.lastMessageTimestamp() == messageTimes.value( chat.id() ) );
  }
  CHECK( core.dispatchChatMessageReceived( originalPeer.id(),
    Protocol::instance().chatMessage( originalChat, QStringLiteral( "正常聊天消息仍然可见" ) ) ) );
  CHECK( notifications == 1 );
  CHECK( ChatManager::instance().chat( originalChat.id() ).messages().last().type() == ChatMessage::Chat );
  core.showTestSendFailure( originalChat.id() );
  CHECK( notifications == 2 );
  CHECK( ChatManager::instance().chat( originalChat.id() ).messages().last().message().contains( QStringLiteral( "未发送" ) ) );
  ChatManager::instance().setChat( originalChat );
  core.chatChanged( originalChat );
  std::puts( "PASS quiet profile updates: local nickname/avatar/info/color and remote nickname/avatar/info synchronize without chat notifications, unread/time changes; normal messages and send failures still arrive" );
  return 0;
}

static int verifyProfileStatus( GuiMain& window )
{
  const bool wasConnected = beeCore->isConnected();
  if( !wasConnected )
    CHECK( beeCore->start() );
  const int originalStatus = Settings::instance().localUser().status();
  bool verified = false;
  std::puts( "Checking profile status integration" );
  QTimer::singleShot( 100, &window, [&]() {
    QDialog* profile = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( !profile ) return;
    std::puts( "Profile modal opened" );
    QPushButton* status = profile->findChild<QPushButton*>( "profileCardStatus" );
    if( status && status->menu() )
    {
      for( QAction* action : status->menu()->actions() )
        if( action->data().toInt() == User::Busy ) action->trigger();
      verified = Settings::instance().localUser().status() == User::Busy &&
        status->menu()->actions().at( 1 )->isChecked();
      for( QAction* action : status->menu()->actions() )
        if( action->data().toInt() == originalStatus ) action->trigger();
    }
    profile->grab().save( QDir::currentPath() + "/ui-031-profile-card.png" );
    profile->reject();
  } );
  QToolButton* profileButton = window.findChild<QToolButton*>( "ShadowTitleProfile" );
  CHECK( profileButton && profileButton->isVisible() );
  profileButton->click();
  CHECK( verified && Settings::instance().localUser().status() == originalStatus );
  if( !wasConnected )
    beeCore->stop();
  std::puts( "PASS profile status integration: native card updates Core and restores the previous state" );
  return 0;
}

static bool atomicWrite( const QString& path, const QByteArray& data )
{
  QSaveFile file( path );
  return file.open( QIODevice::WriteOnly ) && file.write( data ) == data.size() && file.commit();
}

static void settle()
{
  QEventLoop loop;
  QTimer::singleShot( 700, &loop, &QEventLoop::quit );
  loop.exec();
}

static bool symmetricCorners( const QWidget& widget )
{
  const QRegion mask = widget.mask();
  if( mask.isEmpty() ) return false;
  for( int row = 0; row < 18; ++row )
    for( int column = 0; column < 18; ++column )
    {
      const bool included = mask.contains( QPoint( column, row ) );
      if( mask.contains( QPoint( widget.width() - 1 - column, row ) ) != included ||
          mask.contains( QPoint( column, widget.height() - 1 - row ) ) != included ||
          mask.contains( QPoint( widget.width() - 1 - column, widget.height() - 1 - row ) ) != included )
        return false;
    }
  return mask.contains( widget.rect().center() ) && !mask.contains( QPoint() );
}

static int verifyWindowCorners( GuiMain& window )
{
  const QSize originalSize = window.size();
  const QList<QSize> sizes = QList<QSize>() << QSize( 900, 700 ) << QSize( 901, 701 ) << QSize( 1280, 820 );
  for( const QSize& size : sizes )
  {
    window.resize( size );
    QApplication::processEvents();
    CHECK( symmetricCorners( window ) );
    CHECK( window.mask() == ShadowWindowShape::roundedMask( window.size() ) );
  }
  window.grab().save( QDir::currentPath() + "/ui-036-main-corners.png" );
  QDialog dialog( &window );
  QVBoxLayout* layout = new QVBoxLayout( &dialog );
  layout->addWidget( new QLabel( QStringLiteral( "四角统一圆角验证" ), &dialog ) );
  dialog.resize( 500, 320 );
  dialog.show();
  QApplication::processEvents();
  QWidget* surface = dialog.findChild<QWidget*>( "shadowDialogSurface" );
  CHECK( surface && symmetricCorners( *surface ) );
  dialog.grab().save( QDir::currentPath() + "/ui-036-dialog-corners.png" );
  dialog.reject();
  window.showMaximized();
  QApplication::processEvents();
  CHECK( window.mask().isEmpty() );
  window.showNormal();
  window.resize( originalSize );
  QApplication::processEvents();
  CHECK( symmetricCorners( window ) );
  std::puts( "PASS symmetric window corners: four mirrored corners, odd/even sizes, shared dialog radius, maximize and restore" );
  return 0;
}

static int verifyPreviewSending( GuiMain& window, Core& core, ShadowDevPreview& preview )
{
  CHECK( core.devPreviewIsolated() && ShadowDevPreview::testPeerReady() );
  CHECK( QMetaObject::invokeMethod( &window, "showChat", Qt::DirectConnection, Q_ARG( VNumber, VNumber( 88001 ) ) ) );
  settle();
  GuiChat* chat = Q_NULLPTR;
  for( GuiChat* candidate : window.findChildren<GuiChat*>() )
    if( candidate->isVisible() ) chat = candidate;
  CHECK( chat && chat->chatId() == 88001 );
  QObject* transport = preview.findChild<QObject*>( "ShadowPreviewTransport" );
  QTcpServer* server = preview.findChild<QTcpServer*>( "ShadowPreviewPeerServer" );
  CHECK( transport && server && server->serverAddress().isLoopback() && server->serverPort() > 0 );
  QTextEdit* input = chat->findChild<QTextEdit*>( "GuiMessageEdit" );
  QPushButton* send = chat->findChild<QPushButton*>( "ShadowSendButton" );
  QLabel* feedback = chat->findChild<QLabel*>( "ShadowSendFeedback" );
  CHECK( input && send && feedback );
  const int originalCount = ChatManager::instance().chat( 88001 ).messages().size();
  const int receivedBefore = transport->property( "receivedMessages" ).toInt();
  const int queuedBefore = MessageManager::instance().countMessagesToSendInChatId( 88001 );
  chat->addText( QStringLiteral( "1234564 本机收发验证" ) );
  send->click();
  CHECK( input->toPlainText().isEmpty() );
  CHECK( feedback->text().contains( QStringLiteral( "本机测试对端已收到" ) ) );
  CHECK( transport->property( "receivedMessages" ).toInt() == receivedBefore + 1 );
  CHECK( ChatManager::instance().chat( 88001 ).messages().size() == originalCount + 2 );
  CHECK( ChatManager::instance().chat( 88001 ).messages().last().message().contains( QStringLiteral( "1234564" ) ) );
  CHECK( MessageManager::instance().countMessagesToSendInChatId( 88001 ) == queuedBefore );
  const User originalPeer = UserManager::instance().findUser( 87001 );
  User outsidePeer = originalPeer;
  outsidePeer.setHash( "outside-test-fixtures" );
  UserManager::instance().setUser( outsidePeer );
  QString error;
  CHECK( !ShadowDevPreview::sendTestText( 88001, "must not reach peer", false, &error ) );
  CHECK( transport->property( "receivedMessages" ).toInt() == receivedBefore + 1 );
  UserManager::instance().setUser( originalPeer );
  QImage screenshot( 1000, 520, QImage::Format_RGB32 );
  screenshot.fill( QColor( "#DCE8F8" ) );
  chat->addText( QStringLiteral( "图片发送后保留的草稿" ) );
  QImage submittedImage;
  bool screenshotSent = false;
  QTimer::singleShot( 100, chat, [&]() {
    ShadowScreenshotPreview* editor = qobject_cast<ShadowScreenshotPreview*>( QApplication::activeModalWidget() );
    if( !editor ) return;
    QWidget* canvas = editor->findChild<QWidget*>( "ShadowScreenshotCanvas" );
    QPushButton* rectangle = editor->findChild<QPushButton*>( "ShadowScreenshotrectangle" );
    if( !canvas || !rectangle ) { editor->reject(); return; }
    rectangle->click();
    const QPoint begin = canvas->rect().center() - QPoint( 90, 50 );
    const QPoint end = canvas->rect().center() + QPoint( 90, 50 );
    QMouseEvent press( QEvent::MouseButtonPress, begin, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
    QMouseEvent move( QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
    QMouseEvent release( QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
    QApplication::sendEvent( canvas, &press );
    QApplication::sendEvent( canvas, &move );
    QApplication::sendEvent( canvas, &release );
    submittedImage = editor->selectedImage().toImage();
    editor->findChild<QPushButton*>( "ShadowScreenshotSend" )->click();
    screenshotSent = editor->result() == QDialog::Accepted;
    if( !screenshotSent ) editor->reject();
  } );
  chat->previewScreenshot( QPixmap::fromImage( screenshot ) );
  CHECK( screenshotSent && submittedImage != screenshot );
  const QString imagePath = preview.property( "lastTestReceivedFile" ).toString();
  CHECK( QImage( imagePath ).convertToFormat( submittedImage.format() ) == submittedImage );
  CHECK( input->toPlainText() == QStringLiteral( "图片发送后保留的草稿" ) );
  CHECK( transport->property( "receivedMessages" ).toInt() == receivedBefore + 2 );
  QTemporaryDir files;
  CHECK( files.isValid() );
  const QByteArray binary = QByteArray::fromHex( "000102ff10203040" ).repeated( 10000 );
  const QString sourcePath = files.path() + "/test-file.bin";
  QFile source( sourcePath );
  CHECK( source.open( QIODevice::WriteOnly ) && source.write( binary ) == binary.size() );
  source.close();
  CHECK( QMetaObject::invokeMethod( &window, "sendFilesFromChat", Qt::DirectConnection,
    Q_ARG( VNumber, VNumber( 88001 ) ), Q_ARG( QStringList, QStringList() << sourcePath ) ) );
  QFile receivedFile( preview.property( "lastTestReceivedFile" ).toString() );
  CHECK( receivedFile.open( QIODevice::ReadOnly ) && receivedFile.readAll() == binary );
  CHECK( transport->property( "receivedMessages" ).toInt() == receivedBefore + 3 );
  CHECK( core.findChildren<Connection*>().isEmpty() && !core.startFileTransferServer() );
  window.grab().save( QDir::currentPath() + "/ui-037-local-send.png" );
  input->clear();
  server->close();
  chat->addText( QStringLiteral( "失败时保留此输入" ) );
  const int beforeFailure = ChatManager::instance().chat( 88001 ).messages().size();
  send->click();
  CHECK( input->toPlainText() == QStringLiteral( "失败时保留此输入" ) );
  CHECK( feedback->text().contains( QStringLiteral( "未就绪" ) ) );
  CHECK( ChatManager::instance().chat( 88001 ).messages().size() == beforeFailure );
  CHECK( transport->property( "receivedMessages" ).toInt() == receivedBefore + 3 );
  CHECK( verifyWindowCorners( window ) == 0 );
  std::puts( "PASS local preview sending: TCP peer receipt, text via send button, edited screenshot exact pixels, binary file exact bytes, no office peers or regular file server, invalid recipient rejected, failure keeps draft without fake message" );
  return 0;
}

static QList<QImage> notificationIcons( GuiMain& window )
{
  QList<QImage> icons;
  icons << window.windowIcon().pixmap( 32, 32 ).toImage();
  for( QSystemTrayIcon* tray : window.findChildren<QSystemTrayIcon*>() )
    icons << tray->icon().pixmap( 32, 32 ).toImage();
  for( QAction* action : window.findChildren<QAction*>() )
    icons << action->icon().pixmap( 32, 32 ).toImage();
  for( QTabWidget* tabs : window.findChildren<QTabWidget*>() )
    for( int index = 0; index < tabs->count(); ++index )
      icons << tabs->tabIcon( index ).pixmap( 32, 32 ).toImage();
  for( QTreeWidget* tree : window.findChildren<QTreeWidget*>() )
  {
    QTreeWidgetItemIterator item( tree );
    while( *item )
    {
      for( int column = 0; column < tree->columnCount(); ++column )
        icons << (*item)->icon( column ).pixmap( 32, 32 ).toImage();
      ++item;
    }
  }
  return icons;
}

static int verifyTranscript()
{
  ShadowChatTranscript transcript;
  transcript.setStyleSheet( "QTextBrowser { background:#ffffff; color:#243044; border:0; font-family:'Microsoft YaHei UI'; font-size:14px; }" );
  transcript.resize( 640, 260 );
  transcript.show();
  transcript.setHtml( QStringLiteral(
    "<table align=\"left\" width=\"200\" cellspacing=\"0\" cellpadding=\"0\"><tr>"
    "<td width=\"38\"></td><td><table bgcolor=\"#F0F2F4\" cellpadding=\"10\"><tr><td>"
    "你好，<a href=\"file:///synthetic.txt\">文件</a></td></tr></table></td><td width=\"44\">10:21</td></tr></table>"
    "<table align=\"right\" width=\"180\" cellspacing=\"0\" cellpadding=\"0\"><tr>"
    "<td width=\"44\">10:22</td><td><table bgcolor=\"#DDEBFF\" cellpadding=\"10\"><tr><td>"
    "收到</td></tr></table></td><td width=\"38\"></td></tr></table>" ) );
  settle();
  CHECK( transcript.bubbleCount() == 2 );
  const QVector<QRectF> bubbles = transcript.bubbleFrameRects();
  CHECK( bubbles.at( 0 ).width() < 250 && bubbles.at( 1 ).width() < 250 );
  CHECK( bubbles.at( 1 ).left() > bubbles.at( 0 ).right() );
  const QImage rendered = transcript.viewport()->grab().toImage();
  int incomingPixels = 0;
  int outgoingPixels = 0;
  for( int row = 0; row < rendered.height(); ++row )
    for( int column = 0; column < rendered.width(); ++column )
    {
      const QColor color = rendered.pixelColor( column, row );
      incomingPixels += color == QColor( "#F0F2F4" );
      outgoingPixels += color == QColor( "#DDEBFF" );
    }
  CHECK( incomingPixels > 100 && outgoingPixels > 100 );
  rendered.save( QDir::currentPath() + "/ui-chat-bubbles.png" );
  CHECK( transcript.find( QStringLiteral( "收到" ) ) );
  CHECK( transcript.textCursor().selectedText() == QStringLiteral( "收到" ) );
  QTextCursor link = transcript.document()->find( QStringLiteral( "文件" ) );
  CHECK( !link.isNull() );
  link.setPosition( link.selectionStart() );
  const QPoint linkPoint = transcript.cursorRect( link ).center() + QPoint( 4, 0 );
  CHECK( transcript.anchorAt( linkPoint ) == QStringLiteral( "file:///synthetic.txt" ) );
  transcript.resize( 420, 260 );
  settle();
  CHECK( transcript.bubbleCount() == 2 && transcript.horizontalScrollBar()->maximum() == 0 );
  CHECK( transcript.textCursor().selectedText() == QStringLiteral( "收到" ) );
  return 0;
}

static QList<QTreeWidgetItem*> directoryRows( QTreeWidget* list, int kind )
{
  QList<QTreeWidgetItem*> rows;
  QTreeWidgetItemIterator item( list );
  while( *item )
  {
    if( (*item)->data( 0, Qt::UserRole + 1 ).toInt() == kind )
      rows.append( *item );
    ++item;
  }
  return rows;
}

static GuiChatItem* conversationRow( QTreeWidget* list, VNumber id )
{
  for( int index = 0; index < list->topLevelItemCount(); ++index )
  {
    GuiChatItem* item = dynamic_cast<GuiChatItem*>( list->topLevelItem( index ) );
    if( item && item->chatId() == id ) return item;
  }
  return Q_NULLPTR;
}

static bool chooseConversationAction( GuiChatList* sidebar, QTreeWidget* list, GuiChatItem* row, const QString& text )
{
  list->scrollToItem( row );
  bool triggered = false;
  QTimer::singleShot( 80, sidebar, [&]() {
    QMenu* menu = qobject_cast<QMenu*>( QApplication::activePopupWidget() );
    if( !menu ) return;
    for( QAction* action : menu->actions() )
      if( action->text() == text )
      {
        action->trigger();
        triggered = true;
        break;
      }
    menu->close();
  } );
  QMetaObject::invokeMethod( sidebar, "showChatMenu", Qt::DirectConnection,
    Q_ARG( QPoint, list->visualItemRect( row ).center() ) );
  return triggered;
}

static int verifyConversationSidebar( GuiMain& window, Core& core )
{
  CHECK( QMetaObject::invokeMethod( &window, "showModernMessages", Qt::DirectConnection ) );
  GuiChatList* sidebar = window.findChild<GuiChatList*>();
  CHECK( sidebar && sidebar->isVisible() );
  QTreeWidget* list = sidebar->findChild<QTreeWidget*>( "ShadowConversationList" );
  QTabBar* filters = sidebar->findChild<QTabBar*>( "ShadowConversationFilters" );
  CHECK( list && filters && filters->count() == 4 );
  CHECK( filters->tabText( 0 ) == QStringLiteral( "最近" ) && filters->tabText( 3 ) == QStringLiteral( "待处理" ) );
  CHECK( list->topLevelItemCount() == 11 && !conversationRow( list, ID_DEFAULT_CHAT ) );
  const Chat original = ChatManager::instance().chat( 88006 );
  CHECK( original.isValid() );
  const ShadowConversationPreferences preferences;
  QMap<VNumber, bool> originalPins;
  for( const Chat& chat : ChatManager::instance().constChatList() )
    if( !chat.isDefault() )
    {
      originalPins.insert( chat.id(), preferences.pinned( chat ) );
      CHECK( preferences.setPinned( chat, false ) );
    }
  sidebar->updateChats();
  settle();
  QToolButton* expand = sidebar->findChild<QToolButton*>( "ShadowQuickExpand" );
  CHECK( sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).isEmpty() );
  CHECK( !sidebar->findChild<QToolButton*>( "ShadowQuickTransfers" ) );
  CHECK( !sidebar->findChild<QWidget*>( "ShadowQuickArea" )->isVisible() );
  CHECK( expand && !expand->isVisible() );
  const bool wasPinned = originalPins.value( original.id() );
  const bool wasMuted = preferences.muted( original );
  const bool wasPending = preferences.pending( original );
  const bool wasHidden = preferences.hidden( original );
  CHECK( preferences.setHidden( original, false ) );
  Chat changed = original;
  changed.readAllMessages();
  Message mention( Message::Chat, 919101, QStringLiteral( "@%1 这条会话需要确认。" ).arg( Settings::instance().localUser().name() ) );
  changed.addMessage( ChatMessage( changed.privateUserId(), mention, ChatMessage::Chat, false ) );
  changed.addUnreadMessage();
  ChatManager::instance().setChat( changed );
  core.chatChanged( changed );
  settle();
  GuiChatItem* row = conversationRow( list, changed.id() );
  CHECK( row && row->data( 0, GuiChatItem::ChatMentioned ).toBool() );
  CHECK( row->data( 0, GuiChatItem::ChatPreview ).toString().contains( QStringLiteral( "需要确认" ) ) );
  CHECK( row->lastMessageTimestamp().isValid() && row->lastMessageTimestamp() >= mention.timestamp() );
  CHECK( list->visualItemRect( row ).height() >= 80 );
  filters->setCurrentIndex( 1 );
  CHECK( !row->isHidden() );
  for( int index = 0; index < list->topLevelItemCount(); ++index )
  {
    GuiChatItem* item = static_cast<GuiChatItem*>( list->topLevelItem( index ) );
    if( !item->isHidden() ) CHECK( item->unreadMessages() > 0 );
  }
  filters->setCurrentIndex( 2 );
  CHECK( !row->isHidden() );
  window.grab().save( QDir::currentPath() + "/ui-sidebar-mentions.png" );
  filters->setCurrentIndex( 3 );
  CHECK( row->isHidden() );
  CHECK( preferences.setPending( changed, true ) );
  sidebar->updateChat( changed );
  CHECK( !row->isHidden() && row->data( 0, GuiChatItem::ChatPending ).toBool() );
  CHECK( preferences.setPending( changed, false ) );
  sidebar->updateChat( changed );
  CHECK( row->isHidden() );
  filters->setCurrentIndex( 0 );
  CHECK( chooseConversationAction( sidebar, list, row, QStringLiteral( "置顶会话" ) ) );
  CHECK( preferences.pinned( changed ) && preferences.setMuted( changed, true ) );
  sidebar->updateChat( changed );
  filters->setCurrentIndex( 0 );
  settle();
  CHECK( static_cast<GuiChatItem*>( list->topLevelItem( 0 ) )->chatId() == changed.id() );
  CHECK( row->data( 0, GuiChatItem::ChatMuted ).toBool() );
  QList<QToolButton*> shortcuts = sidebar->findChildren<QToolButton*>( "ShadowQuickChat" );
  CHECK( shortcuts.size() == 1 && shortcuts.first()->property( "chatId" ).toULongLong() == changed.id() );
  CHECK( sidebar->findChild<QWidget*>( "ShadowQuickArea" )->isVisible() );
  CHECK( shortcuts.first()->mapTo( sidebar, QPoint() ).x() < 20 );
  CHECK( !expand->isVisible() );
  filters->setCurrentIndex( 3 );
  settle();
  CHECK( sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).size() == 1 );
  filters->setCurrentIndex( 0 );
  QToolButton* pinned = sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).first();
  CHECK( QMetaObject::invokeMethod( pinned, "customContextMenuRequested", Qt::DirectConnection,
    Q_ARG( QPoint, QPoint( 4, 4 ) ) ) );
  QMenu* pinMenu = qobject_cast<QMenu*>( QApplication::activePopupWidget() );
  CHECK( pinMenu && pinMenu->actions().size() == 1 && pinMenu->actions().first()->text() == QStringLiteral( "取消置顶" ) );
  pinMenu->actions().first()->trigger();
  pinMenu->close();
  settle();
  CHECK( !preferences.pinned( changed ) && sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).isEmpty() );
  CHECK( !sidebar->findChild<QWidget*>( "ShadowQuickArea" )->isVisible() );
  for( const Chat& chat : ChatManager::instance().constChatList() )
    if( !chat.isDefault() ) CHECK( preferences.setPinned( chat, true ) );
  sidebar->updateChats();
  settle();
  CHECK( sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).size() == 10 );
  CHECK( expand->isVisible() && expand->accessibleName().contains( QStringLiteral( "置顶会话" ) ) );
  window.grab().save( QDir::currentPath() + "/ui-035-pinned-collapsed.png" );
  expand->click();
  settle();
  CHECK( sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).size() == 11 );
  for( QToolButton* button : sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ) )
    CHECK( preferences.pinned( ChatManager::instance().chat( button->property( "chatId" ).toULongLong() ) ) );
  const Chat temporarilyHidden = ChatManager::instance().chat( 88001 );
  const bool originalHidden = preferences.hidden( temporarilyHidden );
  CHECK( preferences.setHidden( temporarilyHidden, true ) );
  sidebar->updateChat( temporarilyHidden );
  settle();
  CHECK( sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).size() == 10 );
  CHECK( !expand->isVisible() );
  CHECK( preferences.setHidden( temporarilyHidden, originalHidden ) );
  sidebar->updateChat( temporarilyHidden );
  settle();
  expand->click();
  settle();
  QList<Chat> extraPinnedChats;
  QList<User> extraPinnedUsers;
  for( int index = 0; index < 18; ++index )
  {
    User user( 98001 + index );
    user.setName( QStringLiteral( "置顶测试%1（合成）" ).arg( index + 1 ) );
    user.setHash( QStringLiteral( "pinned-overflow-fixture-%1" ).arg( index ) );
    UserManager::instance().setUser( user );
    Group group;
    group.setId( 99001 + index );
    group.setName( user.name() );
    group.setChatType( Group::PrivateChat );
    group.setPrivateId( QStringLiteral( "pinned-overflow-chat-%1" ).arg( index ) );
    group.addUser( ID_LOCAL_USER );
    group.addUser( user.id() );
    Chat chat;
    chat.setGroup( group );
    ChatManager::instance().setChat( chat );
    CHECK( preferences.setPinned( chat, true ) );
    sidebar->updateChat( chat );
    extraPinnedChats.append( chat );
    extraPinnedUsers.append( user );
  }
  settle();
  CHECK( sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).size() == 29 );
  QScrollArea* pinnedScroll = sidebar->findChild<QScrollArea*>( "ShadowPinnedScroll" );
  CHECK( pinnedScroll && pinnedScroll->verticalScrollBar()->maximum() > 0 &&
    pinnedScroll->horizontalScrollBar()->maximum() == 0 );
  QToolButton* lastPinned = sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ).last();
  pinnedScroll->ensureWidgetVisible( lastPinned );
  settle();
  CHECK( pinnedScroll->viewport()->rect().contains( lastPinned->mapTo( pinnedScroll->viewport(), lastPinned->rect().center() ) ) );
  window.grab().save( QDir::currentPath() + "/ui-035-pinned-overflow.png" );
  for( const Chat& chat : extraPinnedChats )
  {
    ChatManager::instance().removeChat( chat );
    core.chatRemoved( chat );
  }
  for( const User& user : extraPinnedUsers )
  {
    UserManager::instance().removeUser( user );
    core.userRemoved( user );
  }
  sidebar->updateChats();
  settle();
  row = conversationRow( list, changed.id() );
  CHECK( row );
  expand->click();
  settle();
  QToolButton* shortcut = Q_NULLPTR;
  for( QToolButton* button : sidebar->findChildren<QToolButton*>( "ShadowQuickChat" ) )
    if( button->property( "chatId" ).toULongLong() == changed.id() ) shortcut = button;
  CHECK( shortcut );
  shortcut->click();
  settle();
  CHECK( row->data( 0, GuiChatItem::ChatActive ).toBool() );
  CHECK( window.findChild<QLabel*>( "ShadowChatTitle" )->text().contains( changed.name() ) );
  sidebar->updateChats();
  row = conversationRow( list, changed.id() );
  CHECK( row && row->data( 0, GuiChatItem::ChatActive ).toBool() );
  bool transfersOpened = false;
  QTimer::singleShot( 80, &window, [&transfersOpened]() {
    QDialog* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( dialog ) { transfersOpened = dialog->objectName() == "ShadowTransferDialog"; dialog->reject(); }
  } );
  CHECK( QMetaObject::invokeMethod( &window, "showModernTransfers", Qt::DirectConnection ) );
  CHECK( transfersOpened );
  for( auto pin = originalPins.constBegin(); pin != originalPins.constEnd(); ++pin )
    CHECK( preferences.setPinned( ChatManager::instance().chat( pin.key() ), pin.value() ) );
  CHECK( preferences.setPinned( original, wasPinned ) && preferences.setMuted( original, wasMuted ) &&
         preferences.setPending( original, wasPending ) && preferences.setHidden( original, wasHidden ) );
  ChatManager::instance().setChat( original );
  core.chatChanged( original );
  sidebar->updateChats();
  filters->setCurrentIndex( 0 );
  settle();
  window.grab().save( QDir::currentPath() + "/ui-sidebar-recent.png" );
  std::puts( "PASS conversation sidebar: summaries/time, unread/@me/pending filters, actual pin/unpin menus, pinned-only grid without file shortcut or empty space, single/10-pin collapsed/11-pin expanded/29-pin scroll, hidden pins, local transfer dialog and persistent selected chat" );
  return 0;
}

static int verifyReply( GuiMain& window, ShadowDevPreview& preview )
{
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  window.findChild<ShadowSearchPanel*>("ShadowSearchPanel")->dismiss();
  settle();
  GuiChat* chat=Q_NULLPTR;
  for(GuiChat* candidate:window.findChildren<GuiChat*>())if(candidate->isVisible())chat=candidate;
  CHECK(chat);
  QTextBrowser* transcript=chat->findChild<QTextBrowser*>("GuiChatViewer");
  QTextEdit* input=chat->findChild<QTextEdit*>("GuiMessageEdit");
  QPushButton* send=chat->findChild<QPushButton*>("ShadowSendButton");
  QWidget* replyBar=chat->findChild<QWidget*>("ShadowReplyPreview");
  CHECK(transcript&&input&&send&&replyBar);
  QTextCursor source=transcript->document()->find(QStringLiteral("这是一段"));
  CHECK(!source.isNull());
  transcript->setTextCursor(source);
  transcript->ensureCursorVisible();
  const QPoint point=transcript->cursorRect(source).center();
  bool replySelected=false;
  QTimer::singleShot(60,chat,[&]() {
    QMenu* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if(!menu)return;
    for(QAction* action:menu->actions())if(action->objectName()=="ShadowReplyAction")
    {
      action->trigger();
      replySelected=true;
      break;
    }
    menu->close();
  });
  CHECK(QMetaObject::invokeMethod(chat,"customContextMenu",Qt::DirectConnection,Q_ARG(QPoint,point)));
  CHECK(replySelected&&replyBar->isVisible());
  CHECK(chat->findChild<QLabel*>("ShadowReplyText")->text().contains(QStringLiteral("林舟")));
  CHECK(chat->findChild<QLabel*>("ShadowReplyText")->text().contains(QStringLiteral("这是一段")));
  input->setPlainText(QStringLiteral("会话 A 的独立正文"));
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88002)));
  CHECK(!replyBar->isVisible()&&input->toPlainText().isEmpty());
  input->setPlainText(QStringLiteral("会话 B 的独立正文"));
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  CHECK(replyBar->isVisible()&&input->toPlainText()==QStringLiteral("会话 A 的独立正文"));
  input->setPlainText(QStringLiteral("引用回复发送验收"));
  QObject* transport=preview.findChild<QObject*>("ShadowPreviewTransport");
  CHECK(transport);
  const int before=transport->property("receivedMessages").toInt();
  const User peer=UserManager::instance().findUser(87001);
  User outside=peer;
  outside.setHash("outside-reply-test-fixtures");
  UserManager::instance().setUser(outside);
  send->click();
  CHECK(input->toPlainText()==QStringLiteral("引用回复发送验收")&&replyBar->isVisible());
  CHECK(transport->property("receivedMessages").toInt()==before);
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88002)));
  CHECK(input->toPlainText()==QStringLiteral("会话 B 的独立正文"));
  CHECK(!chat->findChild<QLabel*>("ShadowSendFeedback")->isVisible());
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  CHECK(input->toPlainText()==QStringLiteral("引用回复发送验收")&&replyBar->isVisible());
  CHECK(chat->findChild<QLabel*>("ShadowSendFeedback")->property("failed").toBool());
  UserManager::instance().setUser(peer);
  send->click();
  settle();
  CHECK(input->toPlainText().isEmpty()&&!replyBar->isVisible());
  CHECK(transport->property("receivedMessages").toInt()==before+1);
  bool storedQuote=false;
  const Chat sentChat=ChatManager::instance().chat(88001);
  for(const ChatMessage& message:sentChat.messages())
    if(message.isFromLocalUser()&&message.message().contains(QStringLiteral("引用回复发送验收")))
      storedQuote=message.message().contains("bee-quote")&&message.message().contains(QStringLiteral("林舟"));
  CHECK(storedQuote&&transcript->toPlainText().contains(QStringLiteral("引用回复发送验收")));
  CHECK(window.grab().save(QDir::currentPath()+"/ui-049-reply-sent.png"));
  source=transcript->document()->find(QStringLiteral("这是一段"));
  transcript->setTextCursor(source);
  transcript->ensureCursorVisible();
  CHECK(QMetaObject::invokeMethod(chat,"replyToMessage",Qt::DirectConnection,Q_ARG(QPoint,transcript->cursorRect(source).center())));
  CHECK(replyBar->isVisible());
  chat->findChild<QToolButton*>("ShadowReplyCancel")->click();
  CHECK(!replyBar->isVisible());
  QTextCursor echo=transcript->document()->find(QStringLiteral("本机测试对端已收到"));
  CHECK(!echo.isNull());
  QTextTable* outer=Q_NULLPTR;
  for(QTextFrame* frame=echo.currentFrame();frame;frame=frame->parentFrame())
    if(QTextTable* table=qobject_cast<QTextTable*>(frame))outer=table;
  CHECK(outer&&outer->columns()==3);
  QTextCursor heading=outer->cellAt(0,1).firstCursorPosition();
  transcript->setTextCursor(heading);
  transcript->ensureCursorVisible();
  CHECK(QMetaObject::invokeMethod(chat,"replyToMessage",Qt::DirectConnection,Q_ARG(QPoint,transcript->cursorRect(heading).center())));
  CHECK(chat->findChild<QLabel*>("ShadowReplyText")->toolTip().contains(QStringLiteral("本机测试对端已收到")));
  chat->findChild<QToolButton*>("ShadowReplyCancel")->click();
  QTemporaryDir imageFiles;
  CHECK(imageFiles.isValid());
  const QString imagePath=imageFiles.filePath("reply-source.png");
  QImage image(100,80,QImage::Format_RGB32);
  image.fill(QColor("#8AADE0"));
  CHECK(image.save(imagePath));
  const QString sender=QStringLiteral("图片发送者（合成）");
  const QString imageUrl=QUrl::fromLocalFile(imagePath).toString();
  const QString imageHtml=QString("<!--shadow-sender:%1--><a href=\"%2\"><img src=\"%2\"></a>")
    .arg(QString::fromLatin1(sender.toUtf8().toBase64()),imageUrl);
  Chat imageChat=ChatManager::instance().chat(88001);
  imageChat.addMessage(ChatMessage(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(imageHtml),ChatMessage::ImagePreview,true));
  ChatManager::instance().setChat(imageChat);
  CHECK(chat->setChat(imageChat));
  settle();
  QTextCursor imageCursor(transcript->document());
  bool imageFound=false;
  for(QTextBlock block=transcript->document()->begin();block.isValid()&&!imageFound;block=block.next())
    for(QTextBlock::iterator fragment=block.begin();!fragment.atEnd();++fragment)
      if(fragment.fragment().charFormat().isImageFormat()&&fragment.fragment().charFormat().toImageFormat().name()==imageUrl)
      {
        imageCursor.setPosition(fragment.fragment().position());
        imageFound=true;
        break;
      }
  CHECK(imageFound);
  transcript->setTextCursor(imageCursor);
  transcript->ensureCursorVisible();
  CHECK(QMetaObject::invokeMethod(chat,"replyToMessage",Qt::DirectConnection,Q_ARG(QPoint,transcript->cursorRect(imageCursor).center())));
  CHECK(chat->findChild<QLabel*>("ShadowReplyText")->text().contains(sender));
  CHECK(chat->findChild<QLabel*>("ShadowReplyText")->toolTip().contains("[图片]"));
  chat->findChild<QToolButton*>("ShadowReplyCancel")->click();
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88002)));
  CHECK(input->toPlainText()==QStringLiteral("会话 B 的独立正文"));
  input->clear();
  std::puts("PASS replies: exact bubble/header source, system image sender, isolated body/quote/failure drafts per chat, failed send retains text/quote, retry reaches local TCP peer, quote stored/rendered inside bubble, cancel");
  return 0;
}

static int verifyMessageBoxes( GuiMain& window )
{
  bool verified=false;
  QTimer::singleShot(80,&window,[&]() {
    QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if(!dialog)return;
    QWidget* surface=dialog->findChild<QWidget*>("shadowDialogSurface");
    QLabel* text=dialog->findChild<QLabel*>("ShadowConfirmationText");
    QPushButton* cancel=dialog->findChild<QPushButton*>("ShadowConfirmationCancel");
    QPushButton* accept=dialog->findChild<QPushButton*>("ShadowConfirmationAccept");
    verified=surface&&text&&text->isVisible()&&cancel&&accept&&cancel->isDefault()&&
      surface->rect().contains(cancel->mapTo(surface,cancel->rect().bottomRight()))&&
      surface->rect().contains(accept->mapTo(surface,accept->rect().bottomRight()))&&
      dialog->grab().save(QDir::currentPath()+"/ui-053-confirmation.png");
    if(cancel)cancel->click();else dialog->reject();
  });
  CHECK(!ShadowDialogStyle::confirm(&window,QStringLiteral("退出账号"),
    QStringLiteral("退出将中断当前收发，未完成文件下次需重新传输。\n退出后将显示登录页，是否继续？"),
    QStringLiteral("退出账号"))&&verified);
  QTimer::singleShot(60,&window,[]() {
    if(QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))
      if(QPushButton* accept=dialog->findChild<QPushButton*>("ShadowConfirmationAccept"))accept->click();
  });
  CHECK(ShadowDialogStyle::confirm(&window,QStringLiteral("确认操作"),QStringLiteral("合成资料确认测试"),QStringLiteral("继续")));
  std::puts("PASS confirmations: unified opaque surface, full text/buttons within bounds, Chinese cancel default, cancel rejects and explicit confirmation accepts");
  return 0;
}

static int verifyWelcome( GuiMain& window )
{
  Settings& settings=Settings::instance();
  const User original=settings.localUser();
  const bool updatePreference=settings.checkNewVersionAtStartup();
  QTemporaryDir files;
  CHECK(files.isValid());
  const QString imagePath=files.filePath("welcome-avatar.png");
  QImage image(160,96,QImage::Format_RGB32);
  image.fill(QColor("#73A0D8"));
  CHECK(image.save(imagePath));
  GuiWizard dialog(&window);
  dialog.loadSettings();
  dialog.show();
  settle();
  QToolButton* avatar=dialog.findChild<QToolButton*>("ShadowOnboardingAvatar");
  QLineEdit* name=dialog.findChild<QLineEdit*>("ShadowOnboardingName");
  QPushButton* enter=dialog.findChild<QPushButton*>("ShadowPrimaryButton");
  CHECK(avatar&&avatar->isVisible()&&name&&enter&&enter->isVisible());
  CHECK(!avatar->icon().isNull()&&avatar->focusPolicy()!=Qt::NoFocus);
  CHECK(!dialog.findChild<QFrame*>("ShadowOnboardingTrust"));
  CHECK(dialog.findChildren<QCheckBox*>().isEmpty());
  for(QPushButton* button:dialog.findChildren<QPushButton*>())
    CHECK(button->text()!=QStringLiteral("选择头像"));
  CHECK(dialog.width()<=620&&dialog.height()<480);
  CHECK(dialog.rect().contains(enter->mapTo(&dialog,enter->rect().bottomRight())));
  CHECK(dialog.grab().save(QDir::currentPath()+"/ui-045-welcome.png"));
  const bool nativeDialogsDisabled=QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
  bool fileSelected=false;
  QTimer pickerDriver;
  QObject::connect(&pickerDriver,&QTimer::timeout,&dialog,[&]() {
    QFileDialog* picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if(!picker)return;
    pickerDriver.stop();
    picker->selectFile(imagePath);
    fileSelected=QMetaObject::invokeMethod(picker,"accept",Qt::DirectConnection);
  });
  pickerDriver.start(30);
  avatar->click();
  pickerDriver.stop();
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,nativeDialogsDisabled);
  CHECK(fileSelected&&!dialog.avatar().isNull());
  CHECK(dialog.avatar().toImage()==QPixmap(imagePath).toImage());
  CHECK(dialog.grab().save(QDir::currentPath()+"/ui-045-welcome-avatar-selected.png"));
  dialog.reject();
  CHECK(settings.localUser().name()==original.name());
  CHECK(settings.localUser().vCard().photo().cacheKey()==original.vCard().photo().cacheKey());
  CHECK(settings.checkNewVersionAtStartup()==updatePreference);
  GuiWizard accepted(&window);
  accepted.loadSettings();
  accepted.findChild<QLineEdit*>("ShadowOnboardingName")->setText(QStringLiteral("  欢迎页验收  "));
  accepted.findChild<QPushButton*>("ShadowPrimaryButton")->click();
  CHECK(accepted.result()==QDialog::Accepted&&accepted.userName()==QStringLiteral("欢迎页验收"));
  CHECK(settings.checkNewVersionAtStartup()==updatePreference);
  std::puts("PASS welcome: compact identity-only page, clickable keyboard-focusable avatar opens actual picker and loads selected image, cancel preserves profile, trimmed nickname accepted, update preference unchanged");
  return 0;
}

static int verifyEmbeddedHistory( GuiMain& window )
{
  window.resize(1120,820);
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  settle();
  const Chat chat=ChatManager::instance().chat(88001);
  CHECK(chat.isValid());
  QToolButton* historyButton=Q_NULLPTR;
  for(QToolButton* button:window.findChildren<QToolButton*>())
    if(button->isVisible()&&button->accessibleName()==QStringLiteral("聊天记录"))historyButton=button;
  CHECK(historyButton);
  historyButton->click();
  settle();
  GuiSavedChat* history=window.findChild<GuiSavedChat*>();
  CHECK(history&&history->isVisible()&&!history->isWindow()&&history->window()==&window);
  CHECK(!window.findChild<ShadowSearchPanel*>("ShadowSearchPanel")->isVisible());
  CHECK(window.size()==QSize(1120,820));
  CHECK(history->width()>=320&&history->width()<=420);
  QWidget* conversation=window.findChild<QWidget*>("ShadowConversationStack");
  CHECK(conversation->width()>=320);
  CHECK(history->mapTo(&window,QPoint()).x()>=conversation->mapTo(&window,QPoint(conversation->width(),0)).x());
  QListWidget* results=history->findChild<QListWidget*>("ShadowHistoryResults");
  QLineEdit* keyword=history->findChild<QLineEdit*>("GuiLineEditFilterHistory");
  QTabBar* categories=history->findChild<QTabBar*>();
  CHECK(results&&results->count()>0&&keyword&&categories&&categories->count()==4);
  const QString original=ChatManager::instance().chatSavedText(chat.name());
  const bool hadArchive=ChatManager::instance().chatHasSavedText(chat.name());
  QTemporaryDir files;
  CHECK(files.isValid());
  const QString imagePath=files.filePath("UI044-image.png");
  QImage image(100,100,QImage::Format_RGB32);
  image.fill(QColor("#8CAFE6"));
  CHECK(image.save(imagePath));
  const QString filePath=files.filePath("UI044-material.txt");
  CHECK(atomicWrite(filePath,"synthetic history attachment"));
  const auto record=[](const QString& sender,const QString& body,int daysAgo) {
    QJsonObject metadata;
    metadata.insert("sender",sender);
    metadata.insert("time",QDateTime::currentDateTime().addDays(-daysAgo).toString(Qt::ISODate));
    return QString("<!--shadow-record:%1-->%2<br>").arg(
      QString::fromLatin1(QJsonDocument(metadata).toJson(QJsonDocument::Compact).toBase64()),body);
  };
  QString archive=record("Alice","UI044 early message",40);
  archive+=record("Bob","UI044 recent message",0);
  archive+=record("Alice",QString("<a href=\"%1\">UI044-material.txt</a>").arg(QUrl::fromLocalFile(filePath).toString()),0);
  archive+=record("Bob",QString("UI044 image <a href=\"%1\"><img src=\"%1\"></a>").arg(QUrl::fromLocalFile(imagePath).toString()),0);
  archive+=record("Alice","<a href=\"https://example.test/UI044\">UI044-link</a>",0);
  QMap<QString,QString> saved;
  saved.insert(chat.name(),original+archive);
  ChatManager::instance().addSavedChats(saved);
  history->showSavedChat(chat.name());
  keyword->setText("UI044");
  CHECK(results->count()==5);
  QComboBox* sender=Q_NULLPTR;
  QComboBox* date=Q_NULLPTR;
  QComboBox* sort=Q_NULLPTR;
  for(QComboBox* filter:history->findChildren<QComboBox*>())
  {
    if(filter->accessibleName()==QStringLiteral("发送者"))sender=filter;
    if(filter->accessibleName()==QStringLiteral("时间范围"))date=filter;
    if(filter->accessibleName()==QStringLiteral("排序"))sort=filter;
  }
  CHECK(sender&&date&&sort);
  sort->setCurrentIndex(1);
  CHECK(results->item(0)->text().contains("UI044 early message"));
  sort->setCurrentIndex(0);
  CHECK(!results->item(0)->text().contains("UI044 early message"));
  sender->setCurrentIndex(sender->findData("Alice"));
  CHECK(results->count()==3);
  sender->setCurrentIndex(0);
  date->setCurrentIndex(1);
  CHECK(results->count()==4);
  date->setCurrentIndex(0);
  categories->setCurrentIndex(1);
  CHECK(results->count()==1&&results->item(0)->text().contains("UI044-material.txt"));
  categories->setCurrentIndex(2);
  CHECK(results->count()==1&&!results->item(0)->icon().isNull());
  CHECK(results->viewMode()==QListView::IconMode);
  categories->setCurrentIndex(3);
  CHECK(results->count()==1&&results->item(0)->text().contains("example.test"));
  categories->setCurrentIndex(0);
  keyword->setText("UI044-no-match");
  CHECK(results->count()==0&&history->findChild<QLabel*>("ShadowHistoryEmpty")->isVisible());
  keyword->setText("UI044");
  results->setCurrentRow(0);
  QPushButton* context=history->findChild<QPushButton*>("ShadowHistoryContext");
  CHECK(context&&context->isEnabled());
  context->click();
  CHECK(history->findChild<QStackedWidget*>("ShadowHistoryPages")->currentIndex()==1);
  CHECK(history->findChild<QTextBrowser*>()->isVisible());
  history->findChild<QPushButton*>("ShadowHistoryBack")->click();
  CHECK(history->findChild<QStackedWidget*>("ShadowHistoryPages")->currentIndex()==0);
  keyword->clear();
  settle();
  CHECK(window.grab().save(QDir::currentPath()+"/ui-044-history-sidebar.png"));
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88002)));
  settle();
  CHECK(history->savedChatName()==ChatManager::instance().chat(88002).name());
  CHECK(keyword->text().isEmpty());
  keyword->setText("UI044");
  CHECK(results->count()==0);
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  settle();
  window.resize(900,740);
  settle();
  CHECK(window.width()==900&&history->isVisible()&&conversation->isVisible());
  CHECK(!window.findChild<QWidget*>("ShadowListPanel")->isVisible());
  CHECK(conversation->width()>=320&&history->width()>=320);
  CHECK(window.grab().save(QDir::currentPath()+"/ui-044-history-narrow.png"));
  history->findChild<QToolButton*>("ShadowHistoryClose")->click();
  settle();
  CHECK(!history->isVisible()&&window.findChild<QWidget*>("ShadowListPanel")->isVisible());
  window.resize(1120,820);
  settle();
  const int fullWidth=conversation->width();
  QToolButton* headerSearch=window.findChild<QToolButton*>("ShadowHeaderAction");
  CHECK(headerSearch);
  headerSearch->click();
  settle();
  CHECK(history->isVisible()&&window.findChildren<GuiSavedChat*>().size()==1&&!history->isWindow());
  CHECK(conversation->width()<fullWidth);
  history->close();
  settle();
  CHECK(conversation->width()==fullWidth);
  if(hadArchive)
  {
    saved[chat.name()]=original;
    ChatManager::instance().addSavedChats(saved);
  }
  else ChatManager::instance().removeSavedTextFromChat(chat.name());
  std::puts("PASS embedded history: composer/header share one child panel, active and archived records, keyword/sender/date/categories, thumbnails, inline source, conversation scoping, 1120/900 layouts, close restores width");
  return 0;
}

static int verifySearch( GuiMain& window )
{
  QLineEdit* launch = window.findChild<QLineEdit*>( "ShadowGlobalSearch" );
  ShadowSearchPanel* panel = window.findChild<ShadowSearchPanel*>( "ShadowSearchPanel" );
  CHECK( launch && panel );
  CHECK( QMetaObject::invokeMethod( &window, "showModernMessages", Qt::DirectConnection ) );
  launch->setFocus();
  settle();
  CHECK( panel->isVisible() && window.isVisible() );
  QLineEdit* input = panel->findChild<QLineEdit*>( "ShadowSearchInput" );
  QTabBar* tabs = panel->findChild<QTabBar*>( "ShadowSearchTabs" );
  QTreeWidget* results = panel->findChild<QTreeWidget*>( "ShadowGlobalSearchResults" );
  CHECK( input && tabs && tabs->count() == 4 && results );
  input->setText( QStringLiteral( "林" ) );
  settle();
  CHECK( launch->text() == QStringLiteral( "林" ) );
  CHECK( directoryRows( results, 1 ).size() >= 1 );
  CHECK( directoryRows( results, 2 ).size() == 1 );
  window.grab().save( QDir::currentPath() + "/ui-global-search-all.png" );
  input->setText( QStringLiteral( "合成" ) );
  settle();
  CHECK( directoryRows( results, 1 ).size() == 5 );
  QPushButton* more = Q_NULLPTR;
  for( QPushButton* button : panel->findChildren<QPushButton*>() )
    if( button->property( "searchMore" ).toBool() ) { more = button; break; }
  CHECK( more );
  more->click();
  CHECK( tabs->currentIndex() == 1 && directoryRows( results, 1 ).size() == 11 );
  QKeyEvent nextCategory( QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier );
  QApplication::sendEvent( input, &nextCategory );
  CHECK( tabs->currentIndex() == 2 && directoryRows( results, 2 ).size() == 1 );
  QKeyEvent previousCategory( QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier );
  QApplication::sendEvent( input, &previousCategory );
  CHECK( tabs->currentIndex() == 1 );
  input->setText( QStringLiteral( "林舟" ) );
  settle();
  CHECK( directoryRows( results, 1 ).size() == 1 );
  QKeyEvent enterContact( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( input, &enterContact );
  settle();
  CHECK( !panel->isVisible() && launch->text().isEmpty() );
  CHECK( window.findChild<QLabel*>( "ShadowChatTitle" )->text().contains( QStringLiteral( "林舟" ) ) );
  panel->open( QStringLiteral( "设计" ) );
  tabs->setCurrentIndex( 2 );
  settle();
  CHECK( directoryRows( results, 2 ).size() == 1 );
  QKeyEvent enterGroup( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( input, &enterGroup );
  settle();
  CHECK( !panel->isVisible() );
  CHECK( window.findChild<QLabel*>( "ShadowChatTitle" )->text().contains( QStringLiteral( "设计讨论组" ) ) );
  panel->open( QStringLiteral( "这是一段" ) );
  tabs->setCurrentIndex( 3 );
  settle();
  CHECK( directoryRows( results, 3 ).size() >= 1 );
  window.grab().save( QDir::currentPath() + "/ui-global-search-messages.png" );
  QKeyEvent enterMessage( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( input, &enterMessage );
  settle();
  bool selectedMessage = false;
  for( GuiChat* chat : window.findChildren<GuiChat*>() )
    if( chat->isVisible() )
      selectedMessage = chat->findChild<QTextBrowser*>( "GuiChatViewer" )->textCursor().selectedText().contains( QStringLiteral( "这是一段" ) );
  CHECK( selectedMessage );
  QMap<QString, QString> archived;
  archived.insert( QStringLiteral( "搜索归档验收（合成）" ), QStringLiteral( "<p>仅在归档中的搜索验收词</p>" ) );
  ChatManager::instance().addSavedChats( archived );
  panel->open( QStringLiteral( "搜索验收词" ) );
  tabs->setCurrentIndex( 3 );
  settle();
  CHECK( directoryRows( results, 3 ).size() == 1 );
  QKeyEvent enterArchive( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( input, &enterArchive );
  settle();
  bool archiveOpened = false;
  for( GuiSavedChat* history : window.findChildren<GuiSavedChat*>() )
    if( history->savedChatName() == QStringLiteral( "搜索归档验收（合成）" ) )
    {
      archiveOpened = history->isVisible()&&!history->isWindow()&&history->window()==&window;
      bool keywordApplied = false;
      for( QLineEdit* filter : history->findChildren<QLineEdit*>() )
        keywordApplied = keywordApplied || filter->text() == QStringLiteral( "搜索验收词" );
      CHECK( keywordApplied );
      history->close();
    }
  CHECK( archiveOpened );
  ChatManager::instance().removeSavedTextFromChat( QStringLiteral( "搜索归档验收（合成）" ) );
  panel->open( QStringLiteral( "没有匹配的搜索验收词" ) );
  settle();
  CHECK( panel->findChild<QLabel*>( "ShadowSearchEmpty" )->isVisible() );
  panel->findChild<QPushButton*>( "ShadowSearchClear" )->click();
  settle();
  CHECK( panel->isVisible() && input->text().isEmpty() && launch->text().isEmpty() );
  QKeyEvent escape( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
  QApplication::sendEvent( input, &escape );
  CHECK( !panel->isVisible() && !window.isMinimized() );
  panel->open( QStringLiteral( "林" ) );
  settle();
  QMouseEvent outside( QEvent::MouseButtonPress, QPointF( 2, 200 ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( window.findChild<QWidget*>( "ShadowWorkspace" ), &outside );
  CHECK( !panel->isVisible() && launch->text().isEmpty() );
  panel->open( QStringLiteral( "林" ) );
  window.resize( 960, 700 );
  settle();
  CHECK( panel->geometry().right() < window.width() && panel->geometry().bottom() < window.height() );
  panel->dismiss();
  window.resize( 1200, 800 );
  std::puts( "PASS global search: categories, more, clear/empty, keyboard, contact/group open, live/archive message location, Escape/outside close and resize" );
  return 0;
}

static int verifyDirectory( GuiMain& window, Core& core )
{
  CHECK( ShadowDirectoryPage::initialForName( QStringLiteral( "林舟" ) ) == "L" );
  CHECK( ShadowDirectoryPage::initialForName( QStringLiteral( "陈语" ) ) == "C" );
  CHECK( ShadowDirectoryPage::initialForName( QStringLiteral( "张遥" ) ) == "Z" );
  CHECK( ShadowDirectoryPage::initialForName( QStringLiteral( "Alice" ) ) == "A" );
  CHECK( ShadowDirectoryPage::initialForName( QStringLiteral( "123" ) ) == "#" );
  QToolButton* navigation = window.findChild<QToolButton*>( "ShadowNavButton" );
  for( QToolButton* candidate : window.findChildren<QToolButton*>( "ShadowNavButton" ) )
    if( candidate->text() == QStringLiteral( "通讯录" ) ) navigation = candidate;
  CHECK( navigation && navigation->text() == QStringLiteral( "通讯录" ) );
  navigation->click();
  settle();
  ShadowDirectoryPage* directory = window.findChild<ShadowDirectoryPage*>( "ShadowDirectoryPage" );
  CHECK( directory && directory->isVisible() );
  CHECK( directory->section() == ShadowDirectoryPage::Contacts );
  CHECK( !window.findChild<QWidget*>( "ShadowConversationPanel" )->isVisible() );
  CHECK( directory->findChild<QWidget*>( "ShadowDirectorySidebar" )->isVisible() );
  QTreeWidget* list = directory->findChild<QTreeWidget*>( "ShadowDirectoryList" );
  QComboBox* sort = directory->findChild<QComboBox*>( "ShadowDirectorySort" );
  QLineEdit* search = window.findChild<QLineEdit*>( "ShadowGlobalSearch" );
  CHECK( list && sort && search );
  const QList<QTreeWidgetItem*> contacts = directoryRows( list, 1 );
  CHECK( contacts.size() == 11 );
  CHECK( contacts.first()->data( 0, Qt::UserRole ).toULongLong() == 87005 );
  for( QTreeWidgetItem* item : contacts )
    CHECK( item->data( 0, Qt::UserRole ).toULongLong() != ID_LOCAL_USER );
  CHECK( !directoryRows( list, 0 ).isEmpty() );
  window.grab().save( QDir::currentPath() + "/ui-directory-contacts.png" );
  search->setText( QStringLiteral( "林舟" ) );
  settle();
  CHECK( directoryRows( list, 1 ).size() == 1 );
  CHECK( directoryRows( list, 1 ).first()->data( 0, Qt::UserRole ).toULongLong() == 87001 );
  User originalContact = UserManager::instance().findUser( 87001 );
  User changedContact = originalContact;
  changedContact.setWorkgroups( QStringList() << QStringLiteral( "临时变更组织（合成）" ) );
  list->setCurrentItem( directoryRows( list, 1 ).first() );
  UserManager::instance().setUser( changedContact );
  core.userChanged( changedContact );
  settle();
  CHECK( directoryRows( list, 1 ).first()->data( 0, Qt::UserRole + 3 ).toString().contains( QStringLiteral( "临时变更" ) ) );
  CHECK( list->currentItem() && list->currentItem()->data( 0, Qt::UserRole ).toULongLong() == 87001 );
  UserManager::instance().setUser( originalContact );
  core.userChanged( originalContact );
  settle();
  search->setText( QStringLiteral( "没有这个联系人_验收" ) );
  settle();
  CHECK( directoryRows( list, 1 ).isEmpty() );
  CHECK( directory->findChild<QWidget*>( "ShadowDirectoryEmpty" )->isVisible() );
  search->clear();
  window.findChild<ShadowSearchPanel*>( "ShadowSearchPanel" )->dismiss();
  User removableContact = UserManager::instance().findUser( 87011 );
  UserManager::instance().removeUser( removableContact );
  core.userRemoved( removableContact );
  settle();
  CHECK( directoryRows( list, 1 ).size() == 10 );
  UserManager::instance().setUser( removableContact );
  core.userChanged( removableContact );
  settle();
  CHECK( directoryRows( list, 1 ).size() == 11 );
  sort->setCurrentIndex( 1 );
  settle();
  CHECK( directoryRows( list, 1 ).first()->data( 0, Qt::UserRole ).toULongLong() == 87011 );
  sort->setCurrentIndex( 0 );
  settle();
  QAbstractButton* indexZ = Q_NULLPTR;
  QWidget* alphabet = directory->findChild<QWidget*>( "ShadowDirectoryAlphabet" );
  CHECK( alphabet );
  for( QAbstractButton* button : alphabet->findChildren<QAbstractButton*>() )
    if( button->text() == "Z" ) indexZ = button;
  CHECK( indexZ && indexZ->isEnabled() );
  indexZ->click();
  settle();
  CHECK( list->verticalScrollBar()->value() > 0 );
  directory->refresh();
  settle();
  CHECK( list->verticalScrollBar()->value() > 0 );
  QAbstractButton* organization = directory->findChild<QAbstractButton*>( "ShadowDirectoryOrganizationButton" );
  QAbstractButton* groups = directory->findChild<QAbstractButton*>( "ShadowDirectoryGroupsButton" );
  QAbstractButton* people = directory->findChild<QAbstractButton*>( "ShadowDirectoryContactsButton" );
  CHECK( organization && groups && people );
  organization->click();
  settle();
  CHECK( directory->section() == ShadowDirectoryPage::Organization );
  CHECK( directoryRows( list, 1 ).size() == 11 && directoryRows( list, 0 ).size() >= 2 );
  window.grab().save( QDir::currentPath() + "/ui-directory-organization.png" );
  groups->click();
  settle();
  CHECK( directory->section() == ShadowDirectoryPage::Groups );
  CHECK( directoryRows( list, 2 ).size() == 1 );
  CHECK( directoryRows( list, 2 ).first()->data( 0, Qt::UserRole ).toULongLong() == 88002 );
  window.grab().save( QDir::currentPath() + "/ui-directory-groups.png" );
  list->setCurrentItem( directoryRows( list, 2 ).first() );
  QKeyEvent openGroup( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( list, &openGroup );
  settle();
  CHECK( !directory->isVisible() );
  CHECK( window.findChild<QLabel*>( "ShadowChatTitle" )->text().contains( QStringLiteral( "设计讨论组" ) ) );
  navigation->click();
  settle();
  people->click();
  search->setText( QStringLiteral( "林舟" ) );
  settle();
  CHECK( directoryRows( list, 1 ).size() == 1 );
  list->setCurrentItem( directoryRows( list, 1 ).first() );
  QKeyEvent openContact( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( list, &openContact );
  settle();
  CHECK( !directory->isVisible() && search->text().isEmpty() );
  CHECK( window.findChild<QLabel*>( "ShadowChatTitle" )->text().contains( QStringLiteral( "林舟" ) ) );
  return 0;
}

int main( int argc, char** argv )
{
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  std::setvbuf( stderr, Q_NULLPTR, _IONBF, 0 );
  qputenv( "QT_QPA_PLATFORM", "offscreen" );
  BeeApplication app( argc, argv );
  app.setFont( ShadowStyle::applicationFont() );
  ShadowDialogChrome::install( &app );
  CHECK( verifyTranscript() == 0 );
  Random::init();
  QTemporaryDir temporary( QDir::currentPath() + "/native-preview-XXXXXX" );
  CHECK( temporary.isValid() );
  const QString profile = temporary.path() + "/synthetic";
  const QString config = temporary.path() + "/config";
  CHECK( QDir().mkpath( profile ) && QDir().mkpath( config ) );
  CHECK( atomicWrite( profile + "/.shadow-dev-preview", "shadow-native-preview-synthetic-v1\n" ) );
  const QStringList arguments = QStringList() << "test" << "--dev-preview" << config << "--profile-root" << profile
    << "--dev-instance" << "11111111-2222-3333-4444-555555555555" << "--dev-seed";
  QString error;
  CHECK( ShadowDevPreview::configure( arguments, &error ) );
  CHECK( ShadowDevPreview::active() );
  CHECK( ShadowDevPreview::seedAccount( &error ) );
  ShadowLocalAccounts accounts;
  CHECK( accounts.authenticate( "preview@example.test", "synthetic-preview-only" ) );
  CHECK( !accounts.authenticate( "preview@example.test", "wrong-password" ) );
  QString email;
  QString folder;
  CHECK( ShadowLocalAccounts::login( &email, &folder ) );
  CHECK( email == "preview@example.test" && folder.startsWith( profile ) );
  CHECK( ShadowDevPreview::validateStyle( "QLabel {color:#123456;}", &error ) );
  CHECK( ShadowDevPreview::validateStyle( "/* { */ QLabel {qproperty-text:\"}\";}", &error ) );
  CHECK( !ShadowDevPreview::validateStyle( "QLabel { color:#123456;", &error ) );
  CHECK( !ShadowDevPreview::validateStyle( "QLabel { color:\"red; }", &error ) );
  CHECK( !ShadowDevPreview::validateStyle( "/* missing terminator", &error ) );
  CHECK( ShadowDevPreview::validateStyle( "QLabel {unknown-qt-property:yes;}", &error ) );
  QJsonObject layout;
  CHECK( ShadowDevPreview::validateLayout( "{\"windowWidth\":1200,\"conversationListWidth\":300}", &layout, &error ) );
  CHECK( !ShadowDevPreview::validateLayout( "{\"windowWidth\":200}", &layout, &error ) );
  CHECK( !ShadowDevPreview::validateLayout( "{\"windowWidth\":1200.5}", &layout, &error ) );
  CHECK( !ShadowDevPreview::validateLayout( "{\"windowWidth\":\"1200\"}", &layout, &error ) );
  CHECK( !ShadowDevPreview::validateLayout( "{\"unexpected\":1}", &layout, &error ) );
  CHECK( !ShadowDevPreview::validateLayout( "{", &layout, &error ) );
  CHECK( !ShadowDevPreview::validateLayout( "[]", &layout, &error ) );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "synthetic-test" );
  settings.setAccountDataFolder( folder );
  settings.load();
  ShadowDevPreview::prepareSettings();
  PreviewCoreProbe core;
  CHECK( core.devPreviewIsolated() );
  app.setProperty( "shadowDevPreview", false );
  CHECK( core.devPreviewIsolated() );
  app.setProperty( "shadowDevPreview", true );
  CHECK( core.start() );
  Listener* listener = core.findChild<Listener*>();
  CHECK( listener && listener->serverAddress() == QHostAddress::LocalHost );
  CHECK( listener->serverPort() > 0 );
  CHECK( !core.startFileTransferServer() );
  core.newPeerFound( QHostAddress( "192.0.2.55" ), 36475 );
  CHECK( core.findChildren<Connection*>().isEmpty() );
  FileTransfer* transfer = core.findChild<FileTransfer*>();
  CHECK( transfer && !transfer->hasActivePeers() );
  FileInfo remote;
  remote.setId( 99801 );
  remote.setNameAndSuffix( "synthetic-download.txt" );
  remote.setHostAddress( QHostAddress( "192.0.2.55" ) );
  remote.setHostPort( 4567 );
  remote.setSize( 64 );
  const QString rejectedTarget = temporary.path() + "/must-not-be-created/download.txt";
  core.downloadFromShareBox( 87001, remote, rejectedTarget );
  CHECK( !transfer->hasActivePeers() && transfer->findChildren<FileTransferPeer*>().isEmpty() );
  CHECK( !QFileInfo::exists( rejectedTarget ) && !QFileInfo::exists( QFileInfo( rejectedTarget ).absolutePath() ) );
  const QString source = folder + "/synthetic-source.txt";
  CHECK( atomicWrite( source, "synthetic send refusal fixture" ) );
  User recipient( 87001 );
  recipient.setName( "synthetic-recipient" );
  recipient.setHash( "test-send-guard" );
  CHECK( !core.sendFileToUser( recipient, source, QString(), false, Chat() ) );
  CHECK( !QFileInfo::exists( folder + "/pending-transfers.json" ) && !transfer->hasActivePeers() );
  GuiMain window;
  window.loadStyle();
  window.resize( 1100, 760 );
  window.show();
  const bool checkStartupSize = app.arguments().contains( "--startup-size-only" );
  if( checkStartupSize )
  {
    settings.setResetGeometryAtStartup( false );
    settings.setLoadOnTrayAtStartup( false );
    settings.setShowMinimizedAtStartup( false );
    window.resize( 1800, 1100 );
    window.showMaximized();
    QApplication::processEvents();
    settings.setGuiGeometry( window.saveGeometry() );
    CHECK( QMetaObject::invokeMethod( &window, "checkWindowFlagsAndShow", Qt::DirectConnection ) );
    QApplication::processEvents();
    CHECK( !window.isMaximized() && !window.isFullScreen() );
    CHECK( window.width() <= 1120 && window.height() <= 820 );
    window.resize( 1300, 900 );
    CHECK( window.width() == 1300 && window.height() == 900 );
    window.resize( 900, 650 );
    settings.setGuiGeometry( window.saveGeometry() );
    CHECK( QMetaObject::invokeMethod( &window, "checkWindowFlagsAndShow", Qt::DirectConnection ) );
    CHECK( window.width() <= 900 && window.height() <= 650 &&
      window.width() >= window.minimumWidth() && window.height() >= window.minimumHeight() );
    CHECK( window.findChild<QWidget*>( "ShadowResizeRight" )->isVisible() );
    window.resize( 1800, 1100 );
    window.showMaximized();
    QApplication::processEvents();
    QSettings session( profile + "/preview-window.ini", QSettings::IniFormat );
    session.setValue( "geometry", window.saveGeometry() );
    session.setValue( "layout", QByteArray( "{\"windowWidth\":1120,\"windowHeight\":820,\"conversationListWidth\":300}" ) );
    session.sync();
    window.showNormal();
    window.resize( 1100, 760 );
  }
  const QString baselineStyle = window.styleSheet();
  QMap<QString,QString> oldFixtureHistory;
  oldFixtureHistory.insert( QString::fromUtf8( "林舟（合成）" ), "&lt;a href=\"file:///old-fixture.txt\"&gt;old markup&lt;/a&gt;" );
  ChatManager::instance().addSavedChats( oldFixtureHistory );
  CHECK( atomicWrite( config + "/preview.qss", "QLabel#ShadowChatTitle {font-size:21px;color:#172033;padding:0px;}" ) );
  CHECK( atomicWrite( config + "/layout.json", checkStartupSize
    ? "{\"windowWidth\":1120,\"windowHeight\":820,\"conversationListWidth\":300}"
    : "{\"windowWidth\":1200,\"windowHeight\":800,\"conversationListWidth\":300}" ) );
  ShadowDevPreview preview( &window, &core );
  settle();
  if( checkStartupSize )
  {
    CHECK( window.width() <= 1120 && window.height() <= 820 && !window.isMaximized() );
    CHECK( window.findChild<QWidget*>( "ShadowListPanel" )->width() == 300 );
    CHECK( symmetricCorners( window ) );
    window.grab().save( QDir::currentPath() + "/ui-042-default-size.png" );
    CHECK( verifyWindowCorners( window ) == 0 );
    core.stop();
    std::puts( "PASS startup size: saved large/maximized state clamped to normal compact window, smaller geometry retained, user enlargement allowed, preview restore capped to 1120x820, corners and resize handles preserved" );
    return 0;
  }
  if( app.arguments().contains( "--local-send-only" ) )
  {
    const int result = verifyPreviewSending( window, core, preview );
    core.stop();
    return result;
  }
  if( app.arguments().contains( "--corners-only" ) )
  {
    const int result = verifyWindowCorners( window );
    core.stop();
    return result;
  }
  if( app.arguments().contains( "--sidebar-only" ) )
  {
    const int result = verifyConversationSidebar( window, core );
    core.stop();
    return result;
  }
  if( app.arguments().contains("--history-only") )
  {
    int result=verifyEmbeddedHistory(window);
    if(result==0)result=verifySearch(window);
    core.stop();
    return result;
  }
  if(app.arguments().contains("--welcome-only"))
  {
    const int result=verifyWelcome(window);
    core.stop();
    return result;
  }
  if(app.arguments().contains("--reply-only"))
  {
    int result=verifyReply(window,preview);
    if(result==0)result=verifyMessageBoxes(window);
    core.stop();
    return result;
  }
  if(app.arguments().contains("--messagebox-only"))
  {
    const int result=verifyMessageBoxes(window);
    core.stop();
    return result;
  }
  if( app.arguments().contains( "--profile-status-only" ) )
    return verifyProfileStatus( window );
  QLabel* title = window.findChild<QLabel*>( "ShadowChatTitle" );
  CHECK( title );
  CHECK( title->font().pixelSize() == 21 );
  CHECK( window.width() == 1200 );
  CHECK( !window.mask().isEmpty() && !window.mask().contains( QPoint() ) );
  CHECK( window.mask().contains( window.rect().center() ) );
  CHECK( !window.findChild<QLabel*>( "ShadowSectionTitle" )->isVisible() );
  CHECK( !window.findChild<QLabel*>( "ShadowSectionHint" )->isVisible() );
  CHECK( !window.findChild<QLabel*>( "ShadowChatSubtitle" )->isVisible() );
  CHECK( !window.findChild<QLabel*>( "ShadowComposerHint" ) );
  QLineEdit* search = window.findChild<QLineEdit*>( "ShadowGlobalSearch" );
  CHECK( search && search->parentWidget()->objectName() == "ShadowTitleBar" );
  CHECK( !window.findChild<QLabel*>( "ShadowWindowTitle" ) );
  CHECK( !window.findChild<QToolButton*>( "ShadowProfileButton" ) );
  CHECK( search->parentWidget()->findChildren<QLabel*>( QString(), Qt::FindDirectChildrenOnly ).isEmpty() );
  CHECK( window.findChild<QToolButton*>( "ShadowTitleProfile" )->isVisible() );
  search->setText( QString::fromUtf8( "林舟" ) );
  settle();
  CHECK( window.findChild<QTreeWidget*>( "ShadowGlobalSearchResults" )->topLevelItemCount() > 0 );
  search->clear();
  window.findChild<ShadowSearchPanel*>( "ShadowSearchPanel" )->dismiss();
  settle();
  CHECK( ChatManager::instance().chat( 88001 ).isValid() );
  CHECK( ChatManager::instance().chat( 88002 ).unreadMessages() == 3 );
  CHECK( ChatManager::instance().chat( 88004 ).messages().isEmpty() );
  QWidget* listPanel = window.findChild<QWidget*>( "ShadowListPanel" );
  QWidget* conversationPanel = window.findChild<QWidget*>( "ShadowConversationPanel" );
  QWidget* conversationStack = window.findChild<QWidget*>( "ShadowConversationStack" );
  QWidget* chatHeader = window.findChild<QWidget*>( "ShadowChatHeader" );
  QSplitter* contentSplitter = window.findChild<QSplitter*>( "ShadowContentSplitter" );
  CHECK( listPanel && conversationPanel && conversationStack && chatHeader && contentSplitter );
  const int listRight = listPanel->mapTo( &window, QPoint( listPanel->width(), 0 ) ).x();
  const int conversationLeft = conversationPanel->mapTo( &window, QPoint() ).x();
  const int stackLeft = conversationStack->mapTo( &window, QPoint() ).x();
  const int headerLeft = chatHeader->mapTo( &window, QPoint() ).x();
  std::fprintf( stdout, "Layout: listRight=%d conversationLeft=%d stackLeft=%d headerLeft=%d handleWidth=%d\n",
                listRight, conversationLeft, stackLeft, headerLeft, contentSplitter->handleWidth() );
  CHECK( headerLeft - listRight == 1 );
  GuiSystemTray* trayIcon = window.findChild<GuiSystemTray*>();
  CHECK( trayIcon && trayIcon->iconStatusIsMessage() );
  const QImage unreadWindowIcon = window.windowIcon().pixmap( 32, 32 ).toImage();
  const QImage unreadTrayIcon = trayIcon->icon().pixmap( 32, 32 ).toImage();
  CHECK( !unreadWindowIcon.isNull() && !unreadTrayIcon.isNull() );
  const QImage shareIcon = IconManager::instance().icon( "desktop-share.png" ).pixmap( 32, 32 ).toImage();
  QList<QAction*> sharingActions;
  for( QAction* action : window.findChildren<QAction*>() )
    if( action->isCheckable() && action->icon().pixmap( 32, 32 ).toImage() == shareIcon )
    {
      action->setChecked( true );
      sharingActions << action;
    }
  CHECK( !sharingActions.isEmpty() );
  const QList<QImage> staticIcons = notificationIcons( window );
  QTabWidget* mainTabs = window.findChild<QTabWidget*>( "GuiTabMain" );
  CHECK( mainTabs );
  QPointer<QWidget> originalPage = mainTabs->currentWidget();
  for( int tick = 2; tick <= 5; ++tick )
  {
    window.onTickEvent( tick );
    CHECK( window.windowIcon().pixmap( 32, 32 ).toImage() == unreadWindowIcon );
    CHECK( trayIcon->icon().pixmap( 32, 32 ).toImage() == unreadTrayIcon );
    CHECK( notificationIcons( window ) == staticIcons );
    CHECK( mainTabs->currentWidget() == originalPage );
    CHECK( ChatManager::instance().chat( 88002 ).unreadMessages() == 3 );
  }
  for( QAction* action : sharingActions )
    action->setChecked( false );
  CHECK( ChatManager::instance().chatSavedText( QString::fromUtf8( "林舟（合成）" ) ).isEmpty() );
  CHECK( QFileInfo::exists( profile + "/fixtures/synthetic-landscape.png" ) );
  CHECK( !QFileInfo::exists( temporary.path() + "/profile" ) );
  QTextBrowser* transcript = window.findChild<QTextBrowser*>( "GuiChatViewer" );
  CHECK( transcript && !transcript->toPlainText().contains( "<a href" ) );
  CHECK( transcript->toHtml().contains( "synthetic-landscape.png" ) );
  CHECK( dynamic_cast<ShadowChatTranscript*>( transcript )->bubbleCount() >= 2 );
  GuiChat* activeChat = Q_NULLPTR;
  for( GuiChat* candidate : window.findChildren<GuiChat*>() )
    if( candidate->isVisible() ) activeChat = candidate;
  CHECK( activeChat );
  const QList<QToolButton*> composerActions = activeChat->findChildren<QToolButton*>( "ShadowComposerAction" );
  CHECK( composerActions.size() == 5 );
  for( QToolButton* action : composerActions )
  {
    CHECK( action->isVisible() && action->size() == QSize( 40, 40 ) );
    CHECK( action->iconSize() == QSize( 24, 24 ) );
    CHECK( !action->accessibleName().isEmpty() );
  }
  QTextEdit* messageEdit = activeChat->findChild<QTextEdit*>( "GuiMessageEdit" );
  QPushButton* sendButton = activeChat->findChild<QPushButton*>( "ShadowSendButton" );
  QLabel* sendFeedback = activeChat->findChild<QLabel*>( "ShadowSendFeedback" );
  CHECK( messageEdit && sendButton && sendFeedback && sendButton->isEnabled() );
  const QString retainedDraft = QStringLiteral( "发送失败时保留这段输入" );
  activeChat->addText( retainedDraft );
  const int messagesBeforeSend = ChatManager::instance().chat( activeChat->chatId() ).messages().size();
  const int pendingBeforeSend = MessageManager::instance().countMessagesToSendInChatId( activeChat->chatId() );
  QTcpServer* blockedPeer = preview.findChild<QTcpServer*>( "ShadowPreviewPeerServer" );
  CHECK( blockedPeer );
  blockedPeer->close();
  sendButton->click();
  CHECK( blockedPeer->listen( QHostAddress::LocalHost, 0 ) );
  CHECK( messageEdit->toPlainText() == retainedDraft );
  CHECK( sendFeedback->isVisible() && sendFeedback->text().contains( QStringLiteral( "未发送" ) ) );
  CHECK( sendFeedback->text().contains( QStringLiteral( "未就绪" ) ) );
  CHECK( ChatManager::instance().chat( activeChat->chatId() ).messages().size() == messagesBeforeSend );
  CHECK( MessageManager::instance().countMessagesToSendInChatId( activeChat->chatId() ) == pendingBeforeSend );
  window.grab().save( QDir::currentPath() + "/ui-send-failure-draft.png" );
  QImage screenshot( 1000, 520, QImage::Format_RGB32 );
  screenshot.fill( QColor( "#2C7FEF" ) );
  QPainter screenshotPainter( &screenshot );
  screenshotPainter.fillRect( 500, 0, 500, 520, QColor( "#FCB64D" ) );
  screenshotPainter.setPen( Qt::white );
  screenshotPainter.setFont( ShadowStyle::applicationFont() );
  screenshotPainter.drawText( QRect( 40, 40, 920, 440 ), Qt::AlignCenter, QStringLiteral( "合成截图 · 发送前可见预览" ) );
  screenshotPainter.end();
  const QString originalDownloadFolder = Settings::instance().downloadDirectory();
  const QString screenshotOutput = temporary.path() + "/screenshot-must-not-be-written";
  Settings::instance().setDownloadDirectory( screenshotOutput );
  bool screenshotVerified = false;
  const bool transfersEnabled = Settings::instance().enableFileTransfer();
  Settings::instance().setEnableFileTransfer( false );
  QTimer::singleShot( 100, activeChat, [&screenshotVerified]() {
    QDialog* screenshotDialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( !screenshotDialog ) return;
    QWidget* canvas = screenshotDialog->findChild<QWidget*>( "ShadowScreenshotCanvas" );
    QLabel* dimensions = screenshotDialog->findChild<QLabel*>( "ShadowScreenshotDimensions" );
    QPushButton* send = screenshotDialog->findChild<QPushButton*>( "ShadowScreenshotSend" );
    if( !canvas || !dimensions || !send )
    {
      screenshotDialog->reject();
      return;
    }
    const QImage visibleImage = canvas->grab().toImage();
    int bluePixels = 0;
    int goldPixels = 0;
    for( int row = 0; row < visibleImage.height(); ++row )
      for( int column = 0; column < visibleImage.width(); ++column )
      {
        bluePixels += visibleImage.pixelColor( column, row ) == QColor( "#2C7FEF" );
        goldPixels += visibleImage.pixelColor( column, row ) == QColor( "#FCB64D" );
      }
    screenshotVerified = bluePixels > 1000 && goldPixels > 1000 && dimensions->text().contains( "1000" );
    screenshotDialog->grab().save( QDir::currentPath() + "/ui-screenshot-preview.png" );
    ShadowScreenshotPreview* editor = qobject_cast<ShadowScreenshotPreview*>( screenshotDialog );
    QWidget* toolbar = screenshotDialog->findChild<QWidget*>( "ShadowScreenshotToolbar" );
    screenshotVerified = screenshotVerified && editor && editor->selectedImage().size() == QSize( 1000, 520 ) &&
      toolbar && toolbar->isVisible() && !screenshotDialog->findChild<QWidget*>( "shadowDialogHeader" );
    send->click();
    QApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
    QLabel* error = screenshotDialog->findChild<QLabel*>( "ShadowScreenshotFeedback" );
    screenshotVerified = screenshotVerified && screenshotDialog->isVisible() && canvas->isVisible() &&
      error && error->isVisible() && error->text().contains( QStringLiteral( "未发送" ) ) &&
      error->text().contains( QStringLiteral( "文件传输" ) ) &&
      error->text().contains( QStringLiteral( "可重试或取消" ) );
    screenshotVerified = screenshotVerified && editor->selectedImage().size() == QSize( 1000, 520 );
    screenshotDialog->grab().save( QDir::currentPath() + "/ui-screenshot-send-blocked.png" );
    screenshotDialog->reject();
  } );
  activeChat->previewScreenshot( QPixmap::fromImage( screenshot ) );
  Settings::instance().setEnableFileTransfer( transfersEnabled );
  std::puts( "Screenshot preview closed after verified refusal" );
  CHECK( screenshotVerified );
  CHECK( !QFileInfo::exists( screenshotOutput ) );
  CHECK( messageEdit->toPlainText() == retainedDraft );
  CHECK( !QFileInfo::exists( folder + "/pending-transfers.json" ) );
  Settings::instance().setDownloadDirectory( originalDownloadFolder );
  if( app.arguments().contains( "--screenshot-only" ) )
  {
    core.stop();
    std::puts( "PASS screenshot editor integration: native chat send refusal keeps edited image and draft, no output or pending transfer" );
    return 0;
  }
  {
    std::puts( "Checking accepted submission acknowledgement" );
    QToolBar submissionTools;
    GuiChat submission;
    submission.setupToolBar( &submissionTools );
    submission.setModernMode( true );
    CHECK( submission.setChat( ChatManager::instance().chat( activeChat->chatId() ) ) );
    submission.addText( QStringLiteral( "已被队列接收的消息" ) );
    QObject::connect( &submission, &GuiChat::newMessage, &submission,
      [&submission]( VNumber, const QString& text, bool ) {
        submission.setMessageSendResult( text, true, QStringLiteral( "已加入待发送队列，尚未送达。" ) );
      } );
    CHECK( QMetaObject::invokeMethod( &submission, "sendMessage", Qt::DirectConnection ) );
    CHECK( submission.findChild<QTextEdit*>( "GuiMessageEdit" )->toPlainText().isEmpty() );
    CHECK( submission.findChild<QLabel*>( "ShadowSendFeedback" )->text().contains( QStringLiteral( "尚未送达" ) ) );
    std::puts( "Accepted submission clears acknowledged draft" );
  }
  QDialog dialog( &window );
  dialog.setWindowTitle( QStringLiteral( "影聊 · 对话框验证" ) );
  QVBoxLayout* dialogLayout = new QVBoxLayout( &dialog );
  QLabel* originalContent = new QLabel( QStringLiteral( "保留原有内容与取消行为" ), &dialog );
  dialogLayout->addWidget( originalContent );
  bool chromeVerified = false;
  QTimer::singleShot( 80, &dialog, [&dialog, originalContent, &chromeVerified]() {
    QToolButton* close = dialog.findChild<QToolButton*>( "shadowDialogClose" );
    QWidget* surface = dialog.findChild<QWidget*>( "shadowDialogSurface" );
    QWidget* shadow = dialog.findChild<QWidget*>( "shadowDialogShadow" );
    chromeVerified = close && dialog.windowFlags().testFlag( Qt::FramelessWindowHint ) &&
      dialog.testAttribute( Qt::WA_TranslucentBackground ) &&
      surface && !surface->mask().contains( QPoint() ) && shadow && shadow->isVisible() &&
      shadow->testAttribute( Qt::WA_TransparentForMouseEvents ) && originalContent->isVisible() &&
      dialog.findChildren<QWidget*>( "shadowDialogHeader" ).size() == 1;
    dialog.grab().save( QDir::currentPath() + "/ui-dialog-chrome.png" );
    if( close ) close->click();
    else dialog.reject();
  } );
  CHECK( dialog.exec() == QDialog::Rejected && chromeVerified );
  const bool soundBeforeSettings = Settings::instance().beepOnNewMessageArrived();
  bool settingsVerified = false;
  QTimer::singleShot( 100, &window, [&settingsVerified]() {
    QDialog* settingsDialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( !settingsDialog ) return;
    QToolButton* close = settingsDialog->findChild<QToolButton*>( "shadowDialogClose" );
    settingsVerified = close && settingsDialog->windowTitle() == QStringLiteral( "影聊设置" );
    settingsDialog->grab().save( QDir::currentPath() + "/ui-settings-dialog.png" );
    if( close ) close->click();
    else settingsDialog->reject();
  } );
  CHECK( QMetaObject::invokeMethod( &window, "showShadowSettings", Qt::DirectConnection ) );
  CHECK( settingsVerified && Settings::instance().beepOnNewMessageArrived() == soundBeforeSettings );
  QPointer<QWidget> transferView = window.findChild<QWidget*>( "GuiFileTransfer" );
  CHECK( transferView );
  bool transfersVerified = false;
  QTimer::singleShot( 100, &window, [&transfersVerified, transferView]() {
    QDialog* transferDialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( !transferDialog ) return;
    transfersVerified = transferView && transferView->isVisible() &&
      transferDialog->findChild<QToolButton*>( "shadowDialogClose" );
    transferDialog->grab().save( QDir::currentPath() + "/ui-transfers-dialog.png" );
    transferDialog->reject();
  } );
  CHECK( QMetaObject::invokeMethod( &window, "showModernTransfers", Qt::DirectConnection ) );
  CHECK( transfersVerified && transferView && window.isAncestorOf( transferView ) );
  CHECK( verifyDirectory( window, core ) == 0 );
  CHECK( verifySearch( window ) == 0 );
  CHECK( verifyConversationSidebar( window, core ) == 0 );
  window.grab().save( QDir::currentPath() + "/ui-private-conversation.png" );
  CHECK( atomicWrite( config + "/preview.qss", "QLabel#ShadowChatTitle {font-size:25px;color:#172033;padding:0px;}" ) );
  settle();
  CHECK( title->font().pixelSize() == 25 );
  CHECK( atomicWrite( config + "/preview.qss", "QLabel#ShadowChatTitle {font-size:25px;color:#196b45;padding:0px;}" ) );
  settle();
  CHECK( title->palette().color( QPalette::WindowText ) == QColor( "#196b45" ) );
  CHECK( atomicWrite( config + "/preview.qss", "QLabel#ShadowChatTitle {font-size:25px;color:#196b45;padding:9px;}" ) );
  settle();
  CHECK( title->contentsRect().x() == 9 && title->height() >= title->fontMetrics().height() + 18 );
  const QString usableStyle = window.styleSheet();
  CHECK( atomicWrite( config + "/preview.qss", "QLabel {" ) );
  CHECK( atomicWrite( config + "/layout.json", "{\"windowWidth\":200}" ) );
  settle();
  CHECK( window.styleSheet() == usableStyle && window.width() == 1200 );
  QFile status( temporary.path() + "/logs/preview-status.json" );
  CHECK( status.open( QIODevice::ReadOnly ) );
  CHECK( !QJsonDocument::fromJson( status.readAll() ).object().value( "diagnostic" ).toString().isEmpty() );
  status.close();
  CHECK( atomicWrite( config + "/layout.json", "{\"windowWidth\":1280,\"windowHeight\":820,\"conversationListWidth\":330}" ) );
  settle();
  CHECK( window.width() == 1280 && window.findChild<QWidget*>( "ShadowListPanel" )->width() == 330 );
  CHECK( chatHeader->mapTo( &window, QPoint() ).x() -
         listPanel->mapTo( &window, QPoint( listPanel->width(), 0 ) ).x() == 1 );
  preview.restoreBaseline();
  CHECK( window.styleSheet() == baselineStyle && window.width() == 1100 );
  CHECK( window.findChild<QWidget*>( "ShadowListPanel" )->minimumWidth() == 272 );
  CHECK( QFileInfo::exists( profile + "/preview-window.ini" ) );
  CHECK( verifyQuietProfileUpdates( core ) == 0 );
  CHECK( verifyProfileStatus( window ) == 0 );
  core.stop();
  std::puts( "PASS native preview: authentication/isolation, fixtures, style/layout reload, static notifications, rounded bubbles/dialogs, screenshot/send feedback; directory Chinese initials, sorting, index jump, search/empty, live updates, categories and keyboard open-chat" );
  return 0;
}
