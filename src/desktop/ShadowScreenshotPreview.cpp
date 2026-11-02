#include "ShadowScreenshotPreview.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QFontMetrics>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace
{
class ScreenshotButton final : public QPushButton
{
public:
  ScreenshotButton( const QString& symbol, const QString& name, const QString& label, QWidget* parent )
    : QPushButton( parent ), m_symbol( symbol )
  {
    setObjectName( name );
    setAccessibleName( label );
    setToolTip( label );
    setFixedSize( 36, 36 );
    setAutoDefault( false );
    setDefault( false );
    setCursor( Qt::PointingHandCursor );
  }

protected:
  void paintEvent( QPaintEvent* ) override
  {
    QPainter painter( this );
    painter.setRenderHint( QPainter::Antialiasing );
    const bool active = isChecked() || isDown();
    const QRectF area = QRectF( rect() ).adjusted( 1, 1, -1, -1 );
    painter.setPen( hasFocus() ? QPen( QColor( "#2563EB" ), 1 ) : QPen( Qt::NoPen ) );
    painter.setBrush( active ? QColor( "#DCEBFF" ) : underMouse() ? QColor( "#E9EDF2" ) : QColor( "#FAFAFB" ) );
    painter.drawRoundedRect( area, 5, 5 );
    QColor color = active ? QColor( "#2563EB" ) : QColor( "#454B54" );
    if( !isEnabled() ) color.setAlpha( 70 );
    painter.translate( width() / 2.0 - 12, height() / 2.0 - 12 );
    painter.setPen( QPen( color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
    painter.setBrush( Qt::NoBrush );
    if( m_symbol == "select" )
    {
      painter.drawRect( QRectF( 5, 5, 14, 14 ) );
      for( const QPointF& point : QList<QPointF>() << QPointF( 5, 5 ) << QPointF( 19, 5 ) <<
           QPointF( 5, 19 ) << QPointF( 19, 19 ) )
      {
        painter.setBrush( QColor( "#FAFAFB" ) );
        painter.drawRect( QRectF( point - QPointF( 2, 2 ), QSizeF( 4, 4 ) ) );
      }
    }
    else if( m_symbol == "rectangle" ) painter.drawRoundedRect( QRectF( 4, 5, 16, 14 ), 1, 1 );
    else if( m_symbol == "ellipse" ) painter.drawEllipse( QRectF( 4, 4, 16, 16 ) );
    else if( m_symbol == "arrow" )
    {
      painter.drawLine( QPointF( 4, 20 ), QPointF( 20, 4 ) );
      painter.drawLine( QPointF( 12, 6 ), QPointF( 20, 4 ) );
      painter.drawLine( QPointF( 18, 12 ), QPointF( 20, 4 ) );
    }
    else if( m_symbol == "pen" )
    {
      painter.drawPolygon( QPolygonF() << QPointF( 5, 15 ) << QPointF( 16, 4 ) <<
        QPointF( 20, 8 ) << QPointF( 9, 19 ) << QPointF( 4, 20 ) );
      painter.drawLine( QPointF( 14, 6 ), QPointF( 18, 10 ) );
    }
    else if( m_symbol == "mosaic" )
    {
      painter.drawRect( QRectF( 3, 3, 18, 18 ) );
      painter.setPen( Qt::NoPen );
      painter.setBrush( color );
      for( int row = 0; row < 3; ++row )
        for( int column = 0; column < 3; ++column )
          if( (row + column) % 2 == 0 ) painter.drawRect( QRectF( 5 + column * 5, 5 + row * 5, 4, 4 ) );
    }
    else if( m_symbol == "text" )
    {
      painter.drawRoundedRect( QRectF( 3, 3, 18, 18 ), 2, 2 );
      painter.drawLine( QPointF( 7, 7 ), QPointF( 17, 7 ) );
      painter.drawLine( QPointF( 12, 7 ), QPointF( 12, 18 ) );
    }
    else if( m_symbol == "undo" || m_symbol == "redo" )
    {
      if( m_symbol == "redo" ) { painter.translate( 24, 0 ); painter.scale( -1, 1 ); }
      QPainterPath path;
      path.moveTo( 5, 8 ); path.lineTo( 14, 8 ); path.cubicTo( 23, 8, 23, 20, 14, 20 );
      painter.drawPath( path );
      painter.drawLine( QPointF( 5, 8 ), QPointF( 9, 4 ) );
      painter.drawLine( QPointF( 5, 8 ), QPointF( 9, 12 ) );
    }
    else if( m_symbol == "color" )
    {
      painter.setBrush( property( "annotationColor" ).value<QColor>() );
      painter.drawEllipse( QRectF( 5, 5, 14, 14 ) );
    }
    else if( m_symbol == "copy" )
    {
      painter.drawRect( QRectF( 8, 8, 12, 12 ) );
      painter.drawPolyline( QPolygonF() << QPointF( 5, 16 ) << QPointF( 3, 16 ) <<
        QPointF( 3, 3 ) << QPointF( 16, 3 ) << QPointF( 16, 5 ) );
    }
    else if( m_symbol == "save" )
    {
      painter.drawLine( QPointF( 12, 3 ), QPointF( 12, 16 ) );
      painter.drawPolyline( QPolygonF() << QPointF( 7, 11 ) << QPointF( 12, 16 ) << QPointF( 17, 11 ) );
      painter.drawPolyline( QPolygonF() << QPointF( 4, 16 ) << QPointF( 4, 21 ) <<
        QPointF( 20, 21 ) << QPointF( 20, 16 ) );
    }
    else if( m_symbol == "cancel" )
    {
      painter.drawLine( QPointF( 5, 5 ), QPointF( 19, 19 ) );
      painter.drawLine( QPointF( 19, 5 ), QPointF( 5, 19 ) );
    }
    else if( m_symbol == "send" )
      painter.drawPolyline( QPolygonF() << QPointF( 4, 12 ) << QPointF( 10, 18 ) << QPointF( 21, 6 ) );
  }

private:
  QString m_symbol;
};
}

class ShadowScreenshotCanvas final : public QWidget
{
public:
  enum Tool { Select, Rectangle, Ellipse, Arrow, Pen, Mosaic, Text };

  ShadowScreenshotCanvas( const QPixmap& image, const QRect& selection, bool desktop, QWidget* parent )
    : QWidget( parent ), m_image( image.toImage() ), m_desktop( desktop )
  {
    m_image.setDevicePixelRatio( 1 );
    m_selection = selection.isEmpty() ? m_image.rect() : selection.intersected( m_image.rect() );
    setObjectName( QStringLiteral( "ShadowScreenshotCanvas" ) );
    setAccessibleName( QStringLiteral( "截图编辑区域，拖动蓝色边框调整范围" ) );
    setMouseTracking( true );
    setFocusPolicy( Qt::StrongFocus );
    m_text = new QLineEdit( this );
    m_text->setObjectName( QStringLiteral( "ShadowScreenshotTextInput" ) );
    m_text->setPlaceholderText( QStringLiteral( "输入文字，Enter 确认" ) );
    m_text->setStyleSheet( QStringLiteral(
      "QLineEdit{background:#FFFFFF;color:#172033;border:1px solid #2563EB;padding:5px;font-size:16px;}" ) );
    m_text->hide();
    m_text->installEventFilter( this );
    connect( m_text, &QLineEdit::editingFinished, this, [this]() { commitText(); } );
  }

  void setTool( Tool tool )
  {
    commitText();
    m_tool = tool;
    setCursor( tool == Select ? Qt::SizeAllCursor : tool == Text ? Qt::IBeamCursor : Qt::CrossCursor );
    setFocus( Qt::OtherFocusReason );
  }
  void setColor( const QColor& color ) { m_color = color; }
  void undo()
  {
    commitText();
    if( m_edits.isEmpty() ) return;
    m_redo.append( m_edits.takeLast() );
    changed();
  }
  void redo()
  {
    if( m_redo.isEmpty() ) return;
    m_edits.append( m_redo.takeLast() );
    changed();
  }
  bool canUndo() const { return !m_edits.isEmpty(); }
  bool canRedo() const { return !m_redo.isEmpty(); }
  QSize selectedSize() const { return m_selection.size(); }
  QRect selectionOnScreen() const { return toView( m_selection ).toAlignedRect(); }
  QImage selectedImage() const
  {
    if( m_image.isNull() || m_selection.isEmpty() ) return QImage();
    QImage result = m_image.copy( m_selection );
    QPainter painter( &result );
    painter.setRenderHint( QPainter::Antialiasing );
    painter.translate( -m_selection.topLeft() );
    for( const Edit& edit : m_edits ) drawEdit( painter, edit );
    return result;
  }
  void commitText()
  {
    if( !m_text->isVisible() ) return;
    const QString text = m_text->text().trimmed();
    m_text->hide();
    m_text->clear();
    if( text.isEmpty() ) return;
    Edit edit;
    edit.tool = Text; edit.start = m_textOrigin; edit.color = m_color; edit.text = text;
    m_edits.append( edit );
    m_redo.clear();
    changed();
  }
  std::function<void()> onChanged;

protected:
  void resizeEvent( QResizeEvent* ) override { if( onChanged ) onChanged(); }
  void paintEvent( QPaintEvent* ) override
  {
    QPainter painter( this );
    painter.fillRect( rect(), QColor( "#34383F" ) );
    if( m_image.isNull() ) return;
    const QRectF imageRect = toView( m_image.rect() );
    const QRectF selected = toView( m_selection );
    painter.setRenderHint( QPainter::SmoothPixmapTransform, scale() < 1.0 );
    painter.drawImage( imageRect, m_image );
    painter.save();
    painter.setClipRect( selected );
    painter.translate( origin() );
    painter.scale( scale(), scale() );
    painter.setRenderHint( QPainter::Antialiasing );
    for( const Edit& edit : m_edits ) drawEdit( painter, edit );
    if( m_drawing ) drawEdit( painter, m_current );
    painter.restore();
    QPainterPath shade;
    shade.addRect( rect() );
    shade.addRect( selected );
    shade.setFillRule( Qt::OddEvenFill );
    painter.fillPath( shade, QColor( 0, 0, 0, 145 ) );
    painter.setRenderHint( QPainter::Antialiasing );
    painter.setPen( QPen( QColor( "#3486FF" ), 1.5 ) );
    painter.setBrush( Qt::NoBrush );
    painter.drawRect( selected );
    painter.setPen( QPen( Qt::white, 1.5 ) );
    painter.setBrush( QColor( "#3486FF" ) );
    for( const QPointF& point : handles() ) painter.drawEllipse( point, 3.8, 3.8 );
  }
  void mousePressEvent( QMouseEvent* event ) override
  {
    if( event->button() == Qt::RightButton ) { window()->close(); return; }
    if( event->button() != Qt::LeftButton || m_image.isNull() ) return;
    commitText();
    const QPointF point = toImage( event->localPos() );
    const int handle = handleAt( event->localPos() );
    if( handle >= 0 || m_tool == Select )
    {
      if( handle < 0 && !m_selection.contains( point.toPoint() ) ) return;
      m_handle = handle;
      m_cropDragging = true;
      m_press = point;
      m_originalSelection = m_selection;
      return;
    }
    if( !m_selection.contains( point.toPoint() ) ) return;
    if( m_tool == Text )
    {
      m_textOrigin = point;
      m_text->setGeometry( qBound( 0, event->pos().x(), qMax( 0, width() - 240 ) ),
        qBound( 0, event->pos().y(), qMax( 0, height() - 36 ) ), qMin( 240, width() ), 36 );
      m_text->show();
      m_text->raise();
      m_text->setFocus();
      return;
    }
    m_current = Edit();
    m_current.tool = m_tool; m_current.color = m_color;
    m_current.start = m_current.end = point;
    m_current.points.append( point );
    m_drawing = true;
  }
  void mouseMoveEvent( QMouseEvent* event ) override
  {
    if( m_cropDragging )
    {
      const QPoint delta = (toImage( event->localPos() ) - m_press).toPoint();
      QRect selection = m_originalSelection;
      if( m_handle < 0 )
      {
        selection.moveTopLeft( QPoint(
          qBound( 0, selection.left() + delta.x(), m_image.width() - selection.width() ),
          qBound( 0, selection.top() + delta.y(), m_image.height() - selection.height() ) ) );
      }
      else
      {
        const int minimum = qMin( 8, qMin( m_image.width(), m_image.height() ) );
        if( m_handle == 0 || m_handle == 6 || m_handle == 7 )
          selection.setLeft( qBound( 0, selection.left() + delta.x(), selection.right() - minimum + 1 ) );
        if( m_handle == 2 || m_handle == 3 || m_handle == 4 )
          selection.setRight( qBound( selection.left() + minimum - 1, selection.right() + delta.x(), m_image.width() - 1 ) );
        if( m_handle == 0 || m_handle == 1 || m_handle == 2 )
          selection.setTop( qBound( 0, selection.top() + delta.y(), selection.bottom() - minimum + 1 ) );
        if( m_handle == 4 || m_handle == 5 || m_handle == 6 )
          selection.setBottom( qBound( selection.top() + minimum - 1, selection.bottom() + delta.y(), m_image.height() - 1 ) );
      }
      m_selection = selection;
      changed();
    }
    else if( m_drawing )
    {
      m_current.end = boundedPoint( toImage( event->localPos() ) );
      if( m_tool == Pen ) m_current.points.append( m_current.end );
      update();
    }
    else
    {
      const int handle = handleAt( event->localPos() );
      const Qt::CursorShape shape = handle < 0 ? (m_tool == Select ? Qt::SizeAllCursor :
        m_tool == Text ? Qt::IBeamCursor : Qt::CrossCursor) :
        handle == 1 || handle == 5 ? Qt::SizeVerCursor :
        handle == 3 || handle == 7 ? Qt::SizeHorCursor :
        handle == 0 || handle == 4 ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
      setCursor( shape );
    }
  }
  void mouseReleaseEvent( QMouseEvent* event ) override
  {
    if( event->button() != Qt::LeftButton ) return;
    if( m_cropDragging )
    {
      mouseMoveEvent( event );
      m_cropDragging = false;
    }
    if( !m_drawing ) return;
    m_current.end = boundedPoint( toImage( event->localPos() ) );
    if( m_current.tool == Pen ) m_current.points.append( m_current.end );
    m_edits.append( m_current );
    m_redo.clear();
    m_drawing = false;
    changed();
  }
  bool eventFilter( QObject* watched, QEvent* event ) override
  {
    if( watched == m_text && event->type() == QEvent::KeyPress )
    {
      QKeyEvent* key = static_cast<QKeyEvent*>( event );
      if( key->key() == Qt::Key_Escape )
      {
        m_text->hide();
        m_text->clear();
        setFocus();
        return true;
      }
      if( key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter )
      {
        commitText();
        setFocus();
        return true;
      }
    }
    return QWidget::eventFilter( watched, event );
  }

private:
  struct Edit
  {
    Tool tool = Pen;
    QPointF start;
    QPointF end;
    QColor color;
    QVector<QPointF> points;
    QString text;
  };
  qreal scale() const
  {
    if( m_desktop ) return qreal( width() ) / qMax( 1, m_image.width() );
    return qMin( 1.0, qMin( qMax( 1, width() - 80 ) / qreal( qMax( 1, m_image.width() ) ),
      qMax( 1, height() - 180 ) / qreal( qMax( 1, m_image.height() ) ) ) );
  }
  QPointF origin() const
  {
    if( m_desktop ) return QPointF();
    return QPointF( (width() - m_image.width() * scale()) / 2,
      (height() - m_image.height() * scale()) / 2 - 25 );
  }
  QRectF toView( const QRect& source ) const
  {
    return QRectF( origin() + QPointF( source.x(), source.y() ) * scale(), QSizeF( source.size() ) * scale() );
  }
  QPointF toImage( const QPointF& point ) const { return (point - origin()) / scale(); }
  QPointF boundedPoint( const QPointF& point ) const
  {
    return QPointF( qBound( qreal( m_selection.left() ), point.x(), qreal( m_selection.right() ) ),
      qBound( qreal( m_selection.top() ), point.y(), qreal( m_selection.bottom() ) ) );
  }
  QList<QPointF> handles() const
  {
    const QRectF bounds = toView( m_selection );
    return QList<QPointF>() << bounds.topLeft() << QPointF( bounds.center().x(), bounds.top() ) <<
      bounds.topRight() << QPointF( bounds.right(), bounds.center().y() ) << bounds.bottomRight() <<
      QPointF( bounds.center().x(), bounds.bottom() ) << bounds.bottomLeft() <<
      QPointF( bounds.left(), bounds.center().y() );
  }
  int handleAt( const QPointF& point ) const
  {
    const QList<QPointF> points = handles();
    for( int index = 0; index < points.size(); ++index )
      if( QRectF( points.at( index ) - QPointF( 8, 8 ), QSizeF( 16, 16 ) ).contains( point ) ) return index;
    return -1;
  }
  void changed()
  {
    update();
    if( onChanged ) onChanged();
  }
  void drawEdit( QPainter& painter, const Edit& edit ) const
  {
    painter.save();
    painter.setPen( QPen( edit.color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
    painter.setBrush( Qt::NoBrush );
    const QRectF area = QRectF( edit.start, edit.end ).normalized();
    if( edit.tool == Rectangle ) painter.drawRect( area );
    else if( edit.tool == Ellipse ) painter.drawEllipse( area );
    else if( edit.tool == Arrow )
    {
      const QPointF direction = edit.end - edit.start;
      const qreal length = std::hypot( direction.x(), direction.y() );
      if( length > 1 )
      {
        const QPointF unit = direction / length;
        const QPointF normal( -unit.y(), unit.x() );
        painter.drawLine( edit.start, edit.end );
        painter.drawPolyline( QPolygonF() << edit.end - unit * 14 + normal * 7 <<
          edit.end << edit.end - unit * 14 - normal * 7 );
      }
    }
    else if( edit.tool == Pen )
    {
      if( edit.points.size() > 1 ) painter.drawPolyline( QPolygonF( edit.points ) );
      else painter.drawPoint( edit.start );
    }
    else if( edit.tool == Mosaic )
    {
      const QRect pixels = area.toAlignedRect().intersected( m_image.rect() );
      if( !pixels.isEmpty() )
      {
        const QImage blurred = m_image.copy( pixels ).scaled( qMax( 1, pixels.width() / 16 ),
          qMax( 1, pixels.height() / 16 ), Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
        painter.setRenderHint( QPainter::SmoothPixmapTransform, false );
        painter.drawImage( pixels, blurred );
      }
    }
    else if( edit.tool == Text )
    {
      QFont font = painter.font();
      font.setFamily( QStringLiteral( "Microsoft YaHei UI" ) );
      font.setPixelSize( 20 );
      painter.setFont( font );
      painter.drawText( edit.start + QPointF( 0, QFontMetrics( font ).ascent() ), edit.text );
    }
    painter.restore();
  }

  QImage m_image;
  QRect m_selection;
  bool m_desktop;
  Tool m_tool = Select;
  QColor m_color = QColor( "#EF4444" );
  QVector<Edit> m_edits;
  QVector<Edit> m_redo;
  Edit m_current;
  bool m_drawing = false;
  bool m_cropDragging = false;
  int m_handle = -1;
  QPointF m_press;
  QRect m_originalSelection;
  QLineEdit* m_text;
  QPointF m_textOrigin;
};

ShadowScreenshotPreview::ShadowScreenshotPreview( const QPixmap& image, QWidget* parent )
  : ShadowScreenshotPreview( image, QRect(), QRect(), parent )
{
}

ShadowScreenshotPreview::ShadowScreenshotPreview( const QPixmap& image, const QRect& desktopGeometry,
  const QRect& selection, QWidget* parent )
  : QDialog( parent )
{
  setObjectName( QStringLiteral( "ShadowScreenshotPreview" ) );
  setProperty( "shadowDialogChromeSkip", true );
  setWindowTitle( QStringLiteral( "截图编辑" ) );
  setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
  setWindowModality( Qt::ApplicationModal );
  const QScreen* display = parent ? parent->screen() : QGuiApplication::primaryScreen();
  const QRect available = desktopGeometry.isEmpty()
    ? (display ? display->availableGeometry() : QRect( 0, 0, 1280, 800 )) : desktopGeometry;
  setGeometry( available );
  QVBoxLayout* layout = new QVBoxLayout( this );
  layout->setContentsMargins( 0, 0, 0, 0 );
  m_canvas = new ShadowScreenshotCanvas( image, selection, !desktopGeometry.isEmpty(), this );
  layout->addWidget( m_canvas );
  m_toolbar = new QWidget( m_canvas );
  m_toolbar->setObjectName( QStringLiteral( "ShadowScreenshotToolbar" ) );
  m_toolbar->setAttribute( Qt::WA_StyledBackground );
  m_toolbar->setStyleSheet( QStringLiteral(
    "QWidget#ShadowScreenshotToolbar{background:#FAFAFB;border:1px solid #D7DDE5;border-radius:6px;}" ) );
  m_toolbarLayout = new QGridLayout( m_toolbar );
  m_toolbarLayout->setContentsMargins( 8, 6, 8, 6 );
  m_toolbarLayout->setSpacing( 2 );
  QButtonGroup* tools = new QButtonGroup( this );
  auto button = [&]( const QString& symbol, const QString& object, const QString& label ) {
    ScreenshotButton* action = new ScreenshotButton( symbol, object, label, m_toolbar );
    m_buttons.append( action );
    return action;
  };
  auto tool = [&]( const QString& symbol, const QString& label, ShadowScreenshotCanvas::Tool kind ) {
    ScreenshotButton* action = button( symbol, "ShadowScreenshot" + symbol, label );
    action->setCheckable( true );
    tools->addButton( action );
    if( kind == ShadowScreenshotCanvas::Select ) action->setChecked( true );
    connect( action, &QPushButton::clicked, this, [this, kind]() { m_canvas->setTool( kind ); } );
  };
  tool( "select", QStringLiteral( "调整截图范围" ), ShadowScreenshotCanvas::Select );
  tool( "rectangle", QStringLiteral( "矩形标注" ), ShadowScreenshotCanvas::Rectangle );
  tool( "ellipse", QStringLiteral( "圆形标注" ), ShadowScreenshotCanvas::Ellipse );
  tool( "arrow", QStringLiteral( "箭头" ), ShadowScreenshotCanvas::Arrow );
  tool( "pen", QStringLiteral( "画笔" ), ShadowScreenshotCanvas::Pen );
  tool( "mosaic", QStringLiteral( "马赛克" ), ShadowScreenshotCanvas::Mosaic );
  tool( "text", QStringLiteral( "文字" ), ShadowScreenshotCanvas::Text );
  ScreenshotButton* color = button( "color", "ShadowScreenshotColor", QStringLiteral( "标注颜色" ) );
  color->setProperty( "annotationColor", QColor( "#EF4444" ) );
  QMenu* colors = new QMenu( color );
  colors->setStyleSheet( QStringLiteral( "QMenu{background:#FFFFFF;color:#172033;padding:5px;}QMenu::item{padding:8px 18px;}" ) );
  const QStringList colorNames = QStringList() << QStringLiteral( "红色" ) << QStringLiteral( "蓝色" ) <<
    QStringLiteral( "绿色" ) << QStringLiteral( "黄色" ) << QStringLiteral( "黑色" ) << QStringLiteral( "白色" );
  const QStringList colorValues = QStringList() << "#EF4444" << "#2563EB" << "#16A34A" << "#FACC15" << "#172033" << "#FFFFFF";
  for( int index = 0; index < colorNames.size(); ++index )
  {
    QAction* action = colors->addAction( colorNames.at( index ) );
    const QColor value( colorValues.at( index ) );
    connect( action, &QAction::triggered, this, [this, color, value]() {
      m_canvas->setColor( value );
      color->setProperty( "annotationColor", value );
      color->update();
    } );
  }
  connect( color, &QPushButton::clicked, this, [color, colors]() {
    colors->popup( color->mapToGlobal( QPoint( 0, color->height() ) ) );
  } );
  m_undo = button( "undo", "ShadowScreenshotUndo", QStringLiteral( "撤销 (Ctrl+Z)" ) );
  m_redo = button( "redo", "ShadowScreenshotRedo", QStringLiteral( "重做 (Ctrl+Y)" ) );
  connect( m_undo, &QPushButton::clicked, this, [this]() { m_canvas->undo(); } );
  connect( m_redo, &QPushButton::clicked, this, [this]() { m_canvas->redo(); } );
  ScreenshotButton* copy = button( "copy", "ShadowScreenshotCopy", QStringLiteral( "复制图片 (Ctrl+C)" ) );
  connect( copy, &QPushButton::clicked, this, [this]() {
    m_canvas->commitText();
    QApplication::clipboard()->setImage( selectedImage().toImage() );
    showFeedback( QStringLiteral( "图片已复制" ), false );
  } );
  ScreenshotButton* save = button( "save", "ShadowScreenshotSave", QStringLiteral( "保存图片 (Ctrl+S)" ) );
  connect( save, &QPushButton::clicked, this, [this]() {
    m_canvas->commitText();
    QString path = QFileDialog::getSaveFileName( this, QStringLiteral( "保存截图" ),
      QStringLiteral( "截图-%1.png" ).arg( QDateTime::currentDateTime().toString( "yyyyMMdd-HHmmss" ) ),
      QStringLiteral( "PNG 图片 (*.png)" ) );
    if( path.isEmpty() ) return;
    if( !path.endsWith( ".png", Qt::CaseInsensitive ) ) path += ".png";
    saveImage( path );
  } );
  ScreenshotButton* cancel = button( "cancel", "ShadowScreenshotCancel", QStringLiteral( "取消 (Esc)" ) );
  ScreenshotButton* send = button( "send", "ShadowScreenshotSend", QStringLiteral( "完成并发送 (Enter)" ) );
  send->setEnabled( !image.isNull() );
  copy->setEnabled( !image.isNull() );
  save->setEnabled( !image.isNull() );
  connect( cancel, &QPushButton::clicked, this, &QDialog::reject );
  connect( send, &QPushButton::clicked, this, [this]() {
    m_canvas->commitText();
    emit sendRequested();
  } );
  m_dimensions = new QLabel( m_canvas );
  m_dimensions->setObjectName( QStringLiteral( "ShadowScreenshotDimensions" ) );
  m_dimensions->setStyleSheet( QStringLiteral(
    "QLabel{background:#33383F;color:#FFFFFF;border-radius:3px;padding:4px 8px;font:12px 'Microsoft YaHei UI';}" ) );
  mp_feedback = new QLabel( m_canvas );
  mp_feedback->setObjectName( QStringLiteral( "ShadowScreenshotFeedback" ) );
  mp_feedback->setTextFormat( Qt::PlainText );
  mp_feedback->setWordWrap( true );
  mp_feedback->setTextInteractionFlags( Qt::TextSelectableByMouse );
  mp_feedback->hide();
  auto shortcut = [&]( const QKeySequence& sequence, QPushButton* action ) {
    QShortcut* key = new QShortcut( sequence, this );
    connect( key, &QShortcut::activated, action, &QPushButton::click );
  };
  shortcut( QKeySequence::Undo, m_undo );
  shortcut( QKeySequence::Redo, m_redo );
  shortcut( QKeySequence::Copy, copy );
  shortcut( QKeySequence::Save, save );
  m_canvas->onChanged = [this]() { positionControls(); };
  m_canvas->setTool( ShadowScreenshotCanvas::Select );
  positionControls();
}

QPixmap ShadowScreenshotPreview::selectedImage() const
{
  return QPixmap::fromImage( m_canvas->selectedImage() );
}

bool ShadowScreenshotPreview::saveImage( const QString& path )
{
  m_canvas->commitText();
  QSaveFile file( path );
  if( !file.open( QIODevice::WriteOnly ) ||
      !selectedImage().toImage().save( &file, "PNG" ) || !file.commit() )
  {
    showFeedback( QStringLiteral( "保存失败，请检查保存位置后重试。" ), true );
    return false;
  }
  showFeedback( QStringLiteral( "截图已保存" ), false );
  return true;
}

void ShadowScreenshotPreview::keyPressEvent( QKeyEvent* event )
{
  if( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter )
  {
    m_canvas->commitText();
    if( !selectedImage().isNull() ) emit sendRequested();
    event->accept();
    return;
  }
  QDialog::keyPressEvent( event );
}

void ShadowScreenshotPreview::showFeedback( const QString& message, bool error )
{
  mp_feedback->setText( message );
  mp_feedback->setStyleSheet( error ? QStringLiteral(
    "QLabel{color:#FFFFFF;background:#9F2525;border-radius:5px;padding:8px;font-size:13px;}" ) :
    QStringLiteral( "QLabel{color:#FFFFFF;background:#263142;border-radius:5px;padding:8px;font-size:13px;}" ) );
  mp_feedback->show();
  positionControls();
}

void ShadowScreenshotPreview::showSendError( const QString& message )
{
  showFeedback( message, true );
}

void ShadowScreenshotPreview::positionControls()
{
  if( !m_toolbar || !m_dimensions || !m_canvas ) return;
  const int columns = qBound( 1, (m_canvas->width() - 40) / 38, m_buttons.size() );
  if( m_toolbarColumns != columns )
  {
    while( QLayoutItem* item = m_toolbarLayout->takeAt( 0 ) ) delete item;
    for( int index = 0; index < m_buttons.size(); ++index )
      m_toolbarLayout->addWidget( m_buttons.at( index ), index / columns, index % columns );
    m_toolbarColumns = columns;
  }
  m_toolbar->adjustSize();
  const QRect selected = m_canvas->selectionOnScreen();
  int top = selected.bottom() + 12;
  if( top + m_toolbar->height() > m_canvas->height() - 12 )
    top = selected.top() - m_toolbar->height() - 12;
  top = qBound( 8, top, qMax( 8, m_canvas->height() - m_toolbar->height() - 8 ) );
  const int left = qBound( 8, selected.right() - m_toolbar->width(),
    qMax( 8, m_canvas->width() - m_toolbar->width() - 8 ) );
  m_toolbar->move( left, top );
  m_toolbar->raise();
  m_dimensions->setText( QStringLiteral( "%1 × %2 px" )
    .arg( m_canvas->selectedSize().width() ).arg( m_canvas->selectedSize().height() ) );
  m_dimensions->adjustSize();
  int badgeTop = selected.top() - m_dimensions->height() - 10;
  if( badgeTop < 8 ) badgeTop = selected.top() + 10;
  m_dimensions->move( qBound( 8, selected.left(), qMax( 8, m_canvas->width() - m_dimensions->width() - 8 ) ),
    qBound( 8, badgeTop, qMax( 8, m_canvas->height() - m_dimensions->height() - 8 ) ) );
  m_dimensions->raise();
  m_undo->setEnabled( m_canvas->canUndo() );
  m_redo->setEnabled( m_canvas->canRedo() );
  if( !mp_feedback->isHidden() )
  {
    mp_feedback->setFixedWidth( qMin( 560, qMax( 100, m_canvas->width() - 32 ) ) );
    mp_feedback->adjustSize();
    const int feedbackTop = top > m_canvas->height() / 2
      ? top - mp_feedback->height() - 10 : top + m_toolbar->height() + 10;
    mp_feedback->move( qBound( 8, left, qMax( 8, m_canvas->width() - mp_feedback->width() - 8 ) ),
      qBound( 8, feedbackTop, qMax( 8, m_canvas->height() - mp_feedback->height() - 8 ) ) );
    mp_feedback->raise();
  }
}
