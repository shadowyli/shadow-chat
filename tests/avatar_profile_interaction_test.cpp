#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChatList.h"
#include "GuiMain.h"
#include "GuiVCard.h"
#include "Message.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowAvatarInteractions.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QCursor>
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

class ExternalOpenProbe : public QObject
{
  Q_OBJECT
public:
  int count = 0;
public slots:
  void capture( const QUrl& ) { ++count; }
};

static User syntheticUser( VNumber userId, const QString& name, const QColor& color )
{
  User user( userId );
  user.setName( name );
  user.setAccountName( QStringLiteral( "private-account-must-not-appear" ) );
  user.setLocalHostName( QStringLiteral( "private-device-must-not-appear" ) );
  user.setDomainName( QStringLiteral( "private-domain-must-not-appear" ) );
  user.setStatus( User::Online );
  user.setStatusDescription( QStringLiteral( "合成状态签名" ) );
  VCard card;
  card.setNickName( name );
  card.setFirstName( QStringLiteral( "Test" ) );
  card.setLastName( QStringLiteral( "Person" ) );
  card.setInfo( QStringLiteral( "合成资料，仅用于原生测试 <b>原样显示</b>" ) );
  card.setEmail( QStringLiteral( "public-%1@example.test" ).arg( userId ) );
  card.setPhoneNumber( QStringLiteral( "000-0000" ) );
  card.setRoomLocation( QStringLiteral( "合成位置" ) );
  card.setBirthday( QDate( 2000, 2, 3 ) );
  QPixmap photo( 64, 64 );
  photo.fill( color );
  card.setPhoto( photo );
  user.setVCard( card );
  user.setWorkgroups( QStringList() << QStringLiteral( "合成工作组 A" ) << QStringLiteral( "合成工作组 B" ) );
  return user;
}

static Chat syntheticChat( VNumber chatId, Group::ChatType type, const QList<VNumber>& users )
{
  Group group;
  group.setId( chatId );
  group.setName( QStringLiteral( "synthetic-chat-%1" ).arg( chatId ) );
  group.setChatType( type );
  foreach( VNumber userId, users )
    group.addUser( userId );
  Chat chat;
  chat.setGroup( group );
  chat.addMessage( ChatMessage( users.last(), Message( Message::Chat, chatId + 100,
    QStringLiteral( "合成消息" ) ), ChatMessage::Chat, false ) );
  return chat;
}

static QUrl profileLink( const User& user, const QString& name = QString() )
{
  const QByteArray encoded = (name.isEmpty() ? user.name() : name).toUtf8().toBase64(
    QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals );
  return QUrl( QStringLiteral( "shadowuser:%1?name=%2" ).arg( user.id() ).arg( QString::fromLatin1( encoded ) ) );
}

static GuiVCard* visibleCard( GuiMain& window )
{
  foreach( GuiVCard* card, window.findChildren<GuiVCard*>() )
    if( card->isVisible() )
      return card;
  return Q_NULLPTR;
}

static QWidget* outsideClickTarget( GuiMain& window, GuiVCard* card, QPoint* globalPoint )
{
  QWidget* central = window.centralWidget();
  if( !central || !central->isVisible() || !card || !card->isVisible() || !card->screen() )
    return Q_NULLPTR;
  QRegion candidates( QRect( central->mapToGlobal( QPoint() ), central->size() ) );
  candidates &= card->screen()->availableGeometry();
  candidates -= card->frameGeometry();
  for( const QRect& rectangle : candidates )
  {
    const QPoint points[] = { rectangle.center(), rectangle.topLeft(), rectangle.topRight(),
                              rectangle.bottomLeft(), rectangle.bottomRight() };
    for( const QPoint& point : points )
    {
      QWidget* target = QApplication::widgetAt( point );
      if( target && target->window() == &window &&
          (target == central || central->isAncestorOf( target )) && target->isVisible() &&
          target->visibleRegion().contains( target->mapFromGlobal( point ) ) )
      {
        *globalPoint = point;
        return target;
      }
    }
  }
  return Q_NULLPTR;
}

static void flushEvents()
{
  QApplication::processEvents();
  QApplication::sendPostedEvents( Q_NULLPTR, QEvent::DeferredDelete );
}

static bool route( GuiMain& window, const QUrl& url, VNumber chatId )
{
  const bool invoked = QMetaObject::invokeMethod( &window, "openUrlFromChat", Qt::DirectConnection,
    Q_ARG( QUrl, url ), Q_ARG( VNumber, chatId ) );
  flushEvents();
  return invoked;
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication app( argc, argv );
  app.setQuitOnLastWindowClosed( false );
  app.setFont( ShadowStyle::applicationFont() );
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  CHECK( QDir().mkpath( temporary.path() + QStringLiteral( "/config" ) ) );
  QSettings::setDefaultFormat( QSettings::IniFormat );
  QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, temporary.path() + QStringLiteral( "/config" ) );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( QStringLiteral( "synthetic-self" ) );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  settings.setShowUsersInWorkgroups( false );
  settings.setShowOnlyOnlineUsers( false );
  settings.setHideEmptyChatsInList( false );
  settings.setChatAutoSave( false );
  settings.setCloseMinimizeInTray( false );
  User local = syntheticUser( ID_LOCAL_USER, QStringLiteral( "合成本人" ), Qt::red );
  settings.setLocalUser( local );
  User peer = syntheticUser( 93001, QStringLiteral( "合成同事 <b>原文</b>" ), Qt::blue );
  User offline = syntheticUser( 93002, QStringLiteral( "合成离线同事" ), Qt::green );
  offline.setStatus( User::Offline );
  User stranger = syntheticUser( 93003, QStringLiteral( "无关同事" ), Qt::yellow );
  UserManager::instance().setUser( peer );
  UserManager::instance().setUser( offline );
  UserManager::instance().setUser( stranger );
  Core core;
  const Chat direct = syntheticChat( 94001, Group::PrivateChat, QList<VNumber>() << ID_LOCAL_USER << peer.id() );
  const Chat second = syntheticChat( 94002, Group::PrivateChat, QList<VNumber>() << ID_LOCAL_USER << offline.id() );
  Chat group = syntheticChat( 94003, Group::GroupChat, QList<VNumber>() << ID_LOCAL_USER << peer.id() );
  group.addMessage( ChatMessage( offline.id(), Message( Message::Chat, 95003,
    QStringLiteral( "合成历史发送者" ) ), ChatMessage::History, false ) );
  ChatManager::instance().setChat( direct );
  ChatManager::instance().setChat( second );
  ChatManager::instance().setChat( group );

  CHECK( ShadowAvatarInteractions::userIdFromLink( profileLink( peer ) ) == peer.id() );
  const QStringList malformed = QStringList() << "shadowuser:0" << "shadowuser:-1" << "shadowuser:+93001" <<
    "shadowuser:18446744073709551616" << "shadowuser://93001" << "shadowuser:93001/extra" <<
    "shadowuser:93001#93002" << "shadowuser:93001?name=a&name=b" << "shadowuser:93001?url=https://example.test" <<
    QStringLiteral( "shadowuser:%1" ).arg( ID_SYSTEM_MESSAGE ) << "https://example.test/shadowuser:93001";
  foreach( const QString& url, malformed )
    CHECK( ShadowAvatarInteractions::userIdFromLink( QUrl( url ) ) == ID_INVALID );
  CHECK( ShadowAvatarInteractions::userForChat( ID_LOCAL_USER, direct ).id() == local.id() );
  CHECK( ShadowAvatarInteractions::userForChat( peer.id(), direct ).id() == peer.id() );
  CHECK( ShadowAvatarInteractions::userForChat( offline.id(), second ).id() == offline.id() );
  CHECK( ShadowAvatarInteractions::userForChat( offline.id(), group ).id() == offline.id() );
  CHECK( !ShadowAvatarInteractions::userForChat( stranger.id(), group ).isValid() );
  CHECK( !ShadowAvatarInteractions::userForChat( offline.id(), direct ).isValid() );
  CHECK( !ShadowAvatarInteractions::userForChat( 93999, direct ).isValid() );
  CHECK( !ShadowAvatarInteractions::userForChat( peer.id(), Chat() ).isValid() );
  const QRect offsetScreen( -1920, -300, 1280, 720 );
  CHECK( offsetScreen.contains( ShadowAvatarInteractions::popupGeometry(
    QPoint( -650, 410 ), QSize( 380, 540 ), offsetScreen ) ) );
  CHECK( offsetScreen.contains( ShadowAvatarInteractions::popupGeometry(
    QPoint( -3000, -1000 ), QSize( 5000, 5000 ), offsetScreen ) ) );
  std::puts( "PASS strict link parsing, local/peer/offline/history routing, unknown and cross-chat refusal, offset screen bounds" );

  GuiMain window;
  window.loadStyle();
  window.resize( 1000, 720 );
  window.show();
  flushEvents();
  QCursor::setPos( window.centralWidget()->mapToGlobal( window.centralWidget()->rect().center() ) );
  ExternalOpenProbe external;
  QDesktopServices::setUrlHandler( QStringLiteral( "shadowuser" ), &external, "capture" );
  CHECK( route( window, profileLink( peer ), direct.id() ) );
  GuiVCard* card = visibleCard( window );
  CHECK( card && card->property( "profileUserId" ).toULongLong() == peer.id() );
  CHECK( card->screen()->availableGeometry().contains( card->frameGeometry() ) );
  CHECK( card->findChild<QLabel*>( "mp_lName" )->text() == peer.name() );
  CHECK( card->findChild<QLabel*>( "mp_lName" )->textFormat() == Qt::PlainText );
  CHECK( card->findChild<QLabel*>( "mp_lEmail" )->text().contains( peer.vCard().email() ) );
  CHECK( card->findChild<QPlainTextEdit*>( "mp_teInfo" )->toPlainText() == peer.vCard().info() );
  CHECK( card->findChild<QPlainTextEdit*>( "mp_teInfo" )->isReadOnly() );
  CHECK( !card->findChild<QLabel*>( "mp_lPath" )->isVisible() );
  CHECK( !card->findChild<QLabel*>( "mp_lStatus" )->isVisible() );
  CHECK( !card->findChild<QPushButton*>( "mp_pbColor" )->isVisible() );
  foreach( QLabel* label, card->findChildren<QLabel*>() )
    CHECK( !label->text().contains( "private-" ) && !label->toolTip().contains( "private-" ) );
  const QPixmap photo = *card->findChild<QLabel*>( "mp_lPhoto" )->pixmap();
  CHECK( !photo.isNull() && photo.toImage().pixelColor( photo.width() / 2, photo.height() / 2 ) == QColor( Qt::blue ) );
  CHECK( card->grab().save( QDir::currentPath() + QStringLiteral( "/public-card-%1.png" ).arg( qEnvironmentVariable( "QT_SCALE_FACTOR", "1" ) ) ) );
  QPointer<GuiVCard> closing = card;
  QTest::keyClick( card->findChild<QPlainTextEdit*>( "mp_teInfo" ), Qt::Key_Escape );
  flushEvents();
  CHECK( closing.isNull() && !visibleCard( window ) );

  CHECK( route( window, profileLink( local ), direct.id() ) );
  card = visibleCard( window );
  CHECK( card && card->property( "profileUserId" ).toULongLong() == local.id() );
  CHECK( !card->findChild<QPushButton*>( "mp_pbChat" )->isVisible() );
  QTest::mouseClick( card->findChild<QToolButton*>( "closePublicProfile" ), Qt::LeftButton );
  flushEvents();
  CHECK( !visibleCard( window ) );
  CHECK( route( window, profileLink( offline ), group.id() ) );
  CHECK( visibleCard( window ) && visibleCard( window )->property( "profileUserId" ).toULongLong() == offline.id() );
  card = visibleCard( window );
  QPoint outsideGlobal;
  QWidget* outsideTarget = outsideClickTarget( window, card, &outsideGlobal );
  if( !outsideTarget )
  {
    CHECK( card->screen() );
    window.move( card->screen()->availableGeometry().topLeft() );
    flushEvents();
    CHECK( visibleCard( window ) == card );
    outsideTarget = outsideClickTarget( window, card, &outsideGlobal );
  }
  CHECK( outsideTarget );
  CHECK( card->screen()->availableGeometry().contains( outsideGlobal ) );
  CHECK( !card->frameGeometry().contains( outsideGlobal ) );
  CHECK( window.centralWidget()->rect().contains( window.centralWidget()->mapFromGlobal( outsideGlobal ) ) );
  CHECK( outsideTarget->window() == &window );
  CHECK( outsideTarget == window.centralWidget() || window.centralWidget()->isAncestorOf( outsideTarget ) );
  CHECK( outsideTarget->visibleRegion().contains( outsideTarget->mapFromGlobal( outsideGlobal ) ) );
  CHECK( QApplication::widgetAt( outsideGlobal ) == outsideTarget );
  std::printf( "Outside click=%d,%d popup=%d,%d %dx%d target=%s\n",
    outsideGlobal.x(), outsideGlobal.y(), card->frameGeometry().x(), card->frameGeometry().y(),
    card->frameGeometry().width(), card->frameGeometry().height(), outsideTarget->metaObject()->className() );
  QTest::mouseClick( outsideTarget, Qt::LeftButton, Qt::NoModifier, outsideTarget->mapFromGlobal( outsideGlobal ) );
  flushEvents();
  CHECK( !visibleCard( window ) );
  CHECK( route( window, profileLink( stranger ), direct.id() ) );
  CHECK( !visibleCard( window ) );
  CHECK( route( window, QUrl( QStringLiteral( "shadowuser:93999?name=%1" ).arg(
    QString::fromLatin1( peer.name().toUtf8().toBase64( QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals ) ) ) ), direct.id() ) );
  CHECK( !visibleCard( window ) );
  foreach( const QString& url, malformed.mid( 0, malformed.size() - 1 ) )
    CHECK( route( window, QUrl( url ), direct.id() ) && !visibleCard( window ) );
  CHECK( external.count == 0 );
  std::puts( "PASS public fields only, literal text, avatar identity, self read-only, Escape/outside/close, private link never sent externally" );

  QTextBrowser linkViewer;
  linkViewer.setOpenLinks( false );
  linkViewer.resize( 180, 100 );
  linkViewer.document()->addResource( QTextDocument::ImageResource, QUrl( "synthetic-avatar" ), photo );
  linkViewer.setHtml( QStringLiteral( "<a href='%1'><img src='synthetic-avatar' width='40' height='40'></a>" )
    .arg( profileLink( peer ).toString( QUrl::FullyEncoded ) ) );
  QObject::connect( &linkViewer, &QTextBrowser::anchorClicked, &window,
    [&]( const QUrl& url ) { route( window, url, direct.id() ); } );
  linkViewer.show();
  flushEvents();
  CHECK( !linkViewer.anchorAt( QPoint( 20, 20 ) ).isEmpty() );
  QTest::mouseClick( linkViewer.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint( 20, 20 ) );
  flushEvents();
  CHECK( visibleCard( window ) && visibleCard( window )->property( "profileUserId" ).toULongLong() == peer.id() );
  visibleCard( window )->close();
  linkViewer.hide();
  flushEvents();
  CHECK( QMetaObject::invokeMethod( &window, "showChat", Qt::DirectConnection, Q_ARG( VNumber, direct.id() ) ) );
  flushEvents();
  QLabel* headerAvatar = window.findChild<QLabel*>( "ShadowChatAvatar" );
  CHECK( headerAvatar && headerAvatar->isVisible() );
  QTest::mouseClick( headerAvatar, Qt::LeftButton );
  flushEvents();
  CHECK( visibleCard( window ) && visibleCard( window )->property( "profileUserId" ).toULongLong() == peer.id() );
  visibleCard( window )->close();
  flushEvents();
  CHECK( QMetaObject::invokeMethod( &window, "showChat", Qt::DirectConnection, Q_ARG( VNumber, second.id() ) ) );
  flushEvents();
  QTest::keyClick( headerAvatar, Qt::Key_Space );
  flushEvents();
  CHECK( visibleCard( window ) && visibleCard( window )->property( "profileUserId" ).toULongLong() == offline.id() );
  visibleCard( window )->close();
  flushEvents();
  CHECK( QMetaObject::invokeMethod( &window, "showChat", Qt::DirectConnection, Q_ARG( VNumber, group.id() ) ) );
  QTest::mouseClick( headerAvatar, Qt::LeftButton );
  flushEvents();
  CHECK( !visibleCard( window ) );
  std::puts( "PASS real QTextBrowser anchor click, header mouse/keyboard, current chat identity and group avatar guard" );

  GuiChatList* chatList = window.findChild<GuiChatList*>();
  CHECK( chatList );
  chatList->updateChats();
  QTreeWidget* conversations = chatList->findChild<QTreeWidget*>( "ShadowConversationList" );
  CHECK( conversations && conversations->isVisible() );
  QTreeWidgetItem* directItem = Q_NULLPTR;
  for( int index = 0; index < conversations->topLevelItemCount(); ++index )
    if( conversations->topLevelItem( index )->data( 0, GuiChatItem::ChatId ).toULongLong() == direct.id() )
      directItem = conversations->topLevelItem( index );
  CHECK( directItem );
  conversations->scrollToItem( directItem );
  flushEvents();
  const QRect row = conversations->visualItemRect( directItem );
  const QPoint avatarPoint = ShadowAvatarInteractions::conversationAvatarRect( row ).center();
  QSignalSpy chatSelected( chatList, SIGNAL( chatSelected( VNumber ) ) );
  QTest::mouseClick( conversations->viewport(), Qt::LeftButton, Qt::NoModifier, avatarPoint );
  flushEvents();
  CHECK( visibleCard( window ) && visibleCard( window )->property( "profileUserId" ).toULongLong() == peer.id() );
  CHECK( chatSelected.count() == 0 );
  visibleCard( window )->close();
  flushEvents();
  QTest::mouseClick( conversations->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint( row.left() + 100, row.center().y() ) );
  flushEvents();
  CHECK( !visibleCard( window ) && chatSelected.count() == 1 );
  CHECK( chatSelected.takeFirst().at( 0 ).toULongLong() == direct.id() );
  std::puts( "PASS conversation avatar opens profile; rest of row still opens its chat" );

  GuiVCard* reusable = new GuiVCard;
  User minimal = syntheticUser( 93004, QStringLiteral( "仅昵称" ), Qt::cyan );
  minimal.setVCard( VCard() );
  minimal.setStatusDescription( QString() );
  minimal.setWorkgroups( QStringList() );
  reusable->setVCard( minimal, ID_INVALID, false );
  reusable->showAt( app.primaryScreen()->availableGeometry().bottomRight() );
  flushEvents();
  CHECK( !reusable->findChild<QLabel*>( "mp_lEmail" )->isVisible() );
  CHECK( !reusable->findChild<QPlainTextEdit*>( "mp_teInfo" )->isVisible() );
  reusable->setVCard( peer, direct.id(), false );
  CHECK( reusable->findChild<QLabel*>( "mp_lEmail" )->isVisible() );
  CHECK( reusable->findChild<QLabel*>( "mp_lStatusDescription" )->isVisible() );
  User verbose = peer;
  QStringList manyGroups;
  for( int index = 0; index < 80; ++index )
    manyGroups << QStringLiteral( "合成长工作组%1" ).arg( index );
  verbose.setWorkgroups( manyGroups );
  reusable->setVCard( verbose, direct.id(), false );
  reusable->showAt( app.primaryScreen()->availableGeometry().bottomRight() );
  flushEvents();
  CHECK( reusable->screen()->availableGeometry().contains( reusable->frameGeometry() ) );
  QScrollArea* details = reusable->findChild<QScrollArea*>( "publicProfileDetails" );
  std::printf( "Long content card=%dx%d body=%dx%d viewport=%dx%d hfw=%d range=%d\n",
    reusable->width(), reusable->height(), details->widget()->width(), details->widget()->height(),
    details->viewport()->width(), details->viewport()->height(),
    details->widget()->layout()->totalHeightForWidth( details->viewport()->width() ),
    details->verticalScrollBar()->maximum() );
  CHECK( reusable->grab().save( QDir::currentPath() + QStringLiteral( "/long-card-%1.png" ).arg( qEnvironmentVariable( "QT_SCALE_FACTOR", "1" ) ) ) );
  CHECK( details && details->verticalScrollBar()->maximum() > 0 );
  details->verticalScrollBar()->setValue( details->verticalScrollBar()->maximum() );
  CHECK( reusable->findChild<QLabel*>( "mp_lWorkgroups" )->text().contains( manyGroups.last() ) );
  reusable->close();
  flushEvents();
  QDesktopServices::unsetUrlHandler( QStringLiteral( "shadowuser" ) );
  std::printf( "PASS missing-field reset, long public fields scroll, bounded popup at scale %s (DPR %.2f)\n",
    qPrintable( qEnvironmentVariable( "QT_SCALE_FACTOR", "1" ) ), window.devicePixelRatioF() );
  return 0;
}

#include "avatar_profile_interaction_test.moc"
