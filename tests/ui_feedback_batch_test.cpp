#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "GuiChatMessage.h"
#include "GuiMain.h"
#include "GuiSavedChat.h"
#include "HistoryRecord.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include "ShadowRichText.h"
#include "UserManager.h"
#include <QAbstractTextDocumentLayout>
#include <QTextTable>
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

static QList<QRectF> bodyAreas( ShadowChatTranscript* transcript )
{
  QList<QRectF> areas;
  QList<QTextFrame*> frames = transcript->document()->rootFrame()->childFrames();
  for( int index = 0; index < frames.size(); ++index ) frames.append( frames[index]->childFrames() );
  for( QTextFrame* frame : frames )
  {
    QTextTable* table = qobject_cast<QTextTable*>( frame );
    if( table && table->format().hasProperty( ShadowChatTranscript::BubbleColorProperty ) )
      areas.append( transcript->document()->documentLayout()->frameBoundingRect( table ) );
  }
  return areas;
}

class FeedbackChat : public GuiChat
{
public:
  using GuiChat::modernBubbleHtml;
};

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication app( argc, argv );
  app.setFont( ShadowStyle::applicationFont() );
  ShadowDialogChrome::install( &app );
  Random::init();
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  app.setProperty( "shadowAccountRoot", temporary.path() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "ui-feedback-125" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  Core core;
  User peer( 98745 );
  peer.setName( QStringLiteral( "测试同事" ) );
  peer.setHash( "synthetic-ui-feedback-peer" );
  UserManager::instance().setUser( peer );
  Group group;
  group.setId( 98746 );
  group.setName( "UI feedback fixture" );
  group.setChatType( Group::GroupChat );
  group.addUser( ID_LOCAL_USER );
  group.addUser( peer.id() );
  Chat archived;
  archived.setGroup( group );
  const QStringList messages = { QStringLiteral( "可以" ), QStringLiteral( "文字信息是程序文字" ),
    QStringLiteral( "我两种 一种是智能分层 一种是切图" ) };
  for( int index = 0; index < messages.size(); ++index )
  {
    Message message = Protocol::instance().chatMessage( archived, messages[index] );
    message.setTimestamp( QDateTime( QDate( 2026, 11, 5 ), QTime( 10, 20, index ) ) );
    archived.addMessage( ChatMessage( index == 1 ? ID_LOCAL_USER : peer.id(), message, ChatMessage::Chat, true ) );
  }
  const QString archive = GuiChatMessage::chatToHtml( archived, false, true, true, true, true, true );
  QMap<QString, QString> histories;
  histories.insert( archived.name(), archive );
  ChatManager::instance().addSavedChats( histories );
  Chat live;
  live.setGroup( group );
  live.addMessage( ChatMessage( ID_LOCAL_USER, Protocol::instance().chatMessage( live, "lo" ), ChatMessage::Chat, true ) );
  ChatManager::instance().setChat( live );
  QToolBar tools;
  FeedbackChat chat;
  chat.setupToolBar( &tools );
  chat.setModernMode( true );
  chat.resize( 1000, 650 );
  CHECK( chat.setChat( live ) );
  chat.show();
  QTest::qWait( 120 );
  auto* transcript = dynamic_cast<ShadowChatTranscript*>( chat.findChild<QTextBrowser*>( "GuiChatViewer" ) );
  CHECK( transcript && transcript->bubbleCount() == 4 );
  const QList<QRectF> areas = bodyAreas( transcript );
  CHECK( areas.size() == 4 );
  bool compactHeight = true;
  for( const QRectF& area : areas )
  {
    std::printf( "Chinese short-message body height=%.2f width=%.2f\n", area.height(), area.width() );
    compactHeight = compactHeight && area.height() < 70;
  }
  CHECK( ChatManager::instance().chatSavedText( archived.name() ) == archive );
  const QString authoredLines = QStringLiteral( "第一行\n\n第二行\n" );
  Message authored = Protocol::instance().chatMessage( archived, authoredLines );
  const ChatMessage authoredMessage( peer.id(), authored, ChatMessage::Chat, true );
  const QString rendered = GuiChatMessage::formatMessage( peer, authoredMessage,
    ID_SYSTEM_MESSAGE, true, true, true, false, true, true );
  QJsonObject metadata;
  metadata.insert( "sender", peer.name() );
  metadata.insert( "time", authoredMessage.timestamp().toString( Qt::ISODate ) );
  metadata.insert( "type", ChatMessage::Chat );
  const QString marked = QStringLiteral( "<!--shadow-record:%1-->%2" ).arg(
    QString::fromLatin1( QJsonDocument( metadata ).toJson( QJsonDocument::Compact ).toBase64() ), rendered );
  const auto authoredEntries = ShadowChatTranscript::historyEntries( marked );
  CHECK( authoredEntries.size() == 1 );
  QTextDocument preserved;
  preserved.setHtml( authoredEntries.first().html );
  QTextDocument storedBody;
  storedBody.setHtml( authoredMessage.message() );
  std::fprintf( stdout, "Authored message=%s preserved=%s\n",
    QJsonDocument( QJsonArray{ authoredLines } ).toJson( QJsonDocument::Compact ).constData(),
    QJsonDocument( QJsonArray{ preserved.toPlainText() } ).toJson( QJsonDocument::Compact ).constData() );
  std::fprintf( stdout, "Stored body=%s converted=%s\n",
    QJsonDocument( QJsonArray{ authoredMessage.message() } ).toJson( QJsonDocument::Compact ).constData(),
    QJsonDocument( QJsonArray{ authoredEntries.first().html } ).toJson( QJsonDocument::Compact ).constData() );
  CHECK( preserved.toPlainText() == storedBody.toPlainText() );
  CHECK( compactHeight );
  std::puts( "PASS synthetic legacy Chinese short messages reload compactly; original archive retained" );
  if( app.arguments().contains( "--height-only" ) ) return 0;
  QStringList incrementalMessages = {
    QStringLiteral( "1111" ), QStringLiteral( "弄到我这里" ),
    QStringLiteral( "<span class=\"bee-quote\">loliv:<br>服务器数据</span><br>我的意思是" ),
    QStringLiteral( "1111" ), QStringLiteral( "弄到我这里" )
  };
  QTextDocument richMessage;
  richMessage.setHtml( QStringLiteral( "<u>1111</u>" ) );
  incrementalMessages.append( richMessage.toHtml() );
  richMessage.setHtml( QStringLiteral( "<u>弄到我这里</u>" ) );
  incrementalMessages.append( richMessage.toHtml() );
  incrementalMessages.append( QStringLiteral( "<p>&nbsp;</p><p>&nbsp;</p><p><u>1111</u></p>" ) );
  for( int index = 0; index < incrementalMessages.size(); ++index )
  {
    QString plain = incrementalMessages[index];
    QString rich;
    if( index >= 5 )
    {
      QTextDocument authored;
      authored.setHtml( plain );
      plain = authored.toPlainText();
      rich = ShadowRichText::serialize( &authored );
      if( index == 7 ) plain = QStringLiteral( "1111" );
    }
    const ChatMessage appended( index % 2 ? ID_LOCAL_USER : peer.id(),
      Protocol::instance().chatMessage( live, plain, rich ), ChatMessage::Chat, true );
    live.addMessage( appended );
    ChatManager::instance().setChat( live );
    CHECK( chat.appendChatMessage( live, appended ) );
    QTest::qWait( 40 );
    const QList<QRectF> appendedAreas = bodyAreas( transcript );
    CHECK( !appendedAreas.isEmpty() );
    const qreal height = appendedAreas.last().height();
    std::printf( "Incremental message %d body height=%.2f\n", index, height );
    if( index != 2 ) CHECK( height < 70 );
  }
  std::puts( "PASS sequential incoming/outgoing short messages and quoted replies remain compact" );
  const QString intentionalLines = QStringLiteral( "\n\n1111" );
  const ChatMessage intentional( peer.id(), Protocol::instance().chatMessage( live, intentionalLines,
    QStringLiteral( "<p>&nbsp;</p><p>&nbsp;</p><p><u>1111</u></p>" ) ), ChatMessage::Chat, true );
  CHECK( ShadowRichText::messageText( intentional.message() ) == intentionalLines );
  CHECK( intentional.message().contains( "<u>" ) );
  const QString legacyRichBody = richMessage.toHtml();
  const QString retainedBubble = chat.modernBubbleHtml( legacyRichBody, peer, false,
    QDateTime::currentDateTime(), 160, "retained-rich-message" );
  transcript->moveCursor( QTextCursor::End );
  transcript->insertHtml( retainedBubble );
  QTest::qWait( 40 );
  const qreal retainedHeight = transcript->bubbleFrameRects().last().height();
  std::printf( "Retained Qt rich paragraph painted body height=%.2f\n", retainedHeight );
  CHECK( retainedHeight < 45 );
  CHECK( transcript->document()->toPlainText().contains( QStringLiteral( "弄到我这里" ) ) );
  if( app.arguments().contains( "--incremental-only" ) ) return 0;
  chat.hide();

  const QString imagePath = temporary.filePath( "wide.png" );
  QImage image( 420, 180, QImage::Format_RGB32 );
  image.fill( QColor( "#72A0D4" ) );
  CHECK( image.save( imagePath ) );
  HistoryRecord media;
  media.sender = peer.name();
  media.timestamp = QDateTime::currentDateTime();
  media.type = ChatMessage::ImagePreview;
  const QString imageUrl = QUrl::fromLocalFile( imagePath ).toString();
  media.html = QStringLiteral( "<a href=\"%1\"><img src=\"%1\"></a>" ).arg( imageUrl );
  histories[archived.name()] += shadowHistoryRecordHtml( media, QString() );
  HistoryRecord longMessage;
  longMessage.sender = peer.name();
  longMessage.timestamp = media.timestamp.addSecs( 1 );
  longMessage.type = ChatMessage::Chat;
  longMessage.html = QString( 180, QChar( 0x957F ) );
  histories[archived.name()] += shadowHistoryRecordHtml( longMessage, QString() );
  ChatManager::instance().addSavedChats( histories );
  GuiSavedChat history( Q_NULLPTR, true );
  history.resize( 390, 750 );
  history.showSavedChat( archived.name() );
  history.show();
  QTest::qWait( 60 );
  QListWidget* results = history.findChild<QListWidget*>( "ShadowHistoryResults" );
  QTabBar* tabs = history.findChild<QTabBar*>( "ShadowHistoryTabs" );
  CHECK( results && tabs && results->count() >= 6 );
  int shortHeight = 0;
  int longHeight = 0;
  for( int index = 0; index < results->count(); ++index )
  {
    QListWidgetItem* item = results->item( index );
    const int height = results->visualItemRect( item ).height();
    const QString preview = item->data( Qt::UserRole + 4 ).toString();
    if( preview == "lo" ) shortHeight = height;
    if( preview.contains( QString( 100, QChar( 0x957F ) ) ) ) longHeight = height;
  }
  CHECK( shortHeight >= 44 && shortHeight <= 65 && longHeight > shortHeight );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR" );
  if( !proof.isEmpty() )
  {
    QDir().mkpath( proof );
    CHECK( history.grab().save( proof + "/compact-history-messages.png" ) );
  }
  tabs->setCurrentIndex( 2 );
  QTest::qWait( 30 );
  bool square = false;
  for( int index = 0; index < results->count(); ++index )
    if( !results->item( index )->data( Qt::UserRole + 1 ).toString().isEmpty() )
    {
      const QRect tile = results->visualItemRect( results->item( index ) );
      CHECK( tile.width() == tile.height() );
      square = true;
    }
  CHECK( square );
  if( !proof.isEmpty() ) CHECK( history.grab().save( proof + "/square-history-media.png" ) );
  CHECK( ChatManager::instance().chatSavedText( archived.name() ) == histories[archived.name()] );
  history.hide();
  std::puts( "PASS compact one-line rows, complete multiline height and square two-column media preserve stored content" );

  GuiMain main;
  main.loadStyle();
  main.resize( 1000, 650 );
  main.show();
  QTest::qWait( 60 );
  QFrame* welcome = main.findChild<QFrame*>( "ShadowWelcomeCard" );
  CHECK( welcome );
  QWidget* placeholder = main.findChild<QWidget*>( "ShadowChatPlaceholder" );
  QStackedWidget* conversationPages = main.findChild<QStackedWidget*>( "ShadowConversationStack" );
  CHECK( placeholder && conversationPages );
  conversationPages->setCurrentWidget( placeholder );
  if( QWidget* search = main.findChild<QWidget*>( "ShadowSearchPanel" ) ) search->hide();
  QTest::qWait( 40 );
  const QImage pixels = placeholder->grab().toImage();
  const qreal ratio = pixels.devicePixelRatio();
  const QPoint cardCorner = welcome->mapTo( placeholder, QPoint( 4, 4 ) );
  const QColor corner = pixels.pixelColor( qRound( cardCorner.x()*ratio ), qRound( cardCorner.y()*ratio ) );
  std::printf( "welcome corner rgba=%d,%d,%d,%d\n", corner.red(), corner.green(), corner.blue(), corner.alpha() );
  CHECK( corner.rgb() == QColor( "#F5F7FB" ).rgb() );
  if( !proof.isEmpty() ) CHECK( placeholder->grab().save( proof + "/borderless-welcome.png" ) );
  QPushButton* directory = Q_NULLPTR;
  for( QPushButton* button : welcome->findChildren<QPushButton*>() )
    if( button->accessibleName() == QStringLiteral( "打开通讯录" ) ) directory = button;
  CHECK( directory && directory->isEnabled() );
  directory->click();
  QTest::qWait( 30 );
  bool contactsSelected = false;
  for( QToolButton* button : main.findChildren<QToolButton*>( "ShadowNavButton" ) )
    if( button->text() == QStringLiteral( "通讯录" ) && button->isChecked() ) contactsSelected = true;
  CHECK( contactsSelected );
  QMenu* mainMenu = main.findChild<QMenu*>( "ShadowMainMenu" );
  CHECK( mainMenu );
  for( QAction* action : mainMenu->actions() )
    CHECK( action->text() != QStringLiteral( "聊天存档" ) && action->text() != QStringLiteral( "文件传输" ) );
  for( QToolButton* button : main.findChildren<QToolButton*>( "ShadowNavButton" ) )
  {
    CHECK( button->focusPolicy() == Qt::TabFocus );
    QTest::mouseClick( button, Qt::LeftButton );
    const QImage selected = button->grab().toImage();
    const QColor edge = selected.pixelColor( 0, selected.height() / 2 );
    CHECK( !(edge.red() > 180 && edge.green() < 100 && edge.blue() < 100) );
  }
  bool aboutVerified = false;
  QTimer::singleShot( 100, &main, [&]() {
    QDialog* about = main.findChild<QDialog*>( "ShadowAboutDialog" );
    if( !about ) return;
    QLabel* description = about->findChild<QLabel*>( "ShadowAboutDescription" );
    QToolButton* close = about->findChild<QToolButton*>( "shadowDialogClose" );
    QLabel* attribution = about->findChild<QLabel*>( "ShadowAboutAttribution" );
    aboutVerified = about->isVisible() && about->property( "shadowDialogChrome" ).toBool() &&
      about->windowFlags().testFlag( Qt::FramelessWindowHint ) && description && close &&
      description->text().contains( "shadowyli" ) && description->text().contains( settings.version( true, true, true ) ) &&
      !attribution &&
      !about->findChild<QPushButton*>( "ShadowAboutLicense" ) &&
      !about->findChild<QPushButton*>( "ShadowAboutClose" ) &&
      about->findChildren<QToolButton*>( "shadowDialogClose" ).size() == 1;
    const QImage image = about->grab().toImage();
    const qreal ratio = about->devicePixelRatioF();
    for( const QPoint& corner : { QPoint( 15, 15 ), QPoint( about->width() - 16, 15 ),
        QPoint( 15, about->height() - 16 ), QPoint( about->width() - 16, about->height() - 16 ) } )
      aboutVerified = aboutVerified &&
        image.pixelColor( qRound( corner.x() * ratio ), qRound( corner.y() * ratio ) ).alpha() < 160;
    if( !proof.isEmpty() ) about->grab().save( proof + "/shadow-about-dialog.png" );
    close->click();
  } );
  QTimer guard;
  guard.setSingleShot( true );
  QObject::connect( &guard, &QTimer::timeout, []() {
    std::fprintf( stderr, "FAIL about dialog timeout\n" );
    std::exit( 1 );
  } );
  guard.start( 5000 );
  CHECK( QMetaObject::invokeMethod( &main, "showAbout", Qt::DirectConnection ) );
  guard.stop();
  CHECK( aboutVerified );
  std::puts( "PASS borderless welcome keeps navigation; compact about has one close control without attribution block" );
  return 0;
}
