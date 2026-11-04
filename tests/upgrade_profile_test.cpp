#include "BeeApplication.h"
#include "BuildSavedChatList.h"
#include "ChatManager.h"
#include "HistoryRecord.h"
#include "MessageManager.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowConversationPreferences.h"
#include "ShadowLocalAccounts.h"
#include "ShadowMessageDelivery.h"
#include "ShadowTransferQueue.h"
#include "UserManager.h"
#include <QtWidgets>
#include <cstdio>
#include <windows.h>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static const QString email = QStringLiteral( "upgrade-profile@example.test" );
static const QString password = QStringLiteral( "synthetic-upgrade-password-only" );
static const QString peerHash = QStringLiteral( "upgrade-profile-peer-hash" );
static const QString groupIdentity = QStringLiteral( "upgrade-profile-group-id" );
static const QString groupName = QStringLiteral( "旧版本合成会话" );
static const QString localText = QStringLiteral( "旧版本本人消息<br>second line retained" );
static const QString peerText = QStringLiteral( "旧版本对端消息" );
static const QString pendingText = QStringLiteral( "升级后仍待发送的消息" );
static const QDateTime timestamp( QDate( 2026, 11, 3 ), QTime( 9, 12, 15 ), Qt::UTC );

class RegistryFixture
{
public:
  RegistryFixture( const QString& token, bool seed ) : handle( nullptr ), active( false )
  {
    name = QStringLiteral( "Software\\ShadowChatUpgradeProfileTest\\%1" ).arg( token );
    DWORD disposition = 0;
    const LONG result = seed
      ? RegCreateKeyExW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( name.utf16() ), 0, nullptr, 0,
          KEY_ALL_ACCESS, nullptr, &handle, &disposition )
      : RegOpenKeyExW( HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>( name.utf16() ), 0, KEY_ALL_ACCESS, &handle );
    if( result == ERROR_SUCCESS && (!seed || disposition == REG_CREATED_NEW_KEY) )
      active = RegOverridePredefKey( HKEY_CURRENT_USER, handle ) == ERROR_SUCCESS;
  }
  ~RegistryFixture()
  {
    if( active ) RegOverridePredefKey( HKEY_CURRENT_USER, nullptr );
    if( handle ) RegCloseKey( handle );
  }
  bool valid() const { return active; }
  QString path() const { return name; }
private:
  QString name;
  HKEY handle;
  bool active;
};

static QByteArray bytes( const QString& path )
{
  QFile file( path );
  return file.open( QIODevice::ReadOnly ) ? file.readAll() : QByteArray();
}

static bool writeJson( const QString& path, const QJsonObject& object )
{
  QSaveFile file( path );
  const QByteArray data = QJsonDocument( object ).toJson();
  return file.open( QIODevice::WriteOnly ) && file.write( data ) == data.size() && file.commit();
}

static QJsonObject hashes( const QString& root )
{
  QJsonObject result;
  QDirIterator iterator( root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories );
  while( iterator.hasNext() )
  {
    const QString path = iterator.next();
    result.insert( QDir( root ).relativeFilePath( path ),
      QString::fromLatin1( QCryptographicHash::hash( bytes( path ), QCryptographicHash::Sha256 ).toHex() ) );
  }
  return result;
}

static QString attachmentPath( const QString& profile )
{
  return QDir( profile ).filePath( QStringLiteral( "received/原稿 & layout.psd" ) );
}

static QByteArray attachmentBytes()
{
  QByteArray result( "synthetic upgrade attachment\n" );
  result.append( '\0' );
  result.append( "binary content retained" );
  return result;
}

static QString startupCommand()
{
  QSettings run( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat );
  return run.value( "ShadowChat" ).toString();
}

static int startupPreference()
{
  QSettings preference( "HKEY_CURRENT_USER\\Software\\shadowyli\\ShadowChat", QSettings::NativeFormat );
  return preference.value( "StartupEnabled", -1 ).toInt();
}

static int seedProfile( const QString& root, const QString& accountsRoot, const QString& profile )
{
  Settings& settings = Settings::instance();
  ShadowLocalAccounts accounts( accountsRoot );
  QString error;
  CHECK( accounts.create( email, password, &error ) );
  accounts.rememberLogin( email );
  accounts.setAutoLoginEnabled( false );
  CHECK( QDir().mkpath( QDir( profile ).filePath( "received" ) ) );
  QFile attachment( attachmentPath( profile ) );
  CHECK( attachment.open( QIODevice::WriteOnly ) );
  CHECK( attachment.write( attachmentBytes() ) == attachmentBytes().size() );
  attachment.close();

  User local = settings.localUser();
  local.setName( QStringLiteral( "合成本人" ) );
  local.setAccountName( "upgrade-local" );
  local.setDomainName( "example.test" );
  local.setHash( "upgrade-profile-local-hash" );
  settings.setLocalUser( local );
  UserManager::instance().setUser( local );
  User peer( 65002 );
  peer.setName( QStringLiteral( "合成对端" ) );
  peer.setAccountName( "upgrade-peer" );
  peer.setDomainName( "example.test" );
  peer.setHash( peerHash );
  peer.setNetworkAddress( NetworkAddress( QHostAddress::LocalHost, 64621 ) );
  UserManager::instance().setUser( peer );
  Group group;
  group.setId( 65001 );
  group.setName( groupName );
  group.setPrivateId( groupIdentity );
  group.setChatType( Group::GroupChat );
  group.setOwnerHash( local.hash() );
  group.setLastModified( timestamp );
  group.addUser( ID_LOCAL_USER );
  group.addUser( peer.id() );
  Chat chat;
  chat.setGroup( group );
  Message outgoing( Message::Chat, 65101, localText );
  outgoing.setTimestamp( timestamp );
  chat.addMessage( ChatMessage( ID_LOCAL_USER, outgoing, ChatMessage::Chat, true ) );
  Message incoming( Message::Chat, 65102, peerText );
  incoming.setTimestamp( timestamp.addSecs( 1 ) );
  chat.addMessage( ChatMessage( peer.id(), incoming, ChatMessage::Chat, true ) );
  Message file = Protocol::instance().systemMessage( QStringLiteral(
    "<!--shadow-attachment--><!--shadow-sender:%1--><a href=\"%2\">原稿 &amp; layout.psd</a>" )
    .arg( QString::fromLatin1( peer.name().toUtf8().toBase64() ),
      QUrl::fromLocalFile( attachment.fileName() ).toString().toHtmlEscaped() ) );
  file.setTimestamp( timestamp.addSecs( 2 ) );
  chat.addMessage( ChatMessage( ID_SYSTEM_MESSAGE, file, ChatMessage::FileTransfer, true ) );
  ChatManager::instance().setChat( chat );
  settings.setUserList( QStringList{ Protocol::instance().saveUser( peer ) } );
  settings.setGroupList( QStringList{ Protocol::instance().saveGroup( group ) } );
  settings.setUserRecognitionMethod( Settings::RecognizeByAccountAndDomain );
  settings.setSaveUserList( true );
  settings.setSaveGroupList( true );
  settings.setChatAutoSave( true );
  settings.setChatSaveUnsentMessages( true );
  settings.setChatSaveFileTransfers( true );
  settings.setChatSaveSystemMessages( true );
  settings.setClearCacheAfterDays( -1 );
  settings.setRemovePartiallyDownloadedFilesAfterDays( -1 );
  settings.setBeepOnNewMessageArrived( false );
  settings.setShowChatMessageOnTray( false );
  settings.setUseReturnToSendMessage( false );
  settings.setDownloadDirectory( QDir::toNativeSeparators( QDir( profile ).filePath( "received" ) ) );
  Message pending( Message::Chat, 65201, pendingText );
  pending.setTimestamp( timestamp.addSecs( 10 ) );
  pending.setImportant();
  MessageManager::instance().addMessageToSend( peer.id(), chat.id(), pending );
  CHECK( MessageManager::instance().saveMessages( true ) );

  ShadowTransferQueue queue;
  CHECK( queue.open( profile ) );
  CHECK( queue.enqueue( peerHash, groupIdentity, true, attachment.fileName(), &error, "upgrade-batch" ) );
  CHECK( queue.tasks().size() == 1 );
  ShadowMessageDelivery delivery;
  CHECK( delivery.open( profile ) );
  CHECK( delivery.begin( "upgrade-delivery", groupIdentity, QStringList{ "delivered-peer", "pending-peer" }, pendingText ) );
  CHECK( delivery.acknowledge( "upgrade-delivery", "delivered-peer" ).delivered == 1 );
  ShadowConversationPreferences preferences( profile );
  CHECK( preferences.setPinned( chat, true ) && preferences.setMuted( chat, true ) );
  CHECK( settings.addStartOnSystemBoot() && settings.hasStartOnSystemBoot() );
  CHECK( settings.removeStartOnSystemBoot() && !settings.hasStartOnSystemBoot() );
  CHECK( startupPreference() == 0 && startupCommand().isEmpty() );
  QJsonObject manifest;
  manifest.insert( "schema", 1 );
  manifest.insert( "writerVersion", settings.version( false, false, false ) );
  manifest.insert( "transferId", queue.tasks().first().id );
  manifest.insert( "dataSha256", hashes( accountsRoot ) );
  CHECK( writeJson( QDir( root ).filePath( "fixture.json" ), manifest ) );
  std::puts( "PASS seed through production account/password, history writer, offline-message writer, file queue, delivery ledger, settings and explicit startup opt-out APIs" );
  return 0;
}

static int verifyProfile( const QString& root, const QString& accountsRoot, const QString& profile, QJsonObject& report )
{
  const QJsonObject manifest = QJsonDocument::fromJson( bytes( QDir( root ).filePath( "fixture.json" ) ) ).object();
  CHECK( manifest.value( "schema" ).toInt() == 1 && manifest.value( "writerVersion" ).toString() == "1.1.9" );
  const QJsonObject originalHashes = manifest.value( "dataSha256" ).toObject();
  CHECK( !originalHashes.isEmpty() && hashes( accountsRoot ) == originalHashes );
  ShadowLocalAccounts accounts( accountsRoot );
  CHECK( accounts.exists( email ) && accounts.authenticate( email.toUpper(), password ) );
  CHECK( !accounts.authenticate( email, "wrong-synthetic-password" ) );
  CHECK( !accounts.autoLoginEnabled() && accounts.autoLoginEmail().isEmpty() );
  QSettings session( QDir( accountsRoot ).filePath( "session.ini" ), QSettings::IniFormat );
  CHECK( session.value( "autoLoginEmail" ).toString() == email );
  Settings& settings = Settings::instance();
  CHECK( !settings.beepOnNewMessageArrived() && !settings.showChatMessageOnTray() );
  CHECK( !settings.useReturnToSendMessage() );
  CHECK( settings.downloadDirectory() == QDir::toNativeSeparators( QDir( profile ).filePath( "received" ) ) );
  CHECK( settings.chatAutoSave() && settings.chatSaveUnsentMessages() );
  CHECK( settings.userList().size() == 1 && settings.groupList().size() == 1 );
  UserManager::instance().setUser( settings.localUser() );
  const User peer = Protocol::instance().loadUser( settings.userList().first() );
  CHECK( peer.isValid() && peer.hash() == peerHash && peer.accountName() == "upgrade-peer" );
  UserManager::instance().setUser( peer );
  const Group group = Protocol::instance().loadGroup( settings.groupList().first() );
  CHECK( group.isValid() && group.privateId() == groupIdentity && group.name() == groupName );
  CHECK( group.usersId().contains( peer.id() ) && group.usersId().contains( ID_LOCAL_USER ) );
  Chat chat;
  chat.setGroup( group );
  ChatManager::instance().setChat( chat );
  MessageManager::instance().loadSavedMessagesAuthCode();
  BuildSavedChatList loader;
  loader.buildList();
  CHECK( loader.savedChats().size() == 1 && loader.savedChats().contains( groupName ) );
  CHECK( loader.savedChatsAuthCode() == MessageManager::instance().savedMessagesAuthCode() );
  CHECK( loader.unsentMessagesAuthCode() == loader.savedChatsAuthCode() && !loader.savedChatsAuthCode().isEmpty() );
  const QList<HistoryRecord> records = shadowChatHistoryRecords( loader.savedChats().value( groupName ) );
  CHECK( records.size() == 3 );
  CHECK( records.at( 0 ).text.contains( QStringLiteral( "旧版本本人消息" ) ) && records.at( 0 ).text.contains( "second line retained" ) );
  CHECK( records.at( 0 ).sender == QStringLiteral( "合成本人" ) && records.at( 0 ).timestamp == timestamp );
  CHECK( records.at( 1 ).text.contains( peerText ) && records.at( 1 ).sender == QStringLiteral( "合成对端" ) );
  CHECK( records.at( 1 ).timestamp == timestamp.addSecs( 1 ) );
  CHECK( records.at( 2 ).type == ChatMessage::FileTransfer && records.at( 2 ).files.size() == 1 );
  CHECK( QDir::cleanPath( QUrl( records.at( 2 ).files.first() ).toLocalFile() ) == QDir::cleanPath( attachmentPath( profile ) ) );
  CHECK( records.at( 2 ).timestamp == timestamp.addSecs( 2 ) );
  CHECK( bytes( attachmentPath( profile ) ) == attachmentBytes() );
  CHECK( loader.unsentMessages().size() == 1 );
  const MessageRecord pending = loader.unsentMessages().first();
  CHECK( pending.isValid() && pending.toUserId() == peer.id() && pending.chatId() == chat.id() );
  CHECK( pending.message().id() == 65201 && pending.message().type() == Message::Chat );
  CHECK( pending.message().text() == pendingText && pending.message().isImportant() );
  CHECK( pending.message().timestamp() == timestamp.addSecs( 10 ) );
  ShadowTransferQueue queue;
  QString error;
  CHECK( queue.open( profile ) && queue.tasks().size() == 1 );
  const ShadowTransferQueue::Task task = queue.tasks().first();
  CHECK( task.id == manifest.value( "transferId" ).toString() && task.batch == "upgrade-batch" );
  CHECK( task.recipient == peerHash && task.chat == groupIdentity && task.group && task.state == "pending" );
  CHECK( task.source == ShadowTransferQueue::canonicalPath( attachmentPath( profile ) ) && task.files.size() == 1 );
  CHECK( task.completed.isEmpty() && ShadowTransferQueue::sourceUnchanged( task, &error ) );
  ShadowConversationPreferences preferences( profile );
  CHECK( preferences.pinned( chat ) && preferences.muted( chat ) && !preferences.hidden( chat ) );
  ShadowMessageDelivery delivery;
  CHECK( delivery.open( profile ) && !delivery.acknowledge( "upgrade-delivery", "delivered-peer" ).changed );
  QTemporaryDir scratch( QDir( root ).filePath( "ledger-read-XXXXXX" ) );
  CHECK( scratch.isValid() );
  CHECK( QFile::copy( QDir( profile ).filePath( "message-delivery.json" ), QDir( scratch.path() ).filePath( "message-delivery.json" ) ) );
  CHECK( delivery.open( scratch.path() ) );
  const ShadowMessageDelivery::Result last = delivery.acknowledge( "upgrade-delivery", "pending-peer" );
  CHECK( last.changed && last.delivered == 2 && last.total == 2 && last.chat == groupIdentity && last.preview == pendingText );
  CHECK( startupPreference() == 0 && startupCommand().isEmpty() && !settings.hasStartOnSystemBoot() );
  CHECK( settings.configureStartOnSystemBoot( false ) && settings.configureStartOnSystemBoot( true ) );
  CHECK( startupPreference() == 0 && startupCommand().isEmpty() && !settings.hasStartOnSystemBoot() );
  CHECK( hashes( accountsRoot ) == originalHashes );
  report.insert( "writerVersion", manifest.value( "writerVersion" ) );
  report.insert( "passwordAuthenticationPassed", true );
  report.insert( "historyRecordsParsed", records.size() );
  report.insert( "unsentMessagesParsed", loader.unsentMessages().size() );
  report.insert( "fileQueueTasksParsed", queue.tasks().size() );
  report.insert( "attachmentPathAndContentsRead", true );
  report.insert( "contactGroupAndSettingsRead", true );
  QJsonArray supportedSettings;
  supportedSettings << "UseKeyReturnToSendMessage" << "BeepOnNewMessageArrived"
    << "ShowChatMessageOnTray" << "DownloadDirectory" << "autoLoginEmail"
    << "autoLoginEnabled" << "conversationPinned" << "conversationMuted";
  report.insert( "supportedSettingsVerified", supportedSettings );
  report.insert( "confirmOnDownloadFileFixedPolicyObserved", settings.confirmOnDownloadFile() );
  report.insert( "deliveryLedgerCompletedOnScratchCopy", last.delivered );
  report.insert( "startupExplicitOptOutPreserved", true );
  report.insert( "dataSha256Unchanged", true );
  report.insert( "dataSha256", originalHashes );
  std::puts( "PASS actual readers: password authentication, persisted contact/group, three history records with sender/time/file path, attachment content, unsent message, file queue, delivery state, settings and explicit startup opt-out" );
  return 0;
}

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  if( argc != 4 )
  {
    std::fputs( "Usage: upgrade_profile_test.exe seed|verify ABSOLUTE_TEMP_FIXTURE EXPECTED_OBJECT_VERSION\n", stderr );
    return 2;
  }
  const QString mode = QString::fromLocal8Bit( argv[1] );
  const QString root = QDir::cleanPath( QDir::fromNativeSeparators( QString::fromLocal8Bit( argv[2] ) ) );
  CHECK( mode == "seed" || mode == "verify" );
  const QRegularExpression tokenPattern( "^ShadowChatUpgradeProfile-([0-9a-f]{32})$" );
  const QRegularExpressionMatch token = tokenPattern.match( QFileInfo( root ).fileName() );
  CHECK( QFileInfo( root ).isAbsolute() && token.hasMatch() && !QFileInfo( root ).isSymLink() );
  CHECK( QDir( QFileInfo( root ).absolutePath() ).canonicalPath().compare( QDir( QDir::tempPath() ).canonicalPath(), Qt::CaseInsensitive ) == 0 );
  if( mode == "seed" )
  {
    CHECK( !QFileInfo::exists( root ) && QDir().mkpath( root ) );
  }
  else CHECK( QFileInfo::exists( QDir( root ).filePath( "fixture.json" ) ) );
  RegistryFixture registry( token.captured( 1 ), mode == "seed" );
  CHECK( registry.valid() );
  qputenv( "QT_QPA_PLATFORM", "offscreen" );
  BeeApplication application( argc, argv );
  Random::init();
  const QString accountsRoot = QDir( root ).filePath( "accounts" );
  const QString profile = QDir( accountsRoot ).filePath( email );
  Settings& settings = Settings::instance();
  const QString expectedObjectVersion = QString::fromLocal8Bit( argv[3] );
  CHECK( settings.version( false, false, false ) == expectedObjectVersion );
  if( mode == "seed" ) CHECK( settings.version( false, false, false ) == "1.1.9" );
  settings.setDefaultFolders();
  settings.createLocalUser( "synthetic-upgrade-profile" );
  settings.setAccountDataFolder( profile );
  settings.load();
  QJsonObject report;
  const int result = mode == "seed" ? seedProfile( root, accountsRoot, profile ) : verifyProfile( root, accountsRoot, profile, report );
  if( result ) return result;
  report.insert( "passed", true );
  report.insert( "mode", mode );
  report.insert( "objectVersion", settings.version( false, false, false ) );
  report.insert( "fixture", root );
  report.insert( "registryFixture", registry.path() );
  report.insert( "coreCreated", false );
  report.insert( "networkStarted", false );
  CHECK( writeJson( QDir( root ).filePath( mode + "-" + settings.version( false, false, false ) + "-result.json" ), report ) );
  std::printf( "PASS %s objectVersion=%s fixture=%s\n", qPrintable( mode ), qPrintable( settings.version( false, false, false ) ), qPrintable( root ) );
  return 0;
}
