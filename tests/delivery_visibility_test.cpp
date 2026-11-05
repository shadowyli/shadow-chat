#include "BeeApplication.h"
#include "GuiChat.h"
#include "Core.h"
#include "Settings.h"
#include "ChatManager.h"
#include "IconManager.h"
#include "Random.h"
#include "Message.h"
#include <QMainWindow>
#include <QToolBar>
#include <cstdio>

class VisibilityChat : public GuiChat
{
public:
  using GuiChat::messageCanBeShowed;
};

static ChatMessage notice( VNumber author, ChatMessage::Type type, const QString& text )
{
  Message wire( Message::Chat, 97341, QString() );
  wire.setText( text );
  return ChatMessage( author, wire, type, true );
}

int main( int argc, char** argv )
{
  BeeApplication app( argc, argv );
  QTemporaryDir directory;
  Random::init();
  Settings::instance().setDefaultFolders();
  Settings::instance().createLocalUser( "delivery-visibility-fixture" );
  Settings::instance().setAccountDataFolder( directory.path() );
  Settings::instance().load();
  IconManager::instance();
  Core core;
  VisibilityChat chat;
  QMainWindow window;
  QToolBar* toolbar = window.addToolBar( "fixture" );
  chat.setupToolBar( toolbar );
  chat.setModernMode( true );
  const QStringList notices = QStringList() << QStringLiteral( "消息“不知道”：已送达 0/2 人" )
    << QStringLiteral( "消息“不知道”：已送达 1/2 人" )
    << QStringLiteral( "消息“失败原因修好了”：已送达 2/2 人" );
  int failures = 0;
  for( const QString& notice : notices )
  {
    if( chat.messageCanBeShowed( ::notice( ID_SYSTEM_MESSAGE, ChatMessage::Other, notice ) ) ) ++failures;
    if( !chat.messageCanBeShowed( ::notice( 97001, ChatMessage::Other, notice ) ) ) ++failures;
    if( !chat.messageCanBeShowed( ::notice( 97001, ChatMessage::Chat, notice ) ) ) ++failures;
  }
  const ChatMessage failed = notice( ID_SYSTEM_MESSAGE, ChatMessage::Other,
    QStringLiteral( "<!--shadow-delivery-notice-->发送失败，请重试。" ) );
  if( !chat.messageCanBeShowed( failed ) ) ++failures;
  std::printf( "delivery visibility: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures );
  return failures ? 1 : 0;
}
