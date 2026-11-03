#include "ShadowColorPanel.h"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QEventLoop>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QToolBar>

namespace
{
class ColorSwatch : public QAbstractButton
{
public:
  explicit ColorSwatch( const QColor& color, QWidget* parent )
    : QAbstractButton( parent ), m_color( color )
  {
    setFixedSize( 26, 26 );
    setCursor( Qt::PointingHandCursor );
    setFocusPolicy( Qt::StrongFocus );
    setCheckable( true );
    setToolTip( color.name().toUpper() );
    setAccessibleName( color.name().toUpper() );
  }

  QColor color() const { return m_color; }

protected:
  void paintEvent( QPaintEvent* ) override
  {
    QPainter painter( this );
    painter.setRenderHint( QPainter::Antialiasing );
    const QRectF swatch = QRectF( rect() ).adjusted( 3.5, 3.5, -3.5, -3.5 );
    if( isChecked() )
    {
      painter.setPen( QPen( QColor( "#2463EB" ), 2 ) );
      painter.setBrush( Qt::NoBrush );
      painter.drawRoundedRect( QRectF( rect() ).adjusted( 1.5, 1.5, -1.5, -1.5 ), 6, 6 );
    }
    painter.setPen( QPen( QColor( "#C9D2DE" ), 1 ) );
    painter.setBrush( m_color );
    painter.drawRoundedRect( swatch, 4, 4 );
    if( hasFocus() )
    {
      painter.setPen( QPen( QColor( "#8CB4FF" ), 1.5, Qt::DotLine ) );
      painter.setBrush( Qt::NoBrush );
      painter.drawRoundedRect( QRectF( rect() ).adjusted( 0.8, 0.8, -0.8, -0.8 ), 7, 7 );
    }
  }

private:
  QColor m_color;
};
}

ShadowColorPanel::ShadowColorPanel( QWidget* parent )
  : ShadowPopup( parent ), m_color( "#2463EB" ), m_title( Q_NULLPTR ), m_hex( Q_NULLPTR ),
    m_preview( Q_NULLPTR ), m_accept( Q_NULLPTR ), m_swatch_group( Q_NULLPTR )
{
  setObjectName( QStringLiteral( "ShadowColorPanel" ) );
  setProperty( "shadowColorPanel", true );
  setPreferredWidth( 270 );
  setGap( 6 );

  QVBoxLayout* layout = contentLayout();
  layout->setContentsMargins( 12, 10, 12, 10 );
  layout->setSpacing( 8 );

  m_title = new QLabel( QStringLiteral( "选择颜色" ), contentWidget() );
  m_title->setObjectName( QStringLiteral( "colorPanelTitle" ) );
  m_title->setStyleSheet( QStringLiteral( "QLabel{font-size:12px;font-weight:600;color:#1F2D44;}" ) );
  layout->addWidget( m_title );

  buildSwatches();

  QHBoxLayout* custom = new QHBoxLayout;
  custom->setSpacing( 8 );
  m_preview = new QLabel( contentWidget() );
  m_preview->setObjectName( QStringLiteral( "colorPanelPreview" ) );
  m_preview->setFixedSize( 28, 28 );
  custom->addWidget( m_preview );
  m_hex = new QLineEdit( contentWidget() );
  m_hex->setObjectName( QStringLiteral( "colorPanelHex" ) );
  m_hex->setMaxLength( 9 );
  m_hex->setPlaceholderText( QStringLiteral( "#RRGGBB" ) );
  m_hex->setAccessibleName( QStringLiteral( "颜色代码" ) );
  m_hex->setStyleSheet( QStringLiteral( "QLineEdit{background:#F7F9FC;border:1px solid #DCE4F0;border-radius:6px;padding:4px 8px;color:#243044;}" ) );
  custom->addWidget( m_hex, 1 );
  layout->addLayout( custom );
  m_error = new QLabel( contentWidget() );
  m_error->setObjectName( QStringLiteral( "colorPanelError" ) );
  m_error->setMinimumHeight( 18 );
  m_error->setWordWrap( true );
  m_error->setStyleSheet( QStringLiteral( "QLabel{color:#B42332;font-size:11px;}" ) );
  layout->addWidget( m_error );

  QHBoxLayout* footer = new QHBoxLayout;
  footer->setSpacing( 8 );
  footer->addStretch();
  QPushButton* cancel = new QPushButton( QStringLiteral( "取消" ), contentWidget() );
  cancel->setObjectName( QStringLiteral( "colorPanelCancel" ) );
  cancel->setAutoDefault( false );
  m_accept = new QPushButton( QStringLiteral( "确定" ), contentWidget() );
  m_accept->setObjectName( QStringLiteral( "colorPanelAccept" ) );
  m_accept->setProperty( "role", "primary" );
  m_accept->setAutoDefault( false );
  m_accept->setDefault( true );
  footer->addWidget( cancel );
  footer->addWidget( m_accept );
  layout->addLayout( footer );

  connect( cancel, &QPushButton::clicked, this, &QWidget::hide );
  connect( m_accept, &QPushButton::clicked, this, &ShadowColorPanel::acceptColor );
  connect( m_hex, &QLineEdit::textEdited, this, [this]( const QString& text ) {
    const QString value = text.trimmed();
    const bool valid = QRegularExpression( QStringLiteral( "^#[0-9A-Fa-f]{6}([0-9A-Fa-f]{2})?$" ) )
      .match( value ).hasMatch() && QColor( value ).isValid();
    if( valid )
    {
      m_color = QColor( value );
      updatePreview( false );
    }
    m_accept->setEnabled( valid );
    m_error->setText( valid ? QString() : QStringLiteral( "请输入完整颜色代码，例如 #2463EB" ) );
    m_hex->setAccessibleDescription( m_error->text() );
  } );
  connect( m_hex, &QLineEdit::returnPressed, this, &ShadowColorPanel::acceptColor );
  updatePreview();
}

void ShadowColorPanel::buildSwatches()
{
  static const QStringList swatch_colors = QStringList()
    << QStringLiteral( "#000000" ) << QStringLiteral( "#172033" ) << QStringLiteral( "#34445E" )
    << QStringLiteral( "#5F6B7A" ) << QStringLiteral( "#8794A8" ) << QStringLiteral( "#C7CFDA" )
    << QStringLiteral( "#FFFFFF" ) << QStringLiteral( "#EEF2F7" )
    << QStringLiteral( "#EF4444" ) << QStringLiteral( "#F97316" ) << QStringLiteral( "#F59E0B" )
    << QStringLiteral( "#EAB308" ) << QStringLiteral( "#84CC16" ) << QStringLiteral( "#22C55E" )
    << QStringLiteral( "#14B8A6" ) << QStringLiteral( "#06B6D4" )
    << QStringLiteral( "#0EA5E9" ) << QStringLiteral( "#3B82F6" ) << QStringLiteral( "#2563EB" )
    << QStringLiteral( "#6366F1" ) << QStringLiteral( "#8B5CF6" ) << QStringLiteral( "#A855F7" )
    << QStringLiteral( "#D946EF" ) << QStringLiteral( "#EC4899" )
    << QStringLiteral( "#FEE2E2" ) << QStringLiteral( "#FFEDD5" ) << QStringLiteral( "#FEF3C7" )
    << QStringLiteral( "#ECFCCB" ) << QStringLiteral( "#DCFCE7" ) << QStringLiteral( "#CCFBF1" )
    << QStringLiteral( "#DBEAFE" ) << QStringLiteral( "#EDE9FE" );

  QWidget* grid_widget = new QWidget( contentWidget() );
  grid_widget->setObjectName( QStringLiteral( "colorPanelSwatches" ) );
  QGridLayout* grid = new QGridLayout( grid_widget );
  grid->setContentsMargins( 0, 0, 0, 0 );
  grid->setSpacing( 3 );
  QButtonGroup* group = new QButtonGroup( grid_widget );
  group->setExclusive( true );
  for( int index = 0; index < swatch_colors.size(); ++index )
  {
    ColorSwatch* swatch = new ColorSwatch( QColor( swatch_colors.at( index ) ), grid_widget );
    swatch->setObjectName( QStringLiteral( "colorSwatch" ) );
    swatch->setProperty( "colorValue", swatch_colors.at( index ) );
    group->addButton( swatch, index );
    m_swatches.append( swatch );
    connect( swatch, &QAbstractButton::clicked, this, [this, swatch]() { chooseColor( swatch->color() ); } );
    grid->addWidget( swatch, index / 8, index % 8 );
  }
  contentLayout()->addWidget( grid_widget );
  m_swatch_group = group;
}

void ShadowColorPanel::setColor( const QColor& color )
{
  if( !color.isValid() )
    return;
  m_color = color;
  updatePreview();
}

QColor ShadowColorPanel::color() const
{
  return m_color;
}

void ShadowColorPanel::setTitle( const QString& title )
{
  m_title->setText( title.isEmpty() ? QStringLiteral( "选择颜色" ) : title );
}

void ShadowColorPanel::chooseColor( const QColor& color )
{
  if( !color.isValid() )
    return;
  m_color = color;
  updatePreview();
}

void ShadowColorPanel::acceptColor()
{
  if( !m_color.isValid() || !m_accept->isEnabled() )
    return;
  emit colorAccepted( m_color );
  hide();
}

void ShadowColorPanel::updatePreview( bool update_text )
{
  m_preview->setStyleSheet( QStringLiteral( "QLabel{background:%1;border:1px solid #C9D2DE;border-radius:6px;}" )
    .arg( m_color.name() ) );
  if( update_text )
    m_hex->setText( m_color.name().toUpper() );
  m_error->clear();
  m_hex->setAccessibleDescription( QString() );
  m_accept->setEnabled( m_color.isValid() );
  foreach( QAbstractButton* swatch, m_swatches )
    swatch->setChecked( QColor( swatch->property( "colorValue" ).toString() ) == m_color );
}

QColor ShadowColorPanel::pick( QWidget* anchor, const QColor& initial, QWidget* parent, const QString& title )
{
  if( !anchor )
    return QColor();
  QPointer<ShadowColorPanel> panel = new ShadowColorPanel( parent ? parent : anchor->window() );
  panel->setAnchor( anchor );
  panel->setFocusOnClose( anchor );
  if( !title.isEmpty() )
    panel->setTitle( title );
  panel->setColor( initial );
  QColor result;
  QEventLoop loop;
  QObject::connect( panel, &ShadowColorPanel::colorAccepted, &loop, [&]( const QColor& color ) {
    result = color;
    loop.quit();
  } );
  QObject::connect( panel, &ShadowPopup::popupHidden, &loop, &QEventLoop::quit );
  QObject::connect( panel, &QObject::destroyed, &loop, &QEventLoop::quit );
  panel->showPopup();
  if( panel && panel->isVisible() )
    loop.exec();
  delete panel.data();
  return result;
}

QColor ShadowColorPanel::pickAtPoint( const QPoint& global_point, const QColor& initial, QWidget* parent,
                                      const QString& title )
{
  return pickAtRect( QRect( global_point, QSize( 1, 1 ) ), initial, parent, title );
}

QColor ShadowColorPanel::pickForAction( QObject* source, const QColor& initial, QWidget* parent,
                                       const QString& title )
{
  QAction* action = qobject_cast<QAction*>( source );
  if( action )
  {
    for( QWidget* widget : action->associatedWidgets() )
    {
      QToolBar* bar = qobject_cast<QToolBar*>( widget );
      QWidget* button = bar ? bar->widgetForAction( action ) : Q_NULLPTR;
      if( button && button->isVisible() )
        return pick( button, initial, parent, title );
      QMenu* menu = qobject_cast<QMenu*>( widget );
      if( menu )
      {
        const QRect local = menu->actionGeometry( action );
        if( !local.isEmpty() )
          return pickAtRect( QRect( menu->mapToGlobal( local.topLeft() ), local.size() ),
                             initial, parent, title );
      }
    }
  }
  QWidget* focus = QApplication::focusWidget();
  return pick( focus && focus->isVisible() && parent && focus->window() == parent->window()
    ? focus : parent, initial, parent, title );
}

QColor ShadowColorPanel::pickAtRect( const QRect& global_anchor_rect, const QColor& initial, QWidget* parent,
                                     const QString& title )
{
  QPointer<ShadowColorPanel> panel = new ShadowColorPanel( parent );
  panel->setAnchorRect( global_anchor_rect );
  if( parent )
    panel->setFocusOnClose( parent );
  if( !title.isEmpty() )
    panel->setTitle( title );
  panel->setColor( initial );
  QColor result;
  QEventLoop loop;
  QObject::connect( panel, &ShadowColorPanel::colorAccepted, &loop, [&]( const QColor& color ) {
    result = color;
    loop.quit();
  } );
  QObject::connect( panel, &ShadowPopup::popupHidden, &loop, &QEventLoop::quit );
  QObject::connect( panel, &QObject::destroyed, &loop, &QEventLoop::quit );
  panel->showPopup();
  if( panel && panel->isVisible() )
    loop.exec();
  delete panel.data();
  return result;
}
