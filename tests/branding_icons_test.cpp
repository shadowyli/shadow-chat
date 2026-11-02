#include "BeeApplication.h"
#include "Chat.h"
#include "GuiSystemTray.h"
#include "IconManager.h"
#include "Settings.h"
#include <QtTest>
#include <windows.h>
#include <shellapi.h>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

struct NativeCall
{
  DWORD operation;
  UINT flags;
  DWORD infoFlags;
  QString title;
  QString body;
  QImage tray;
  QImage balloon;
  BOOL result;
};

static QVector<NativeCall> nativeCalls;
static bool forwardToShell = false;
typedef BOOL (WINAPI *NotifyFunction)( DWORD, PNOTIFYICONDATAW );
static NotifyFunction originalNotify = nullptr;

static QImage nativeImage( HICON handle )
{
  if( !handle )
    return QImage();
  ICONINFO info = {};
  if( !GetIconInfo( handle, &info ) )
    return QImage();
  BITMAP bitmap = {};
  GetObject( info.hbmColor ? info.hbmColor : info.hbmMask, sizeof( bitmap ), &bitmap );
  const int width = bitmap.bmWidth;
  const int height = info.hbmColor ? bitmap.bmHeight : bitmap.bmHeight / 2;
  if( info.hbmColor )
    DeleteObject( info.hbmColor );
  if( info.hbmMask )
    DeleteObject( info.hbmMask );
  BITMAPINFO format = {};
  format.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
  format.bmiHeader.biWidth = width;
  format.bmiHeader.biHeight = -height;
  format.bmiHeader.biPlanes = 1;
  format.bmiHeader.biBitCount = 32;
  format.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  HDC context = CreateCompatibleDC( nullptr );
  HBITMAP surface = CreateDIBSection( context, &format, DIB_RGB_COLORS, &pixels, nullptr, 0 );
  if( !surface || !pixels )
  {
    if( surface )
      DeleteObject( surface );
    DeleteDC( context );
    return QImage();
  }
  HGDIOBJ previous = SelectObject( context, surface );
  memset( pixels, 0, width * height * 4 );
  DrawIconEx( context, 0, 0, handle, width, height, 0, nullptr, DI_NORMAL );
  QImage image( static_cast<uchar*>( pixels ), width, height, QImage::Format_ARGB32_Premultiplied );
  QImage result = image.copy();
  SelectObject( context, previous );
  DeleteObject( surface );
  DeleteDC( context );
  return result;
}

static BOOL WINAPI captureNotify( DWORD operation, PNOTIFYICONDATAW data )
{
  NativeCall call;
  call.operation = operation;
  call.flags = data->uFlags;
  call.infoFlags = data->dwInfoFlags;
  call.title = QString::fromWCharArray( data->szInfoTitle );
  call.body = QString::fromWCharArray( data->szInfo );
  if( data->uFlags & NIF_ICON )
    call.tray = nativeImage( data->hIcon );
  if( data->uFlags & NIF_INFO )
    call.balloon = nativeImage( data->hBalloonIcon );
  call.result = forwardToShell ? originalNotify( operation, data ) : TRUE;
  nativeCalls.append( call );
  return call.result;
}

static bool hookNativeNotifications()
{
  HMODULE module = GetModuleHandleW( L"qwindows.dll" );
  if( !module )
    return false;
  uchar* base = reinterpret_cast<uchar*>( module );
  IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>( base );
  IMAGE_NT_HEADERS* image = reinterpret_cast<IMAGE_NT_HEADERS*>( base + dos->e_lfanew );
  const DWORD imports = image->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if( !imports )
    return false;
  IMAGE_IMPORT_DESCRIPTOR* library = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>( base + imports );
  for( ; library->Name; ++library )
  {
    if( !library->OriginalFirstThunk )
      continue;
    IMAGE_THUNK_DATA* names = reinterpret_cast<IMAGE_THUNK_DATA*>( base + library->OriginalFirstThunk );
    IMAGE_THUNK_DATA* functions = reinterpret_cast<IMAGE_THUNK_DATA*>( base + library->FirstThunk );
    for( ; names->u1.AddressOfData; ++names, ++functions )
    {
      if( IMAGE_SNAP_BY_ORDINAL( names->u1.Ordinal ) )
        continue;
      IMAGE_IMPORT_BY_NAME* name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>( base + names->u1.AddressOfData );
      if( strcmp( reinterpret_cast<const char*>( name->Name ), "Shell_NotifyIconW" ) != 0 )
        continue;
      DWORD previous = 0;
      if( !VirtualProtect( &functions->u1.Function, sizeof( functions->u1.Function ), PAGE_READWRITE, &previous ) )
        return false;
      originalNotify = reinterpret_cast<NotifyFunction>( functions->u1.Function );
      functions->u1.Function = reinterpret_cast<ULONG_PTR>( &captureNotify );
      DWORD ignored = 0;
      VirtualProtect( &functions->u1.Function, sizeof( functions->u1.Function ), previous, &ignored );
      return true;
    }
  }
  return false;
}

static QImage iconImage( const QIcon& icon, int size )
{
  const bool highDpi = QApplication::testAttribute( Qt::AA_UseHighDpiPixmaps );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps, false );
  QImage image = icon.pixmap( QSize( size, size ) ).toImage().convertToFormat( QImage::Format_ARGB32_Premultiplied );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps, highDpi );
  return image;
}

static bool matchesIcon( const QImage& actual, const QIcon& expected, int tolerance = 0 )
{
  if( actual.isNull() || actual.width() != actual.height() )
    return false;
  QImage reference = iconImage( expected, actual.width() );
  if( reference.size() != actual.size() )
    return false;
  const QImage converted = actual.convertToFormat( QImage::Format_ARGB32_Premultiplied );
  if( reference == converted )
    return true;
  int largest = 0;
  for( int row = 0; row < reference.height(); ++row )
    for( int column = 0; column < reference.width(); ++column )
    {
      const QRgb expectedPixel = reference.pixel( column, row );
      const QRgb actualPixel = converted.pixel( column, row );
      for( int shift : { 0, 8, 16, 24 } )
        largest = qMax( largest, qAbs( int( (expectedPixel >> shift) & 255 ) - int( (actualPixel >> shift) & 255 ) ) );
    }
  return largest <= tolerance;
}

static QImage latestTrayImage()
{
  for( int index = nativeCalls.size() - 1; index >= 0; --index )
    if( !nativeCalls.at( index ).tray.isNull() )
      return nativeCalls.at( index ).tray;
  return QImage();
}

static int iconUpdates()
{
  int count = 0;
  for( const NativeCall& call : nativeCalls )
    if( call.flags & NIF_ICON )
      ++count;
  return count;
}

class TimerObserver : public QObject
{
public:
  int events = 0;

protected:
  bool eventFilter( QObject*, QEvent* event ) override
  {
    if( event->type() == QEvent::Timer )
      ++events;
    return false;
  }
};

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  QStandardPaths::setTestModeEnabled( true );
  BeeApplication application( argc, argv );
  const QStringList arguments = application.arguments();
  forwardToShell = arguments.contains( "--native-notification" );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_BRANDING_PROOF_DIR", QDir::currentPath() );
  CHECK( QDir().mkpath( proof ) );
  const QIcon normal( ":/images/shadowchat/shadowchat-app.ico" );
  const QIcon unread( ":/images/shadowchat/shadowchat-unread.ico" );
  CHECK( !normal.isNull() && !unread.isNull() );
  CHECK( matchesIcon( iconImage( QApplication::windowIcon(), 32 ), normal ) );
  QWidget initialWindow;
  initialWindow.setWindowTitle( QStringLiteral( "影聊图标专项测试（本机合成）" ) );
  initialWindow.resize( 480, 180 );
  CHECK( matchesIcon( iconImage( initialWindow.windowIcon(), 32 ), normal ) );
  HWND window = reinterpret_cast<HWND>( initialWindow.winId() );
  HICON initialIcon = reinterpret_cast<HICON>( SendMessageW( window, WM_GETICON, ICON_BIG, 0 ) );
  if( !initialIcon )
    initialIcon = reinterpret_cast<HICON>( GetClassLongPtrW( window, GCLP_HICON ) );
  CHECK( nativeImage( initialIcon ).save( proof + "/initial-windows-app-icon.png" ) );
  CHECK( iconImage( normal, nativeImage( initialIcon ).width() ).save( proof + "/expected-initial-icon.png" ) );
  CHECK( matchesIcon( nativeImage( initialIcon ), normal, 1 ) );

  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  QImage obsoleteTheme( 32, 32, QImage::Format_ARGB32 );
  obsoleteTheme.fill( Qt::magenta );
  const QStringList oldNames = { "beebeep.png", "beebeep-tray.png", "beebeep-message.png", "beebeep-message-tray.png" };
  for( const QString& name : oldNames )
    CHECK( obsoleteTheme.save( temporary.path() + "/" + name ) );
  IconManager::instance().setSourcePath( temporary.path() );
  const QList<int> sizes = { 16, 20, 24, 32, 40, 48, 64, 128, 256 };
  for( int size : sizes )
  {
    for( const QString& name : oldNames )
    {
      const bool isUnread = name.contains( "message" );
      const QIcon& expected = isUnread ? unread : normal;
      CHECK( IconManager::instance().iconPath( name ).startsWith( ":/images/shadowchat/" ) );
      CHECK( matchesIcon( iconImage( IconManager::instance().icon( name ), size ), expected ) );
    }
  }
  for( const QString& name : oldNames )
  {
    const QImage resource( ":/images/" + name );
    const QImage expected( name.contains( "message" ) ?
      ":/images/shadowchat/shadowchat-unread-source.png" : ":/images/shadowchat/shadowchat-mark-source.png" );
    CHECK( !resource.isNull() && resource == expected );
  }
  CHECK( QImage( ":images/beebeep.png" ) == QImage( ":/images/shadowchat/shadowchat-mark-source.png" ) );
  CHECK( hookNativeNotifications() );
  GuiSystemTray tray;
  TimerObserver observer;
  tray.installEventFilter( &observer );
  CHECK( !tray.iconStatusIsMessage() && tray.chatId() == ID_INVALID );
  CHECK( matchesIcon( iconImage( tray.icon(), 32 ), normal ) );
  tray.show();
  QTest::qWait( 50 );
  CHECK( matchesIcon( latestTrayImage(), normal, 1 ) );
  CHECK( latestTrayImage().save( proof + "/tray-normal.png" ) );
  tray.setUnreadMessages( 77221, 3 );
  CHECK( tray.iconStatusIsMessage() && tray.chatId() == 77221 );
  CHECK( matchesIcon( iconImage( tray.icon(), 32 ), unread ) );
  CHECK( matchesIcon( latestTrayImage(), unread, 1 ) );
  CHECK( latestTrayImage().save( proof + "/tray-unread.png" ) );
  const qint64 unreadKey = tray.icon().cacheKey();
  const int updates = iconUpdates();
  tray.setUnreadMessages( 77222, 4 );
  QTest::qWait( 2600 );
  const int idleUpdates = iconUpdates() - updates;
  CHECK( tray.chatId() == 77222 );
  CHECK( tray.icon().cacheKey() == unreadKey );
  CHECK( iconUpdates() == updates );
  CHECK( observer.events == 0 && tray.findChildren<QTimer*>().isEmpty() );
  tray.setUnreadMessages( 77222, 0 );
  CHECK( !tray.iconStatusIsMessage() && tray.chatId() == ID_INVALID );
  CHECK( matchesIcon( latestTrayImage(), normal, 1 ) );
  CHECK( latestTrayImage().save( proof + "/tray-cleared.png" ) );
  tray.setUnreadMessages( 77221, 1 );
  tray.setNextChatToRead( Chat() );
  CHECK( !tray.iconStatusIsMessage() && tray.chatId() == ID_INVALID );
  CHECK( matchesIcon( latestTrayImage(), normal, 1 ) );
  Settings::instance().setAccountDataFolder( temporary.path() + "/profile" );
  Settings::instance().load();
  Settings::instance().setShowNotificationOnTray( true );
  const QString body = QStringLiteral( "影聊品牌图标本机验证；未连接聊天服务，未向任何人发送消息。" );
  tray.showNewMessageArrived( 77221, body, false );
  CHECK( tray.iconStatusIsMessage() && tray.chatId() == 77221 );
  CHECK( !nativeCalls.isEmpty() );
  const NativeCall notification = nativeCalls.last();
  CHECK( notification.flags & NIF_INFO );
  CHECK( (notification.infoFlags & 0xf) == NIIF_USER );
  CHECK( notification.title == Settings::instance().programName() );
  CHECK( notification.body == body );
  CHECK( matchesIcon( notification.balloon, normal, 1 ) );
  CHECK( !matchesIcon( notification.balloon, unread ) );
  CHECK( notification.balloon.save( proof + "/native-notification-image.png" ) );
  CHECK( notification.result );
  const int beforeSuppressed = nativeCalls.size();
  Settings::instance().setShowNotificationOnTray( false );
  tray.showUserStatusChanged( 77221, "suppressed synthetic notification" );
  CHECK( nativeCalls.size() == beforeSuppressed );
  Settings::instance().setShowNotificationOnTray( true );
  if( !forwardToShell )
  {
    tray.showNewFileArrived( 77221, "synthetic file", true );
    CHECK( nativeCalls.last().title == Settings::instance().programName() );
    CHECK( matchesIcon( nativeCalls.last().balloon, normal, 1 ) );
    tray.showUserStatusChanged( 77221, "synthetic status" );
    CHECK( matchesIcon( nativeCalls.last().balloon, normal, 1 ) );
  }
  QJsonObject report;
  report["qtVersion"] = QString::fromLatin1( qVersion() );
  report["devicePixelRatio"] = initialWindow.devicePixelRatioF();
  report["sourceSha256"] = QStringLiteral( "62b0336451868dbc018a7b857ace7a39bad6971d88c24b5a5c3a5f87a67235f2" );
  report["nativeShellForwarded"] = forwardToShell;
  report["nativeShellAccepted"] = notification.result != FALSE;
  report["notificationTitle"] = notification.title;
  report["nativeNotificationImagePixels"] = notification.balloon.width();
  report["nativeTrayImagePixels"] = latestTrayImage().width();
  report["unreadStableMilliseconds"] = 2600;
  report["trayTimerEvents"] = observer.events;
  report["trayIconUpdatesDuringIdle"] = idleUpdates;
  report["initialNativeWindowIconPixelMatch"] = true;
  report["themeOverrideBlocked"] = true;
  report["legacyResourcePixelsMatch"] = true;
  report["nativeIconPixelsMatch"] = true;
  report["nativePremultiplicationChannelTolerance"] = 1;
  QFile output( proof + "/branding-results.json" );
  CHECK( output.open( QIODevice::WriteOnly ) );
  output.write( QJsonDocument( report ).toJson( QJsonDocument::Indented ) );
  output.close();
  if( forwardToShell )
    QTest::qWait( 12000 );
  tray.setUnreadMessages( 77221, 0 );
  tray.hide();
  IconManager::instance().setSourcePath( QString() );
  std::printf( "PASS initial Windows app icon; 9 icon sizes; legacy resources and theme override; unread/clear; 2600ms no timer or icon rotation; native Shell_NotifyIconW red notification pixels/title/body%s\n",
    forwardToShell ? "; one synthetic notification accepted by Windows Shell" : "" );
  return 0;
}
