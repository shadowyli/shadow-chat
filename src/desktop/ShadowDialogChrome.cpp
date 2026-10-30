#include "ShadowDialogChrome.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QRegion>
#include <QSizeGrip>
#include <QThread>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>

namespace
{
const char installedProperty[] = "_shadowDialogChromeInstalled";
const char decoratedProperty[] = "shadowDialogChrome";
const int headerHeight = 52;
const int cornerRadius = 12;
const int shadowPadding = 24;

class DialogShadow final : public QWidget
{
public:
  explicit DialogShadow( QWidget* parent )
    : QWidget( parent )
  {
    setObjectName( QStringLiteral( "shadowDialogShadow" ) );
    setAttribute( Qt::WA_TransparentForMouseEvents );
    setFocusPolicy( Qt::NoFocus );
  }

protected:
  void paintEvent( QPaintEvent* ) override
  {
    QPainter painter( this );
    painter.setRenderHint( QPainter::Antialiasing );
    const QRectF surface = QRectF( rect() ).adjusted( shadowPadding, shadowPadding,
      -shadowPadding, -shadowPadding ).translated( 0, 5 );
    for( int distance = 20; distance >= 0; --distance )
    {
      const int alpha = qRound( 42.0 * std::exp( -distance * distance / 98.0 ) );
      painter.setPen( QPen( QColor( 15, 23, 42, alpha ), 2 ) );
      painter.setBrush( Qt::NoBrush );
      painter.drawRoundedRect( surface.adjusted( -distance, -distance, distance, distance ),
        cornerRadius + distance, cornerRadius + distance );
    }
  }
};

class DialogCloseButton final : public QToolButton
{
public:
  explicit DialogCloseButton( QWidget* parent )
    : QToolButton( parent )
  {
    setObjectName( QStringLiteral( "shadowDialogClose" ) );
    setFixedSize( 40, 36 );
    setFocusPolicy( Qt::StrongFocus );
    setAccessibleName( QStringLiteral( "关闭对话框" ) );
    setAccessibleDescription( QStringLiteral( "关闭且不保存更改" ) );
    setToolTip( QStringLiteral( "关闭" ) );
    setStyleSheet( QStringLiteral(
      "QToolButton#shadowDialogClose { background: transparent; border: 1px solid transparent;"
      " border-radius: 6px; padding: 0; margin: 0; }"
      "QToolButton#shadowDialogClose:hover { background: #F3F4F6; }"
      "QToolButton#shadowDialogClose:pressed { background: #FEE2E2; }"
      "QToolButton#shadowDialogClose:focus { border-color: #64748B; }" ) );
  }

protected:
  void paintEvent( QPaintEvent* event ) override
  {
    QToolButton::paintEvent( event );
    QPainter painter( this );
    painter.setRenderHint( QPainter::Antialiasing );
    const QColor color = isDown() ? QColor( "#B91C1C" ) :
                         underMouse() ? QColor( "#DC2626" ) : QColor( "#64748B" );
    painter.setPen( QPen( color, 1.6, Qt::SolidLine, Qt::RoundCap ) );
    const QPointF center( width() / 2.0, height() / 2.0 );
    painter.drawLine( center + QPointF( -4.5, -4.5 ), center + QPointF( 4.5, 4.5 ) );
    painter.drawLine( center + QPointF( 4.5, -4.5 ), center + QPointF( -4.5, 4.5 ) );
  }

  void keyPressEvent( QKeyEvent* event ) override
  {
    if( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter )
    {
      event->accept();
      click();
      return;
    }
    QToolButton::keyPressEvent( event );
  }
};

class DialogChrome final : public QObject
{
public:
  explicit DialogChrome( QDialog* dialog )
    : QObject( dialog ), m_dialog( dialog ), m_dragging( false ), m_dragMoved( false )
  {
    dialog->setProperty( decoratedProperty, true );
    const int chromeHeight = dialog->property( "shadowDialogTitleHidden" ).toBool() ? 44 : headerHeight;
    const QSize originalSize = dialog->size();
    const QSize minimumSize = dialog->minimumSize();
    const QSize maximumSize = dialog->maximumSize();
    const bool wasResized = dialog->testAttribute( Qt::WA_Resized );
    const QPointer<QWidget> focusWidget = dialog->focusWidget();

    Qt::WindowFlags flags = dialog->windowFlags();
    flags |= Qt::FramelessWindowHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    dialog->setWindowFlags( flags );

    QPalette palette = dialog->palette();
    palette.setColor( QPalette::Window, Qt::transparent );
    dialog->setPalette( palette );
    dialog->setAutoFillBackground( false );
    dialog->setAttribute( Qt::WA_TranslucentBackground );
    dialog->setStyleSheet( dialog->styleSheet() + QStringLiteral(
      "QDialog[shadowDialogChrome=true] { background: transparent; border: 0; }" ) );
    QLayout* bodyLayout = dialog->layout();
    const QLayout::SizeConstraint constraint = bodyLayout->sizeConstraint();

    m_shadow = new DialogShadow( dialog );
    m_surface = new QWidget( dialog );
    m_surface->setObjectName( QStringLiteral( "shadowDialogSurface" ) );
    m_surface->setAttribute( Qt::WA_StyledBackground );
    m_surface->setStyleSheet( QStringLiteral(
      "QWidget#shadowDialogSurface { background: #FFFFFF; border: 1px solid #CBD5E1;"
      " border-radius: 12px; }" ) );
    QWidget* body = new QWidget( m_surface );
    body->setObjectName( QStringLiteral( "shadowDialogBody" ) );
    body->setContentsMargins( dialog->contentsMargins() );
    body->setStyleSheet( QStringLiteral(
      "QWidget#shadowDialogBody { background: #FFFFFF; border: 0; }" ) );
    body->setAttribute( Qt::WA_StyledBackground );
    body->setLayout( bodyLayout );
    dialog->setContentsMargins( 0, 0, 0, 0 );

    m_header = new QWidget( m_surface );
    m_header->setObjectName( QStringLiteral( "shadowDialogHeader" ) );
    m_header->setFixedHeight( chromeHeight );
    m_header->setAttribute( Qt::WA_StyledBackground );
    m_header->setStyleSheet( QStringLiteral(
      "QWidget#shadowDialogHeader { background: #FFFFFF; border: 0;"
      " border-bottom: 1px solid #E8EDF2; }" ) );
    QHBoxLayout* headerLayout = new QHBoxLayout( m_header );
    headerLayout->setContentsMargins( 16, chromeHeight == headerHeight ? 8 : 4, 8, chromeHeight == headerHeight ? 7 : 4 );
    headerLayout->setSpacing( 12 );
    headerLayout->setDirection( QBoxLayout::LeftToRight );

    m_title = new QLabel( m_header );
    m_title->setObjectName( QStringLiteral( "shadowDialogTitle" ) );
    m_title->setTextFormat( Qt::PlainText );
    m_title->setTextInteractionFlags( Qt::NoTextInteraction );
    m_title->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
    m_title->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    m_title->setAccessibleDescription( QStringLiteral( "对话框标题" ) );
    m_title->setStyleSheet( QStringLiteral(
      "QLabel#shadowDialogTitle { color: #172033; background: transparent;"
      " font-size: 14px; font-weight: 600; border: 0; padding: 0; margin: 0; }" ) );

    DialogCloseButton* closeButton = new DialogCloseButton( m_header );
    headerLayout->addWidget( m_title, 1 );
    if( dialog->property( "shadowDialogTitleHidden" ).toBool() ) headerLayout->addStretch();
    headerLayout->addWidget( closeButton );
    connect( closeButton, &QToolButton::clicked, dialog, &QDialog::reject );
    connect( dialog, &QWidget::windowTitleChanged, this, [this]() { updateTitle(); } );

    QVBoxLayout* surfaceLayout = new QVBoxLayout( m_surface );
    surfaceLayout->setContentsMargins( 1, 1, 1, 1 );
    surfaceLayout->setSpacing( 0 );
    surfaceLayout->addWidget( m_header );
    surfaceLayout->addWidget( body, 1 );
    QVBoxLayout* outerLayout = new QVBoxLayout( dialog );
    outerLayout->setContentsMargins( shadowPadding, shadowPadding, shadowPadding, shadowPadding );
    outerLayout->setSpacing( 0 );
    outerLayout->setSizeConstraint( constraint );
    outerLayout->addWidget( m_surface );

    const int frameSize = shadowPadding * 2 + 2;
    dialog->setMaximumSize( qMin( QWIDGETSIZE_MAX, qMax( maximumSize.width(), m_header->minimumSizeHint().width() ) + frameSize ),
                           qMin( QWIDGETSIZE_MAX, maximumSize.height() + chromeHeight + frameSize ) );
    dialog->setMinimumSize( qMin( QWIDGETSIZE_MAX, minimumSize.width() + frameSize ),
                           qMin( QWIDGETSIZE_MAX, minimumSize.height() + chromeHeight + frameSize ) );
    bodyLayout->activate();
    outerLayout->activate();
    if( wasResized )
      dialog->resize( qMin( QWIDGETSIZE_MAX, originalSize.width() + frameSize ),
        qMin( QWIDGETSIZE_MAX, originalSize.height() + chromeHeight + frameSize ) );
    dialog->setAttribute( Qt::WA_Resized, wasResized );
    if( focusWidget )
      focusWidget->setFocus( Qt::OtherFocusReason );

    dialog->installEventFilter( this );
    m_surface->installEventFilter( this );
    m_header->installEventFilter( this );
    m_title->installEventFilter( this );
    updateTitle();
    updateMask();
  }

protected:
  bool eventFilter( QObject* watched, QEvent* event ) override
  {
    if( !m_dialog )
      return false;

    if( qobject_cast<QSizeGrip*>( watched ) &&
        (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::Show) )
      updateSizeGrips();

    if( watched == m_surface && event->type() == QEvent::Resize )
      updateMask();

    if( watched == m_dialog )
    {
      switch( event->type() )
      {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::WindowStateChange:
        case QEvent::ChildPolished:
          updateMask();
          break;
        case QEvent::Hide:
        case QEvent::WindowDeactivate:
          m_dragging = false;
          break;
        default:
          break;
      }
      return false;
    }

    if( watched == m_title && (event->type() == QEvent::Resize || event->type() == QEvent::FontChange) )
      updateTitle();

    if( watched != m_header && watched != m_title )
      return false;

    switch( event->type() )
    {
      case QEvent::MouseButtonPress:
      {
        QMouseEvent* mouse = static_cast<QMouseEvent*>( event );
        if( mouse->button() != Qt::LeftButton || m_dialog->isMaximized() || m_dialog->isFullScreen() )
          break;
        m_pressPosition = mouse->globalPos();
        m_windowPosition = m_dialog->pos();
        m_dragging = true;
        m_dragMoved = false;
        event->accept();
        return true;
      }
      case QEvent::MouseMove:
      {
        QMouseEvent* mouse = static_cast<QMouseEvent*>( event );
        if( !m_dragging )
          break;
        if( !(mouse->buttons() & Qt::LeftButton) )
        {
          m_dragging = false;
          break;
        }
        const QPoint distance = mouse->globalPos() - m_pressPosition;
        if( m_dragMoved || distance.manhattanLength() >= QApplication::startDragDistance() )
        {
          m_dragMoved = true;
          m_dialog->move( m_windowPosition + distance );
        }
        event->accept();
        return true;
      }
      case QEvent::MouseButtonRelease:
        if( static_cast<QMouseEvent*>( event )->button() == Qt::LeftButton && m_dragging )
        {
          m_dragging = false;
          event->accept();
          return true;
        }
        break;
      case QEvent::UngrabMouse:
        m_dragging = false;
        break;
      default:
        break;
    }
    return false;
  }

private:
  void updateSizeGrips()
  {
    if( !m_dialog || !m_surface )
      return;
    const QRect bounds = m_surface->geometry().adjusted( 4, 4, -4, -4 );
    for( QSizeGrip* grip : m_dialog->findChildren<QSizeGrip*>( QString(), Qt::FindDirectChildrenOnly ) )
    {
      grip->installEventFilter( this );
      const QPoint position( m_dialog->isRightToLeft() ? bounds.left() : bounds.right() - grip->width() + 1,
        bounds.bottom() - grip->height() + 1 );
      if( grip->pos() != position )
        grip->move( position );
    }
  }

  void updateTitle()
  {
    if( !m_dialog || !m_title )
      return;
    QString title = m_dialog->windowTitle();
    title.replace( QStringLiteral( "BeeBEEP" ), QStringLiteral( "影聊" ), Qt::CaseInsensitive );
    if( title.trimmed().isEmpty() )
      title = QStringLiteral( "影聊" );
    m_title->setAccessibleName( title );
    m_title->setToolTip( title );
    const bool hidden = m_dialog->property( "shadowDialogTitleHidden" ).toBool();
    m_title->setText( hidden ? QString() : m_title->fontMetrics().elidedText( title, Qt::ElideRight, m_title->width() ) );
    m_title->setVisible( !hidden );
    if( m_dialog->windowTitle() != title )
      m_dialog->setWindowTitle( title );
  }

  void updateMask()
  {
    if( !m_dialog || !m_surface || m_dialog->size().isEmpty() )
      return;
    const bool expanded = m_dialog->isMaximized() || m_dialog->isFullScreen();
    const int padding = expanded ? 0 : shadowPadding;
    m_dialog->layout()->setContentsMargins( padding, padding, padding, padding );
    m_shadow->setGeometry( m_dialog->rect() );
    m_shadow->setVisible( !expanded );
    m_shadow->lower();
    m_shadow->update();
    updateSizeGrips();
    if( expanded )
    {
      m_surface->clearMask();
      return;
    }
    const qreal radius = qMin( qreal( cornerRadius ),
                              qMin( m_surface->width(), m_surface->height() ) / 2.0 );
    QPainterPath path;
    path.addRoundedRect( QRectF( m_surface->rect() ), radius, radius );
    const QRegion mask( path.toFillPolygon().toPolygon() );
    if( m_surface->mask() != mask )
      m_surface->setMask( mask );
  }

  QPointer<QDialog> m_dialog;
  QPointer<DialogShadow> m_shadow;
  QPointer<QWidget> m_surface;
  QPointer<QWidget> m_header;
  QPointer<QLabel> m_title;
  QPoint m_pressPosition;
  QPoint m_windowPosition;
  bool m_dragging;
  bool m_dragMoved;
};

bool canDecorate( QDialog* dialog )
{
  if( !dialog || !dialog->isWindow() || dialog->isVisible() || !dialog->layout() ||
      dialog->property( decoratedProperty ).toBool() ||
      dialog->property( "shadowDialogChromeSkip" ).toBool() ||
      dialog->windowFlags().testFlag( Qt::FramelessWindowHint ) ||
      dialog->testAttribute( Qt::WA_DontShowOnScreen ) ||
      dialog->testAttribute( Qt::WA_TranslucentBackground ) ||
      !dialog->mask().isEmpty() )
    return false;
  if( dialog->windowType() != Qt::Dialog && dialog->windowType() != Qt::Window )
    return false;
  return !dialog->inherits( "QFileDialog" ) && !dialog->inherits( "QColorDialog" ) &&
         !dialog->inherits( "QFontDialog" ) && !dialog->inherits( "QPrintDialog" ) &&
         !dialog->inherits( "QPageSetupDialog" );
}

class DialogChromeFilter final : public QObject
{
public:
  explicit DialogChromeFilter( QApplication* application )
    : QObject( application )
  {
  }

protected:
  bool eventFilter( QObject* watched, QEvent* event ) override
  {
    if( event->type() == QEvent::Polish || event->type() == QEvent::Show )
    {
      QDialog* dialog = qobject_cast<QDialog*>( watched );
      if( canDecorate( dialog ) )
        new DialogChrome( dialog );
    }
    return false;
  }
};
}

void ShadowDialogChrome::install( QApplication* application )
{
  if( !application )
    application = qobject_cast<QApplication*>( QCoreApplication::instance() );
  if( !application || application != QCoreApplication::instance() ||
      application->thread() != QThread::currentThread() ||
      application->property( installedProperty ).toBool() )
    return;

  application->setProperty( installedProperty, true );
  application->installEventFilter( new DialogChromeFilter( application ) );
  const QWidgetList windows = QApplication::topLevelWidgets();
  for( QWidget* window : windows )
  {
    QDialog* dialog = qobject_cast<QDialog*>( window );
    if( canDecorate( dialog ) )
      new DialogChrome( dialog );
  }
}
