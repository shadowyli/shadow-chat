#include "ShadowWindowResize.h"
#include "ShadowWindowShape.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static void mouse( QWidget* target, QEvent::Type type, const QPoint& global,
  Qt::MouseButton button, Qt::MouseButtons buttons )
{
  QMouseEvent event( type, target->mapFromGlobal( global ), global, button, buttons, Qt::NoModifier );
  QApplication::sendEvent( target, &event );
}

static void drag( QWidget* grip, const QPoint& delta )
{
  const QPoint start = grip->mapToGlobal( grip->rect().center() );
  mouse( grip, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton );
  mouse( grip, QEvent::MouseMove, start + delta / 2, Qt::NoButton, Qt::LeftButton );
  mouse( grip, QEvent::MouseMove, start + delta, Qt::NoButton, Qt::LeftButton );
  mouse( grip, QEvent::MouseButtonRelease, start + delta, Qt::LeftButton, Qt::NoButton );
  QApplication::processEvents();
}

int main( int argc, char** argv )
{
  QApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  ShadowDialogChrome::install( &application );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  QWidget window;
  window.setWindowFlags( Qt::Window | Qt::FramelessWindowHint );
  window.setMinimumSize( 320, 240 );
  window.setMaximumSize( 1400, 1000 );
  window.setGeometry( 100, 100, 700, 500 );
  window.setMask( ShadowWindowShape::roundedMask( window.size() ) );
  new ShadowWindowResize( &window );
  window.show();
  application.processEvents();
  struct Direction
  {
    const char* name;
    Qt::Edges edges;
    QPoint delta;
  };
  const Direction directions[] = {
    { "Left", Qt::LeftEdge, QPoint( -40, 0 ) }, { "Right", Qt::RightEdge, QPoint( 40, 0 ) },
    { "Top", Qt::TopEdge, QPoint( 0, -30 ) }, { "Bottom", Qt::BottomEdge, QPoint( 0, 30 ) },
    { "TopLeft", Qt::TopEdge | Qt::LeftEdge, QPoint( -40, -30 ) },
    { "TopRight", Qt::TopEdge | Qt::RightEdge, QPoint( 40, -30 ) },
    { "BottomLeft", Qt::BottomEdge | Qt::LeftEdge, QPoint( -40, 30 ) },
    { "BottomRight", Qt::BottomEdge | Qt::RightEdge, QPoint( 40, 30 ) }
  };
  for( const Direction& direction : directions )
  {
    window.setGeometry( 100, 100, 700, 500 );
    application.processEvents();
    const QRect original = window.geometry();
    QWidget* grip = window.findChild<QWidget*>( QStringLiteral( "ShadowResize%1" ).arg( direction.name ) );
    CHECK( grip && grip->isVisible() );
    const QPoint at = grip->mapTo( &window, grip->rect().center() );
    CHECK( ShadowWindowResize::edgesAt( &window, at ) == direction.edges );
    const bool horizontal = direction.edges.testFlag( Qt::LeftEdge ) || direction.edges.testFlag( Qt::RightEdge );
    const bool vertical = direction.edges.testFlag( Qt::TopEdge ) || direction.edges.testFlag( Qt::BottomEdge );
    drag( grip, direction.delta );
    CHECK( window.width() == original.width() + (horizontal ? 40 : 0) );
    CHECK( window.height() == original.height() + (vertical ? 30 : 0) );
    if( direction.edges.testFlag( Qt::LeftEdge ) ) CHECK( window.geometry().right() == original.right() );
    else CHECK( window.x() == original.x() );
    if( direction.edges.testFlag( Qt::TopEdge ) ) CHECK( window.geometry().bottom() == original.bottom() );
    else CHECK( window.y() == original.y() );
    CHECK( QWidget::mouseGrabber() == Q_NULLPTR );
  }
  window.setGeometry( 100, 100, 700, 500 );
  application.processEvents();
  const int fixedRight = window.geometry().right();
  drag( window.findChild<QWidget*>( "ShadowResizeLeft" ), QPoint( 2000, 0 ) );
  CHECK( window.width() == 320 && window.geometry().right() == fixedRight );
  drag( window.findChild<QWidget*>( "ShadowResizeRight" ), QPoint( 2500, 0 ) );
  CHECK( window.width() == 1400 );
  drag( window.findChild<QWidget*>( "ShadowResizeBottom" ), QPoint( 0, -2000 ) );
  CHECK( window.height() == 240 );
  window.setGeometry( 100, 100, 700, 500 );
  window.setMinimumWidth( 700 );
  window.setMaximumWidth( 700 );
  application.processEvents();
  CHECK( ShadowWindowResize::edgesAt( &window, QPoint( 1, 200 ) ) == Qt::Edges() );
  const QRect fixedWidthGeometry = window.geometry();
  drag( window.findChild<QWidget*>( "ShadowResizeTopLeft" ), QPoint( -50, -40 ) );
  CHECK( window.width() == 700 && window.geometry().right() == fixedWidthGeometry.right() &&
    window.height() == fixedWidthGeometry.height() + 40 );
  window.setMinimumWidth( 320 );
  window.setMaximumWidth( 1400 );
  window.showMaximized();
  application.processEvents();
  CHECK( !window.findChild<QWidget*>( "ShadowResizeRight" )->isVisible() );
  CHECK( ShadowWindowResize::edgesAt( &window, QPoint( 1, 200 ) ) == Qt::Edges() );
  window.showNormal();
  application.processEvents();
  CHECK( window.findChild<QWidget*>( "ShadowResizeRight" )->isVisible() );
  CHECK( ShadowWindowResize::edgesAt( &window, QPoint( 80, 80 ) ) == Qt::Edges() );
  CHECK( ShadowWindowResize::edgesAt( &window, QPoint( -1, 80 ) ) == Qt::Edges() );
  QDialog dialog( &window );
  QVBoxLayout* layout = new QVBoxLayout( &dialog );
  QPushButton* content = new QPushButton( QStringLiteral( "内部按钮仍可点击" ), &dialog );
  layout->addWidget( content );
  dialog.setMinimumSize( 400, 300 );
  dialog.resize( 600, 440 );
  dialog.show();
  application.processEvents();
  QWidget* surface = dialog.findChild<QWidget*>( "shadowDialogSurface" );
  CHECK( surface && dialog.findChild<QWidget*>( "shadowDialogShadow" ) );
  QWidget* dialogGrip = surface->findChild<QWidget*>( "ShadowResizeLeft" );
  CHECK( dialogGrip && dialogGrip->isVisible() && dialogGrip->parentWidget() == surface );
  const int dialogWidth = dialog.width();
  drag( dialogGrip, QPoint( -45, 0 ) );
  CHECK( dialog.width() == dialogWidth + 45 );
  CHECK( !surface->mask().contains( QPoint() ) );
  bool clicked = false;
  QObject::connect( content, &QPushButton::clicked, &dialog, [&]() { clicked = true; } );
  const QPoint point = content->mapToGlobal( content->rect().center() );
  mouse( content, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton );
  mouse( content, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton );
  CHECK( clicked );
  dialog.grab().save( QDir::currentPath() + "/ui-041-resizable-dialog.png" );
  dialog.reject();
  std::puts( "PASS border resizing: all 4 sides and 4 corners, opposite-edge anchoring, min/max bounds, fixed width, pointer capture release, maximized disable/restore, dialog surface with shadow and rounded mask, normal controls still clickable" );
  return 0;
}
