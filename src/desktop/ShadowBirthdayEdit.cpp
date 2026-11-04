#include "ShadowBirthdayEdit.h"
#include "ShadowCalendarPopup.h"
#include "ShadowStyle.h"
#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>
#include <QWheelEvent>

namespace
{
const QDate kEmptyDate( 1, 1, 1 );
const QDate kFirstSelectableDate( 1900, 1, 1 );
}

ShadowBirthdayEdit::ShadowBirthdayEdit( QWidget* parent )
  : QDateEdit( parent ), m_has_date( false ), m_calendar_action( Q_NULLPTR ),
    m_clear_action( Q_NULLPTR ), m_calendar( Q_NULLPTR )
{
  setObjectName( QStringLiteral( "mp_deBirthday" ) );
  setCalendarPopup( false );
  setDisplayFormat( QStringLiteral( "yyyy / MM / dd" ) );
  setMinimumDate( kEmptyDate );
  setMaximumDate( QDate( 9999, 12, 31 ) );
  setSpecialValueText( QStringLiteral( "未填写" ) );
  setButtonSymbols( QAbstractSpinBox::NoButtons );
  setReadOnly( true );
  setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
  setFocusPolicy( Qt::StrongFocus );
  setCursor( Qt::IBeamCursor );
  if( lineEdit() )
  {
    lineEdit()->setReadOnly( true );
    lineEdit()->setCursor( Qt::IBeamCursor );
  }

  m_calendar_action = lineEdit()->addAction( calendarIcon( QColor( "#5F6B7A" ) ), QLineEdit::TrailingPosition );
  m_calendar_action->setToolTip( QStringLiteral( "打开日历选择生日" ) );
  m_calendar_action->setText( QStringLiteral( "选择生日" ) );
  connect( m_calendar_action, &QAction::triggered, this, &ShadowBirthdayEdit::showCalendar );

  m_clear_action = lineEdit()->addAction( calendarIcon( QColor( "#8794A8" ) ), QLineEdit::TrailingPosition );
  m_clear_action->setIcon( QIcon() );
  {
    QPixmap clear_pixmap( 14, 14 );
    clear_pixmap.fill( Qt::transparent );
    QPainter painter( &clear_pixmap );
    painter.setRenderHint( QPainter::Antialiasing );
    painter.setPen( QPen( QColor( "#8794A8" ), 1.4, Qt::SolidLine, Qt::RoundCap ) );
    painter.drawLine( QPointF( 3.5, 3.5 ), QPointF( 10.5, 10.5 ) );
    painter.drawLine( QPointF( 10.5, 3.5 ), QPointF( 3.5, 10.5 ) );
    painter.end();
    m_clear_action->setIcon( QIcon( clear_pixmap ) );
  }
  m_clear_action->setToolTip( QStringLiteral( "清除生日" ) );
  m_clear_action->setText( QStringLiteral( "清除生日" ) );
  connect( m_clear_action, &QAction::triggered, this, &ShadowBirthdayEdit::clearBirthday );

  setDate( kEmptyDate );
  updateAppearance();
}

QDate ShadowBirthdayEdit::emptyDate()
{
  return kEmptyDate;
}

int ShadowBirthdayEdit::minimumSelectableYear() const
{
  return kFirstSelectableDate.year();
}

void ShadowBirthdayEdit::setBirthday( const QDate& date )
{
  const bool valid = date.isValid();
  m_has_date = valid;
  m_date = valid ? date : QDate();
  setDate( valid ? date : kEmptyDate );
  updateAppearance();
}

QDate ShadowBirthdayEdit::birthday() const
{
  return m_has_date ? m_date : QDate();
}

bool ShadowBirthdayEdit::hasBirthday() const
{
  return m_has_date;
}

bool ShadowBirthdayEdit::isEmpty() const
{
  return !m_has_date;
}

void ShadowBirthdayEdit::clearBirthday()
{
  if( m_calendar && m_calendar->isVisible() )
    m_calendar->hide();
  const bool changed = m_has_date;
  m_has_date = false;
  m_date = QDate();
  setDate( kEmptyDate );
  updateAppearance();
  if( changed )
    emit birthdayChanged( QDate() );
}

void ShadowBirthdayEdit::showCalendar()
{
  if( m_calendar && m_calendar->isVisible() )
  {
    m_calendar->hide();
    return;
  }
  if( !m_calendar )
  {
    m_calendar = new ShadowCalendarPopup( this );
    m_calendar->setObjectName( QStringLiteral( "ShadowCalendarPopup" ) );
    m_calendar->setAnchor( this );
    m_calendar->setFocusOnClose( this );
    connect( m_calendar, &ShadowCalendarPopup::dateSelected, this, [this]( const QDate& date ) {
      if( !date.isValid() )
        return;
      m_has_date = true;
      m_date = date;
      setDate( date );
      updateAppearance();
      emit birthdayChanged( date );
    } );
    connect( m_calendar, &ShadowCalendarPopup::cleared, this, &ShadowBirthdayEdit::clearBirthday );
  }
  m_calendar->setCalendarDate( birthday(), QDate::currentDate() );
  m_calendar->showPopup();
}

void ShadowBirthdayEdit::updateAppearance()
{
  setSpecialValueText( m_has_date ? QString() : QStringLiteral( "未填写" ) );
  setProperty( "empty", !m_has_date );
  if( m_clear_action )
    m_clear_action->setVisible( m_has_date );
  setToolTip( m_has_date ? QStringLiteral( "生日：%1 · 点击日历图标修改" ).arg( m_date.toString( QStringLiteral( "yyyy-MM-dd" ) ) )
                         : QStringLiteral( "尚未填写生日 · 点击日历图标选择" ) );
  lineEdit()->setToolTip( toolTip() );
  setAccessibleName( QStringLiteral( "生日" ) );
  setAccessibleDescription( toolTip() );
  style()->unpolish( this );
  style()->polish( this );
  lineEdit()->style()->unpolish( lineEdit() );
  lineEdit()->style()->polish( lineEdit() );
  update();
}

QIcon ShadowBirthdayEdit::calendarIcon( const QColor& color ) const
{
  QPixmap pixmap( 16, 16 );
  pixmap.fill( Qt::transparent );
  QPainter painter( &pixmap );
  painter.setRenderHint( QPainter::Antialiasing );
  painter.setPen( QPen( color, 1.2 ) );
  painter.setBrush( Qt::NoBrush );
  painter.drawRoundedRect( QRectF( 2.5, 3.5, 11, 10 ), 2, 2 );
  painter.drawLine( QPointF( 2.5, 6.5 ), QPointF( 13.5, 6.5 ) );
  painter.drawLine( QPointF( 5.5, 1.8 ), QPointF( 5.5, 4.2 ) );
  painter.drawLine( QPointF( 10.5, 1.8 ), QPointF( 10.5, 4.2 ) );
  painter.setBrush( color );
  painter.setPen( Qt::NoPen );
  painter.drawRect( QRectF( 5, 8.5, 2, 2 ) );
  painter.drawRect( QRectF( 9, 8.5, 2, 2 ) );
  painter.end();
  return QIcon( pixmap );
}

void ShadowBirthdayEdit::changeEvent( QEvent* event )
{
  QDateEdit::changeEvent( event );
  if( event->type() == QEvent::EnabledChange || event->type() == QEvent::ReadOnlyChange )
    updateAppearance();
}

void ShadowBirthdayEdit::wheelEvent( QWheelEvent* event )
{
  event->accept();
}

void ShadowBirthdayEdit::stepBy( int )
{
}

void ShadowBirthdayEdit::keyPressEvent( QKeyEvent* event )
{
  if( event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter )
  {
    event->accept();
    showCalendar();
    return;
  }
  QDateEdit::keyPressEvent( event );
}
