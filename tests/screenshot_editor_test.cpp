#include "ShadowScreenshotPreview.h"
#include "ShadowRegionCapture.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static void mouse( QWidget* target, QEvent::Type type, const QPoint& point, Qt::MouseButton button, Qt::MouseButtons buttons )
{
  QMouseEvent event( type, point, target->mapToGlobal( point ), button, buttons, Qt::NoModifier );
  QApplication::sendEvent( target, &event );
}

static void drag( QWidget* target, const QPoint& begin, const QPoint& end )
{
  mouse( target, QEvent::MouseButtonPress, begin, Qt::LeftButton, Qt::LeftButton );
  mouse( target, QEvent::MouseMove, (begin + end) / 2, Qt::NoButton, Qt::LeftButton );
  mouse( target, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton );
  mouse( target, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton );
  QApplication::processEvents();
}

static int redPixels( const QImage& image )
{
  int count = 0;
  for( int row = 0; row < image.height(); ++row )
    for( int column = 0; column < image.width(); ++column )
    {
      const QColor color = image.pixelColor( column, row );
      if( color.red() > 210 && color.green() < 110 && color.blue() < 110 ) ++count;
    }
  return count;
}

int main( int argc, char** argv )
{
  QApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  ShadowDialogChrome::install( &application );
  QImage desktop( 1100, 720, QImage::Format_RGB32 );
  desktop.fill( QColor( "#FFFFFF" ) );
  QPainter fixture( &desktop );
  fixture.fillRect( 0, 0, 1100, 44, QColor( "#E8F0F4" ) );
  fixture.fillRect( 0, 44, 80, 676, QColor( "#172033" ) );
  fixture.fillRect( 80, 44, 250, 676, QColor( "#F4F7FC" ) );
  fixture.setPen( QColor( "#172033" ) );
  fixture.setFont( QFont( "Microsoft YaHei UI", 13 ) );
  fixture.drawText( QRect( 360, 100, 620, 80 ), Qt::AlignLeft,
    QStringLiteral( "合成聊天界面 · 截图编辑回归\n只包含测试数据" ) );
  for( int row = 240; row < 320; ++row )
    for( int column = 430; column < 550; ++column )
      fixture.fillRect( column, row, 1, 1, (row + column) % 2 ? Qt::black : Qt::white );
  fixture.end();
  const QRect initial( 360, 150, 650, 330 );
  ShadowRegionCapture capture( QPixmap::fromImage( desktop ) );
  drag( &capture, initial.topLeft(), initial.bottomRight() );
  CHECK( capture.result() == QDialog::Accepted && capture.selectedImage().size() == initial.size() );
  CHECK( capture.screenImage().toImage() == desktop && capture.selectedRect() == initial );
  ShadowScreenshotPreview editor( QPixmap::fromImage( desktop ), QRect( -1100, 0, 1100, 720 ), initial, Q_NULLPTR );
  editor.show();
  application.processEvents();
  QWidget* canvas = editor.findChild<QWidget*>( "ShadowScreenshotCanvas" );
  QWidget* toolbar = editor.findChild<QWidget*>( "ShadowScreenshotToolbar" );
  QLabel* dimensions = editor.findChild<QLabel*>( "ShadowScreenshotDimensions" );
  CHECK( canvas && toolbar && dimensions && editor.windowFlags().testFlag( Qt::FramelessWindowHint ) );
  CHECK( !editor.findChild<QWidget*>( "shadowDialogHeader" ) && !editor.findChild<QScrollArea*>() );
  CHECK( editor.selectedImage().toImage() == desktop.copy( initial ) );
  CHECK( editor.rect().contains( toolbar->geometry() ) && !initial.intersects( toolbar->geometry() ) );
  CHECK( dimensions->text().contains( "650" ) && dimensions->text().contains( "330" ) );
  CHECK( editor.x() == -1100 );
  const QImage rendered = canvas->grab().toImage();
  const qreal deviceScale = rendered.devicePixelRatio();
  CHECK( rendered.pixelColor( qRound( 395 * deviceScale ), qRound( 180 * deviceScale ) ) == QColor( Qt::white ) );
  CHECK( rendered.pixelColor( qRound( 340 * deviceScale ), qRound( 120 * deviceScale ) ).red() < 150 );
  editor.grab().save( QDir::currentPath() + "/screenshot-editor-initial.png" );
  auto tool = [&]( const QString& name ) {
    QPushButton* action = editor.findChild<QPushButton*>( "ShadowScreenshot" + name );
    if( action ) action->click();
  };
  tool( "rectangle" );
  drag( canvas, QPoint( 600, 215 ), QPoint( 900, 360 ) );
  CHECK( redPixels( editor.selectedImage().toImage() ) > 1500 );
  const QImage rectangle = editor.selectedImage().toImage();
  tool( "Undo" );
  CHECK( editor.selectedImage().toImage() == desktop.copy( initial ) );
  tool( "Redo" );
  CHECK( editor.selectedImage().toImage() == rectangle );
  tool( "arrow" );
  drag( canvas, QPoint( 640, 400 ), QPoint( 820, 280 ) );
  CHECK( redPixels( editor.selectedImage().toImage() ) > redPixels( rectangle ) );
  tool( "ellipse" );
  drag( canvas, QPoint( 590, 200 ), QPoint( 680, 280 ) );
  tool( "pen" );
  drag( canvas, QPoint( 500, 420 ), QPoint( 620, 390 ) );
  tool( "mosaic" );
  drag( canvas, QPoint( 430, 240 ), QPoint( 550, 320 ) );
  const QImage obscured = editor.selectedImage().toImage();
  CHECK( obscured.pixelColor( 100, 120 ) != desktop.pixelColor( 460, 270 ) );
  tool( "text" );
  mouse( canvas, QEvent::MouseButtonPress, QPoint( 700, 170 ), Qt::LeftButton, Qt::LeftButton );
  mouse( canvas, QEvent::MouseButtonRelease, QPoint( 700, 170 ), Qt::LeftButton, Qt::NoButton );
  QLineEdit* text = editor.findChild<QLineEdit*>( "ShadowScreenshotTextInput" );
  CHECK( text && text->isVisible() );
  text->setText( QStringLiteral( "请修改这里" ) );
  QKeyEvent enter( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
  QApplication::sendEvent( text, &enter );
  CHECK( !text->isVisible() && editor.selectedImage().toImage() != obscured );
  editor.grab().save( QDir::currentPath() + "/screenshot-editor-annotated.png" );
  const QImage edited = editor.selectedImage().toImage();
  tool( "Copy" );
  CHECK( QApplication::clipboard()->image() == edited );
  CHECK( editor.selectedImage().toImage() == edited );
  QTemporaryDir output;
  CHECK( output.isValid() );
  const QString savedPath = output.path() + "/edited.png";
  CHECK( editor.saveImage( savedPath ) );
  CHECK( QImage( savedPath ).convertToFormat( edited.format() ) == edited );
  CHECK( !editor.saveImage( output.path() + "/missing/failed.png" ) );
  CHECK( !QFileInfo::exists( output.path() + "/missing/failed.png" ) &&
    editor.selectedImage().toImage() == edited );
  editor.showSendError( QStringLiteral( "未发送：合成拒绝原因。截图仍保留，可重试或取消。" ) );
  CHECK( editor.isVisible() && editor.findChild<QLabel*>( "ShadowScreenshotFeedback" )->isVisible() );
  CHECK( editor.selectedImage().toImage() == edited );
  int sendCount = 0;
  QImage submitted;
  QObject::connect( &editor, &ShadowScreenshotPreview::sendRequested, &editor, [&]() {
    ++sendCount; submitted = editor.selectedImage().toImage();
  } );
  tool( "Send" );
  CHECK( sendCount == 1 && submitted == edited );
  tool( "select" );
  drag( canvas, QPoint( 1010, 480 ), QPoint( 970, 440 ) );
  CHECK( editor.selectedImage().size() == QSize( 610, 290 ) );
  CHECK( dimensions->text().contains( "610" ) && dimensions->text().contains( "290" ) );
  drag( canvas, QPoint( 800, 390 ), QPoint( 830, 405 ) );
  CHECK( editor.selectedImage().size() == QSize( 610, 290 ) );
  CHECK( editor.selectedImage().toImage() != edited.copy( 0, 0, 610, 290 ) );
  editor.grab().save( QDir::currentPath() + "/screenshot-editor-failure.png" );
  editor.resize( 440, 340 );
  application.processEvents();
  CHECK( editor.rect().contains( toolbar->geometry() ) );
  for( QPushButton* action : toolbar->findChildren<QPushButton*>() )
    CHECK( toolbar->rect().contains( action->geometry() ) && action->isVisible() && !action->accessibleName().isEmpty() );
  tool( "Cancel" );
  CHECK( editor.result() == QDialog::Rejected && sendCount == 1 );
  ShadowScreenshotPreview standalone( QPixmap::fromImage( desktop ) );
  standalone.show();
  application.processEvents();
  CHECK( standalone.selectedImage().toImage() == desktop );
  standalone.reject();
  ShadowScreenshotPreview empty( QPixmap(), Q_NULLPTR );
  CHECK( !empty.findChild<QPushButton*>( "ShadowScreenshotSend" )->isEnabled() );
  std::puts( "PASS screenshot editor: frozen context and negative screen origin, selected pixels, dimming, toolbar, crop resize/move, rectangle/ellipse/arrow/pen/mosaic/text, undo/redo, clipboard, PNG save and failure, edited send result, failure retention, small screen, cancel and empty image" );
  return 0;
}
