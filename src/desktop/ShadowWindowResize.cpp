#include "ShadowWindowResize.h"
#include "ShadowWindowShape.h"

#include <QEvent>
#include <QMouseEvent>
#include <QVariant>
#include <QWidget>

namespace
{
const int borderWidth = 7;
const int cornerSize = ShadowWindowShape::cornerRadius;

Qt::Edges permittedEdges( const QWidget* window, Qt::Edges edges )
{
  if( !window || window->isMaximized() || window->isFullScreen() ) return Qt::Edges();
  if( window->minimumWidth() == window->maximumWidth() ) edges &= ~(Qt::LeftEdge | Qt::RightEdge);
  if( window->minimumHeight() == window->maximumHeight() ) edges &= ~(Qt::TopEdge | Qt::BottomEdge);
  return edges;
}
}

ShadowWindowResize::ShadowWindowResize( QWidget* window, QWidget* surface )
  : QObject( window ), m_window( window ), m_surface( surface ? surface : window )
{
  setObjectName( QStringLiteral( "ShadowWindowResize" ) );
  struct Handle
  {
    const char* name;
    Qt::Edges edges;
  };
  const Handle handles[] = {
    { "Left", Qt::LeftEdge }, { "Right", Qt::RightEdge },
    { "Top", Qt::TopEdge }, { "Bottom", Qt::BottomEdge },
    { "TopLeft", Qt::TopEdge | Qt::LeftEdge }, { "TopRight", Qt::TopEdge | Qt::RightEdge },
    { "BottomLeft", Qt::BottomEdge | Qt::LeftEdge }, { "BottomRight", Qt::BottomEdge | Qt::RightEdge }
  };
  for( const Handle& handle : handles )
  {
    QWidget* grip = new QWidget( m_surface );
    grip->setObjectName( QStringLiteral( "ShadowResize%1" ).arg( QLatin1String( handle.name ) ) );
    grip->setProperty( "resizeEdges", int( handle.edges ) );
    grip->setAttribute( Qt::WA_NoSystemBackground );
    grip->setFocusPolicy( Qt::NoFocus );
    grip->setMouseTracking( true );
    grip->installEventFilter( this );
    m_handles.insert( int( handle.edges ), grip );
  }
  window->installEventFilter( this );
  if( m_surface != window ) m_surface->installEventFilter( this );
  updateHandles();
}

Qt::Edges ShadowWindowResize::edgesAt( const QWidget* window, const QPoint& position )
{
  if( !window || !window->rect().contains( position ) ) return Qt::Edges();
  const bool leftCorner = position.x() < cornerSize;
  const bool rightCorner = position.x() >= window->width() - cornerSize;
  const bool topCorner = position.y() < cornerSize;
  const bool bottomCorner = position.y() >= window->height() - cornerSize;
  Qt::Edges edges;
  if( topCorner && leftCorner ) edges = Qt::TopEdge | Qt::LeftEdge;
  else if( topCorner && rightCorner ) edges = Qt::TopEdge | Qt::RightEdge;
  else if( bottomCorner && leftCorner ) edges = Qt::BottomEdge | Qt::LeftEdge;
  else if( bottomCorner && rightCorner ) edges = Qt::BottomEdge | Qt::RightEdge;
  else if( position.x() < borderWidth ) edges = Qt::LeftEdge;
  else if( position.x() >= window->width() - borderWidth ) edges = Qt::RightEdge;
  else if( position.y() < borderWidth ) edges = Qt::TopEdge;
  else if( position.y() >= window->height() - borderWidth ) edges = Qt::BottomEdge;
  return permittedEdges( window, edges );
}

Qt::Edges ShadowWindowResize::allowedEdges( Qt::Edges edges ) const
{
  return permittedEdges( m_window, edges );
}

void ShadowWindowResize::updateHandles()
{
  if( !m_window || !m_surface ) return;
  const int width = m_surface->width();
  const int height = m_surface->height();
  for( auto handle = m_handles.constBegin(); handle != m_handles.constEnd(); ++handle )
  {
    const Qt::Edges edges( handle.key() );
    const Qt::Edges active = allowedEdges( edges );
    QWidget* grip = handle.value();
    QRect bounds;
    if( edges == Qt::LeftEdge ) bounds = QRect( 0, cornerSize, borderWidth, qMax( 0, height - cornerSize * 2 ) );
    else if( edges == Qt::RightEdge ) bounds = QRect( width - borderWidth, cornerSize, borderWidth, qMax( 0, height - cornerSize * 2 ) );
    else if( edges == Qt::TopEdge ) bounds = QRect( cornerSize, 0, qMax( 0, width - cornerSize * 2 ), borderWidth );
    else if( edges == Qt::BottomEdge ) bounds = QRect( cornerSize, height - borderWidth, qMax( 0, width - cornerSize * 2 ), borderWidth );
    else bounds = QRect( edges.testFlag( Qt::LeftEdge ) ? 0 : width - cornerSize,
      edges.testFlag( Qt::TopEdge ) ? 0 : height - cornerSize, cornerSize, cornerSize );
    grip->setGeometry( bounds );
    const bool horizontal = active.testFlag( Qt::LeftEdge ) || active.testFlag( Qt::RightEdge );
    const bool vertical = active.testFlag( Qt::TopEdge ) || active.testFlag( Qt::BottomEdge );
    grip->setCursor( horizontal && vertical ?
      ((active.testFlag( Qt::LeftEdge ) == active.testFlag( Qt::TopEdge )) ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor) :
      horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor );
    grip->setVisible( m_window->isVisible() && active != Qt::Edges() );
    grip->raise();
  }
}

void ShadowWindowResize::resizeTo( const QPoint& globalPosition )
{
  if( !m_window || m_resizing == Qt::Edges() ) return;
  const QPoint delta = globalPosition - m_pressPosition;
  QRect bounds = m_originalGeometry;
  if( m_resizing.testFlag( Qt::LeftEdge ) || m_resizing.testFlag( Qt::RightEdge ) )
  {
    const int width = qBound( m_window->minimumWidth(),
      m_originalGeometry.width() + (m_resizing.testFlag( Qt::LeftEdge ) ? -delta.x() : delta.x()),
      m_window->maximumWidth() );
    bounds.setWidth( width );
    if( m_resizing.testFlag( Qt::LeftEdge ) ) bounds.moveRight( m_originalGeometry.right() );
  }
  if( m_resizing.testFlag( Qt::TopEdge ) || m_resizing.testFlag( Qt::BottomEdge ) )
  {
    const int height = qBound( m_window->minimumHeight(),
      m_originalGeometry.height() + (m_resizing.testFlag( Qt::TopEdge ) ? -delta.y() : delta.y()),
      m_window->maximumHeight() );
    bounds.setHeight( height );
    if( m_resizing.testFlag( Qt::TopEdge ) ) bounds.moveBottom( m_originalGeometry.bottom() );
  }
  m_window->setGeometry( bounds );
}

void ShadowWindowResize::finishResize()
{
  m_resizing = Qt::Edges();
  QWidget* grabber = QWidget::mouseGrabber();
  if( grabber && m_handles.values().contains( grabber ) ) grabber->releaseMouse();
}

bool ShadowWindowResize::eventFilter( QObject* watched, QEvent* event )
{
  if( !m_window || !m_surface ) return false;
  if( watched == m_window || watched == m_surface )
  {
    if( event->type() == QEvent::Resize || event->type() == QEvent::Show ||
        event->type() == QEvent::WindowStateChange || event->type() == QEvent::LayoutRequest )
      updateHandles();
    if( event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::WindowStateChange ) finishResize();
    return false;
  }
  QWidget* grip = qobject_cast<QWidget*>( watched );
  if( !grip || !m_handles.values().contains( grip ) ) return false;
  if( event->type() == QEvent::MouseButtonPress )
  {
    QMouseEvent* mouse = static_cast<QMouseEvent*>( event );
    if( mouse->button() != Qt::LeftButton ) return false;
    m_resizing = allowedEdges( Qt::Edges( grip->property( "resizeEdges" ).toInt() ) );
    if( m_resizing == Qt::Edges() ) return false;
    m_pressPosition = mouse->globalPos();
    m_originalGeometry = m_window->geometry();
    grip->grabMouse();
    return true;
  }
  if( event->type() == QEvent::MouseMove && m_resizing != Qt::Edges() )
  {
    QMouseEvent* mouse = static_cast<QMouseEvent*>( event );
    if( mouse->buttons().testFlag( Qt::LeftButton ) ) resizeTo( mouse->globalPos() );
    else finishResize();
    return true;
  }
  if( event->type() == QEvent::MouseButtonRelease && m_resizing != Qt::Edges() )
  {
    QMouseEvent* mouse = static_cast<QMouseEvent*>( event );
    if( mouse->button() != Qt::LeftButton ) return false;
    resizeTo( mouse->globalPos() );
    finishResize();
    return true;
  }
  if( event->type() == QEvent::UngrabMouse ) m_resizing = Qt::Edges();
  return false;
}
