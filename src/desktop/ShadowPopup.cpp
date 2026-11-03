#include "ShadowPopup.h"
#include <QApplication>
#include <QAbstractButton>
#include <QEvent>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
const int kCardRadius = 12;

struct MenuAnchor
{
  QPointer<QWidget> widget;
  QRect rect;
  bool directPosition = false;
  bool submenu = false;
};

class PopupMenuTracker : public QObject
{
public:
  explicit PopupMenuTracker( QObject* parent ) : QObject( parent )
  {
    qApp->installEventFilter( this );
  }

  void setAnchor( QMenu* menu, QWidget* widget, const QRect& rect )
  {
    MenuAnchor state;
    state.widget = widget;
    state.rect = rect;
    m_anchors.insert( menu, state );
    observe( menu );
  }

protected:
  bool eventFilter( QObject* watched, QEvent* event ) override
  {
    QMenu* menu = qobject_cast<QMenu*>( watched );
    if( menu && event->type() == QEvent::Polish )
      prepare( menu );
    if( menu && event->type() == QEvent::Show )
    {
      prepare( menu );
      if( !m_anchors.contains( menu ) )
        capture( menu );
      position( menu );
      QPointer<QMenu> guard = menu;
      QTimer::singleShot( 0, menu, [this, guard]() {
        if( guard && guard->isVisible() )
          position( guard );
      } );
    }
    if( menu && event->type() == QEvent::Hide )
    {
      const MenuAnchor state = m_anchors.take( menu );
      if( state.widget && qobject_cast<QAbstractButton*>( state.widget.data() ) &&
          state.widget->isVisible() && state.widget->window()->isActiveWindow() )
        state.widget->setFocus( Qt::OtherFocusReason );
    }
    if( menu && event->type() == QEvent::Paint )
    {
      QPainter painter( menu );
      ShadowPopup::paintCard( &painter, menu->rect().adjusted( ShadowPopup::shadowMargin,
        ShadowPopup::shadowMargin, -ShadowPopup::shadowMargin, -ShadowPopup::shadowMargin ) );
    }
    QWidget* widget = qobject_cast<QWidget*>( watched );
    if( widget && (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
        event->type() == QEvent::LayoutRequest || event->type() == QEvent::Hide ||
        event->type() == QEvent::WindowStateChange) )
    {
      const QList<QMenu*> menus = m_anchors.keys();
      for( QMenu* tracked : menus )
      {
        const MenuAnchor state = m_anchors.value( tracked );
        if( tracked == widget || !tracked->isVisible() || !state.widget ||
            (widget != state.widget && !widget->isAncestorOf( state.widget )) )
          continue;
        if( event->type() == QEvent::Hide ||
            (event->type() == QEvent::WindowStateChange && widget->isMinimized()) )
          tracked->hide();
        else
          position( tracked );
      }
    }
    return QObject::eventFilter( watched, event );
  }

private:
  void observe( QMenu* menu )
  {
    if( menu->property( "shadowMenuObserved" ).toBool() )
      return;
    menu->setProperty( "shadowMenuObserved", true );
    connect( menu, &QObject::destroyed, this, [this, menu]() { m_anchors.remove( menu ); } );
  }

  void prepare( QMenu* menu )
  {
    if( menu->property( "shadowMenuPrepared" ).toBool() )
      return;
    menu->setProperty( "shadowMenuPrepared", true );
    menu->setWindowFlag( Qt::NoDropShadowWindowHint, true );
    menu->setAttribute( Qt::WA_TranslucentBackground );
    menu->setStyleSheet( menu->styleSheet() + QStringLiteral(
      "QMenu{background:transparent;color:#34445E;border:0;padding:16px;menu-scrollable:1;}"
      "QMenu::item{padding:7px 22px 7px 12px;border-radius:5px;}"
      "QMenu::item:selected{background:#EAF2FF;color:#2457B8;}"
      "QMenu::separator{height:1px;background:#E8EDF4;margin:4px 6px;}" ) );
    observe( menu );
  }

  QWidget* buttonForMenu( QWidget* widget, QMenu* menu ) const
  {
    for( QWidget* candidate = widget; candidate; candidate = candidate->parentWidget() )
    {
      QToolButton* tool = qobject_cast<QToolButton*>( candidate );
      QPushButton* button = qobject_cast<QPushButton*>( candidate );
      if( (tool && tool->menu() == menu) || (button && button->menu() == menu) )
        return candidate;
    }
    return Q_NULLPTR;
  }

  void capture( QMenu* menu )
  {
    MenuAnchor state;
    QWidget* trigger = buttonForMenu( menu->parentWidget(), menu );
    if( !trigger )
      trigger = buttonForMenu( QApplication::focusWidget(), menu );
    if( !trigger )
      trigger = buttonForMenu( QApplication::widgetAt( QCursor::pos() ), menu );
    if( trigger )
    {
      state.widget = trigger;
      state.rect = trigger->rect();
    }
    else
    {
      for( QWidget* associated : menu->menuAction()->associatedWidgets() )
      {
        QMenu* parent_menu = qobject_cast<QMenu*>( associated );
        if( parent_menu && parent_menu != menu && parent_menu->isVisible() )
        {
          state.widget = parent_menu;
          state.rect = parent_menu->actionGeometry( menu->menuAction() );
          state.submenu = true;
          break;
        }
      }
      if( !state.widget )
      {
        QWidget* owner = menu->parentWidget();
        if( !owner )
          owner = QApplication::activeWindow();
        if( owner && owner != menu )
        {
          state.widget = owner;
          state.rect = QRect( owner->mapFromGlobal( menu->pos() ), QSize( 1, 1 ) );
          state.directPosition = true;
        }
      }
    }
    m_anchors.insert( menu, state );
    observe( menu );
  }

  void position( QMenu* menu )
  {
    const MenuAnchor state = m_anchors.value( menu );
    if( !state.widget )
      return;
    const QRect anchor( state.widget->mapToGlobal( state.rect.topLeft() ), state.rect.size() );
    const QRect available = ShadowPopup::availableGeometryFor( anchor.center() );
    menu->setMinimumSize( 0, 0 );
    menu->setMaximumSize( available.size() );
    QRect geometry = ShadowPopup::placeGeometry( anchor, menu->sizeHint(), available );
    if( state.directPosition || state.submenu )
    {
      QPoint origin = anchor.topLeft();
      if( state.submenu )
      {
        origin.setX( anchor.right() + 1 );
        if( origin.x() + geometry.width() > available.right() + 1 )
          origin.setX( anchor.left() - geometry.width() );
      }
      geometry.moveTopLeft( QPoint(
        qBound( available.left(), origin.x(), available.right() - geometry.width() + 1 ),
        qBound( available.top(), origin.y(), available.bottom() - geometry.height() + 1 ) ) );
    }
    menu->setGeometry( geometry );
  }

  QHash<QMenu*, MenuAnchor> m_anchors;
};

PopupMenuTracker* menuTracker()
{
  static QPointer<PopupMenuTracker> tracker;
  if( !tracker && qApp )
    tracker = new PopupMenuTracker( qApp );
  return tracker;
}
}

ShadowPopup::ShadowPopup( QWidget* parent )
  : QWidget( parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint ),
    m_content( Q_NULLPTR ), m_contentLayout( Q_NULLPTR ), m_scroll( Q_NULLPTR ),
    m_placement( Automatic ), m_gap( 6 ), m_preferredWidth( 0 )
{
  setObjectName( QStringLiteral( "ShadowPopup" ) );
  setAttribute( Qt::WA_TranslucentBackground, true );
  setAttribute( Qt::WA_DeleteOnClose, false );
  setFocusPolicy( Qt::StrongFocus );
  setProperty( "shadowPopup", true );
  setWindowTitle( QString() );

  QVBoxLayout* outer = new QVBoxLayout( this );
  outer->setContentsMargins( shadowMargin, shadowMargin, shadowMargin, shadowMargin );
  outer->setSpacing( 0 );
  outer->setSizeConstraint( QLayout::SetNoConstraint );
  m_scroll = new QScrollArea( this );
  m_scroll->setObjectName( QStringLiteral( "ShadowPopupScroll" ) );
  m_scroll->setFrameShape( QFrame::NoFrame );
  m_scroll->setWidgetResizable( true );
  m_scroll->setStyleSheet( QStringLiteral(
    "QScrollArea#ShadowPopupScroll,QScrollArea#ShadowPopupScroll>QWidget{background:transparent;border:0;}" ) );
  m_content = new QWidget;
  m_content->setObjectName( QStringLiteral( "ShadowPopupContent" ) );
  m_content->setAttribute( Qt::WA_TranslucentBackground, true );
  m_contentLayout = new QVBoxLayout( m_content );
  m_contentLayout->setContentsMargins( 14, 12, 14, 12 );
  m_contentLayout->setSpacing( 8 );
  m_scroll->setWidget( m_content );
  outer->addWidget( m_scroll );
  qApp->installEventFilter( this );

  setStyleSheet( QStringLiteral(
    "QWidget#ShadowPopupContent{background:transparent;color:#34445E;font-family:'Microsoft YaHei UI';font-size:12px;}"
    "QLabel{background:transparent;color:#34445E;}"
    "QToolButton{background:transparent;border:1px solid transparent;border-radius:6px;color:#5F6B7A;padding:4px;}"
    "QToolButton:hover{background:#EDF3FC;color:#1D4ED8;}"
    "QToolButton:focus{border-color:#8CB4FF;}"
    "QPushButton{background:#F3F6FB;color:#34445E;border:1px solid #DCE4F0;border-radius:6px;min-height:22px;padding:4px 12px;}"
    "QPushButton:hover{background:#EAF0FC;border-color:#B6C9ED;}"
    "QPushButton:focus{border-color:#2463EB;}"
    "QPushButton[role=primary]{background:#2463EB;color:white;border-color:#2463EB;}"
    "QPushButton[role=primary]:hover{background:#1D52CB;}" ) );
}

ShadowPopup::~ShadowPopup()
{
  if( m_anchorWindow )
    m_anchorWindow->removeEventFilter( this );
  if( m_anchor )
    m_anchor->removeEventFilter( this );
}

void ShadowPopup::setAnchor( QWidget* anchor, const QPoint& anchor_offset )
{
  if( m_anchor )
    m_anchor->removeEventFilter( this );
  m_anchor = anchor;
  m_anchorOffset = anchor_offset;
  m_anchorRectOverride = QRect();
  trackAnchor( anchor );
  if( isVisible() )
  {
    place();
    raise();
  }
}

QWidget* ShadowPopup::anchor() const
{
  return m_anchor.data();
}

void ShadowPopup::setAnchorRect( const QRect& global_anchor_rect )
{
  setAnchor( parentWidget() );
  m_anchorRectOverride = global_anchor_rect;
  if( m_anchor )
    m_anchorRectOverride.moveTopLeft( m_anchor->mapFromGlobal( global_anchor_rect.topLeft() ) );
  if( isVisible() )
  {
    place();
    raise();
  }
}

QRect ShadowPopup::anchorRect() const
{
  if( m_anchorRectOverride.isValid() )
    return m_anchor ? QRect( m_anchor->mapToGlobal( m_anchorRectOverride.topLeft() ),
      m_anchorRectOverride.size() ) : m_anchorRectOverride;
  if( !m_anchor )
    return QRect();
  QRect rect = m_anchor->rect();
  rect.moveTopLeft( m_anchor->mapToGlobal( QPoint() ) );
  rect.translate( m_anchorOffset );
  return rect;
}

void ShadowPopup::setPlacement( Placement placement )
{
  m_placement = placement;
  if( isVisible() )
  {
    place();
    raise();
  }
}

ShadowPopup::Placement ShadowPopup::placement() const
{
  return m_placement;
}

void ShadowPopup::setGap( int gap )
{
  m_gap = qMax( 0, gap );
  if( isVisible() )
  {
    place();
    raise();
  }
}

void ShadowPopup::setFocusOnClose( QWidget* widget )
{
  m_focusOnClose = widget;
}

QWidget* ShadowPopup::contentWidget() const
{
  return m_content;
}

QVBoxLayout* ShadowPopup::contentLayout() const
{
  return m_contentLayout;
}

void ShadowPopup::trackAnchor( QWidget* anchor )
{
  if( m_anchorWindow )
  {
    m_anchorWindow->removeEventFilter( this );
    m_anchorWindow = Q_NULLPTR;
  }
  if( !anchor )
    return;
  anchor->installEventFilter( this );
  QWidget* window = anchor->window();
  if( window && window != anchor )
  {
    m_anchorWindow = window;
    m_anchorWindow->installEventFilter( this );
  }
}

QRect ShadowPopup::availableGeometryFor( const QPoint& global_point, const QScreen* preferred_screen )
{
  QScreen* screen = preferred_screen ? const_cast<QScreen*>( preferred_screen ) : QGuiApplication::screenAt( global_point );
  if( !screen )
    screen = QGuiApplication::primaryScreen();
  return screen ? screen->availableGeometry() : QRect( 0, 0, 1280, 800 );
}

QRect ShadowPopup::placeGeometry( const QRect& anchor_rect, const QSize& popup_size,
                                  const QRect& available_geometry, Placement placement, int gap )
{
  if( available_geometry.isEmpty() )
    return QRect( anchor_rect.topLeft(), popup_size );
  const QSize size( qMin( qMax( 1, popup_size.width() ), available_geometry.width() ),
                    qMin( qMax( 1, popup_size.height() ), available_geometry.height() ) );
  const int below = anchor_rect.bottom() + 1 + gap;
  const int above = anchor_rect.top() - gap - size.height();
  const bool below_fits = below + size.height() <= available_geometry.bottom() + 1;
  const bool above_fits = above >= available_geometry.top();
  int vertical = below;
  switch( placement )
  {
  case Above:
    vertical = above_fits || !below_fits ? above : below;
    break;
  case Below:
    vertical = below_fits || !above_fits ? below : above;
    break;
  default:
    if( below_fits )
      vertical = below;
    else if( above_fits )
      vertical = above;
    else
      vertical = anchor_rect.center().y() > available_geometry.center().y() ? above : below;
    break;
  }
  vertical = qBound( available_geometry.top(), vertical,
                     qMax( available_geometry.top(), available_geometry.bottom() - size.height() + 1 ) );
  int horizontal = anchor_rect.center().x() - size.width() / 2;
  horizontal = qBound( available_geometry.left(), horizontal,
                       qMax( available_geometry.left(), available_geometry.right() - size.width() + 1 ) );
  return QRect( QPoint( horizontal, vertical ), size );
}

QRect ShadowPopup::placementGeometryFor( const QSize& size ) const
{
  const QRect anchor_rect = anchorRect();
  const QScreen* screen = Q_NULLPTR;
  if( m_anchor && m_anchor->screen() )
    screen = m_anchor->screen();
  else if( qApp )
    screen = QGuiApplication::screenAt( anchor_rect.center() );
  const QRect available = availableGeometryFor( anchor_rect.center(),
    QGuiApplication::screenAt( anchor_rect.center() ) ? Q_NULLPTR : screen );
  return placeGeometry( anchor_rect, size, available, m_placement, m_gap );
}

void ShadowPopup::place()
{
  if( !isVisible() )
    return;
  QSize hint = sizeHint();
  if( hint.isEmpty() )
    hint = size();
  layout()->activate();
  const QRect geometry = placementGeometryFor( hint.expandedTo( minimumSize() ).boundedTo( maximumSize() ) );
  if( minimumWidth() > geometry.width() || minimumHeight() > geometry.height() )
    setMinimumSize( minimumSize().boundedTo( geometry.size() ) );
  setGeometry( geometry );
}

void ShadowPopup::showPopup()
{
  if( !m_anchor && !m_anchorRectOverride.isValid() )
    return;
  QSize hint = sizeHint();
  if( hint.isEmpty() )
    hint = size();
  layout()->activate();
  const QRect geometry = placementGeometryFor( hint.expandedTo( minimumSize() ).boundedTo( maximumSize() ) );
  if( minimumWidth() > geometry.width() || minimumHeight() > geometry.height() )
    setMinimumSize( minimumSize().boundedTo( geometry.size() ) );
  setGeometry( geometry );
  show();
  raise();
  if( !isAncestorOf( QApplication::focusWidget() ) )
    setFocus( Qt::PopupFocusReason );
}

void ShadowPopup::setPreferredWidth( int width )
{
  m_preferredWidth = qMax( 0, width );
}

QSize ShadowPopup::sizeHint() const
{
  QSize hint = m_content->sizeHint() + QSize( 2 * shadowMargin, 2 * shadowMargin );
  if( m_preferredWidth )
    hint.setWidth( m_preferredWidth );
  return hint;
}

void ShadowPopup::installMenuTracking()
{
  menuTracker();
}

void ShadowPopup::paintCard( QPainter* painter, const QRect& card_rect, int radius,
                             const QColor& background, const QColor& border )
{
  if( !painter || card_rect.isEmpty() )
    return;
  painter->save();
  painter->setRenderHint( QPainter::Antialiasing, true );
  painter->setPen( Qt::NoPen );
  for( int distance = shadowMargin; distance >= 1; --distance )
  {
    QColor shade( 23, 36, 59 );
    shade.setAlphaF( 0.014f + 0.010f * ( shadowMargin - distance + 1 ) );
    painter->setBrush( shade );
    const QRect layer = card_rect.adjusted( -distance, -distance, distance, distance );
    painter->drawRoundedRect( layer, radius + distance, radius + distance );
  }
  painter->setBrush( background );
  painter->setPen( QPen( border, 1 ) );
  painter->drawRoundedRect( QRectF( card_rect ).adjusted( 0.5, 0.5, -0.5, -0.5 ), radius, radius );
  painter->restore();
}

bool ShadowPopup::showMenuAt( QMenu* menu, QWidget* anchor, const QPoint& local_anchor )
{
  if( !menu || !anchor )
    return false;
  menu->ensurePolished();
  menuTracker()->setAnchor( menu, anchor, local_anchor.isNull() ? anchor->rect() : QRect( local_anchor, QSize( 1, 1 ) ) );
  menu->popup( anchor->mapToGlobal( QPoint( 0, anchor->height() ) ) );
  return true;
}

bool ShadowPopup::showMenuAtGlobal( QMenu* menu, const QRect& global_anchor_rect )
{
  if( !menu )
    return false;
  installMenuTracking();
  menu->ensurePolished();
  const QSize size = menu->sizeHint();
  if( size.isEmpty() )
    return false;
  const QRect available = availableGeometryFor( global_anchor_rect.center() );
  const QRect geometry = placeGeometry( global_anchor_rect, size, available, Automatic, 2 );
  QWidget* owner = menu->parentWidget();
  if( owner )
    menuTracker()->setAnchor( menu, owner, QRect( owner->mapFromGlobal( global_anchor_rect.topLeft() ),
      global_anchor_rect.size() ) );
  menu->popup( geometry.topLeft() );
  return true;
}

void ShadowPopup::paintEvent( QPaintEvent* )
{
  QPainter painter( this );
  paintCard( &painter, rect().adjusted( shadowMargin, shadowMargin, -shadowMargin, -shadowMargin ), kCardRadius );
}

void ShadowPopup::showEvent( QShowEvent* event )
{
  QWidget::showEvent( event );
  place();
}

void ShadowPopup::keyPressEvent( QKeyEvent* event )
{
  if( event->key() == Qt::Key_Escape )
  {
    event->accept();
    hide();
    return;
  }
  QWidget::keyPressEvent( event );
}

void ShadowPopup::hideEvent( QHideEvent* event )
{
  QWidget::hideEvent( event );
  QWidget* focus_target = m_focusOnClose.data();
  if( !focus_target && m_anchor && m_anchor->isVisible() )
    focus_target = m_anchor.data();
  if( focus_target && focus_target->isVisible() )
  {
    QPointer<QWidget> target = focus_target;
    QTimer::singleShot( 0, focus_target, [target]() {
      if( target && target->isVisible() &&
          (!QApplication::activeWindow() || target->window()->isActiveWindow()) )
        target->setFocus( Qt::OtherFocusReason );
    } );
  }
  emit popupHidden();
}

bool ShadowPopup::eventFilter( QObject* watched, QEvent* event )
{
  QWidget* widget = qobject_cast<QWidget*>( watched );
  if( isVisible() && QApplication::activePopupWidget() == this )
  {
    if( event->type() == QEvent::KeyPress && widget && widget->window() == this &&
        static_cast<QKeyEvent*>( event )->key() == Qt::Key_Escape )
    {
      hide();
      return true;
    }
    if( event->type() == QEvent::MouseButtonPress && widget && widget->window() != this &&
        !frameGeometry().contains( static_cast<QMouseEvent*>( event )->globalPos() ) )
      hide();
  }
  if( isVisible() && event->type() == QEvent::FocusIn && widget && m_content->isAncestorOf( widget ) )
    m_scroll->ensureWidgetVisible( widget );
  if( isVisible() && m_anchorRectOverride.isValid() && widget == parentWidget() &&
      event->type() == QEvent::Move )
    place();
  if( watched == m_anchor && event->type() == QEvent::Hide )
  {
    hide();
    return false;
  }
  if( widget && m_anchor && widget != this &&
      (widget == m_anchor || widget->isAncestorOf( m_anchor )) &&
      (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) )
  {
    if( isVisible() )
    {
      place();
      raise();
    }
    return false;
  }
  if( widget && m_anchor && (widget == m_anchor || widget->isAncestorOf( m_anchor )) &&
      (event->type() == QEvent::Hide || (event->type() == QEvent::WindowStateChange && widget->isMinimized())) )
    hide();
  return QWidget::eventFilter( watched, event );
}
