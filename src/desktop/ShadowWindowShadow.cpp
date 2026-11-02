#include "ShadowWindowShadow.h"
#include "ShadowWindowShape.h"
#include <QApplication>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QScopedValueRollback>
#include <QTimer>
#include <QWidget>
#include <cmath>
#include <cstring>

#ifdef Q_OS_WIN
#include <qt_windows.h>

namespace
{
const wchar_t shadowClassName[] = L"ShadowChat.WindowShadow";
const int shadowPadding = 12;

LRESULT CALLBACK shadowWindowProcedure( HWND window, UINT message, WPARAM parameter, LPARAM data )
{
  if( message == WM_NCHITTEST ) return HTTRANSPARENT;
  if( message == WM_MOUSEACTIVATE ) return MA_NOACTIVATE;
  return DefWindowProcW( window, message, parameter, data );
}

HWND createShadowWindow()
{
  const HINSTANCE instance = GetModuleHandleW( nullptr );
  WNDCLASSW windowClass = {};
  windowClass.lpfnWndProc = shadowWindowProcedure;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = shadowClassName;
  if( !RegisterClassW( &windowClass ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS )
    return nullptr;
  return CreateWindowExW( WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
    shadowClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr );
}

bool paintShadow( HWND window, const RECT& bounds, const QSize& size, qreal scale,
  const QRegion& mask )
{
  QImage image( size, QImage::Format_ARGB32_Premultiplied );
  if( image.isNull() ) return false;
  image.fill( Qt::transparent );
  const int padding = qCeil( shadowPadding * scale );
  QPainter painter( &image );
  painter.setRenderHint( QPainter::Antialiasing );
  painter.scale( scale, scale );
  painter.translate( padding / scale, padding / scale );
  const QRectF surface( 0, 2, (bounds.right - bounds.left) / scale,
    (bounds.bottom - bounds.top) / scale );
  painter.setPen( Qt::NoPen );
  int previousAlpha = 0;
  for( int distance = 10; distance >= 0; --distance )
  {
    const int alpha = qRound( 28.0 * std::exp( -distance * distance / 32.0 ) );
    const int layerAlpha = qRound( (alpha - previousAlpha) * 255.0 / (255 - previousAlpha) );
    painter.setBrush( QColor( 15, 23, 42, layerAlpha ) );
    painter.drawRoundedRect( surface.adjusted( -distance, -distance, distance, distance ),
      ShadowWindowShape::cornerRadius + distance, ShadowWindowShape::cornerRadius + distance );
    previousAlpha = alpha;
  }
  painter.setCompositionMode( QPainter::CompositionMode_Clear );
  painter.setClipRegion( mask );
  painter.fillRect( mask.boundingRect(), Qt::transparent );
  painter.end();

  HDC screen = GetDC( nullptr );
  if( !screen ) return false;
  HDC memory = CreateCompatibleDC( screen );
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
  info.bmiHeader.biWidth = size.width();
  info.bmiHeader.biHeight = -size.height();
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  HBITMAP bitmap = memory ? CreateDIBSection( memory, &info, DIB_RGB_COLORS, &pixels, nullptr, 0 ) : nullptr;
  bool painted = false;
  if( bitmap && pixels )
  {
    std::memcpy( pixels, image.constBits(), static_cast<size_t>( image.sizeInBytes() ) );
    HGDIOBJ previousBitmap = SelectObject( memory, bitmap );
    POINT position = { bounds.left - padding, bounds.top - padding };
    POINT source = { 0, 0 };
    SIZE nativeSize = { size.width(), size.height() };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    painted = UpdateLayeredWindow( window, screen, &position, &nativeSize, memory,
      &source, 0, &blend, ULW_ALPHA ) != FALSE;
    SelectObject( memory, previousBitmap );
  }
  if( bitmap ) DeleteObject( bitmap );
  if( memory ) DeleteDC( memory );
  ReleaseDC( nullptr, screen );
  return painted;
}
}

struct ShadowWindowShadow::NativeState
{
  HWND window = nullptr;
  QSize size;
  QRegion mask;
  qreal scale = 0;
  bool synchronizing = false;
};
#else
struct ShadowWindowShadow::NativeState {};
#endif

ShadowWindowShadow::ShadowWindowShadow( QWidget* window )
  : QObject( window ), m_native( new NativeState ), m_window( window )
{
  setObjectName( QStringLiteral( "ShadowWindowShadow" ) );
#ifdef Q_OS_WIN
  if( window )
  {
    window->installEventFilter( this );
    qApp->installNativeEventFilter( this );
    synchronize();
  }
#endif
}

ShadowWindowShadow::~ShadowWindowShadow()
{
#ifdef Q_OS_WIN
  if( qApp ) qApp->removeNativeEventFilter( this );
  if( m_native->window ) DestroyWindow( m_native->window );
#endif
  delete m_native;
}

void ShadowWindowShadow::hide()
{
#ifdef Q_OS_WIN
  if( m_native->window ) ShowWindow( m_native->window, SW_HIDE );
#endif
}

void ShadowWindowShadow::synchronize()
{
#ifdef Q_OS_WIN
  if( m_native->synchronizing ) return;
  QScopedValueRollback<bool> synchronizing( m_native->synchronizing, true );
  const HWND target = m_window ? reinterpret_cast<HWND>( m_window->internalWinId() ) : nullptr;
  if( !target || !m_window->isVisible() || !IsWindowVisible( target ) || IsIconic( target ) ||
      IsZoomed( target ) || m_window->isMinimized() || m_window->isMaximized() || m_window->isFullScreen() )
  {
    hide();
    return;
  }
  RECT bounds;
  if( !GetWindowRect( target, &bounds ) ) return;
  const qreal scale = m_window->devicePixelRatioF();
  const int padding = qCeil( shadowPadding * scale );
  const QSize size( bounds.right - bounds.left + 2 * padding, bounds.bottom - bounds.top + 2 * padding );
  const QRegion mask = m_window->mask().isEmpty() ? QRegion( m_window->rect() ) : m_window->mask();
  if( !m_native->window ) m_native->window = createShadowWindow();
  if( !m_native->window ) return;
  if( m_native->size != size || m_native->scale != scale || m_native->mask != mask )
  {
    if( !paintShadow( m_native->window, bounds, size, scale, mask ) )
    {
      hide();
      return;
    }
    m_native->size = size;
    m_native->scale = scale;
    m_native->mask = mask;
  }
  const bool topmost = (GetWindowLongPtrW( target, GWL_EXSTYLE ) & WS_EX_TOPMOST) != 0;
  const bool shadowTopmost = (GetWindowLongPtrW( m_native->window, GWL_EXSTYLE ) & WS_EX_TOPMOST) != 0;
  if( topmost != shadowTopmost )
    SetWindowPos( m_native->window, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
  SetWindowPos( m_native->window, target, bounds.left - padding, bounds.top - padding,
    size.width(), size.height(), SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW );
#endif
}

bool ShadowWindowShadow::eventFilter( QObject* watched, QEvent* event )
{
  if( watched == m_window )
  {
    switch( event->type() )
    {
      case QEvent::Hide:
      case QEvent::Destroy:
        hide();
        break;
      case QEvent::Move:
      case QEvent::Resize:
      case QEvent::Show:
      case QEvent::WindowStateChange:
      case QEvent::WindowActivate:
      case QEvent::WindowDeactivate:
      case QEvent::ZOrderChange:
      case QEvent::WinIdChange:
      case QEvent::ScreenChangeInternal:
        synchronize();
        break;
      default:
        break;
    }
  }
  return false;
}

bool ShadowWindowShadow::nativeEventFilter( const QByteArray&, void* message, long* )
{
#ifdef Q_OS_WIN
  const MSG* nativeMessage = static_cast<MSG*>( message );
  if( m_window && nativeMessage &&
      nativeMessage->hwnd == reinterpret_cast<HWND>( m_window->internalWinId() ) )
  {
    switch( nativeMessage->message )
    {
      case WM_DESTROY:
        hide();
        break;
      case WM_SHOWWINDOW:
        if( !nativeMessage->wParam ) hide();
        else QTimer::singleShot( 0, this, [this]() { synchronize(); } );
        break;
      case WM_SIZE:
        if( nativeMessage->wParam == SIZE_MINIMIZED || nativeMessage->wParam == SIZE_MAXIMIZED ) hide();
        break;
      case WM_WINDOWPOSCHANGED:
      case WM_ACTIVATE:
        synchronize();
        break;
      case WM_DPICHANGED:
        QTimer::singleShot( 0, this, [this]() { synchronize(); } );
        break;
      default:
        break;
    }
  }
#else
  Q_UNUSED( message );
#endif
  return false;
}
