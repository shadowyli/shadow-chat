#include "BeeApplication.h"
#include "ChatManager.h"
#include "GuiChatItem.h"
#include "IconManager.h"
#include "Message.h"
#include "MessageManager.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowConversationDelegate.h"
#include "UserManager.h"
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static ChatMessage message( VNumber author, ChatMessage::Type type, const QString& html, const QDateTime& timestamp )
{
  Message wire( Message::System, 991, QString() );
  wire.setText( html );
  wire.setTimestamp( timestamp );
  return ChatMessage( author, wire, type, true );
}

static void settle()
{
  for( int iteration = 0; iteration < 4; ++iteration ) QApplication::processEvents();
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  Random::init();
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "notice-preview-fixture" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  User peer( 97221 );
  peer.setName( QStringLiteral( "测试同事" ) );
  peer.setHash( "conversation-notice-synthetic-peer" );
  peer.setStatus( User::Offline );
  UserManager::instance().setUser( peer );
  Group group;
  group.setId( 98221 );
  group.setName( "notice-preview-chat" );
  group.setChatType( Group::PrivateChat );
  group.addUser( ID_LOCAL_USER );
  group.addUser( peer.id() );
  Chat initial;
  initial.setGroup( group );
  const QDateTime base( QDate( 2026, 11, 2 ), QTime( 9, 0 ) );
  const QString visible = QStringLiteral( "请查收设计说明" );
  initial.addMessage( message( peer.id(), ChatMessage::Chat, visible, base ) );
  initial.setLastMessageTimestamp( base.addSecs( 600 ) );
  initial.addUnreadMessage();
  QTreeWidget list;
  list.setHeaderHidden( true );
  list.setRootIsDecorated( false );
  list.setIndentation( 0 );
  list.setItemDelegate( new ShadowConversationDelegate( &list ) );
  list.setStyleSheet( "QTreeWidget { background:white; border:0; outline:0; }" );
  list.resize( 380, 560 );
  GuiChatItem item( &list );
  item.setChatId( initial.id() );
  const QString legacyIcon = QStringLiteral( "<img src=\":/images/unsent-message.png\">" );
  const QString modernIcon = IconManager::instance().toHtml( "unsent-message.png", "*m*" );
  const QStringList routine = QStringList()
    << legacyIcon + QStringLiteral( "消息即将发送给 测试同事。" )
    << QStringLiteral( "<img src=\":/images/unsent-message.svg\">离线消息将发送给 测试同事。" )
    << modernIcon + QStringLiteral( "The message will be delivered to test peer." )
    << modernIcon + QStringLiteral( "The voice message will be delivered to test peer." )
    << QStringLiteral( "Offline messages will be sent to test peer." )
    << QStringLiteral( "离线消息已发送给 测试同事。" )
    << QStringLiteral( "<!--shadow-delivery-notice-->已进入正常发送流程" )
    << QStringLiteral( "消息“设计说明”：已送达 0/2 人" )
    << QStringLiteral( "消息“失败原因已修复”：已送达 2/2 人" );
  int cases = 0;
  for( ChatMessage::Type type : { ChatMessage::Other, ChatMessage::UserInfo, ChatMessage::System } )
    for( const QString& notice : routine )
    {
      Chat chat = initial;
      chat.addMessage( message( ID_SYSTEM_MESSAGE, type, notice, base.addSecs( 60 ) ) );
      chat.addMessage( message( ID_SYSTEM_MESSAGE, ChatMessage::Header, "header", base.addSecs( 120 ) ) );
      CHECK( item.updateItem( chat ) );
      CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == visible );
      CHECK( item.lastMessageTimestamp() == base );
      CHECK( item.toolTip( 0 ).endsWith( visible ) );
      CHECK( item.unreadMessages() == initial.unreadMessages() );
      CHECK( chat.messages().size() == 3 && chat.messages().at( 1 ).message() == notice );
      ++cases;
    }
  const QStringList failures = QStringList()
    << legacyIcon + QStringLiteral( "无法发送离线消息，连接失败。" )
    << QStringLiteral( "<!--shadow-delivery-notice-->发送失败，请重试。" )
    << modernIcon + QStringLiteral( "Unable to send the message to test peer." )
    << QStringLiteral( "Nobody has received the message." )
    << QStringLiteral( "Unsent messages had an incorrect authorization code and will not be sent." );
  for( ChatMessage::Type type : { ChatMessage::Other, ChatMessage::UserInfo, ChatMessage::System } )
    for( const QString& failure : failures )
    {
      Chat chat = initial;
      chat.addMessage( message( ID_SYSTEM_MESSAGE, type, failure, base.addSecs( 60 ) ) );
      CHECK( item.updateItem( chat ) );
      CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() != visible );
      CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString().contains( "send", Qt::CaseInsensitive ) ||
        item.data( 0, GuiChatItem::ChatPreview ).toString().contains( "sent", Qt::CaseInsensitive ) ||
        item.data( 0, GuiChatItem::ChatPreview ).toString().contains( "received", Qt::CaseInsensitive ) ||
        item.data( 0, GuiChatItem::ChatPreview ).toString().contains( QStringLiteral( "失败" ) ) );
      CHECK( item.lastMessageTimestamp() == base.addSecs( 60 ) );
      ++cases;
    }
  for( VNumber author : QList<VNumber>{ ID_LOCAL_USER, peer.id() } )
    for( const QString& text : routine + failures )
    {
      Chat chat = initial;
      chat.addMessage( message( author, ChatMessage::Chat, text, base.addSecs( 60 ) ) );
      CHECK( item.updateItem( chat ) );
      QTextDocument expected;
      expected.setHtml( text );
      const QString prefix = author == ID_LOCAL_USER ? QStringLiteral( "我：" ) : QString();
      CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() ==
        prefix + expected.toPlainText().remove( QChar::ObjectReplacementCharacter ).simplified() );
      CHECK( item.lastMessageTimestamp() == base.addSecs( 60 ) );
      ++cases;
    }
  for( ChatMessage::Type type : { ChatMessage::Other, ChatMessage::UserInfo } )
  {
    Chat chat = initial;
    const QString literal = QStringLiteral( "离线消息将发送给 测试同事。" );
    chat.addMessage( message( peer.id(), type, literal, base.addSecs( 60 ) ) );
    CHECK( item.updateItem( chat ) );
    CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == literal );
    ++cases;
  }
  Chat onlyNotices;
  onlyNotices.setGroup( group );
  for( const QString& notice : routine )
    onlyNotices.addMessage( message( ID_SYSTEM_MESSAGE, ChatMessage::Other, notice, base ) );
  CHECK( item.updateItem( onlyNotices ) );
  CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == QStringLiteral( "暂无消息" ) );
  CHECK( !item.lastMessageTimestamp().isValid() );
  Chat chain = initial;
  for( const QString& notice : routine )
    chain.addMessage( message( ID_SYSTEM_MESSAGE, ChatMessage::Other, notice, base.addSecs( 90 ) ) );
  CHECK( item.updateItem( chain ) );
  CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == visible );
  const QString file = QStringLiteral( "<a href=\"file:///synthetic/design.txt\">design.txt</a>" );
  const QString sticker = QStringLiteral( "<!--shadow-sticker--><img src=\"file:///synthetic/sticker.png\">" );
  struct Attachment { ChatMessage::Type type; QString html; QString preview; };
  const Attachment attachments[] = {
    { ChatMessage::FileTransfer, file, QStringLiteral( "[文件] design.txt" ) },
    { ChatMessage::ImagePreview, sticker, QStringLiteral( "[表情]" ) },
    { ChatMessage::Chat, QStringLiteral( "<img src=\":/emoticons/1f600.png\">" ), QStringLiteral( "[表情]" ) },
    { ChatMessage::ImagePreview, QStringLiteral( "<img src=\"file:///synthetic/photo.png\">" ), QStringLiteral( "[图片]" ) },
    { ChatMessage::Voice, "voice", QStringLiteral( "[语音]" ) }
  };
  for( const Attachment& attachment : attachments )
  {
    Chat chat = initial;
    chat.addMessage( message( peer.id(), attachment.type, attachment.html, base.addSecs( 60 ) ) );
    chat.addMessage( message( ID_SYSTEM_MESSAGE, ChatMessage::Other, routine.first(), base.addSecs( 90 ) ) );
    CHECK( item.updateItem( chat ) );
    CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == attachment.preview );
    ++cases;
  }
  Message outgoing( Message::Chat, 98222, QString() );
  outgoing.setText( visible );
  MessageManager::instance().addMessageToSend( peer.id(), initial.id(), outgoing );
  CHECK( item.updateItem( chain ) );
  CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == QStringLiteral( "[待发送] " ) + visible );
  CHECK( item.data( 0, GuiChatItem::ChatQueued ).toInt() == 1 && item.onlineUsers() == 0 );
  const qint64 offlineIcon = item.icon( 0 ).cacheKey();
  const QString queuedPreview = item.data( 0, GuiChatItem::ChatPreview ).toString();
  list.show();
  settle();
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR", QDir::currentPath() );
  QDir().mkpath( proof );
  CHECK( list.grab().save( proof + "/conversation-queued.png" ) );
  QElapsedTimer idle;
  idle.start();
  while( idle.elapsed() < 1200 )
  {
    application.processEvents();
    QThread::msleep( 10 );
  }
  CHECK( item.icon( 0 ).cacheKey() == offlineIcon );
  CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == queuedPreview );
  CHECK( MessageManager::instance().removeUnsentMessages( peer.id(), initial.id() ) == 1 );
  CHECK( item.updateItem( chain ) );
  CHECK( item.data( 0, GuiChatItem::ChatPreview ).toString() == visible );
  CHECK( item.data( 0, GuiChatItem::ChatQueued ).toInt() == 0 );
  CHECK( list.grab().save( proof + "/conversation-cleared.png" ) );
  for( const Attachment& attachment : attachments )
  {
    Chat chat = initial;
    chat.addMessage( message( peer.id(), attachment.type, attachment.html, base ) );
    chat.addMessage( message( ID_SYSTEM_MESSAGE, ChatMessage::Other, routine.first(), base.addSecs( 90 ) ) );
    GuiChatItem* row = new GuiChatItem( &list );
    row->setChatId( initial.id() );
    CHECK( row->updateItem( chat ) );
  }
  Chat failed = initial;
  failed.addMessage( message( ID_SYSTEM_MESSAGE, ChatMessage::System,
    QStringLiteral( "发送失败，请重试。" ), base.addSecs( 60 ) ) );
  GuiChatItem* failureRow = new GuiChatItem( &list );
  failureRow->setChatId( initial.id() );
  CHECK( failureRow->updateItem( failed ) );
  settle();
  CHECK( list.grab().save( proof + "/conversation-notice-filtered.png" ) );
  std::printf( "PASS %s DPR %.2f: %d notice/failure/user-text/attachment cases, visible timestamp, notice chain fallback, queue clear, static offline icon and original messages preserved\n",
    qPrintable( QApplication::platformName() ), list.devicePixelRatioF(), cases );
  return 0;
}
