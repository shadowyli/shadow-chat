#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "GuiSavedChat.h"
#include "HistoryRecord.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowHistoryPresentation.h"
#include "ShadowStyle.h"
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

class MediaChat : public GuiChat
{
public:
  using GuiChat::modernFileMessageToText;
  using GuiChat::modernHistoryHtml;
};

static QString record( const QString& html, int type, int second )
{
  HistoryRecord value;
  value.html = html;
  value.sender = "Media fixture";
  value.timestamp = QDateTime( QDate( 2026, 11, 5 ), QTime( 12, 0, second ) );
  value.type = type;
  return shadowHistoryRecordHtml( value, QString() );
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
  settings.createLocalUser( "media-presentation-fixture" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  Core core;
  const QString imagePath = temporary.filePath( "portrait.png" );
  const QString stickerPath = temporary.filePath( "marked-sticker.png" );
  const QString hashedPath = temporary.filePath( QString( 64, QLatin1Char( 'a' ) ) + ".png" );
  QPixmap image( 320, 220 );
  image.fill( QColor( "#609BEA" ) );
  CHECK( image.save( imagePath ) && image.save( stickerPath ) && image.save( hashedPath ) );
  const QString videoPath = temporary.filePath( "clip.mp4" );
  const QString documentPath = temporary.filePath( "brief.pdf" );
  for( const QString& path : QStringList{ videoPath, documentPath } )
  {
    QFile fixture( path );
    CHECK( fixture.open( QIODevice::WriteOnly ) && fixture.write( "presentation fixture only" ) > 0 );
  }
  const QString imageUrl = QUrl::fromLocalFile( imagePath ).toString();
  const QString stickerUrl = QUrl::fromLocalFile( stickerPath ).toString();
  const QString videoUrl = QUrl::fromLocalFile( videoPath ).toString();
  const QString imageHtml = QStringLiteral( "<a href=\"%1\"><img src=\"%1\"></a>" ).arg( imageUrl );
  const QString videoHtml = QStringLiteral( "<!--shadow-attachment--><a href=\"%1\">clip.mp4</a>" ).arg( videoUrl );
  QString archive = record( imageHtml + QStringLiteral( "<a href=\"%1\">portrait.png</a>" ).arg( imageUrl ), ChatMessage::ImagePreview, 0 );
  archive += record( imageHtml + " caption retained", ChatMessage::ImagePreview, 1 );
  archive += record( videoHtml, ChatMessage::FileTransfer, 2 );
  archive += record( imageHtml + " user wrote portrait.png explicitly", ChatMessage::Chat, 3 );
  archive += record( QStringLiteral( "<!--shadow-sticker--><a href=\"%1\"><img src=\"%1\"></a>" ).arg( stickerUrl ), ChatMessage::ImagePreview, 4 );
  archive += record( QStringLiteral( "<!--shadow-transfer-log-->正在发送 <a href=\"%1\">marked-sticker.png</a> 到同事" ).arg( stickerUrl ), ChatMessage::FileTransfer, 5 );
  archive += record( QStringLiteral( "<a href=\"%1\"><img src=\"%1\"></a>" ).arg( QUrl::fromLocalFile( hashedPath ).toString() ), ChatMessage::ImagePreview, 6 );
  archive += record( QStringLiteral( "<!--shadow-attachment--><a href=\"%1\">brief.pdf</a>" ).arg( QUrl::fromLocalFile( documentPath ).toString() ), ChatMessage::FileTransfer, 7 );
  const QList<HistoryRecord> parsed = shadowChatHistoryRecords( archive );
  CHECK( parsed.size() == 8 );
  CHECK( shadowHistoryMessagePreview( parsed[0] ) == QStringLiteral( "[图片和视频]" ) );
  CHECK( shadowHistoryMessagePreview( parsed[1] ) == QStringLiteral( "[图片和视频] caption retained" ) );
  CHECK( shadowHistoryMessagePreview( parsed[2] ) == QStringLiteral( "[图片和视频]" ) );
  CHECK( shadowHistoryMessagePreview( parsed[3] ).contains( "user wrote portrait.png explicitly" ) );
  CHECK( shadowHistoryMessagePreview( parsed[5] ) == QStringLiteral( "[图片和视频]" ) );
  Group group;
  group.setId( 98731 );
  group.setName( "Media presentation" );
  group.setChatType( Group::PrivateChat );
  group.addUser( ID_LOCAL_USER );
  Chat conversation;
  conversation.setGroup( group );
  ChatManager::instance().setChat( conversation );
  QMap<QString, QString> histories;
  histories.insert( conversation.name(), archive );
  ChatManager::instance().addSavedChats( histories );
  GuiSavedChat history( Q_NULLPTR, true );
  history.resize( 390, 650 );
  history.showSavedChat( conversation.name() );
  history.show();
  QTest::qWait( 60 );
  QListWidget* results = history.findChild<QListWidget*>( "ShadowHistoryResults" );
  QTabBar* tabs = history.findChild<QTabBar*>( "ShadowHistoryTabs" );
  CHECK( results && tabs && results->count() == 8 && results->frameShape() == QFrame::NoFrame && results->frameWidth() == 0 );
  QListWidgetItem* video = Q_NULLPTR;
  for( int index = 0; index < results->count(); ++index )
    if( results->item( index )->data( Qt::UserRole + 1 ).toString() == videoUrl ) video = results->item( index );
  CHECK( video && !video->data( Qt::UserRole + 6 ).value<QPixmap>().isNull() );
  CHECK( !video->data( Qt::UserRole + 4 ).toString().contains( "clip.mp4" ) );
  QUrl opened;
  QObject::connect( &history, &GuiSavedChat::openUrl, [&]( const QUrl& url ) { opened = url; } );
  results->scrollToItem( video );
  QTest::qWait( 30 );
  const QRect area = results->visualItemRect( video ).adjusted( 4, 2, -4, -2 );
  QTest::mouseClick( results->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint( area.left() + 81, area.top() + 68 ) );
  QTest::qWait( 30 );
  CHECK( opened == QUrl( videoUrl ) && results->currentItem() == video );
  tabs->setCurrentIndex( 2 );
  QTest::qWait( 30 );
  int mediaCount = 0;
  for( int index = 0; index < results->count(); ++index )
  {
    const QString target = results->item( index )->data( Qt::UserRole + 1 ).toString();
    CHECK( target != stickerUrl );
    if( !target.isEmpty() ) ++mediaCount;
  }
  CHECK( mediaCount == 3 && ChatManager::instance().chatSavedText( conversation.name() ) == archive );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR" );
  if( !proof.isEmpty() )
  {
    QDir().mkpath( proof );
    CHECK( history.grab().save( proof + "/history-media-no-inner-frame.png" ) );
  }
  tabs->setCurrentIndex( 1 );
  CHECK( results->count() == 1 && results->item( 0 )->data( Qt::UserRole + 4 ).toString().contains( "brief.pdf" ) );
  std::puts( "PASS media previews hide generated filenames, preserve captions/documents, exclude only marked stickers and keep source records" );
  QToolBar tools;
  MediaChat chat;
  chat.setupToolBar( &tools );
  chat.setModernMode( true );
  CHECK( chat.setChat( conversation ) );
  Message message = Protocol::instance().systemMessage( videoHtml + QStringLiteral( "<!--shadow-outgoing:1-->" ) );
  const QString videoBubble = chat.modernFileMessageToText( ChatMessage( ID_SYSTEM_MESSAGE, message, ChatMessage::FileTransfer, true ) );
  CHECK( videoBubble.contains( "<!--shadow-video-->" ) && videoBubble.contains( videoUrl.toHtmlEscaped() ) &&
    videoBubble.contains( "shadowtransfer:" ) );
  QTextDocument visible;
  visible.setHtml( videoBubble );
  CHECK( !visible.toPlainText().contains( "clip.mp4" ) && !visible.toPlainText().contains( "MiB" ) );
  CHECK( chat.modernHistoryHtml( record( videoHtml, ChatMessage::FileTransfer, 2 ) ).contains( "<!--shadow-video-->" ) );
  std::puts( "PASS live and archived video bubbles use a compact play card with the original URL and unchanged transfer indicators" );
  return 0;
}
