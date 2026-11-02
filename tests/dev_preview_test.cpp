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
#include "GuiStickerPicker.h"
#include "GuiMessageEdit.h"
#include "ShadowStickerStore.h"
#include "ShadowRichText.h"
#include "ChatMessageData.h"
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
  CHECK( feedback->isHidden() && feedback->text().isEmpty() );
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
  const QByteArray animationBytes=QByteArray::fromBase64(
    "R0lGODlhGAAMAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQACgAAACwAAAAAGAAMAAAIHgABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFiwwDAgAh+QQBCgABACwAAAAAGAAMAIEAAP8AAAAAAAAAAAAIHgABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFiwwDAgA7");
  const QString stickerFolder=QDir(Settings::instance().dataFolder()).filePath("stickers");
  CHECK(QDir().mkpath(stickerFolder));
  const QString stickerPath=QDir(stickerFolder).filePath(
    QString::fromLatin1(QCryptographicHash::hash(animationBytes,QCryptographicHash::Sha256).toHex())+".gif");
  QFile stickerSource(stickerPath);
  CHECK(stickerSource.open(QIODevice::WriteOnly)&&stickerSource.write(animationBytes)==animationBytes.size());
  stickerSource.close();
  CHECK(QMetaObject::invokeMethod(&window,"sendFilesFromChat",Qt::DirectConnection,
    Q_ARG(VNumber,VNumber(88001)),Q_ARG(QStringList,QStringList{stickerPath})));
  const QString receivedAnimationPath=preview.property("lastTestReceivedFile").toString();
  QFile receivedAnimation(receivedAnimationPath);
  CHECK(receivedAnimation.open(QIODevice::ReadOnly)&&receivedAnimation.readAll()==animationBytes);
  CHECK(transport->property("receivedMessages").toInt()==receivedBefore+4);
  const QList<ChatMessage> deliveredMessages=ChatManager::instance().chat(88001).messages();
  CHECK(deliveredMessages.at(deliveredMessages.size()-2).message().contains("<!--shadow-sticker-->"));
  QTextBrowser* animationTranscript=chat->findChild<QTextBrowser*>("GuiChatViewer");
  bool smallAnimatedSticker=false;
  for(QTextBlock block=animationTranscript->document()->begin();block.isValid();block=block.next())
    for(QTextBlock::iterator iterator=block.begin();!iterator.atEnd();++iterator)
    {
      const QTextFragment fragment=iterator.fragment();
      if(!fragment.charFormat().isImageFormat())continue;
      const QTextImageFormat image=fragment.charFormat().toImageFormat();
      if(QUrl(image.name()).toLocalFile()==receivedAnimationPath)
        smallAnimatedSticker=image.width()==24&&image.height()==12;
    }
  CHECK(smallAnimatedSticker);
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
  CHECK( transport->property( "receivedMessages" ).toInt() == receivedBefore + 4 );
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

static int verifyComposer( GuiMain& window, ShadowDevPreview& preview )
{
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  window.findChild<ShadowSearchPanel*>("ShadowSearchPanel")->dismiss();
  window.resize(1120,820);
  settle();
  GuiChat* chat=Q_NULLPTR;
  for(GuiChat* candidate:window.findChildren<GuiChat*>())if(candidate->isVisible())chat=candidate;
  CHECK(chat);
  GuiMessageEdit* editor=chat->findChild<GuiMessageEdit*>("GuiMessageEdit");
  QTextBrowser* transcript=chat->findChild<QTextBrowser*>("GuiChatViewer");
  QPushButton* send=chat->findChild<QPushButton*>("ShadowSendButton");
  QToolButton* stickerButton=Q_NULLPTR;
  for(QToolButton* button:chat->findChildren<QToolButton*>())if(button->property("stickerPicker").toBool())stickerButton=button;
  QObject* transport=preview.findChild<QObject*>("ShadowPreviewTransport");
  CHECK(editor&&transcript&&send&&stickerButton&&transport);
  QTemporaryDir files;
  CHECK(files.isValid());
  ShadowStickerStore store(Settings::instance().dataFolder());
  const int originalStickers=store.list().size();
  for(int index=0;index<24;++index)
  {
    QImage image(48,48,QImage::Format_RGB32);
    image.fill(QColor::fromHsv(index*13,150,220));
    const QString path=files.filePath(QString("sticker-fixture-%1.png").arg(index,2,10,QChar('0')));
    CHECK(image.save(path)&&store.addFile(path));
  }
  stickerButton->click();
  auto* picker=chat->findChild<GuiStickerPicker*>("ShadowStickerPicker");
  CHECK(picker&&picker->isVisible()&&picker->windowType()==Qt::Popup&&!picker->isModal());
  CHECK(QApplication::activeModalWidget()!=picker);
  const QRect screen=stickerButton->screen()->availableGeometry();
  CHECK(screen.contains(picker->geometry()));
  CHECK(picker->geometry().bottom()<=stickerButton->mapToGlobal(QPoint()).y());
  QListWidget* grid=picker->findChild<QListWidget*>("StickerGrid");
  CHECK(grid&&grid->count()==originalStickers+25&&grid->item(0)->data(Qt::UserRole+2).toBool());
  QLineEdit* search=picker->findChild<QLineEdit*>("StickerSearch");
  search->setText("sticker-fixture-07");
  CHECK(grid->count()==2);
  search->clear();
  const QRect desktop=QRect(window.mapToGlobal(QPoint()),window.size()).united(picker->geometry());
  QPixmap proof(desktop.size());
  proof.fill(Qt::white);
  {
    QPainter painter(&proof);
    painter.drawPixmap(window.mapToGlobal(QPoint())-desktop.topLeft(),window.grab());
    painter.drawPixmap(picker->pos()-desktop.topLeft(),picker->grab());
  }
  CHECK(proof.save(QDir::currentPath()+"/ui-066-sticker-popup.png"));
  const int beforeSticker=transport->property("receivedMessages").toInt();
  QListWidgetItem* chosen=grid->item(1);
  const QString chosenPath=chosen->data(Qt::UserRole+1).toString();
  const QPoint point=grid->visualItemRect(chosen).center();
  QMouseEvent press(QEvent::MouseButtonPress,point,grid->viewport()->mapToGlobal(point),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease,point,grid->viewport()->mapToGlobal(point),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
  QApplication::sendEvent(grid->viewport(),&press);
  QApplication::sendEvent(grid->viewport(),&release);
  CHECK(!picker->isVisible()&&transport->property("receivedMessages").toInt()==beforeSticker+1);
  QFile original(chosenPath),received(preview.property("lastTestReceivedFile").toString());
  CHECK(original.open(QIODevice::ReadOnly)&&received.open(QIODevice::ReadOnly)&&original.readAll()==received.readAll());
  stickerButton->click();
  for(QToolButton* button:picker->findChildren<QToolButton*>())if(button->accessibleName()==QStringLiteral("最近使用"))button->click();
  CHECK(grid->count()==2);
  for(QToolButton* button:picker->findChildren<QToolButton*>())if(button->accessibleName()==QStringLiteral("我的收藏"))button->click();
  QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
  QApplication::sendEvent(picker,&escape);
  CHECK(!picker->isVisible());
  stickerButton->click();
  QWidget* header=window.findChild<QWidget*>("ShadowChatHeader");
  QMouseEvent outside(QEvent::MouseButtonPress,QPoint(10,10),header->mapToGlobal(QPoint(10,10)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
  QApplication::sendEvent(header,&outside);
  CHECK(!picker->isVisible());

  QImage receivedImage(79,61,QImage::Format_RGB32);
  receivedImage.fill(QColor("#FD8B51"));
  const QString originalPath=files.filePath("received-original.png");
  CHECK(receivedImage.save(originalPath));
  QFile imageFile(originalPath);
  CHECK(imageFile.open(QIODevice::ReadOnly));
  const QByteArray originalBytes=imageFile.readAll();
  imageFile.close();
  transcript->setHtml(QString("<a href=\"%1\"><img src=\"%1\" width=\"79\" height=\"61\"></a>").arg(QUrl::fromLocalFile(originalPath).toString()));
  QApplication::processEvents();
  QPoint imagePoint;
  for(int row=0;row<100&&imagePoint.isNull();++row)
    for(int column=0;column<150;++column)if(!transcript->anchorAt(QPoint(column,row)).isEmpty()){imagePoint=QPoint(column,row);break;}
  CHECK(!imagePoint.isNull());
  bool chatFavorite=false;
  QTimer::singleShot(50,chat,[&]() {
    if(QMenu* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget()))
    {
      for(QAction* action:menu->actions())if(action->objectName()=="FavoriteStickerAction")
      {chatFavorite=action->text()==QStringLiteral("添加到表情");action->trigger();break;}
      menu->close();
    }
  });
  CHECK(QMetaObject::invokeMethod(chat,"customContextMenu",Qt::DirectConnection,Q_ARG(QPoint,imagePoint))&&chatFavorite);
  CHECK(store.list().size()==originalStickers+25);
  CHECK(QFile::remove(originalPath));
  const QString imageHash=QString::fromLatin1(QCryptographicHash::hash(originalBytes,QCryptographicHash::Sha256).toHex());
  bool independent=false;
  for(const auto& sticker:store.list())if(sticker.id==imageHash)
  {
    QFile kept(sticker.path);
    independent=kept.open(QIODevice::ReadOnly)&&kept.readAll()==originalBytes;
  }
  CHECK(independent);
  CHECK(chat->setChat(ChatManager::instance().chat(88001)));
  editor->clearMessage();
  const int beforeDraft=transport->property("receivedMessages").toInt();
  QImage draftImage(83,57,QImage::Format_RGB32);
  draftImage.fill(QColor("#79B3D8"));
  QMimeData mime;
  mime.setImageData(draftImage);
  CHECK(QMetaObject::invokeMethod(chat,"checkAndSendImage",Qt::DirectConnection,Q_ARG(const QMimeData*,&mime)));
  CHECK(editor->draftImages().size()==1&&transport->property("receivedMessages").toInt()==beforeDraft);
  QTextCursor imageCursor(editor->document());
  imageCursor.setPosition(0);
  const QPoint draftPoint=editor->cursorRect(imageCursor).topLeft()+QPoint(8,8);
  bool draftFavorite=false;
  QTimer::singleShot(50,editor,[&]() {
    if(QMenu* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget()))
    {
      for(QAction* action:menu->actions())if(action->objectName()=="ComposerFavoriteStickerAction")
      {action->trigger();draftFavorite=true;break;}
      menu->close();
    }
  });
  QContextMenuEvent context(QContextMenuEvent::Mouse,draftPoint,editor->viewport()->mapToGlobal(draftPoint));
  QApplication::sendEvent(editor->viewport(),&context);
  CHECK(draftFavorite&&store.list().size()==originalStickers+26&&editor->draftImages().size()==1);
  bool blankFavorite=false;
  QTimer::singleShot(50,editor,[&]() {
    if(QMenu* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget()))
    {
      for(QAction* action:menu->actions())
        if(action->objectName()=="ComposerFavoriteStickerAction")blankFavorite=true;
      menu->close();
    }
  });
  const QPoint blankPoint=editor->viewport()->rect().topRight()+QPoint(-10,10);
  QContextMenuEvent blankContext(QContextMenuEvent::Mouse,blankPoint,editor->viewport()->mapToGlobal(blankPoint));
  QApplication::sendEvent(editor->viewport(),&blankContext);
  CHECK(!blankFavorite);
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88002)));
  CHECK(editor->draftImages().isEmpty());
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  CHECK(editor->draftImages().size()==1&&editor->draftImages().first().second==draftImage);
  const User peer=UserManager::instance().findUser(87001);
  User denied=peer;
  denied.setHash("outside-composer-fixtures");
  UserManager::instance().setUser(denied);
  send->click();
  CHECK(editor->draftImages().size()==1&&transport->property("receivedMessages").toInt()==beforeDraft);
  UserManager::instance().setUser(peer);
  send->click();
  CHECK(editor->draftImages().isEmpty()&&transport->property("receivedMessages").toInt()==beforeDraft+1);
  CHECK(QImage(preview.property("lastTestReceivedFile").toString())==draftImage);

  QToolButton* formatToggle=chat->findChild<QToolButton*>("ShadowFormatToggle");
  QToolBar* formatBar=chat->findChild<QToolBar*>("ShadowComposerFormatBar");
  CHECK(formatToggle&&formatBar&&!formatBar->isVisible());
  formatToggle->click();
  CHECK(formatBar->isVisible()&&editor->property("shadowFormattingMode").toBool());
  editor->setPlainText(QStringLiteral("格式发送验收"));
  editor->selectAll();
  formatBar->findChild<QAction*>("FormatBold")->trigger();
  formatBar->findChild<QAction*>("FormatHeading2")->trigger();
  formatBar->findChild<QAction*>("FormatColor1")->trigger();
  formatBar->findChild<QAction*>("FormatHighlight2")->trigger();
  CHECK(editor->richText().contains("<h2>")&&editor->richText().contains("<b>")&&editor->richText().contains("background-color"));
  UserManager::instance().setUser(denied);
  send->click();
  CHECK(editor->message()==QStringLiteral("格式发送验收")&&editor->richText().contains("<h2>"));
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88002)));
  CHECK(!editor->richText().contains(QStringLiteral("格式发送验收")));
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,88001)));
  CHECK(editor->richText().contains("<h2>")&&editor->richText().contains("background-color"));
  UserManager::instance().setUser(peer);
  send->click();
  CHECK(editor->message().isEmpty());
  bool richDelivered=false;
  const Chat sent=ChatManager::instance().chat(88001);
  for(const ChatMessage& message:sent.messages())
    if(message.isFromLocalUser()&&message.message().contains(QStringLiteral("格式发送验收")))
      richDelivered=message.message().contains("<h2>")&&message.message().contains("<b>")&&message.message().contains("background-color");
  CHECK(richDelivered);
  QTextCursor replySource=transcript->document()->find(QStringLiteral("格式发送验收"));
  CHECK(!replySource.isNull());
  transcript->setTextCursor(replySource);
  transcript->ensureCursorVisible();
  CHECK(QMetaObject::invokeMethod(chat,"replyToMessage",Qt::DirectConnection,
    Q_ARG(QPoint,transcript->cursorRect(replySource).center())));
  editor->setPlainText(QStringLiteral("格式引用回复验收"));
  editor->selectAll();
  QTextCharFormat replyFormat;
  replyFormat.setFontWeight(QFont::Bold);
  QTextCursor replyCursor=editor->textCursor();
  replyCursor.mergeCharFormat(replyFormat);
  send->click();
  bool styledReply=false;
  for(const ChatMessage& message:ChatManager::instance().chat(88001).messages())
    if(message.isFromLocalUser()&&message.message().contains(QStringLiteral("格式引用回复验收")))
    {
      styledReply=message.message().contains("<blockquote>")&&message.message().contains("<b>");
    }
  CHECK(styledReply&&!chat->findChild<QWidget*>("ShadowReplyPreview")->isVisible());
  CHECK(transcript->toHtml().contains("#9abde7",Qt::CaseInsensitive));
  editor->setPlainText(QStringLiteral("链接验证"));
  editor->selectAll();
  QTimer::singleShot(50,editor,[]() {
    if(QInputDialog* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
    {dialog->setTextValue("https://example.test/format");dialog->accept();}
  });
  formatBar->findChild<QAction*>("FormatLink")->trigger();
  CHECK(editor->richText().contains("href=\"https://example.test/format\""));
  const int beforeKeyboard=transport->property("receivedMessages").toInt();
  QTextCursor end=editor->textCursor();
  end.movePosition(QTextCursor::End);
  editor->setTextCursor(end);
  QKeyEvent newline(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier,"\r");
  QApplication::sendEvent(editor,&newline);
  CHECK(editor->document()->blockCount()==2&&transport->property("receivedMessages").toInt()==beforeKeyboard);
  editor->insertPlainText(QStringLiteral("第二行"));
  QKeyEvent keyboardSend(QEvent::KeyPress,Qt::Key_Return,Qt::ControlModifier,"\r");
  QApplication::sendEvent(editor,&keyboardSend);
  CHECK(editor->toPlainText().isEmpty()&&transport->property("receivedMessages").toInt()==beforeKeyboard+1);
  editor->setPlainText("one\ntwo");
  editor->selectAll();
  formatBar->findChild<QAction*>("FormatNumberedList")->trigger();
  CHECK(editor->richText().contains("<ol>"));
  formatBar->findChild<QAction*>("FormatCode")->trigger();
  CHECK(editor->richText().contains("<code>"));
  CHECK(window.grab().save(QDir::currentPath()+"/ui-069-composer-formatting.png"));
  QToolButton* highlight= formatBar->findChild<QToolButton*>("FormatHighlight");
  CHECK(highlight&&highlight->menu());
  QMenu* highlightMenu=highlight->menu();
  highlightMenu->popup(highlight->mapToGlobal(QPoint(0,-highlightMenu->sizeHint().height())));
  QApplication::processEvents();
  CHECK(highlightMenu->width()>highlightMenu->height()*2);
  CHECK(highlightMenu->grab().save(QDir::currentPath()+"/ui-069-highlight-palette.png"));
  highlightMenu->hide();
  const Message legacy=Protocol::instance().chatMessage(sent,"readable body","<b>readable body</b>");
  CHECK(legacy.text()=="readable body"&&Protocol::instance().dataFromChatMessage(legacy).richText()=="<b>readable body</b>");
  CHECK(ChatMessage(87001,legacy,ChatMessage::Chat,true).message().contains("<b>"));
  const Message unsafe=Protocol::instance().chatMessage(sent,"readable body","<img src=\"file:///must-not-load.png\"><a href=\"javascript:alert(1)\">readable body</a>");
  const QString safe=ChatMessage(87001,unsafe,ChatMessage::Chat,true).message();
  CHECK(!safe.contains("<img")&&!safe.contains("javascript:")&&!safe.contains("file:///"));
  const Message mismatch=Protocol::instance().chatMessage(sent,"readable body","<b>spoofed content</b>");
  CHECK(!ChatMessage(87001,mismatch,ChatMessage::Chat,true).message().contains("spoofed"));
  QAction* autoHide=chat->findChild<QAction*>("ShadowCaptureAutoHide");
  CHECK(autoHide&&autoHide->isCheckable()&&chat->findChild<QAction*>("ShadowCaptureScreenshot")&&chat->findChild<QAction*>("ShadowRecordScreen"));
  autoHide->setChecked(false);
  CHECK(!QSettings(QDir(Settings::instance().dataFolder()).filePath("ui-preferences.ini"),QSettings::IniFormat).value("capture/autoHide").toBool());
  autoHide->setChecked(true);
  if(QGuiApplication::platformName()=="offscreen")
  {
    for(bool hide:{false,true})
    {
      autoHide->setChecked(hide);
      bool captureShown=false;
      bool expectedVisibility=false;
      QTimer::singleShot(300,chat,[&]() {
        if(QDialog* capture=qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        {
          captureShown=true;
          expectedVisibility=window.isVisible()!=hide;
          capture->reject();
        }
      });
      chat->findChild<QAction*>("ShadowCaptureScreenshot")->trigger();
      settle();
      CHECK(captureShown&&expectedVisibility&&window.isVisible());
    }
  }
  QMenu* captureMenu=chat->findChild<QMenu*>("ShadowCaptureMenu");
  CHECK(captureMenu);
  QWidget* captureButton=captureMenu->parentWidget();
  captureMenu->popup(captureButton->mapToGlobal(QPoint(0,-captureMenu->sizeHint().height())));
  QApplication::processEvents();
  CHECK(captureMenu->grab().save(QDir::currentPath()+"/ui-070-capture-menu.png"));
  captureMenu->hide();
  CHECK(chat->findChild<QToolButton*>("ShadowComposerMedia"));
  window.resize(900,700);
  settle();
  CHECK(send->isVisible()&&send->parentWidget()->rect().contains(send->geometry()));
  CHECK(formatBar->isVisible()&&formatBar->parentWidget()->rect().contains(formatBar->geometry()));
  CHECK(window.grab().save(QDir::currentPath()+"/ui-069-narrow-composer.png"));
  window.resize(1120,820);
  editor->clearMessage();
  formatToggle->click();
  CHECK(!formatBar->isVisible());
  std::puts("PASS composer: anchored nonmodal sticker popup/search/recent/outside/Escape, original-byte send, precise chat/draft image favorites with independent copies, per-chat image/rich drafts and failed retry, TCP rich rendering/styled reply/link and readable legacy text, safe formatting metadata, list/code, Enter/Ctrl+Enter, horizontal palettes/narrow layout, capture menu preference and media entry");
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
      storedQuote=(message.message().contains("bee-quote")||message.message().contains("<blockquote>"))&&message.message().contains(QStringLiteral("林舟"));
  CHECK(storedQuote&&transcript->toPlainText().contains(QStringLiteral("引用回复发送验收")));
  CHECK(transcript->toHtml().contains("#9abde7",Qt::CaseInsensitive));
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
  settle();
  QTextTable* echoBody=Q_NULLPTR;
  for(QTextFrame* frame=echo.currentFrame();frame;frame=frame->parentFrame())
    if(QTextTable* table=qobject_cast<QTextTable*>(frame))
      if(table->frameFormat().hasProperty(ShadowChatTranscript::BubbleColorProperty))echoBody=table;
  CHECK(echoBody);
  const QRectF echoRectangle=transcript->document()->documentLayout()->frameBoundingRect(echoBody);
  const QTextTableCellFormat echoFormat=echoBody->cellAt(0,0).format().toTableCellFormat();
  const QPoint headingPoint(qRound(echoRectangle.left()-echoFormat.leftPadding()+8),
    qRound(echoRectangle.top()-echoFormat.topPadding()-8-transcript->verticalScrollBar()->value()));
  CHECK(dynamic_cast<ShadowChatTranscript*>(transcript)->messageFrameAt(headingPoint)==outer);
  CHECK(QMetaObject::invokeMethod(chat,"replyToMessage",Qt::DirectConnection,Q_ARG(QPoint,headingPoint)));
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
  GuiChat* liveView=Q_NULLPTR;
  for(GuiChat* candidate:window.findChildren<GuiChat*>())
    if(candidate->isVisible()&&candidate->chatId()==chat.id())liveView=candidate;
  CHECK(liveView);
  GuiMessageEdit* editor=liveView->findChild<GuiMessageEdit*>("GuiMessageEdit");
  ShadowChatTranscript* transcript=liveView->findChild<ShadowChatTranscript*>("GuiChatViewer");
  QWidget* conversationPanel=window.findChild<QWidget*>("ShadowConversationPanel");
  QStackedWidget* conversation=window.findChild<QStackedWidget*>("ShadowConversationStack");
  QWidget* listPanel=window.findChild<QWidget*>("ShadowListPanel");
  QTreeWidget* conversationList=window.findChild<QTreeWidget*>("ShadowConversationList");
  ShadowSearchPanel* globalSearch=window.findChild<ShadowSearchPanel*>("ShadowSearchPanel");
  CHECK(editor&&transcript&&conversationPanel&&conversation&&listPanel&&conversationList&&globalSearch);
  globalSearch->dismiss();
  const GuiMessageEdit::Draft originalDraft=editor->draft();
  const int originalDraftPosition=editor->textCursor().position();
  const int originalDraftAnchor=editor->textCursor().anchor();
  editor->setHtml(QStringLiteral("<p><b>UI044 未发送草稿</b>：查看历史不得改变草稿。</p>"));
  QTextCursor draftCursor=editor->textCursor();
  draftCursor.setPosition(4);
  draftCursor.setPosition(9,QTextCursor::KeepAnchor);
  editor->setTextCursor(draftCursor);
  const QString draft=editor->toHtml();
  const int draftPosition=draftCursor.position();
  const int draftAnchor=draftCursor.anchor();
  const auto draftUnchanged=[&]() {
    QTextDocument expected,actual;
    expected.setDefaultFont(editor->document()->defaultFont());
    actual.setDefaultFont(editor->document()->defaultFont());
    expected.setHtml(draft);
    actual.setHtml(editor->toHtml());
    return expected.toHtml()==actual.toHtml();
  };
  GuiSavedChat* previousHistory=window.findChild<GuiSavedChat*>();
  if(previousHistory)previousHistory->hide();
  settle();
  const QRect conversationBefore=conversation->geometry();
  const QRect panelBefore=conversationPanel->geometry();
  const QRect listBefore=listPanel->geometry();
  const QRect liveBefore=liveView->geometry();
  const bool listVisibleBefore=listPanel->isVisible();
  QWidget* pageBefore=conversation->currentWidget();
  QTreeWidgetItem* selectedConversation=conversationList->currentItem();
  QToolButton* historyButton=Q_NULLPTR;
  for(QToolButton* button:liveView->findChildren<QToolButton*>())
    if(button->isVisible()&&button->accessibleName()==QStringLiteral("聊天记录"))historyButton=button;
  CHECK(historyButton);
  historyButton->click();
  settle();
  GuiSavedChat* history=window.findChild<GuiSavedChat*>();
  CHECK(history&&history->isVisible()&&!history->isWindow()&&history->window()==&window);
  CHECK(!globalSearch->isVisible());
  CHECK(window.size()==QSize(1120,820));
  const auto overlayGeometry=[&]() {
    return history->parentWidget()==conversationPanel&&conversationPanel->layout()->indexOf(history)<0&&
      history->width()==qMin(380,conversationPanel->width())&&
      history->geometry().right()==conversationPanel->rect().right()&&history->y()==0&&
      history->height()==conversationPanel->height()&&
      history->geometry().intersects(conversation->geometry());
  };
  CHECK(overlayGeometry());
  CHECK(conversation->geometry()==conversationBefore&&conversationPanel->geometry()==panelBefore);
  CHECK(listPanel->geometry()==listBefore&&listPanel->isVisible()==listVisibleBefore);
  CHECK(liveView->geometry()==liveBefore&&conversation->currentWidget()==pageBefore);
  CHECK(conversationList->currentItem()==selectedConversation&&draftUnchanged());
  QListWidget* results=history->findChild<QListWidget*>("ShadowHistoryResults");
  QLineEdit* keyword=history->findChild<QLineEdit*>("GuiLineEditFilterHistory");
  QTabBar* categories=history->findChild<QTabBar*>();
  QPushButton* context=history->findChild<QPushButton*>("ShadowHistoryContext");
  QStackedWidget* historyPages=history->findChild<QStackedWidget*>("ShadowHistoryPages");
  QTextBrowser* internalSource=history->findChild<QTextBrowser*>();
  CHECK(results&&results->count()>0&&keyword&&categories&&categories->count()==4&&context);
  CHECK(historyPages&&historyPages->count()==1&&internalSource&&!internalSource->isVisible());
  const QMap<QString,QString> originalHistories=ChatManager::instance().constHistoryMap();
  QTemporaryDir files;
  CHECK(files.isValid());
  QStringList imageUrls;
  for(int index=0;index<6;++index)
  {
    const QString imagePath=files.filePath(QString("UI044-image-%1.png").arg(index));
    QImage image(160,100,QImage::Format_RGB32);
    image.fill(QColor(QStringList{"#8CAFE6","#ADD5C3","#E8C59A","#B8B0D6","#95C5CF","#D5B4C7"}.at(index)));
    CHECK(image.save(imagePath));
    imageUrls.append(QUrl::fromLocalFile(imagePath).toString());
  }
  const QString filePath=files.filePath("UI044-material.txt");
  CHECK(atomicWrite(filePath,"synthetic history attachment"));
  const QDateTime referenceTime(QDate::currentDate(),QTime(12,0));
  const auto record=[&](const QString& sender,const QString& body,int daysAgo) {
    QJsonObject metadata;
    metadata.insert("sender",sender);
    if(daysAgo>=0)metadata.insert("time",referenceTime.addDays(-daysAgo).toString(Qt::ISODate));
    metadata.insert("type",int(ChatMessage::Chat));
    metadata.insert("outgoing",sender==Settings::instance().localUser().name());
    return QString("<!--shadow-record:%1-->%2<br>").arg(
      QString::fromLatin1(QJsonDocument(metadata).toJson(QJsonDocument::Compact).toBase64()),body);
  };
  QString archive=record("Alice","UI044 early message",40);
  archive+=record("Bob","UI044 recent message",0);
  archive+=record("Alice",QString("<a href=\"%1\">UI044-material.txt</a>").arg(QUrl::fromLocalFile(filePath).toString()),0);
  for(int index=0;index<imageUrls.size();++index)
    archive+=record(index%2?"Alice":"Bob",QString("UI044 image <a href=\"%1\"><img src=\"%1\"></a>").arg(imageUrls.at(index)),
      index<3?0:index<5?1:-1);
  archive+=record("Alice","<a href=\"https://example.test/UI044\">UI044-link</a>",0);
  const QString duplicateText=QStringLiteral("UI044 repeated context target");
  const QString duplicate=record("Alice",duplicateText,2);
  archive+=duplicate;
  for(int index=0;index<44;++index)
  {
    if(index==22)archive+=duplicate;
    archive+=record("Bob",QString("UI044 context spacer %1 — %2").arg(index).arg(QString(100,QLatin1Char('x'))),0);
  }
  QMap<QString,QString> saved=originalHistories;
  saved.insert(chat.name(),originalHistories.value(chat.name())+archive);
  const QString archiveOnlyName=QStringLiteral("UI044 archive-only（合成）");
  CHECK(!ChatManager::instance().findChatByName(archiveOnlyName).isValid()&&!saved.contains(archiveOnlyName));
  QString archiveOnly=record("Alice","UI044 archive-only target",3);
  for(int index=0;index<20;++index)
    archiveOnly+=record("Bob",QString("UI044 archive-only spacer %1 — %2").arg(index).arg(QString(80,QLatin1Char('x'))),0);
  saved.insert(archiveOnlyName,archiveOnly);
  ChatManager::instance().addSavedChats(saved);
  history->showSavedChat(chat.name());
  keyword->setText("UI044");
  const QList<HistoryRecord> fixtureRecords=shadowChatHistoryRecords(archive);
  const QList<HistoryRecord> archivedRecords=shadowChatHistoryRecords(saved.value(chat.name()));
  const QStringList archivedKeys=shadowHistoryRecordKeys(archivedRecords);
  CHECK(results->count()==fixtureRecords.size());
  const auto fixtureCount=[&](const QString& selectedSender,bool todayOnly) {
    int count=0;
    for(const HistoryRecord& entry:fixtureRecords)
      if((selectedSender.isEmpty()||entry.sender==selectedSender)&&
         (!todayOnly||(entry.timestamp.isValid()&&entry.timestamp.date()==QDate::currentDate())))++count;
    return count;
  };
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
  CHECK(results->count()==fixtureCount("Alice",false));
  sender->setCurrentIndex(0);
  date->setCurrentIndex(1);
  CHECK(results->count()==fixtureCount(QString(),true));
  date->setCurrentIndex(0);
  categories->setCurrentIndex(1);
  CHECK(results->count()==1&&results->item(0)->text().contains("UI044-material.txt"));
  categories->setCurrentIndex(2);
  settle();
  CHECK(results->viewMode()==QListView::IconMode);
  QSet<QString> expectedMedia;
  QMap<QString,int> expectedDates;
  for(const HistoryRecord& entry:fixtureRecords)
    for(const QString& url:entry.media)
      if(!expectedMedia.contains(url))
      {
        expectedMedia.insert(url);
        ++expectedDates[entry.timestamp.isValid()?entry.timestamp.toString("yyyy-MM-dd"):QStringLiteral("日期未记录")];
      }
  results->doItemsLayout();
  QSet<QString> actualMedia;
  QMap<QString,int> actualDates;
  QSet<QString> dateHeaders;
  QMap<QString,QMap<int,QList<QRect>>> mediaRows;
  QString currentDate;
  for(int row=0;row<results->count();++row)
  {
    QListWidgetItem* item=results->item(row);
    const QRect area=results->visualItemRect(item);
    if(item->data(Qt::UserRole).toInt()<0)
    {
      currentDate=item->text();
      CHECK(expectedDates.contains(currentDate)&&!dateHeaders.contains(currentDate));
      CHECK(!(item->flags()&Qt::ItemIsSelectable)&&area.left()==0&&area.width()>=results->viewport()->width()-2);
      dateHeaders.insert(currentDate);
      continue;
    }
    const int recordIndex=item->data(Qt::UserRole).toInt();
    CHECK(recordIndex>=0&&recordIndex<archivedRecords.size()&&!currentDate.isEmpty());
    const HistoryRecord& entry=archivedRecords.at(recordIndex);
    const QString url=item->data(Qt::UserRole+1).toString();
    CHECK(expectedMedia.contains(url)&&entry.media.contains(url)&&item->text().isEmpty());
    CHECK(currentDate==(entry.timestamp.isValid()?entry.timestamp.toString("yyyy-MM-dd"):QStringLiteral("日期未记录")));
    CHECK(area.width()<=results->viewport()->width()/2+1&&area.width()>=results->viewport()->width()/2-1);
    CHECK(area.left()>=0&&area.right()<results->viewport()->width());
    actualMedia.insert(url);
    ++actualDates[currentDate];
    mediaRows[currentDate][area.top()].append(area);
  }
  CHECK(actualMedia==expectedMedia&&actualDates==expectedDates);
  CHECK(results->count()==expectedMedia.size()+expectedDates.size());
  int pairedRows=0;
  for(auto day=mediaRows.constBegin();day!=mediaRows.constEnd();++day)
    for(const QList<QRect>& row:day.value())
    {
      CHECK(row.size()<=2);
      if(row.size()==2){CHECK(!row.first().intersects(row.last()));++pairedRows;}
    }
  int expectedPairs=0;
  for(int count:expectedDates)expectedPairs+=count/2;
  CHECK(pairedRows==expectedPairs&&results->horizontalScrollBar()->maximum()==0);
  CHECK(window.grab().save(QDir::currentPath()+"/ui-044-history-media-overlay.png"));
  categories->setCurrentIndex(3);
  CHECK(results->count()==1&&results->item(0)->text().contains("example.test"));
  categories->setCurrentIndex(0);
  keyword->setText("UI044-no-match");
  CHECK(results->count()==0&&history->findChild<QLabel*>("ShadowHistoryEmpty")->isVisible());
  CHECK(QMetaObject::invokeMethod(history,"showHistoryContext",Qt::DirectConnection));
  CHECK(history->isVisible()&&!results->currentItem()&&draftUnchanged());
  QList<int> duplicateIndices;
  for(int index=0;index<archivedRecords.size();++index)
    if(archivedRecords.at(index).text==duplicateText)duplicateIndices.append(index);
  CHECK(duplicateIndices.size()==2);
  const QString firstKey=archivedKeys.at(duplicateIndices.first());
  const QString secondKey=archivedKeys.at(duplicateIndices.last());
  CHECK(firstKey!=secondKey&&firstKey.section(':',0,0)==secondKey.section(':',0,0));
  CHECK(firstKey.endsWith(":0")&&secondKey.endsWith(":1"));
  const auto anchorPosition=[](QTextDocument* document,const QString& key) {
    const QString name=QStringLiteral("shadow-context-%1").arg(key);
    for(QTextBlock block=document->begin();block.isValid();block=block.next())
    {
      if(block.charFormat().anchorNames().contains(name))return block.position();
      for(QTextBlock::iterator fragment=block.begin();!fragment.atEnd();++fragment)
      {
        const QTextFragment text=fragment.fragment();
        if(text.isValid()&&text.charFormat().anchorNames().contains(name))return text.position();
      }
    }
    return -1;
  };
  const auto anchorVisible=[&](ShadowChatTranscript* viewer,const QString& key) {
    const int position=anchorPosition(viewer->document(),key);
    if(position<0)return false;
    QTextCursor cursor(viewer->document());
    cursor.setPosition(position);
    const QRect area=viewer->cursorRect(cursor);
    return area.bottom()>=0&&area.top()<viewer->viewport()->height();
  };
  QObject observer;
  QString requestedChat,requestedKey;
  QObject::connect(history,&GuiSavedChat::contextRequested,&observer,
    [&](const QString& name,const QString& key) {requestedChat=name;requestedKey=key;});
  bool contextError=false;
  QTimer contextGuard;
  contextGuard.setSingleShot(true);
  QObject::connect(&contextGuard,&QTimer::timeout,&observer,[&]() {
    if(QMessageBox* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
    {
      contextError=true;
      dialog->reject();
    }
  });
  QList<int> anchorPositions,scrollPositions;
  for(int duplicateIndex:duplicateIndices)
  {
    if(!history->isVisible())historyButton->click();
    keyword->setText(duplicateText);
    settle();
    CHECK(results->count()==2&&overlayGeometry());
    QListWidgetItem* chosen=Q_NULLPTR;
    for(int row=0;row<results->count();++row)
      if(results->item(row)->data(Qt::UserRole).toInt()==duplicateIndex)chosen=results->item(row);
    CHECK(chosen);
    results->setCurrentItem(chosen);
    CHECK(context->isEnabled());
    transcript->verticalScrollBar()->setValue(transcript->verticalScrollBar()->maximum());
    const QString key=archivedKeys.at(duplicateIndex);
    requestedKey.clear();
    contextGuard.start(2000);
    context->click();
    contextGuard.stop();
    settle();
    CHECK(!contextError&&requestedChat==chat.name()&&requestedKey==key);
    CHECK(!history->isVisible()&&historyPages->currentIndex()==0&&!internalSource->isVisible());
    CHECK(liveView->isVisible()&&liveView->property("shadowHistoryContextKey").toString()==key);
    CHECK(anchorVisible(transcript,key)&&transcript->verticalScrollBar()->maximum()>0);
    anchorPositions.append(anchorPosition(transcript->document(),key));
    scrollPositions.append(transcript->verticalScrollBar()->value());
    CHECK(draftUnchanged()&&ChatManager::instance().constHistoryMap()==saved);
    CHECK(editor->textCursor().position()==draftPosition&&editor->textCursor().anchor()==draftAnchor);
    CHECK(conversation->geometry()==conversationBefore&&listPanel->geometry()==listBefore);
    CHECK(listPanel->isVisible()==listVisibleBefore&&conversation->currentWidget()==pageBefore);
  }
  std::fprintf(stdout,"History duplicate positions: anchors=%d/%d scroll=%d/%d\n",
    anchorPositions.first(),anchorPositions.last(),scrollPositions.first(),scrollPositions.last());
  std::fflush(stdout);
  CHECK(anchorPositions.first()<anchorPositions.last()&&scrollPositions.first()<scrollPositions.last());
  CHECK(anchorPosition(transcript->document(),firstKey)>=0&&anchorPosition(transcript->document(),secondKey)>=0);
  CHECK(window.grab().save(QDir::currentPath()+"/ui-044-history-main-context.png"));
  const QString locatedHtml=transcript->document()->toHtml();
  const int locatedScroll=transcript->verticalScrollBar()->value();
  CHECK(!liveView->showHistoryRecord(chat.name(),QString(64,QLatin1Char('0'))+":999"));
  CHECK(transcript->document()->toHtml()==locatedHtml&&transcript->verticalScrollBar()->value()==locatedScroll);
  CHECK(liveView->property("shadowHistoryContextKey").toString()==secondKey&&draftUnchanged());
  CHECK(ChatManager::instance().constHistoryMap()==saved);
  historyButton->click();
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
  QToolButton* close=history->findChild<QToolButton*>("ShadowHistoryClose");
  QToolButton* headerSearch=window.findChild<QToolButton*>("ShadowHeaderAction");
  CHECK(close&&headerSearch);
  close->click();
  for(int width:{1120,900})
  {
    window.resize(width,740);
    settle();
    const QRect beforeConversation=conversation->geometry();
    const QRect beforePanel=conversationPanel->geometry();
    const QRect beforeList=listPanel->geometry();
    const bool beforeListVisible=listPanel->isVisible();
    const bool beforeConversationVisible=conversation->isVisible();
    QWidget* beforePage=conversation->currentWidget();
    QTreeWidgetItem* beforeSelected=conversationList->currentItem();
    const QList<QTreeWidgetItem*> beforeSelection=conversationList->selectedItems();
    headerSearch->click();
    settle();
    CHECK(window.width()==width&&history->isVisible()&&overlayGeometry());
    CHECK(conversation->geometry()==beforeConversation&&conversationPanel->geometry()==beforePanel);
    CHECK(listPanel->geometry()==beforeList&&listPanel->isVisible()==beforeListVisible);
    CHECK(conversation->isVisible()==beforeConversationVisible&&conversation->currentWidget()==beforePage);
    CHECK(conversationList->currentItem()==beforeSelected&&conversationList->selectedItems()==beforeSelection);
    if(!draftUnchanged()) {
      atomicWrite(QDir::currentPath()+"/history-draft-before.html",draft.toUtf8());
      atomicWrite(QDir::currentPath()+"/history-draft-after.html",editor->toHtml().toUtf8());
      std::fprintf(stdout,"History draft mismatch at width=%d, history windows=%d\n",width,window.findChildren<GuiSavedChat*>().size());
      std::fflush(stdout);
    }
    CHECK(draftUnchanged()&&window.findChildren<GuiSavedChat*>().size()==1);
    CHECK(window.grab().save(QDir::currentPath()+QString("/ui-044-history-overlay-%1.png").arg(width)));
    if(width==900)CHECK(window.grab().save(QDir::currentPath()+"/ui-044-history-narrow.png"));
    close->click();
    settle();
    CHECK(!history->isVisible()&&conversation->geometry()==beforeConversation);
    CHECK(listPanel->geometry()==beforeList&&listPanel->isVisible()==beforeListVisible);
    CHECK(conversationList->currentItem()==beforeSelected&&conversationList->selectedItems()==beforeSelection);
  }
  window.resize(1120,820);
  settle();
  headerSearch->click();
  settle();
  QKeyEvent openGlobal(QEvent::KeyPress,Qt::Key_K,Qt::ControlModifier);
  QApplication::sendEvent(&window,&openGlobal);
  settle();
  CHECK(globalSearch->isVisible()&&globalSearch->findChild<QLineEdit*>("ShadowSearchInput")->hasFocus());
  headerSearch->click();
  settle();
  CHECK(history->isVisible()&&!globalSearch->isVisible()&&overlayGeometry());
  QList<VNumber> chatsBeforeArchive;
  for(const Chat& current:ChatManager::instance().constChatList())chatsBeforeArchive.append(current.id());
  CHECK(QMetaObject::invokeMethod(&window,"showSavedChatSelected",Qt::DirectConnection,Q_ARG(QString,archiveOnlyName)));
  settle();
  CHECK(history->isVisible()&&history->savedChatName()==archiveOnlyName);
  keyword->setText("UI044 archive-only target");
  CHECK(results->count()==1);
  results->setCurrentRow(0);
  const QList<HistoryRecord> onlyRecords=shadowChatHistoryRecords(archiveOnly);
  const QString onlyKey=shadowHistoryRecordKeys(onlyRecords).first();
  contextGuard.start(2000);
  context->click();
  contextGuard.stop();
  settle();
  GuiChat* archivedView=window.findChild<GuiChat*>("ShadowArchivedContextChat");
  QWidget* archivePage=window.findChild<QWidget*>("ShadowArchivedContextPage");
  CHECK(!contextError&&archivedView&&archivePage&&conversation->currentWidget()==archivePage);
  CHECK(!history->isVisible()&&archivedView->isVisible()&&requestedChat==archiveOnlyName&&requestedKey==onlyKey);
  ShadowChatTranscript* archivedTranscript=archivedView->findChild<ShadowChatTranscript*>("GuiChatViewer");
  GuiMessageEdit* archivedEditor=archivedView->findChild<GuiMessageEdit*>("GuiMessageEdit");
  CHECK(archivedTranscript&&archivedTranscript->isReadOnly()&&archivedEditor&&!archivedEditor->isVisible());
  CHECK(anchorVisible(archivedTranscript,onlyKey)&&archivedView->property("shadowHistoryContextKey").toString()==onlyKey);
  CHECK(!ChatManager::instance().findChatByName(archiveOnlyName).isValid());
  QList<VNumber> chatsAfterArchive;
  for(const Chat& current:ChatManager::instance().constChatList())chatsAfterArchive.append(current.id());
  CHECK(chatsAfterArchive==chatsBeforeArchive&&ChatManager::instance().constHistoryMap()==saved);
  CHECK(draftUnchanged());
  CHECK(window.grab().save(QDir::currentPath()+"/ui-044-history-archive-context.png"));
  const Chat afterContext=ChatManager::instance().chat(chat.id());
  CHECK(afterContext.messages().size()==chat.messages().size());
  for(int index=0;index<chat.messages().size();++index)
  {
    const ChatMessage& before=chat.messages().at(index);
    const ChatMessage& after=afterContext.messages().at(index);
    CHECK(before.message()==after.message()&&before.type()==after.type()&&before.timestamp()==after.timestamp()&&
      before.userId()==after.userId());
  }
  ChatManager::instance().addSavedChats(originalHistories);
  CHECK(QMetaObject::invokeMethod(&window,"showChat",Qt::DirectConnection,Q_ARG(VNumber,chat.id())));
  CHECK(liveView->setChat(ChatManager::instance().chat(chat.id())));
  editor->setDraft(originalDraft);
  QTextCursor restoreDraft=editor->textCursor();
  restoreDraft.setPosition(originalDraftAnchor);
  restoreDraft.setPosition(originalDraftPosition,QTextCursor::KeepAnchor);
  editor->setTextCursor(restoreDraft);
  CHECK(ChatManager::instance().constHistoryMap()==originalHistories);
  std::puts("PASS embedded history integration: overlay preserves 1120/900 conversation and list bounds/state, real media counts with full-width date headers and two columns, exact duplicate occurrence anchors and transcript scroll, draft/archive/live messages unchanged, missing key rejected, read-only archive context without fake manager chat, Ctrl+K search preserved");
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
  if( qEnvironmentVariableIsEmpty( "QT_QPA_PLATFORM" ) )
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
  if(app.arguments().contains("--composer-only"))
  {
    const int result=verifyComposer(window,preview);
    core.stop();
    return result;
  }
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
    CHECK( submission.findChild<QLabel*>( "ShadowSendFeedback" )->isHidden() );
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
