#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowImageAlbum.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QtWidgets>
#include <QTextTable>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

struct ImageBubble
{
  QTextTable* frame;
  QTextTable* body;
};

static QList<ImageBubble> imageBubbles( ShadowChatTranscript* transcript )
{
  QList<ImageBubble> result;
  for( QTextFrame* frame : transcript->document()->rootFrame()->childFrames() )
  {
    QTextTable* row = qobject_cast<QTextTable*>( frame );
    if( !row || row->columns() != 3 || row->rows() != 1 ) continue;
    for( QTextFrame* nested : row->childFrames() )
    {
      QTextTable* body = qobject_cast<QTextTable*>( nested );
      if( body && body->rows() == 1 && body->columns() == 1 &&
          body->format().hasProperty( ShadowChatTranscript::BubbleColorProperty ) )
        result.append( ImageBubble{ row, body } );
    }
  }
  return result;
}

static ChatMessage photo( const User& sender, const QString& path, const QString& album = QString(),
  const QString& caption = QString() )
{
  const QString url = QUrl::fromLocalFile( path ).toString().toHtmlEscaped();
  QString body = QStringLiteral( "<!--shadow-sender:%1--><a href=\"%2\"><img src=\"%2\"></a>" )
    .arg( QString::fromLatin1( sender.name().toUtf8().toBase64() ), url );
  if( !album.isEmpty() ) body.prepend( QStringLiteral( "<!--shadow-album:%1-->" ).arg( album ) );
  if( !caption.isEmpty() ) body += "<br>" + caption.toHtmlEscaped();
  Message message = Protocol::instance().systemMessage( body );
  message.setTimestamp( QDateTime( QDate( 2026, 10, 30 ), QTime( 18, 24, 16 ) ) );
  return ChatMessage( ID_SYSTEM_MESSAGE, message, ChatMessage::ImagePreview, false );
}

int main( int argc, char** argv )
{
  BeeApplication application( argc, argv );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  application.setFont( ShadowStyle::applicationFont() );
  Random::init();
  QTemporaryDir folder;
  CHECK( folder.isValid() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "image-bubble-test" );
  settings.setAccountDataFolder( folder.path() );
  settings.load();
  settings.setChatMessagesToShow( -1 );
  User local = settings.localUser();
  local.setName( QStringLiteral( "我方（合成）" ) );
  local.setColor( "#2563EB" );
  settings.setLocalUser( local );
  User remote( 87001 );
  remote.setName( QStringLiteral( "胡子（合成）" ) );
  remote.setHash( "image-bubble-peer" );
  remote.setColor( "#F47A73" );
  UserManager::instance().setUser( remote );
  Core core;
  QImage landscape( 480, 260, QImage::Format_RGB32 );
  landscape.fill( QColor( "#548AC5" ) );
  QPainter details( &landscape );
  details.setPen( Qt::white );
  details.setFont( ShadowStyle::applicationFont() );
  details.drawText( landscape.rect(), Qt::AlignCenter, QStringLiteral( "合成图片 · 气泡验证" ) );
  details.end();
  const QString landscapePath = folder.path() + "/landscape.png";
  CHECK( landscape.save( landscapePath ) );
  QImage portrait( 180, 480, QImage::Format_RGB32 );
  portrait.fill( QColor( "#E7AD69" ) );
  const QString portraitPath = folder.path() + "/portrait.png";
  CHECK( portrait.save( portraitPath ) );
  QFile original( landscapePath );
  CHECK( original.open( QIODevice::ReadOnly ) );
  const QByteArray originalBytes = original.readAll();
  original.close();
  Group group;
  group.setId( 88001 );
  group.setName( QStringLiteral( "图片气泡回归（合成）" ) );
  group.setChatType( Group::GroupChat );
  group.addUser( ID_LOCAL_USER );
  group.addUser( remote.id() );
  Chat conversation;
  conversation.setGroup( group );
  conversation.addMessage( photo( local, landscapePath ) );
  conversation.addMessage( photo( remote, portraitPath, QString(), QStringLiteral( "手动调下这个" ) ) );
  QStringList albumPaths;
  for( int index = 0; index < 12; ++index )
  {
    const QString path = folder.path() + QString( "/album-%1.png" ).arg( index );
    CHECK( QFile::copy( landscapePath, path ) );
    albumPaths.append( path );
    conversation.addMessage( photo( local, path, "11111111-1111-1111-1111-111111111111" ) );
  }
  ChatManager::instance().setChat( conversation );
  QMainWindow host;
  QToolBar* tools = host.addToolBar( "test-tools" );
  GuiChat* chat = new GuiChat( &host );
  chat->setupToolBar( tools );
  chat->setModernMode( true );
  host.setCentralWidget( chat );
  tools->hide();
  host.resize( 1100, 820 );
  host.show();
  CHECK( chat->setChat( conversation ) );
  application.processEvents();
  ShadowChatTranscript* transcript = dynamic_cast<ShadowChatTranscript*>(
    chat->findChild<QTextBrowser*>( "GuiChatViewer" ) );
  CHECK( transcript && transcript->bubbleCount() == 3 );
  QList<ImageBubble> bubbles = imageBubbles( transcript );
  CHECK( bubbles.size() == 3 );
  CHECK( bubbles.at( 0 ).frame->format().alignment().testFlag( Qt::AlignRight ) );
  CHECK( bubbles.at( 1 ).frame->format().alignment().testFlag( Qt::AlignLeft ) );
  CHECK( bubbles.at( 2 ).frame->format().alignment().testFlag( Qt::AlignRight ) );
  CHECK( bubbles.at( 0 ).body->format().property( ShadowChatTranscript::BubbleColorProperty ).value<QColor>() == QColor( "#DDEBFF" ) );
  CHECK( bubbles.at( 1 ).body->format().property( ShadowChatTranscript::BubbleColorProperty ).value<QColor>() == QColor( "#F0F2F4" ) );
  CHECK( transcript->toPlainText().contains( QStringLiteral( "手动调下这个" ) ) );
  CHECK( transcript->toPlainText().contains( QStringLiteral( "12 张图片" ) ) &&
    transcript->toPlainText().contains( QStringLiteral( "还有 3 张" ) ) );
  CHECK( transcript->toPlainText().contains( QStringLiteral( "胡子（合成）" ) ) );
  CHECK( transcript->toPlainText().contains( "18:24" ) );
  int mediaCount = 0;
  int avatarCount = 0;
  bool fullAlbumLinked = false;
  int firstPhotoPosition = -1;
  QString firstPhotoLink;
  for( QTextBlock block = transcript->document()->begin(); block.isValid(); block = block.next() )
    for( QTextBlock::iterator iterator = block.begin(); !iterator.atEnd(); ++iterator )
    {
      const QTextFragment fragment = iterator.fragment();
      if( !fragment.isValid() || !fragment.charFormat().isImageFormat() ) continue;
      const QTextImageFormat image = fragment.charFormat().toImageFormat();
      if( image.name().contains( "chat-avatar-" ) ) { ++avatarCount; continue; }
      ++mediaCount;
      const QUrl link( image.anchorHref() );
      const QStringList originals = shadowAlbumPaths( link );
      if( originals.size() == 12 ) fullAlbumLinked = originals == albumPaths;
      if( firstPhotoPosition < 0 ) { firstPhotoPosition = fragment.position(); firstPhotoLink = image.anchorHref(); }
      const QVariant resource = transcript->document()->resource( QTextDocument::ImageResource, QUrl( image.name() ) );
      CHECK( resource.isValid() );
      QTextCursor cursor( transcript->document() );
      cursor.setPosition( fragment.position() );
      QTextFrame* frame = cursor.currentFrame();
      bool inside = false;
      while( frame )
      {
        if( frame->frameFormat().hasProperty( ShadowChatTranscript::BubbleColorProperty ) ) { inside = true; break; }
        frame = frame->parentFrame();
      }
      CHECK( inside );
    }
  CHECK( mediaCount == 11 && avatarCount == 3 && fullAlbumLinked );
  const QVector<QRectF> rectangles = transcript->bubbleFrameRects();
  for( int index = 0; index < rectangles.size(); ++index )
  {
    const QRectF area = rectangles.at( index );
    CHECK( area.width() <= 560 && area.width() > 40 );
    transcript->verticalScrollBar()->setValue( qMax( 0, int( area.top() ) - 20 ) );
    application.processEvents();
    const QPoint sample( int( area.left() ) + 3, int( area.top() ) + 20 - transcript->verticalScrollBar()->value() );
    const QImage rendered = transcript->viewport()->grab().toImage();
    const qreal scale = rendered.devicePixelRatio();
    CHECK( rendered.pixelColor( qRound( sample.x() * scale ), qRound( sample.y() * scale ) ) ==
      bubbles.at( index ).body->format().property( ShadowChatTranscript::BubbleColorProperty ).value<QColor>() );
  }
  QTextCursor imageCursor( transcript->document() );
  imageCursor.setPosition( firstPhotoPosition );
  transcript->setTextCursor( imageCursor );
  transcript->ensureCursorVisible();
  application.processEvents();
  const QRect cursor = transcript->cursorRect( imageCursor );
  CHECK( transcript->anchorAt( QPoint( cursor.left() + 3, cursor.center().y() ) ) == firstPhotoLink );
  transcript->verticalScrollBar()->setValue( 0 );
  host.grab().save( QDir::currentPath() + "/image-bubbles-wide.png" );
  int largeRemovals = 0;
  QObject::connect( transcript->document(), &QTextDocument::contentsChange, &host,
    [&]( int, int removed, int ) { if( removed > 64 ) ++largeRemovals; } );
  const ChatMessage appended = photo( local, portraitPath );
  conversation.addMessage( appended );
  CHECK( chat->appendChatMessage( conversation, appended ) );
  application.processEvents();
  CHECK( transcript->bubbleCount() == 4 && largeRemovals == 0 );
  chat->resize( 550, 720 );
  host.resize( 550, 720 );
  application.processEvents();
  CHECK( transcript->horizontalScrollBar()->maximum() == 0 );
  for( const QRectF& area : transcript->bubbleFrameRects() )
    CHECK( area.right() <= transcript->viewport()->width() + 1 );
  CHECK( transcript->find( QStringLiteral( "手动调下这个" ) ) );
  CHECK( transcript->textCursor().selectedText() == QStringLiteral( "手动调下这个" ) );
  host.grab().save( QDir::currentPath() + "/image-bubbles-narrow.png" );
  CHECK( original.open( QIODevice::ReadOnly ) && original.readAll() == originalBytes );
  std::puts( "PASS native image bubbles: outgoing/incoming colors and alignment, avatars/time/caption, actual rounded background pixels, all images nested inside bubbles, original link hit test, 12-image scope/9 thumbnails/remainder, incremental append, narrow layout and unchanged source bytes" );
  return 0;
}
