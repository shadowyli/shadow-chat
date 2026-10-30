#include "ShadowScreenshotPreview.h"
#include "ShadowDialogStyle.h"

#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace
{
  class ScreenshotCanvas : public QWidget
  {
  public:
    explicit ScreenshotCanvas( const QPixmap& image, QWidget* parent )
      : QWidget( parent ), m_image( image ), m_originalSize( false )
    {
      setObjectName( QStringLiteral( "ShadowScreenshotCanvas" ) );
      setAccessibleName( QStringLiteral( "所截图片预览" ) );
      setMinimumSize( 240, 180 );
      setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    }

    void setOriginalSize( bool enabled )
    {
      m_originalSize = enabled;
      setMinimumSize( enabled ? m_image.size() : QSize( 240, 180 ) );
      updateGeometry();
      update();
    }

  protected:
    void paintEvent( QPaintEvent* ) override
    {
      QPainter painter( this );
      painter.fillRect( rect(), QColor( "#F4F6F9" ) );
      if( m_image.isNull() )
        return;
      const QSize available = size() - QSize( 24, 24 );
      const QSize target = m_originalSize ? m_image.size()
        : m_image.size().scaled( available, Qt::KeepAspectRatio ).boundedTo( m_image.size() );
      const QRect area( QPoint( (width() - target.width()) / 2, (height() - target.height()) / 2 ), target );
      painter.setRenderHint( QPainter::SmoothPixmapTransform, !m_originalSize );
      painter.drawPixmap( area, m_image );
    }

  private:
    QPixmap m_image;
    bool m_originalSize;
  };
}

ShadowScreenshotPreview::ShadowScreenshotPreview( const QPixmap& image, QWidget* parent )
  : QDialog( parent )
{
  setObjectName( QStringLiteral( "ShadowScreenshotPreview" ) );
  setWindowTitle( QStringLiteral( "截图预览" ) );
  setWindowModality( Qt::WindowModal );
  ShadowDialogStyle::apply( this );
  const QScreen* display = parent ? parent->screen() : QGuiApplication::primaryScreen();
  const QSize available = display ? display->availableGeometry().size() : QSize( 1280, 800 );
  const QSize bounds = available - QSize( 64, 96 );
  setMinimumSize( qMin( 460, bounds.width() ), qMin( 380, bounds.height() ) );
  resize( qMin( 760, bounds.width() ), qMin( 570, bounds.height() ) );

  QVBoxLayout* layout = new QVBoxLayout( this );
  layout->setContentsMargins( 20, 16, 20, 18 );
  layout->setSpacing( 12 );
  QHBoxLayout* details = new QHBoxLayout;
  QLabel* dimensions = new QLabel( QStringLiteral( "%1 × %2 像素" ).arg( image.width() ).arg( image.height() ), this );
  dimensions->setObjectName( QStringLiteral( "ShadowScreenshotDimensions" ) );
  dimensions->setProperty( "role", "hint" );
  details->addWidget( dimensions );
  details->addStretch();
  QPushButton* original = new QPushButton( QStringLiteral( "查看原图" ), this );
  original->setObjectName( QStringLiteral( "ShadowScreenshotOriginal" ) );
  original->setCheckable( true );
  original->setAutoDefault( false );
  details->addWidget( original );
  layout->addLayout( details );

  QScrollArea* scroll = new QScrollArea( this );
  scroll->setObjectName( QStringLiteral( "ShadowScreenshotScroll" ) );
  scroll->setFrameShape( QFrame::NoFrame );
  scroll->setWidgetResizable( true );
  ScreenshotCanvas* canvas = new ScreenshotCanvas( image, scroll );
  scroll->setWidget( canvas );
  layout->addWidget( scroll, 1 );
  connect( original, &QPushButton::toggled, this, [canvas, original]( bool enabled ) {
    canvas->setOriginalSize( enabled );
    original->setText( enabled ? QStringLiteral( "适应窗口" ) : QStringLiteral( "查看原图" ) );
  } );

  mp_feedback = new QLabel( this );
  mp_feedback->setObjectName( QStringLiteral( "ShadowScreenshotFeedback" ) );
  mp_feedback->setWordWrap( true );
  mp_feedback->setTextFormat( Qt::PlainText );
  mp_feedback->setStyleSheet( QStringLiteral( "color:#B42318;background:#FFF3F1;border-radius:6px;padding:9px;" ) );
  mp_feedback->hide();
  layout->addWidget( mp_feedback );

  QHBoxLayout* buttons = new QHBoxLayout;
  buttons->addStretch();
  QPushButton* cancel = new QPushButton( QStringLiteral( "取消" ), this );
  cancel->setAutoDefault( false );
  buttons->addWidget( cancel );
  QPushButton* send = new QPushButton( this );
  send->setObjectName( QStringLiteral( "ShadowScreenshotSend" ) );
  send->setText( QStringLiteral( "发送" ) );
  send->setProperty( "role", "primary" );
  send->setAutoDefault( false );
  send->setDefault( false );
  send->setEnabled( !image.isNull() );
  buttons->addWidget( send );
  connect( cancel, &QPushButton::clicked, this, &QDialog::reject );
  connect( send, &QPushButton::clicked, this, &ShadowScreenshotPreview::sendRequested );
  layout->addLayout( buttons );
  cancel->setFocus();
}

void ShadowScreenshotPreview::showSendError( const QString& message )
{
  mp_feedback->setText( message );
  mp_feedback->show();
}
