#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChatMessage.h"
#include "HistoryRecord.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowSearchPanel.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

static QString archived( const QString& html, const QString& sender, const QDateTime& time )
{
  HistoryRecord record;
  record.html = html;
  record.sender = sender;
  record.timestamp = time;
  record.type = ChatMessage::Chat;
  return shadowHistoryRecordHtml( record, QString() );
}

static QTreeWidgetItem* findResult( QTreeWidget* results, const QString& needle )
{
  for( int index = 0; index < results->topLevelItemCount(); ++index )
  {
    QTreeWidgetItem* item = results->topLevelItem( index );
    if( item->data( 0, Qt::UserRole + 1 ).toInt() == 3 && item->text( 0 ).contains( needle ) ) return item;
  }
  return Q_NULLPTR;
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  Random::init();
  QTemporaryDir folder;
  CHECK( folder.isValid() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "search-panel-fixture" );
  settings.setAccountDataFolder( folder.path() );
  settings.load();
  Core core;
  User sender( 98171 );
  sender.setName( "loliv" );
  sender.setHash( "synthetic-search-sender" );
  sender.setColor( "#2878F0" );
  UserManager::instance().setUser( sender );
  Group group;
  group.setId( 98172 );
  group.setName( "Search fixture" );
  group.setChatType( Group::PrivateChat );
  group.addUser( ID_LOCAL_USER );
  group.addUser( sender.id() );
  Chat chat;
  chat.setGroup( group );
  ChatMessage live( sender.id(), Protocol::instance().chatMessage( chat, "live loliv content" ), ChatMessage::Chat, true );
  chat.addMessage( live );
  ChatManager::instance().setChat( chat );
  const QDateTime time( QDate( 2026, 11, 5 ), QTime( 15, 49, 12 ) );
  const QString date = "<font color=#8B95A2>(2026-11-05 15:49:12)</font>";
  const QString name = "<font color=#2878F0><b>loliv</b></font>";
  const QString literal = "(2026-11-05 15:49:12) loliv: literal body stays";
  QString history = archived( date + " " + name + ": clean compact<br>", "loliv", time );
  history += archived( "<br>" + name + ":<br> " + date + " clean expanded<br>", "loliv", time );
  history += archived( name + " " + date + " clean RTL<br>", "loliv", time );
  history += archived( "<font color=red><b>!!</b></font> " + date + " " + name + ": important body<br>", "loliv", time );
  history += archived( date + " " + name + ": " + literal + "<br>", "loliv", time );
  history += archived( date + " " + name + ": <a href=\"file:///C:/fixture/report.png\">report.png</a> 下载<br>", "loliv", time );
  history += archived( date + " " + name + ": mismatched metadata stays<br>", "Different sender", time );
  history += archived( literal + "<br>", "loliv", time );
  history += archived( GuiChatMessage::formatMessage( sender, live, ID_SYSTEM_MESSAGE, true, true, true, false, true, true ),
    "loliv", live.timestamp() );
  const QString legacy = date + " " + name + ": clean legacy<br>";
  QMap<QString, QString> histories;
  histories.insert( chat.name(), history );
  histories.insert( "Legacy fixture", legacy );
  ChatManager::instance().addSavedChats( histories );
  QWidget owner;
  owner.resize( 1050, 800 );
  owner.show();
  ShadowSearchPanel panel( &owner );
  panel.open( "loliv" );
  QTest::qWait( 180 );
  QTreeWidget* results = panel.findChild<QTreeWidget*>( "ShadowGlobalSearchResults" );
  QTabBar* tabs = panel.findChild<QTabBar*>( "ShadowSearchTabs" );
  QLineEdit* input = panel.findChild<QLineEdit*>( "ShadowSearchInput" );
  CHECK( results && tabs && input );
  QTreeWidgetItem* header = Q_NULLPTR;
  for( int index = 0; index < results->topLevelItemCount(); ++index )
    if( results->topLevelItem( index )->text( 0 ) == QStringLiteral( "消息  11" ) )
      header = results->topLevelItem( index );
  CHECK( header );
  QWidget* actions = results->itemWidget( header, 0 );
  CHECK( actions && actions->objectName() == "ShadowSearchSectionActions" );
  CHECK( actions->findChildren<QLabel*>().isEmpty() );
  QPushButton* more = actions->findChild<QPushButton*>();
  CHECK( more && more->property( "searchMore" ).toBool() );
  std::puts( "PASS section titles have one paint owner; actions only contain the more button" );
  more->click();
  QTest::qWait( 30 );
  CHECK( tabs->currentIndex() == 3 && results->topLevelItemCount() == 12 );
  CHECK( findResult( results, "clean compact" )->text( 0 ) == "clean compact" );
  CHECK( findResult( results, "clean expanded" )->text( 0 ) == "clean expanded" );
  CHECK( findResult( results, "clean RTL" )->text( 0 ) == "clean RTL" );
  CHECK( findResult( results, "important body" )->text( 0 ) == "!! important body" );
  CHECK( findResult( results, QStringLiteral( "下载" ) )->text( 0 ) == QStringLiteral( "[图片和视频] 下载" ) );
  CHECK( findResult( results, "clean legacy" )->text( 0 ) == "clean legacy" );
  CHECK( findResult( results, "mismatched metadata" )->text( 0 ).contains( "2026-11-05" ) );
  int literalCount = 0;
  int liveCount = 0;
  for( int index = 0; index < results->topLevelItemCount(); ++index )
  {
    QTreeWidgetItem* item = results->topLevelItem( index );
    if( item->text( 0 ) == literal ) ++literalCount;
    if( item->text( 0 ) == "live loliv content" ) ++liveCount;
  }
  CHECK( literalCount == 2 && liveCount == 2 );
  CHECK( findResult( results, "clean compact" )->data( 0, Qt::UserRole + 2 ).toString() ==
    QStringLiteral( "Search fixture · loliv · 聊天记录    2026/11/05 15:49" ) );
  CHECK( ChatManager::instance().constHistoryMap() == histories );
  std::puts( "PASS compact/expanded/RTL/important/legacy previews omit only verified headers; literal body and file labels stay" );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR" );
  if( !proof.isEmpty() )
  {
    QDir().mkpath( proof );
    CHECK( panel.grab().save( proof + "/search-panel.png" ) );
  }
  bool activated = false;
  QObject::connect( &panel, &ShadowSearchPanel::messageActivated,
    [&]( VNumber id, const QString& chatName, const QString& query, bool isArchive ) {
      activated = id == ID_INVALID && chatName == chat.name() && query == "loliv" && isArchive;
    } );
  results->setCurrentItem( findResult( results, "clean compact" ) );
  QTest::keyClick( input, Qt::Key_Return );
  CHECK( activated && !panel.isVisible() );
  panel.open( "literal body stays" );
  QTest::qWait( 180 );
  CHECK( findResult( results, "literal body stays" ) );
  QTest::keyClick( input, Qt::Key_Escape );
  CHECK( !panel.isVisible() && ChatManager::instance().constHistoryMap() == histories );
  std::puts( "PASS sender matching, more category, Enter activation, Escape and original history retention" );
  return 0;
}
