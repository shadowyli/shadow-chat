#include "ShadowWindowShadow.h"
#include "ShadowWindowShape.h"
#include "ShadowWindowResize.h"
#include <QtWidgets>
#include <QTest>
#include <cstdio>
#include <cstring>

#ifdef SHADOW_WINDOW_MAIN_PREVIEW
#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowDevPreview.h"
#include "ShadowStyle.h"
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s (Win32 %lu)\n", __LINE__, #value, GetLastError() ); return 1; } } while(false)

class WhiteBackground final : public QWidget
{
public:
  int clicks = 0;
  WhiteBackground() : QWidget( nullptr, Qt::Window | Qt::FramelessWindowHint )
  {
    setStyleSheet( QStringLiteral( "background: white;" ) );
  }
protected:
  void mousePressEvent( QMouseEvent* event ) override
  {
    ++clicks;
    QWidget::mousePressEvent( event );
  }
};

class RoundedWindow final : public QMainWindow
{
public:
  RoundedWindow()
  {
    setWindowFlags( Qt::Window | Qt::FramelessWindowHint );
    setMinimumSize( 320, 240 );
    setStyleSheet( QStringLiteral( "QMainWindow { background: #F6F8FA; }"
      "QLineEdit { background: white; border: 1px solid #E3E7EC; border-radius: 6px; padding: 8px; }" ) );
    QWidget* body = new QWidget( this );
    QVBoxLayout* layout = new QVBoxLayout( body );
    layout->setContentsMargins( 32, 24, 32, 24 );
    QLabel* title = new QLabel( QStringLiteral( "Windows Qt Widgets" ), body );
    QFont titleFont = title->font();
    titleFont.setPointSize( 16 );
    title->setFont( titleFont );
    layout->addWidget( title );
    layout->addWidget( new QLabel( QStringLiteral( "Native outer shadow / rounded mask / real desktop capture" ), body ) );
    layout->addWidget( new QLineEdit( QStringLiteral( "Synthetic test window" ), body ) );
    layout->addStretch();
    setCentralWidget( body );
    new ShadowWindowResize( this );
    new ShadowWindowShadow( this );
  }
protected:
  void resizeEvent( QResizeEvent* event ) override
  {
    QMainWindow::resizeEvent( event );
    updateMask();
  }
  void changeEvent( QEvent* event ) override
  {
    QMainWindow::changeEvent( event );
    if( event->type() == QEvent::WindowStateChange ) updateMask();
  }
private:
  void updateMask()
  {
    if( isMaximized() || isFullScreen() ) clearMask();
    else setMask( ShadowWindowShape::roundedMask( size() ) );
  }
};

static HWND findShadow()
{
  HWND found = nullptr;
  EnumWindows( []( HWND window, LPARAM data ) -> BOOL {
    wchar_t className[80] = {};
    DWORD processId = 0;
    GetWindowThreadProcessId( window, &processId );
    GetClassNameW( window, className, 80 );
    if( processId == GetCurrentProcessId() && std::wcscmp( className, L"ShadowChat.WindowShadow" ) == 0 )
    {
      *reinterpret_cast<HWND*>( data ) = window;
      return FALSE;
    }
    return TRUE;
  }, reinterpret_cast<LPARAM>( &found ) );
  return found;
}

static QRect nativeGeometry( HWND window )
{
  RECT bounds = {};
  GetWindowRect( window, &bounds );
  return QRect( bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top );
}

static bool aligned( QWidget& window, HWND shadow )
{
  const int padding = qCeil( 12 * window.devicePixelRatioF() );
  return nativeGeometry( shadow ) ==
    nativeGeometry( reinterpret_cast<HWND>( window.winId() ) ).adjusted( -padding, -padding, padding, padding );
}

static void settle()
{
  QTest::qWait( 120 );
}

static QImage desktopCapture( const QRect& area )
{
  HDC screen = GetDC( nullptr );
  HDC memory = CreateCompatibleDC( screen );
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
  info.bmiHeader.biWidth = area.width();
  info.bmiHeader.biHeight = -area.height();
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  HBITMAP bitmap = CreateDIBSection( memory, &info, DIB_RGB_COLORS, &pixels, nullptr, 0 );
  HGDIOBJ previous = SelectObject( memory, bitmap );
  BitBlt( memory, 0, 0, area.width(), area.height(), screen, area.x(), area.y(), SRCCOPY | CAPTUREBLT );
  const QImage image = QImage( static_cast<uchar*>( pixels ), area.width(), area.height(), QImage::Format_RGB32 ).copy();
  SelectObject( memory, previous );
  DeleteObject( bitmap );
  DeleteDC( memory );
  ReleaseDC( nullptr, screen );
  return image;
}

static void drag( QWidget* grip, const QPoint& delta )
{
  const QPoint start = grip->mapToGlobal( grip->rect().center() );
  QMouseEvent press( QEvent::MouseButtonPress, grip->rect().center(), start,
    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( grip, &press );
  QMouseEvent move( QEvent::MouseMove, grip->mapFromGlobal( start + delta ), start + delta,
    Qt::NoButton, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( grip, &move );
  QMouseEvent release( QEvent::MouseButtonRelease, grip->mapFromGlobal( start + delta ), start + delta,
    Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
  QApplication::sendEvent( grip, &release );
}

static int verifyShadow( QWidget& window, WhiteBackground& background )
{
  window.resize( 760, 520 );
  window.move( background.geometry().center() - QPoint( 380, 260 ) );
  const QRect expectedGeometry = window.geometry();
  const QMargins expectedMargins = window.contentsMargins();
  window.show();
  window.raise();
  window.activateWindow();
  settle();
  HWND target = reinterpret_cast<HWND>( window.winId() );
  const HWND shadow = findShadow();
  CHECK( shadow && IsWindowVisible( shadow ) && aligned( window, shadow ) );
  CHECK( window.geometry() == expectedGeometry && window.contentsMargins() == expectedMargins );
  CHECK( GetWindow( shadow, GW_OWNER ) == nullptr );
  CHECK( GetWindow( target, GW_HWNDNEXT ) == shadow );
  const LONG_PTR extendedStyle = GetWindowLongPtrW( shadow, GWL_EXSTYLE );
  CHECK( (extendedStyle & (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) ==
    (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW) );
  CHECK( !(extendedStyle & WS_EX_APPWINDOW) );
  CHECK( SendMessageW( shadow, WM_NCHITTEST, 0, 0 ) == HTTRANSPARENT );
  CHECK( SendMessageW( shadow, WM_MOUSEACTIVATE, 0, 0 ) == MA_NOACTIVATE );
  CHECK( GetForegroundWindow() != shadow );
  std::puts( "PASS native layered HWND: unowned tool window, no taskbar/activation, below main window, unchanged content geometry" );

  const DWORD initialGdi = GetGuiResources( GetCurrentProcess(), GR_GDIOBJECTS );
  for( int iteration = 0; iteration < 48; ++iteration )
  {
    const QRect before = nativeGeometry( target );
    CHECK( SetWindowPos( target, nullptr, before.x() + (iteration % 2 ? -3 : 3),
      before.y() + (iteration % 2 ? -2 : 2), before.width() + (iteration % 2 ? -4 : 4),
      before.height() + (iteration % 2 ? -3 : 3), SWP_NOACTIVATE | SWP_NOZORDER ) );
    CHECK( aligned( window, shadow ) );
  }
  settle();
  CHECK( GetGuiResources( GetCurrentProcess(), GR_GDIOBJECTS ) <= initialGdi + 3 );
  CHECK( GetForegroundWindow() != shadow );
  CHECK( window.mask() == ShadowWindowShape::roundedMask( window.size() ) );
  const QRect beforeNegativeMove = nativeGeometry( target );
  CHECK( SetWindowPos( target, nullptr, -80, 60, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE ) );
  CHECK( aligned( window, shadow ) );
  CHECK( SetWindowPos( target, nullptr, beforeNegativeMove.x(), beforeNegativeMove.y(), 0, 0,
    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE ) );
  settle();
  std::puts( "PASS synchronous Win32 move/resize, negative desktop coordinates, stable GDI resource count and round mask" );

  const char* directions[] = { "Left", "Right", "Top", "Bottom", "TopLeft", "TopRight", "BottomLeft", "BottomRight" };
  const QPoint deltas[] = { QPoint( -8, 0 ), QPoint( 8, 0 ), QPoint( 0, -8 ), QPoint( 0, 8 ),
    QPoint( -8, -8 ), QPoint( 8, -8 ), QPoint( -8, 8 ), QPoint( 8, 8 ) };
  for( int index = 0; index < 8; ++index )
  {
    QWidget* grip = window.findChild<QWidget*>( QStringLiteral( "ShadowResize%1" ).arg( directions[index] ) );
    CHECK( grip && grip->isVisible() );
    const QSize before = window.size();
    drag( grip, deltas[index] );
    CHECK( window.width() == before.width() + qAbs( deltas[index].x() ) );
    CHECK( window.height() == before.height() + qAbs( deltas[index].y() ) );
    CHECK( aligned( window, shadow ) && QWidget::mouseGrabber() == nullptr );
  }
  window.setGeometry( expectedGeometry );
  settle();
  for( int row = 0; row < 18; ++row )
    for( int column = 0; column < 18; ++column )
    {
      const bool included = window.mask().contains( QPoint( column, row ) );
      CHECK( included == window.mask().contains( QPoint( window.width() - column - 1, row ) ) );
      CHECK( included == window.mask().contains( QPoint( column, window.height() - row - 1 ) ) );
      CHECK( included == window.mask().contains( QPoint( window.width() - column - 1, window.height() - row - 1 ) ) );
    }
  std::puts( "PASS all four resize edges and corners with shadow; original symmetric mask preserved" );

  const QRect mainBounds = nativeGeometry( target );
  POINT outside = { mainBounds.left() - qMax( 2, qRound( 3 * window.devicePixelRatioF() ) ), mainBounds.center().y() };
  const HWND backdrop = reinterpret_cast<HWND>( background.winId() );
  CHECK( WindowFromPoint( outside ) == backdrop );
  struct RestorePointer
  {
    POINT position;
    RestorePointer() { GetCursorPos( &position ); }
    ~RestorePointer() { SetCursorPos( position.x, position.y ); }
  } restorePointer;
  if( SetCursorPos( outside.x, outside.y ) )
  {
    INPUT input[2] = {};
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    CHECK( SendInput( 2, input, sizeof( INPUT ) ) == 2 );
    settle();
    CHECK( background.clicks == 1 && GetForegroundWindow() == backdrop );
    CHECK( GetForegroundWindow() != shadow );
    window.move( window.pos() + QPoint( 1, 0 ) );
    CHECK( GetForegroundWindow() == backdrop );
    window.move( window.pos() - QPoint( 1, 0 ) );
    CHECK( GetForegroundWindow() == backdrop );
    std::puts( "PASS real Windows mouse input traverses visible shadow to underlying white window" );
  }
  else
    std::puts( "SKIP live mouse injection: input desktop unavailable; native WindowFromPoint and hit tests passed" );
  window.raise();
  window.activateWindow();
  settle();

  window.hide();
  settle();
  CHECK( !IsWindowVisible( shadow ) );
  window.show();
  settle();
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  window.showMinimized();
  settle();
  CHECK( !IsWindowVisible( shadow ) );
  window.showNormal();
  settle();
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  window.showMaximized();
  settle();
  CHECK( !IsWindowVisible( shadow ) );
  window.showNormal();
  settle();
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  window.showFullScreen();
  settle();
  CHECK( !IsWindowVisible( shadow ) );
  window.showNormal();
  settle();
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  ShowWindow( target, SW_HIDE );
  settle();
  CHECK( !IsWindowVisible( shadow ) );
  ShowWindow( target, SW_SHOWNOACTIVATE );
  settle();
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  std::puts( "PASS hide/show, minimize, maximize, fullscreen and native hide/restore" );

  CHECK( SetWindowPos( target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE ) );
  settle();
  CHECK( GetWindowLongPtrW( shadow, GWL_EXSTYLE ) & WS_EX_TOPMOST );
  CHECK( GetWindow( target, GW_HWNDNEXT ) == shadow );
  CHECK( SetWindowPos( target, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE ) );
  settle();
  CHECK( !(GetWindowLongPtrW( shadow, GWL_EXSTYLE ) & WS_EX_TOPMOST) );
  window.setWindowFlag( Qt::WindowStaysOnTopHint, true );
  window.show();
  settle();
  target = reinterpret_cast<HWND>( window.winId() );
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  CHECK( GetWindow( target, GW_HWNDNEXT ) == shadow );
  window.setWindowFlag( Qt::WindowStaysOnTopHint, false );
  window.show();
  settle();
  target = reinterpret_cast<HWND>( window.winId() );
  CHECK( IsWindowVisible( shadow ) && aligned( window, shadow ) );
  std::puts( "PASS topmost band changes and Qt native handle recreation" );

  window.setGeometry( expectedGeometry );
  window.raise();
  window.activateWindow();
  settle();
  const int scalePercent = qRound( 100 * window.devicePixelRatioF() );
  const QString prefix = QStringLiteral( "native-white-%1" ).arg( scalePercent );
  const QRect captureBounds = nativeGeometry( target ).adjusted( -32, -32, 32, 32 );
  const QImage withShadow = desktopCapture( captureBounds );
  ShowWindow( shadow, SW_HIDE );
  settle();
  const QImage withoutShadow = desktopCapture( captureBounds );
  window.move( window.pos() + QPoint( 1, 0 ) );
  window.move( window.pos() - QPoint( 1, 0 ) );
  settle();
  CHECK( IsWindowVisible( shadow ) );
  bool blackCapture = true;
  for( int row = 0; row < withShadow.height() && blackCapture; ++row )
    for( int column = 0; column < withShadow.width(); ++column )
      if( withShadow.pixelColor( column, row ) != QColor( Qt::black ) )
      {
        blackCapture = false;
        break;
      }
  if( blackCapture )
  {
    std::printf( "SKIP desktop pixel comparison at DPI=%d%%: Windows returned an entirely black desktop (locked/unavailable); behavior tests still passed\n", scalePercent );
    window.hide();
    return 0;
  }
  CHECK( withShadow.save( prefix + QStringLiteral( ".png" ) ) );
  CHECK( withoutShadow.save( prefix + QStringLiteral( "-control.png" ) ) );
  const QRect localBounds = nativeGeometry( target ).translated( -captureBounds.topLeft() );
  const int nearDistance = qMax( 2, qRound( 3 * window.devicePixelRatioF() ) );
  const QPoint samples[] = {
    QPoint( localBounds.left() - nearDistance, localBounds.center().y() ),
    QPoint( localBounds.right() + nearDistance, localBounds.center().y() ),
    QPoint( localBounds.center().x(), localBounds.top() - nearDistance ),
    QPoint( localBounds.center().x(), localBounds.bottom() + nearDistance )
  };
  for( const QPoint& point : samples )
  {
    const QColor before = withoutShadow.pixelColor( point );
    const QColor after = withShadow.pixelColor( point );
    CHECK( before == QColor( Qt::white ) );
    CHECK( after.red() >= 220 && after.red() <= 253 && after.red() < before.red() );
  }
  CHECK( withShadow.pixelColor( 0, withShadow.height() / 2 ) == QColor( Qt::white ) );
  CHECK( withShadow.pixelColor( withShadow.width() / 2, 0 ) == QColor( Qt::white ) );
  CHECK( withShadow.pixelColor( withShadow.width() - 1, withShadow.height() / 2 ) == QColor( Qt::white ) );
  CHECK( withShadow.pixelColor( withShadow.width() / 2, withShadow.height() - 1 ) == QColor( Qt::white ) );
  CHECK( withShadow.pixelColor( localBounds.center() ) == withoutShadow.pixelColor( localBounds.center() ) );
  std::printf( "PASS real desktop pixel comparison: soft shadow on all four sides, white beyond compact halo, unchanged content; DPI=%d%%\n", scalePercent );
  std::printf( "Saved %s.png and matching no-shadow control\n", prefix.toUtf8().constData() );
  window.hide();
  return 0;
}
#endif

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
#ifdef SHADOW_WINDOW_MAIN_PREVIEW
  BeeApplication application( argc, argv );
  Random::init();
  QTemporaryDir temporary( QDir::currentPath() + "/window-shadow-synthetic-XXXXXX" );
  CHECK( temporary.isValid() );
  const QString profile = temporary.path() + "/profile";
  const QString config = temporary.path() + "/config";
  CHECK( QDir().mkpath( profile ) && QDir().mkpath( config ) );
  QFile marker( profile + "/.shadow-dev-preview" );
  CHECK( marker.open( QIODevice::WriteOnly ) );
  CHECK( marker.write( "shadow-native-preview-synthetic-v1\n" ) > 0 );
  marker.close();
  QString error;
  const QStringList arguments = QStringList() << application.applicationFilePath()
    << "--dev-preview" << config << "--profile-root" << profile
    << "--dev-instance" << "11111111-2222-3333-4444-555555555555";
  CHECK( ShadowDevPreview::configure( arguments, &error ) );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "window-shadow-synthetic" );
  CHECK( QDir().mkpath( profile + "/account" ) );
  settings.setAccountDataFolder( profile + "/account" );
  settings.load();
  ShadowDevPreview::prepareSettings();
  application.setFont( ShadowStyle::applicationFont() );
  Core core;
  CHECK( core.devPreviewIsolated() );
#else
  QApplication application( argc, argv );
#endif
#ifdef Q_OS_WIN
  application.setQuitOnLastWindowClosed( false );
  WhiteBackground background;
  const QRect available = application.primaryScreen()->availableGeometry();
  background.setGeometry( available );
  background.show();
  settle();
  CHECK( !findShadow() );
#ifdef SHADOW_WINDOW_MAIN_PREVIEW
  GuiMain* window = new GuiMain;
  new ShadowDevPreview( window, &core );
  QTest::qWait( 700 );
  CHECK( window->findChild<QObject*>( QStringLiteral( "ShadowWindowShadow" ) ) );
#else
  RoundedWindow* window = new RoundedWindow;
#endif
  CHECK( verifyShadow( *window, background ) == 0 );
  const HWND shadow = findShadow();
  delete window;
  settle();
  CHECK( !IsWindow( shadow ) && !findShadow() );
  std::puts( "PASS shadow HWND destroyed with parent; no persistent windows or real user data" );
  return 0;
#else
  std::puts( "SKIP: native Windows shadow requires the Windows Qt platform" );
  return 0;
#endif
}
