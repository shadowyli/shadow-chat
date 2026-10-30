#include "ShadowDevPreview.h"
#include "ShadowBuild.h"
#include "ShadowLocalAccounts.h"
#include "Settings.h"
#include "GuiMain.h"
#include "GuiChat.h"
#include "ShadowDirectoryPage.h"
#include "GuiChatList.h"
#include "GuiUserList.h"
#include "GuiGroupList.h"
#include "GuiFileTransfer.h"
#include "ChatManager.h"
#include "UserManager.h"
#include "Core.h"
#include "Message.h"
#include "Protocol.h"
#include "Updater.h"
#include <QtWidgets>

namespace {
const char* markerText = "shadow-native-preview-synthetic-v1";
QString option( const QStringList& arguments, const QString& name )
{
  const int index = arguments.indexOf( name );
  return index >= 0 && index + 1 < arguments.size() ? arguments.at( index + 1 ) : QString();
}
bool writeJson( const QString& filename, const QJsonObject& object )
{
  QSaveFile file( filename );
  const QByteArray data = QJsonDocument( object ).toJson();
  return file.open( QIODevice::WriteOnly ) && file.write( data ) == data.size() && file.commit();
}
}

bool ShadowDevPreview::active()
{
#if defined(SHADOWCHAT_BUILD_LOCAL) && !defined(SHADOWCHAT_BUILD_MAIN) && !defined(SHADOWCHAT_BUILD_DEV)
  return qApp && qApp->property( "shadowDevPreview" ).toBool();
#else
  return false;
#endif
}

bool ShadowDevPreview::checkArguments( int argc, char** argv, QString* error )
{
  QStringList arguments;
  for( int index = 0; index < argc; ++index )
    arguments << QString::fromLocal8Bit( argv[index] );
  bool requested = false;
  for( const QString& argument : arguments )
    requested = requested || argument.startsWith( "--dev-" );
  if( !requested ) return true;
#if !defined(SHADOWCHAT_BUILD_LOCAL) || defined(SHADOWCHAT_BUILD_MAIN) || defined(SHADOWCHAT_BUILD_DEV)
  *error = "Native dev preview arguments require an explicit local build.";
  return false;
#else
  const QStringList valued = QStringList() << "--dev-preview" << "--dev-instance" << "--profile-root";
  for( int index = 1; index < arguments.size(); ++index )
  {
    const QString argument = arguments.at( index );
    if( valued.contains( argument ) )
    {
      if( arguments.count( argument ) != 1 || index + 1 >= arguments.size() || arguments.at( index + 1 ).startsWith( "--" ) )
      {
        *error = "Missing or duplicate preview argument: " + argument;
        return false;
      }
      ++index;
    }
    else if( argument != "--dev-seed" )
    {
      *error = "Unknown argument in dev preview: " + argument;
      return false;
    }
  }
  if( !QDir::isAbsolutePath( option( arguments, "--dev-preview" ) ) ||
      !QDir::isAbsolutePath( option( arguments, "--profile-root" ) ) ||
      !QRegularExpression( "^[a-f0-9-]{36}$" ).match( option( arguments, "--dev-instance" ) ).hasMatch() )
  {
    *error = "Dev preview requires absolute config/profile directories and a UUID instance.";
    return false;
  }
  return true;
#endif
}

bool ShadowDevPreview::configure( const QStringList& arguments, QString* error )
{
  if( !arguments.contains( "--dev-preview" ) ) return true;
  const QUrl updateSource = Updater::productionManifestUrl();
  const QHostAddress updateHost( updateSource.host() );
  if( !updateSource.isValid() || (updateSource.scheme() != "http" && updateSource.scheme() != "https") ||
      updateHost.isNull() || !updateHost.isLoopback() )
  {
    *error = "Native preview requires the actual update source to use a literal loopback address.";
    return false;
  }
  const QString profile = QFileInfo( option( arguments, "--profile-root" ) ).canonicalFilePath();
  if( profile.isEmpty() || !QFileInfo( profile ).isDir() )
  {
    *error = "Synthetic dev-preview profile directory does not exist.";
    return false;
  }
  QFile marker( QDir( profile ).filePath( ".shadow-dev-preview" ) );
  if( !marker.open( QIODevice::ReadOnly ) || marker.readAll().trimmed() != markerText )
  {
    *error = "Refusing a profile without the synthetic dev-preview ownership marker.";
    return false;
  }
  const QString config = QFileInfo( option( arguments, "--dev-preview" ) ).canonicalFilePath();
  if( config.isEmpty() || !QFileInfo( config ).isDir() )
  {
    *error = "Dev preview config directory does not exist.";
    return false;
  }
  qApp->setProperty( "shadowDevPreview", true );
  qApp->setProperty( "shadowDevConfig", config );
  qApp->setProperty( "shadowDevInstance", option( arguments, "--dev-instance" ) );
  qApp->setProperty( "shadowDevSeed", arguments.contains( "--dev-seed" ) );
  qApp->setProperty( "shadowAccountRoot", profile );
  return true;
}

bool ShadowDevPreview::seedAccount( QString* error )
{
  if( !active() || !qApp->property( "shadowDevSeed" ).toBool() ) return true;
  ShadowLocalAccounts accounts;
  const QString email = "preview@example.test";
  const QString password = "synthetic-preview-only";
  if( !accounts.exists( email ) && !accounts.create( email, password, error ) ) return false;
  if( !accounts.authenticate( email, password ) )
  {
    *error = "Synthetic account password differs; use normal login or a new preview profile.";
    return false;
  }
  accounts.setAutoLoginEnabled( true );
  accounts.rememberLogin( email );
  return true;
}

void ShadowDevPreview::prepareSettings()
{
  if( !active() ) return;
  Settings& settings = Settings::instance();
  settings.setFirstTime( false );
  settings.setAskChangeUserAtStartup( false );
  settings.setBeepOnNewMessageArrived( false );
  settings.setHideEmptyChatsInList( false );
  settings.setDelayConnectionAtStartup( 0 );
  const QString output = QFileInfo( qApp->property( "shadowAccountRoot" ).toString() ).dir().absolutePath();
  QDir().mkpath( output + "/logs" );
  settings.setLogPath( output + "/logs" );
  settings.setLogToFile( true );
  User user = settings.localUser();
  user.setName( QString::fromUtf8( "预览体验员" ) );
  VCard card = user.vCard();
  card.setNickName( QString::fromUtf8( "预览体验员" ) );
  card.setInfo( QString::fromUtf8( "本地合成资料 · 开发预览" ) );
  user.setVCard( card );
  settings.setLocalUser( user );
}

bool ShadowDevPreview::validateStyle( const QByteArray& data, QString* error )
{
  if( data.size() > 256 * 1024 || data.contains( '\0' ) )
  {
    *error = "QSS exceeds 256 KiB or contains NUL.";
    return false;
  }
  const QString text = QString::fromUtf8( data );
  bool comment = false;
  QChar quote;
  bool escaped = false;
  int braces = 0;
  for( int index = 0; index < text.size(); ++index )
  {
    const QChar character = text.at( index );
    const QChar next = index + 1 < text.size() ? text.at( index + 1 ) : QChar();
    if( comment )
    {
      if( character == '*' && next == '/' ) { comment = false; ++index; }
      continue;
    }
    if( !quote.isNull() )
    {
      if( escaped ) escaped = false;
      else if( character == '\\' ) escaped = true;
      else if( character == quote ) quote = QChar();
      continue;
    }
    if( character == '/' && next == '*' ) { comment = true; ++index; }
    else if( character == '\'' || character == '"' ) quote = character;
    else if( character == '{' ) ++braces;
    else if( character == '}' && --braces < 0 ) break;
  }
  if( braces != 0 || comment || !quote.isNull() )
  {
    *error = "QSS has unbalanced braces, comment or quote. Other Qt syntax/property errors are not validated.";
    return false;
  }
  return true;
}

bool ShadowDevPreview::validateLayout( const QByteArray& data, QJsonObject* values, QString* error )
{
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson( data, &parseError );
  if( data.size() > 8192 || parseError.error != QJsonParseError::NoError || !document.isObject() )
  {
    *error = "Layout must be a JSON object <= 8 KiB: " + parseError.errorString();
    return false;
  }
  const QJsonObject object = document.object();
  const QMap<QString, QPair<int,int> > limits = {
    { "windowWidth", qMakePair( 900, 2560 ) },
    { "windowHeight", qMakePair( 600, 1600 ) },
    { "conversationListWidth", qMakePair( 272, 380 ) }
  };
  for( auto iterator = object.begin(); iterator != object.end(); ++iterator )
  {
    if( !limits.contains( iterator.key() ) || !iterator.value().isDouble() )
    {
      *error = "Unknown layout key or non-number: " + iterator.key();
      return false;
    }
    const double value = iterator.value().toDouble();
    const auto range = limits.value( iterator.key() );
    if( value != qFloor( value ) || value < range.first || value > range.second )
    {
      *error = QString( "%1 requires an integer in [%2, %3]." ).arg( iterator.key() ).arg( range.first ).arg( range.second );
      return false;
    }
  }
  *values = object;
  return true;
}

ShadowDevPreview::ShadowDevPreview( GuiMain* window, Core* core )
  : QObject( window ), m_window( window ), m_revision( 0 ), m_started( false ), m_stopping( false )
{
  setObjectName( "ShadowDevPreview" );
  m_config = qApp->property( "shadowDevConfig" ).toString();
  m_profileRoot = QFileInfo( qApp->property( "shadowAccountRoot" ).toString() ).canonicalFilePath();
  m_output = QFileInfo( m_profileRoot ).dir().absolutePath();
  m_instance = qApp->property( "shadowDevInstance" ).toString();
  QDir().mkpath( m_output + "/logs" );
  QDir().mkpath( m_output + "/control" );
  m_baselineStyle = window->styleSheet();
  m_baselineGeometry = window->geometry();
  QWidget* list = window->findChild<QWidget*>( "ShadowListPanel" );
  m_baselineListMin = list ? list->minimumWidth() : 272;
  m_baselineListMax = list ? list->maximumWidth() : 380;
  m_debounce.setSingleShot( true );
  m_debounce.setInterval( 180 );
  connect( &m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() { m_debounce.start(); } );
  connect( &m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() { m_debounce.start(); } );
  connect( &m_debounce, &QTimer::timeout, this, &ShadowDevPreview::reload );
  connect( &m_poll, &QTimer::timeout, this, &ShadowDevPreview::pollControl );
  m_poll.start( 250 );
  QShortcut* shortcut = new QShortcut( QKeySequence( "Ctrl+Alt+R" ), window );
  connect( shortcut, &QShortcut::activated, this, &ShadowDevPreview::reload );
  QShortcut* baseline = new QShortcut( QKeySequence( "Ctrl+Alt+0" ), window );
  connect( baseline, &QShortcut::activated, this, &ShadowDevPreview::restoreBaseline );
  window->installEventFilter( this );
  connect( qApp, &QCoreApplication::aboutToQuit, this, &ShadowDevPreview::saveSession );
  connect( core, &Core::savedChatListAvailable, this, &ShadowDevPreview::clearFixtureHistory );
  QTimer::singleShot( 350, this, [this, core]() {
    seedFixtures( core );
    reload();
    restoreSession();
    m_window->setWindowTitle( QString::fromUtf8( "影聊 · 原生开发预览 [LOCAL / SYNTHETIC]" ) );
    m_window->showNormal();
    m_window->raise();
    m_started = true;
    report( "ready" );
  } );
  watchFiles();
}

void ShadowDevPreview::watchFiles()
{
  QStringList paths;
  paths << m_config << QFileInfo( m_config ).dir().absolutePath()
        << m_config + "/preview.qss" << m_config + "/layout.json";
  for( const QString& filename : paths )
    if( QFileInfo::exists( filename ) && !m_watcher.files().contains( filename ) && !m_watcher.directories().contains( filename ) )
      m_watcher.addPath( filename );
}

void ShadowDevPreview::reload()
{
  watchFiles();
  QStringList diagnostics;
  QFile style( m_config + "/preview.qss" );
  if( !style.open( QIODevice::ReadOnly ) ) diagnostics << "Cannot read preview.qss; keeping last usable style.";
  else
  {
    const QByteArray data = style.read( 256 * 1024 + 1 );
    QString error;
    if( !validateStyle( data, &error ) ) diagnostics << error;
    else
    {
      m_window->setStyleSheet( m_baselineStyle + "\n" + QString::fromUtf8( data ) );
      m_styleData = data;
    }
  }
  QFile layout( m_config + "/layout.json" );
  if( !layout.open( QIODevice::ReadOnly ) ) diagnostics << "Cannot read layout.json; keeping last usable layout.";
  else
  {
    const QByteArray data = layout.read( 8193 );
    QJsonObject values;
    QString error;
    if( !validateLayout( data, &values, &error ) ) diagnostics << error;
    else if( data != m_layoutData )
    {
      m_window->resize( values.value( "windowWidth" ).toInt( m_baselineGeometry.width() ),
                        values.value( "windowHeight" ).toInt( m_baselineGeometry.height() ) );
      if( QWidget* list = m_window->findChild<QWidget*>( "ShadowListPanel" ) )
      {
        list->setMinimumWidth( m_baselineListMin );
        list->setMaximumWidth( m_baselineListMax );
        if( values.contains( "conversationListWidth" ) )
          list->setFixedWidth( values.value( "conversationListWidth" ).toInt() );
        if( QSplitter* splitter = qobject_cast<QSplitter*>( list->parentWidget() ) )
          splitter->setSizes( QList<int>() << list->width()
                             << splitter->width() - list->width() - splitter->handleWidth() );
      }
      m_layout = values;
      m_layoutData = data;
    }
  }
  ++m_revision;
  QTimer::singleShot( 30, this, [this, diagnostics]() { report( "reload", diagnostics.join( " " ) ); } );
}

void ShadowDevPreview::restoreBaseline()
{
  m_window->setStyleSheet( m_baselineStyle );
  m_window->setGeometry( m_baselineGeometry );
  if( QWidget* list = m_window->findChild<QWidget*>( "ShadowListPanel" ) )
  {
    list->setMinimumWidth( m_baselineListMin );
    list->setMaximumWidth( m_baselineListMax );
  }
  m_styleData.clear();
  m_layoutData.clear();
  m_layout = QJsonObject();
  ++m_revision;
  report( "baseline" );
}

bool ShadowDevPreview::eventFilter( QObject* watched, QEvent* event )
{
  if( watched == m_window && m_started && (event->type() == QEvent::Move || event->type() == QEvent::Resize) )
    saveSession();
  return QObject::eventFilter( watched, event );
}

void ShadowDevPreview::saveSession()
{
  if( !m_started ) return;
  QSettings session( m_profileRoot + "/preview-window.ini", QSettings::IniFormat );
  session.setValue( "geometry", m_window->saveGeometry() );
  session.setValue( "layout", m_layoutData );
  if( QLabel* title = m_window->findChild<QLabel*>( "ShadowSectionTitle" ) ) session.setValue( "page", title->text() );
  for( GuiChat* chat : m_window->findChildren<GuiChat*>() )
    if( chat->isVisible() ) session.setValue( "chat", QString::number( chat->chatId() ) );
  session.sync();
}

void ShadowDevPreview::restoreSession()
{
  QSettings session( m_profileRoot + "/preview-window.ini", QSettings::IniFormat );
  if( session.value( "layout" ).toByteArray() == m_layoutData )
    m_window->restoreGeometry( session.value( "geometry" ).toByteArray() );
  const VNumber chat = session.value( "chat", "88001" ).toString().toULongLong();
  if( ChatManager::instance().chat( chat ).isValid() )
    QMetaObject::invokeMethod( m_window, "showChat", Qt::DirectConnection, Q_ARG( VNumber, chat ) );
  const QString page = session.value( "page" ).toString();
  const char* method = "showModernMessages";
  if( page == QString::fromUtf8( "通讯录" ) ) method = "showModernContacts";
  else if( page == QString::fromUtf8( "群聊" ) ) method = "showModernGroups";
  else if( page == QString::fromUtf8( "组织架构" ) ) method = "showModernOrganization";
  else if( page == QString::fromUtf8( "系统活动" ) ) method = "showModernActivities";
  else if( page == QString::fromUtf8( "聊天存档" ) ) method = "showModernSavedChats";
  QMetaObject::invokeMethod( m_window, method, Qt::DirectConnection );
}

void ShadowDevPreview::pollControl()
{
  if( QLabel* network = m_window->findChild<QLabel*>( "ShadowNetworkText" ) )
  {
    const QString text = QString::fromUtf8( "本机预览 · 不连接办公网络" );
    if( network->text() != text ) network->setText( text );
  }
  if( m_stopping ) return;
  const QString filename = m_output + "/control/" + m_instance + ".stop";
  if( QFileInfo::exists( filename ) )
  {
    m_stopping = true;
    saveSession();
    report( "stopping" );
    QMetaObject::invokeMethod( m_window, "forceShutdown", Qt::QueuedConnection );
    return;
  }
  static int ticks = 0;
  if( ++ticks % 4 == 0 ) saveSession();
}

void ShadowDevPreview::report( const QString& event, const QString& diagnostic )
{
  if( QLabel* network = m_window->findChild<QLabel*>( "ShadowNetworkText" ) )
    network->setText( QString::fromUtf8( "本机预览 · 不连接办公网络" ) );
  QJsonObject status;
  status["event"] = event;
  status["pid"] = static_cast<double>( QCoreApplication::applicationPid() );
  status["executable"] = QCoreApplication::applicationFilePath();
  status["instance"] = m_instance;
  status["revision"] = m_revision;
  status["diagnostic"] = diagnostic;
  status["network"] = "loopback-only; discovery/Hive/mDNS/file-server disabled";
  status["qssValidation"] = "size/NUL/balanced braces-comments-quotes only; no general Qt QSS parser";
  status["width"] = m_window->width();
  status["height"] = m_window->height();
  status["visible"] = m_window->isVisible();
  status["windowTitle"] = m_window->windowTitle();
  if( QLabel* network = m_window->findChild<QLabel*>( "ShadowNetworkText" ) )
    status["networkLabel"] = network->text();
  status["qssSha256"] = QString::fromLatin1( QCryptographicHash::hash( m_styleData, QCryptographicHash::Sha256 ).toHex() );
  if( QWidget* list = m_window->findChild<QWidget*>( "ShadowListPanel" ) ) status["listWidth"] = list->width();
  if( QLabel* title = m_window->findChild<QLabel*>( "ShadowChatTitle" ) )
  {
    status["titleFontPx"] = title->font().pixelSize();
    status["titleColor"] = title->palette().color( QPalette::WindowText ).name();
    status["titleContentsX"] = title->contentsRect().x();
    status["titleHeight"] = title->height();
    status["titleFontMetricsHeight"] = title->fontMetrics().height();
  }
  if( QWidget* list = m_window->findChild<QWidget*>( "ShadowListPanel" ) )
    if( QWidget* conversation = m_window->findChild<QWidget*>( "ShadowConversationPanel" ) )
      status["interPanelGap"] = conversation->mapTo( m_window, QPoint() ).x() -
        list->mapTo( m_window, QPoint( list->width(), 0 ) ).x();
  status["roundedCornerMask"] = !m_window->mask().isEmpty();
  if( ShadowDirectoryPage* directory = m_window->findChild<ShadowDirectoryPage*>( "ShadowDirectoryPage" ) )
  {
    status["directoryVisible"] = directory->isVisible();
    status["directorySection"] = static_cast<int>( directory->section() );
    if( QTreeWidget* list = directory->findChild<QTreeWidget*>( "ShadowDirectoryList" ) )
    {
      int people = 0;
      int groups = 0;
      QTreeWidgetItemIterator item( list );
      while( *item )
      {
        const int kind = (*item)->data( 0, Qt::UserRole + 1 ).toInt();
        people += kind == 1;
        groups += kind == 2;
        ++item;
      }
      status["directoryContactRows"] = people;
      status["directoryGroupRows"] = groups;
    }
  }
  if( QWidget* search = m_window->findChild<QWidget*>( "ShadowGlobalSearch" ) )
    status["searchInTitleBar"] = search->parentWidget() &&
      search->parentWidget()->objectName() == QStringLiteral( "ShadowTitleBar" );
  for( GuiChat* chat : m_window->findChildren<GuiChat*>() )
  {
    if( !chat->isVisible() ) continue;
    status["visibleChatId"] = QString::number( chat->chatId() );
    status["visibleChatMessageCount"] = ChatManager::instance().chat( chat->chatId() ).messages().size();
    if( QTextBrowser* browser = chat->findChild<QTextBrowser*>( "GuiChatViewer" ) )
    {
      const QString plain = browser->toPlainText();
      status["visibleChatPlainHasRawMarkup"] = plain.contains( "<a href" ) || plain.contains( "<img src" );
      status["visibleFixtureWelcomeCount"] = plain.count( QString::fromUtf8( "这是一段合成会话。" ) );
      status["visibleChatHasFixtureImage"] = browser->toHtml().contains( "synthetic-landscape.png" );
      status["visibleChatHasFixtureFileLink"] = browser->toHtml().contains( ".txt" ) && browser->toHtml().contains( "href=\"file:" );
      const QUrl imageUrl = QUrl::fromLocalFile( m_profileRoot + "/fixtures/synthetic-landscape.png" );
      const QVariant resource = browser->document()->resource( QTextDocument::ImageResource, imageUrl );
      status["fixtureImageLoaded"] = !qvariant_cast<QImage>( resource ).isNull() || !qvariant_cast<QPixmap>( resource ).isNull();
    }
  }
  status["utc"] = QDateTime::currentDateTimeUtc().toString( Qt::ISODateWithMs );
  writeJson( m_output + "/logs/preview-status.json", status );
  if( event == "ready" ) writeJson( m_output + "/control/" + m_instance + ".ready.json", status );
  QFile log( m_output + "/logs/preview-events.jsonl" );
  if( log.open( QIODevice::WriteOnly | QIODevice::Append ) )
    log.write( QJsonDocument( status ).toJson( QJsonDocument::Compact ) + '\n' );
  if( event == "reload" || event == "ready" || event == "baseline" )
    m_window->grab().save( m_output + "/logs/preview.png" );
  if( !diagnostic.isEmpty() ) qWarning() << "Native preview:" << diagnostic;
}

void ShadowDevPreview::seedFixtures( Core* core )
{
  clearFixtureHistory();
  for( const User& previous : UserManager::instance().userList().toList() )
  {
    if( !previous.hash().startsWith( "native-preview-fixture-" ) ) continue;
    for( const Chat& previousChat : QList<Chat>( ChatManager::instance().constChatList() ) )
    {
      if( previousChat.hasUser( previous.id() ) && !previousChat.isDefault() )
      {
        ChatManager::instance().removeSavedTextFromChat( previousChat.name() );
        ChatManager::instance().removeChat( previousChat );
        core->chatRemoved( previousChat );
      }
    }
    UserManager::instance().removeUser( previous );
    core->userRemoved( previous );
  }
  const QString assets = m_profileRoot + "/fixtures";
  QDir().mkpath( assets );
  const QString imagePath = assets + "/synthetic-landscape.png";
  QImage image( 420, 220, QImage::Format_RGB32 );
  image.fill( QColor( "#DCE8F8" ) );
  QPainter painter( &image );
  painter.fillRect( 0, 150, 420, 70, QColor( "#7390AF" ) );
  painter.setPen( QColor( "#172033" ) );
  painter.setFont( QFont( "Microsoft YaHei UI", 16 ) );
  painter.drawText( image.rect(), Qt::AlignCenter, QString::fromUtf8( "合成图片 · Native Preview" ) );
  painter.end();
  image.save( imagePath );
  const QString documentPath = assets + "/" + QString::fromUtf8( "合成资料_用于检查非常长的文件名和布局省略号_2026_最终确认版_v012.txt" );
  QFile document( documentPath );
  if( document.open( QIODevice::WriteOnly ) ) document.write( "Synthetic preview attachment. No office content.\n" );
  const QStringList names = QStringList() << QString::fromUtf8( "林舟（合成）" ) << QString::fromUtf8( "陈语（合成）" )
    << QString::fromUtf8( "长名字联系人_用于检查截断（合成）" ) << QString::fromUtf8( "空会话联系人（合成）" )
    << QString::fromUtf8( "Alice（合成）" ) << QString::fromUtf8( "白禾（合成）" )
    << QString::fromUtf8( "丁然（合成）" ) << QString::fromUtf8( "方晴（合成）" )
    << QString::fromUtf8( "顾宁（合成）" ) << QString::fromUtf8( "许知（合成）" )
    << QString::fromUtf8( "张遥（合成）" );
  for( int index = 0; index < names.size(); ++index )
  {
    User user( 87001 + index );
    user.setAccountName( QString( "fixture%1@example.test" ).arg( index + 1 ) );
    user.setName( names.at( index ) );
    user.setHash( QString( "native-preview-fixture-%1" ).arg( index ) );
    user.setStatus( User::Offline );
    const QStringList colors = QStringList() << "#527EE8" << "#DF8977" << "#8B78CA" << "#4BA394"
      << "#A78659" << "#5C9ABD" << "#C578A8" << "#799D61";
    user.setColor( colors.at( index % colors.size() ) );
    user.setWorkgroups( QStringList() << (index % 2
      ? QString::fromUtf8( "项目策划组（合成）" ) : QString::fromUtf8( "视觉设计组（合成）" )) );
    user.setNetworkAddress( NetworkAddress( QHostAddress::LocalHost, 49000 + index ) );
    VCard card;
    card.setNickName( user.name() );
    card.setEmail( user.accountName() );
    card.setInfo( QString::fromUtf8( "完全合成的本地预览联系人" ) );
    user.setVCard( card );
    UserManager::instance().setUser( user );
    core->userChanged( user );
    Group group;
    group.setId( 88001 + index );
    group.setName( index == 1 ? QString::fromUtf8( "设计讨论组（合成）" ) : user.name() );
    group.setChatType( index == 1 ? Group::GroupChat : Group::PrivateChat );
    group.setPrivateId( QString( "native-preview-chat-%1" ).arg( index ) );
    group.addUser( ID_LOCAL_USER );
    group.addUser( user.id() );
    if( index == 1 ) group.addUser( 87001 );
    Chat chat;
    chat.setGroup( group );
    if( index >= 4 )
    {
      const QStringList summaries = QStringList() << QString::fromUtf8( "已收到，下午一起确认设计稿。" )
        << QString::fromUtf8( "新一版方案已经整理好了。" ) << QString::fromUtf8( "请确认这份排期，有问题随时告诉我。" )
        << QString::fromUtf8( "明天的讨论会改到十点。" ) << QString::fromUtf8( "这里的细节需要再确认一下。" )
        << QString::fromUtf8( "收到，我继续跟进。" ) << QString::fromUtf8( "文件整理完毕，请查收。" );
      QString content = summaries.value( index - 4 );
      if( index == 8 ) content.prepend( QStringLiteral( "@%1 " ).arg( Settings::instance().localUser().name() ) );
      Message message( Message::Chat, 89000 + index * 10, content );
      message.setTimestamp( QDateTime::currentDateTime().addSecs( -index * 160 ) );
      chat.addMessage( ChatMessage( user.id(), message, ChatMessage::Chat, false ) );
      if( index == 4 || index == 8 ) chat.addUnreadMessage();
    }
    else if( index != 3 )
    {
      QStringList messages;
      messages << QString::fromUtf8( "这是一段合成会话。修改 dev-preview/preview.qss 后保存，原生窗口会自动刷新。" );
      messages << QString::fromUtf8( "长文检查：段落、换行和内容滚动应保持可用。\n" ).repeated( 12 );
      messages << QString( "<a href=\"%1\">%2</a>" ).arg( QUrl::fromLocalFile( documentPath ).toString(), QFileInfo( documentPath ).fileName() );
      messages << QString( "<a href=\"%1\"><img src=\"%1\" width=\"320\"></a>" ).arg( QUrl::fromLocalFile( imagePath ).toString() );
      const QList<int> previewOrder = QList<int>() << 0 << 3 << 2 << 1;
      for( int messageIndex : previewOrder )
      {
        if( messageIndex >= 2 )
        {
          chat.addMessage( ChatMessage( ID_SYSTEM_MESSAGE, Protocol::instance().systemMessage( messages.at( messageIndex ) ),
            messageIndex == 3 ? ChatMessage::ImagePreview : ChatMessage::FileTransfer, false ) );
        }
        else
        {
          Message message( Message::Chat, 89000 + index * 10 + messageIndex, messages.at( messageIndex ) );
          chat.addMessage( ChatMessage( messageIndex % 2 ? ID_LOCAL_USER : user.id(), message, ChatMessage::Chat, false ) );
        }
      }
      if( index == 1 ) { chat.addUnreadMessage(); chat.addUnreadMessage(); chat.addUnreadMessage(); }
    }
    chat.setMessagesSaved();
    ChatManager::instance().setChat( chat );
    core->chatChanged( chat );
  }
  for( GuiChatList* list : m_window->findChildren<GuiChatList*>() ) list->updateChats();
  for( GuiUserList* list : m_window->findChildren<GuiUserList*>() ) list->updateUsers();
  if( GuiFileTransfer* transfers = m_window->findChild<GuiFileTransfer*>() )
  {
    FileInfo file;
    file.setId( 89901 );
    file.setNameAndSuffix( QFileInfo( documentPath ).fileName() );
    file.setPath( documentPath );
    file.setSize( 8 * 1024 * 1024 );
    const User user = UserManager::instance().findUser( 87001 );
    transfers->setProgress( 89901, user, file, 1024 * 1024, 2 );
    transfers->setMessage( 89901, user, file, QString::fromUtf8( "合成失败：接收方离线，请重新发送。" ), FileTransferPeer::Error );
  }
}

void ShadowDevPreview::clearFixtureHistory()
{
  const QStringList names = QStringList() << QString::fromUtf8( "林舟（合成）" ) << QString::fromUtf8( "陈语（合成）" )
    << QString::fromUtf8( "长名字联系人_用于检查截断（合成）" ) << QString::fromUtf8( "空会话联系人（合成）" )
    << QString::fromUtf8( "设计讨论组（合成）" );
  for( const QString& name : names ) ChatManager::instance().removeSavedTextFromChat( name );
  for( const User& user : UserManager::instance().userList().toList() )
    if( user.hash().startsWith( QStringLiteral( "native-preview-fixture-" ) ) )
      ChatManager::instance().removeSavedTextFromChat( user.name() );
  for( GuiChat* chat : m_window->findChildren<GuiChat*>() )
  {
    const Chat current = ChatManager::instance().chat( chat->chatId() );
    if( current.privateId().startsWith( "native-preview-chat-" ) ) chat->setChat( current );
  }
}
