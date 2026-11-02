#include "BeeApplication.h"
#include "BeeUtils.h"
#include "BuildSavedChatList.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "GuiChatMessage.h"
#include "MessageManager.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowImageAlbum.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QtWidgets>
#include <QTextTable>
#include <algorithm>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

class HistoryTestChat : public GuiChat
{
public:
  using GuiChat::modernBubbleHtml;
  using GuiChat::submitImage;
};

struct Bubble
{
  QTextTable* row;
  QTextTable* body;
  QString text;
};

static QList<Bubble> bubbles( ShadowChatTranscript* transcript )
{
  QList<Bubble> result;
  QList<QTextFrame*> frames = transcript->document()->rootFrame()->childFrames();
  for( int index = 0; index < frames.size(); ++index )
    frames.append( frames.at( index )->childFrames() );
  for( QTextFrame* frame : frames )
  {
    QTextTable* body = qobject_cast<QTextTable*>( frame );
    if( !body || !body->format().hasProperty( ShadowChatTranscript::BubbleColorProperty ) )
      continue;
    QTextTable* row = qobject_cast<QTextTable*>( body->parentFrame() );
    QTextCursor cursor( transcript->document() );
    cursor.setPosition( body->firstPosition() );
    cursor.setPosition( body->lastPosition(), QTextCursor::KeepAnchor );
    result.append( Bubble{ row, body, cursor.selection().toPlainText() } );
  }
  std::sort( result.begin(), result.end(), []( const Bubble& left, const Bubble& right ) {
    return left.body->firstPosition() < right.body->firstPosition();
  } );
  return result;
}

static Chat conversation()
{
  Group group;
  group.setId( 96301 );
  group.setName( QStringLiteral( "Synthetic history bubbles" ) );
  group.setChatType( Group::GroupChat );
  group.addUser( ID_LOCAL_USER );
  group.addUser( 96302 );
  Chat chat;
  chat.setGroup( group );
  return chat;
}

static QString marked( const QString& sender, const QDateTime& timestamp, const QString& html,
                       int type = ChatMessage::Chat )
{
  QJsonObject metadata;
  metadata.insert( "sender", sender );
  metadata.insert( "time", timestamp.toString( Qt::ISODate ) );
  metadata.insert( "type", type );
  return QStringLiteral( "<!--shadow-record:%1-->%2" ).arg(
    QString::fromLatin1( QJsonDocument( metadata ).toJson( QJsonDocument::Compact ).toBase64() ), html );
}

static QByteArray bytes( const QString& path )
{
  QFile file( path );
  return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

static void settle( QApplication& application )
{
  for( int iteration = 0; iteration < 5; ++iteration )
    application.processEvents();
}

static int seed( QApplication& application, const QString& root )
{
  Settings& settings = Settings::instance();
  const QString imagePath = QDir( root ).filePath( "beeimgtmp-20261102-101350.png" );
  const QString secondPath = QDir( root ).filePath( QStringLiteral( "original & 第二张.png" ) );
  const QString filePath = QDir( root ).filePath( "brief & layout.psd" );
  QImage original( 900, 600, QImage::Format_RGB32 );
  original.fill( QColor( "#417BC1" ) );
  CHECK( original.save( imagePath ) && original.save( secondPath ) );
  QFile attachment( filePath );
  CHECK( attachment.open( QIODevice::WriteOnly ) );
  CHECK( attachment.write( "synthetic attachment, preserve bytes" ) > 0 );
  attachment.close();
  const QString imageUrl = QUrl::fromLocalFile( imagePath ).toString().toHtmlEscaped();
  const QString secondUrl = QUrl::fromLocalFile( secondPath ).toString().toHtmlEscaped();
  const QString fileUrl = QUrl::fromLocalFile( filePath ).toString().toHtmlEscaped();
  QString archive = QStringLiteral(
    "<font color='#888888'>(2026-11-02 10:12:03)</font> <b>local fixture</b>: legacy outgoing<br>"
    "legacy continuation<br><span class='bee-quote'>quoted first<br>"
    "(2026-10-01 09:00:00) quoted author: retained quote</span><br>"
    "<font color='#888888'>(2026-11-02 10:13:50)</font> <b>离线旧成员</b>: "
    "<a href=\"%1\"><img src=\"%1\" width='18' height='12'></a><br>"
    "<a href=\"%1\">beeimgtmp-20261102-101350.png</a><br>image caption retained<br>"
    "(2026-11-02 10:14:00) 离线旧成员: legacy attachment <a href=\"%2\">brief &amp; layout.psd</a><br>"
    "<a href='https://example.invalid/?a=1&amp;b=2'>query link</a><br>"
    "(2026-11-02 10:15:00) 已断开连接<br>" ).arg( imageUrl, fileUrl );
  User peer( 96302 );
  peer.setName( QStringLiteral( "离线新成员" ) );
  peer.setHash( "synthetic-history-peer" );
  UserManager::instance().setUser( peer );
  Chat live = conversation();
  const QDateTime timestamp( QDate( 2026, 11, 2 ), QTime( 11, 0, 1 ) );
  Message text( Message::Chat, 96310, QStringLiteral( "saved local text<br>second saved line" ) );
  text.setTimestamp( timestamp );
  live.addMessage( ChatMessage( ID_LOCAL_USER, text, ChatMessage::Chat, true ) );
  text.setText( QStringLiteral( "saved incoming <b>formatted</b><br><span class='bee-quote'>new quoted content</span>" ) );
  text.setTimestamp( timestamp.addSecs( 1 ) );
  live.addMessage( ChatMessage( peer.id(), text, ChatMessage::Chat, true ) );
  for( const QString& address : QStringList{ imageUrl, secondUrl } )
  {
    Message image = Protocol::instance().systemMessage( QStringLiteral(
      "<!--shadow-sender:%1--><!--shadow-album:11111111-1111-1111-1111-111111111111-->"
      "<br><div align='center'><a href=\"%2\"><img src=\"%2\" width='20' height='14'></a></div>" )
      .arg( QString::fromLatin1( settings.localUser().name().toUtf8().toBase64() ), address ) );
    image.setTimestamp( timestamp.addSecs( 2 ) );
    live.addMessage( ChatMessage( ID_SYSTEM_MESSAGE, image, ChatMessage::ImagePreview, true ) );
  }
  Message file = Protocol::instance().systemMessage( QStringLiteral(
    "<!--shadow-attachment--><!--shadow-sender:%1-->file card <a href=\"%2\">brief &amp; layout.psd</a>" )
    .arg( QString::fromLatin1( peer.name().toUtf8().toBase64() ), fileUrl ) );
  file.setTimestamp( timestamp.addSecs( 3 ) );
  live.addMessage( ChatMessage( ID_SYSTEM_MESSAGE, file, ChatMessage::FileTransfer, true ) );
  archive += GuiChatMessage::chatToHtml( live, false, false, true, true, true, true );
  HistoryTestChat renderer;
  const QString native = renderer.modernBubbleHtml(
    "<table><tr><td>native saved bubble</td></tr><tr><td>native nested quote</td></tr></table>",
    peer, false, timestamp.addSecs( 4 ), 280, "saved-native-id" );
  archive += marked( peer.name(), timestamp.addSecs( 4 ),
    "<table width='100%'><tr><td>" + native + "</td></tr></table>" );
  const QString duplicate = marked( peer.name(), timestamp.addSecs( 5 ), "<p>exact duplicate</p>" );
  archive += duplicate + duplicate;
  QMap<QString,QString> histories;
  histories.insert( live.name(), archive );
  ChatManager::instance().addSavedChats( histories );
  CHECK( MessageManager::instance().saveMessages( true ) );
  settings.save();
  CHECK( !bytes( settings.savedChatsFilePath() ).isEmpty() );
  settle( application );
  std::puts( "PASS seed: real history writer, legacy HTML, native nested HTML, attachments, album, duplicate messages" );
  return 0;
}

static int verify( QApplication& application, const QString& root )
{
  Settings& settings = Settings::instance();
  const QByteArray archiveBefore = bytes( settings.savedChatsFilePath() );
  const QString imagePath = QDir( root ).filePath( "beeimgtmp-20261102-101350.png" );
  const QString secondPath = QDir( root ).filePath( QStringLiteral( "original & 第二张.png" ) );
  const QString filePath = QDir( root ).filePath( "brief & layout.psd" );
  const QByteArray imageBefore = bytes( imagePath );
  const QByteArray secondBefore = bytes( secondPath );
  const QByteArray fileBefore = bytes( filePath );
  CHECK( !archiveBefore.isEmpty() && !imageBefore.isEmpty() && !fileBefore.isEmpty() );
  BuildSavedChatList loader;
  loader.buildList();
  ChatManager::instance().addSavedChats( loader.savedChats() );
  const QMap<QString,QString> loaded = ChatManager::instance().constHistoryMap();
  CHECK( loaded.contains( conversation().name() ) );
  CHECK( !UserManager::instance().findUser( 96302 ).isValid() );
  QMainWindow host;
  QToolBar* tools = host.addToolBar( "synthetic tools" );
  HistoryTestChat* chat = new HistoryTestChat;
  chat->setupToolBar( tools );
  chat->setModernMode( true );
  host.setCentralWidget( chat );
  tools->hide();
  host.resize( 1180, 800 );
  host.show();
  Chat live = conversation();
  ChatManager::instance().setChat( live );
  CHECK( chat->setChat( live ) );
  settle( application );
  ShadowChatTranscript* transcript = dynamic_cast<ShadowChatTranscript*>(
    chat->findChild<QTextBrowser*>( "GuiChatViewer" ) );
  CHECK( transcript );
  QList<Bubble> rows = bubbles( transcript );
  std::printf( "Reloaded bubbles: %d\n", rows.size() );
  CHECK( rows.size() == 10 && transcript->bubbleCount() == 10 );
  const QStringList expected = { "legacy outgoing", "image caption retained", "legacy attachment",
    "saved local text", "saved incoming", "张图片", "file card", "native saved bubble", "exact duplicate", "exact duplicate" };
  const QSet<int> outgoing{ 0, 3, 5 };
  for( int index = 0; index < rows.size(); ++index )
  {
    CHECK( rows.at( index ).text.contains( expected.at( index ) ) );
    CHECK( rows.at( index ).row );
    CHECK( rows.at( index ).row->format().alignment().testFlag(
      outgoing.contains( index ) ? Qt::AlignRight : Qt::AlignLeft ) );
    CHECK( rows.at( index ).body->format().property( ShadowChatTranscript::BubbleColorProperty ).value<QColor>() ==
      QColor( outgoing.contains( index ) ? "#DDEBFF" : "#F0F2F4" ) );
  }
  const QString plain = transcript->toPlainText();
  CHECK( plain.contains( "legacy continuation" ) && plain.contains( "retained quote" ) );
  CHECK( plain.contains( "second saved line" ) && plain.contains( "new quoted content" ) );
  CHECK( plain.contains( QStringLiteral( "离线旧成员" ) ) && plain.contains( QStringLiteral( "离线新成员" ) ) );
  CHECK( plain.contains( QStringLiteral( "已断开连接" ) ) );
  CHECK( plain.contains( "2026-11-02" ) && plain.contains( "10:12" ) && plain.contains( "10:13" ) );
  CHECK( !plain.contains( "(2026-11-02 10:12:03)" ) );
  CHECK( plain.contains( QStringLiteral( "截图-20261102-101350.png" ) ) && !plain.contains( "beeimgtmp-" ) );
  CHECK( plain.count( "exact duplicate" ) == 2 );
  int fullImageLinks = 0;
  int media = 0;
  bool queryLink = false;
  bool actualFile = false;
  bool actualImage = false;
  for( QTextBlock block = transcript->document()->begin(); block.isValid(); block = block.next() )
    for( auto iterator = block.begin(); !iterator.atEnd(); ++iterator )
    {
      const QTextFragment fragment = iterator.fragment();
      if( !fragment.isValid() )
        continue;
      const QTextCharFormat format = fragment.charFormat();
      const QUrl url( format.anchorHref() );
      queryLink = queryLink || (url.scheme() == "https" && url.query() == "a=1&b=2");
      actualFile = actualFile || (url.isLocalFile() && url.toLocalFile() == filePath);
      actualImage = actualImage || (url.isLocalFile() && url.toLocalFile() == imagePath);
      if( !format.isImageFormat() )
        continue;
      const QTextImageFormat image = format.toImageFormat();
      if( image.name().contains( "chat-avatar-" ) || image.name().startsWith( "shadowtransfer:" ) )
        continue;
      ++media;
      CHECK( image.width() >= 140 && image.height() >= 90 );
      CHECK( transcript->document()->resource( QTextDocument::ImageResource, QUrl( image.name() ) ).isValid() );
      if( shadowAlbumPaths( url ).contains( imagePath ) )
        ++fullImageLinks;
      QTextCursor cursor( transcript->document() );
      cursor.setPosition( fragment.position() );
      bool inside = false;
      for( QTextFrame* frame = cursor.currentFrame(); frame; frame = frame->parentFrame() )
        inside = inside || frame->frameFormat().hasProperty( ShadowChatTranscript::BubbleColorProperty );
      CHECK( inside );
    }
  std::printf( "Media=%d album-original-links=%d query=%d file=%d image=%d\n",
    media, fullImageLinks, int( queryLink ), int( actualFile ), int( actualImage ) );
  CHECK( media == 3 && fullImageLinks >= 2 && queryLink && actualFile && actualImage );
  for( const QRectF& area : transcript->bubbleFrameRects() )
    CHECK( area.width() > 40 && area.width() <= 560 );
  host.resize( 560, 760 );
  settle( application );
  CHECK( transcript->horizontalScrollBar()->maximum() == 0 );
  host.resize( 1180, 800 );
  settle( application );
  CHECK( transcript->find( "legacy continuation" ) );
  CHECK( transcript->textCursor().selectedText() == "legacy continuation" );
  chat->loadSavedMessages();
  settle( application );
  CHECK( transcript->bubbleCount() == 10 );
  Message incoming( Message::Chat, 96311, "live after relogin" );
  incoming.setTimestamp( QDateTime( QDate( 2026, 11, 2 ), QTime( 12, 0, 0 ) ) );
  const ChatMessage appended( ID_LOCAL_USER, incoming, ChatMessage::Chat, true );
  live.addMessage( appended );
  ChatManager::instance().setChat( live );
  CHECK( chat->appendChatMessage( live, appended ) );
  settle( application );
  CHECK( transcript->bubbleCount() == 11 && bubbles( transcript ).last().text.contains( "live after relogin" ) );
  CHECK( chat->setChat( live ) );
  settle( application );
  CHECK( transcript->bubbleCount() == 11 );
  settings.setChatMessagesToShow( 4 );
  CHECK( chat->setChat( conversation() ) );
  settle( application );
  CHECK( transcript->bubbleCount() == 4 );
  CHECK( transcript->toPlainText().contains( "file card" ) && transcript->toPlainText().count( "exact duplicate" ) == 2 );
  CHECK( transcript->toPlainText().contains( "native nested quote" ) );
  settings.setChatMessagesToShow( -1 );
  ChatManager::instance().setChat( conversation() );
  CHECK( chat->setChat( conversation() ) );
  settings.setDownloadDirectory( QDir( root ).filePath( "screenshots" ) );
  settings.setEnableFileTransfer( true );
  QStringList newPaths;
  QObject::connect( chat, &GuiChat::sendFileFromChatRequest, chat, [&]( VNumber, const QString& path ) {
    newPaths.append( path );
    chat->setFileSendResult( true, QStringLiteral( "synthetic accepted" ) );
  } );
  QImage screenshot( 90, 60, QImage::Format_RGB32 );
  screenshot.fill( Qt::blue );
  CHECK( chat->submitImage( screenshot ) && chat->submitImage( screenshot ) );
  CHECK( newPaths.size() == 2 && newPaths.first() != newPaths.last() );
  for( const QString& path : newPaths )
  {
    CHECK( QFileInfo( path ).fileName().startsWith( "ShadowChat-Screenshot-" ) );
    CHECK( QRegularExpression( "ShadowChat-Screenshot-\\d{8}-\\d{6}-\\d{3}-[0-9a-f-]+\\.png" )
      .match( QFileInfo( path ).fileName() ).hasMatch() );
    CHECK( QFileInfo::exists( path ) && !QImage( path ).isNull() );
  }
  CHECK( ChatManager::instance().constHistoryMap() == loaded );
  CHECK( bytes( settings.savedChatsFilePath() ) == archiveBefore );
  CHECK( bytes( imagePath ) == imageBefore && bytes( secondPath ) == secondBefore && bytes( filePath ) == fileBefore );
  std::puts( "PASS fresh process: native bubbles/directions/senders/time/order, rich quotes, real links, album sizes, reload, append, whole-record limit, screenshot names, unchanged archive/assets" );
  return 0;
}

static int worker( int argc, char** argv )
{
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  const QString mode = application.arguments().at( 1 );
  const QString root = application.arguments().at( 2 );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "history-bubbles-fixture" );
  settings.setAccountDataFolder( root );
  settings.load();
  settings.setChatMessagesToShow( -1 );
  settings.setChatMessageFilter( QBitArray( ChatMessage::NumTypes, false ) );
  User local = settings.localUser();
  local.setName( "local fixture" );
  settings.setLocalUser( local );
  Random::init();
  MessageManager::instance().loadSavedMessagesAuthCode();
  Core core;
  return mode == "seed" ? seed( application, root ) : verify( application, root );
}

int main( int argc, char** argv )
{
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  if( argc > 1 )
    return worker( argc, argv );
  QCoreApplication application( argc, argv );
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert( "QT_QPA_PLATFORM", "offscreen" );
  for( const QString& mode : QStringList{ "seed", "reload", "restart-again" } )
  {
    std::printf( "Running phase: %s\n", mode.toUtf8().constData() );
    QProcess process;
    process.setProcessEnvironment( environment );
    process.setProcessChannelMode( QProcess::ForwardedChannels );
    process.start( application.applicationFilePath(), QStringList{ mode, temporary.path() } );
    CHECK( process.waitForFinished( 60000 ) );
    std::printf( "Phase exit: %s status=%d code=%d\n", mode.toUtf8().constData(),
                 int( process.exitStatus() ), process.exitCode() );
    CHECK( process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 );
  }
  std::puts( "PASS production history save and two independent process reloads using only temporary synthetic data" );
  return 0;
}
