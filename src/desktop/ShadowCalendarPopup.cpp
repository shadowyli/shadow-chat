#include "ShadowCalendarPopup.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
const int kDayRows = 6;
const int kDayColumns = 7;
const int kMinimumYear = 1900;
const int kYearPageSize = 12;

QToolButton* createGridButton( QWidget* parent, const QString& object_name, int width, int height )
{
  QToolButton* button = new QToolButton( parent );
  button->setObjectName( object_name );
  button->setAutoRaise( true );
  button->setFocusPolicy( Qt::StrongFocus );
  button->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
  button->setMinimumSize( width, height );
  return button;
}
}

ShadowCalendarPopup::ShadowCalendarPopup( QWidget* parent )
  : ShadowPopup( parent ), m_maximum( QDate::currentDate() ), m_view_date( QDate::currentDate() ),
    m_view( DayView ), m_previous( Q_NULLPTR ), m_next( Q_NULLPTR ), m_month_button( Q_NULLPTR ),
    m_year_button( Q_NULLPTR ), m_stack( Q_NULLPTR ), m_day_page( Q_NULLPTR ),
    m_month_page( Q_NULLPTR ), m_year_page( Q_NULLPTR ), m_day_grid( Q_NULLPTR ),
    m_month_grid( Q_NULLPTR ), m_year_grid( Q_NULLPTR )
{
  setObjectName( QStringLiteral( "ShadowCalendarPopup" ) );
  setProperty( "shadowCalendar", true );
  setPreferredWidth( 304 );
  setGap( 4 );
  buildUi();
}

void ShadowCalendarPopup::buildUi()
{
  QVBoxLayout* layout = contentLayout();
  layout->setContentsMargins( 14, 10, 14, 10 );
  layout->setSpacing( 6 );

  QHBoxLayout* header = new QHBoxLayout;
  header->setSpacing( 4 );
  m_previous = new QToolButton( contentWidget() );
  m_previous->setObjectName( QStringLiteral( "calendarPrevious" ) );
  m_previous->setText( QStringLiteral( "‹" ) );
  m_previous->setToolTip( QStringLiteral( "上一个月" ) );
  m_previous->setAccessibleName( QStringLiteral( "上一个月" ) );
  m_previous->setFixedSize( 28, 28 );
  header->addWidget( m_previous );

  m_month_button = new QToolButton( contentWidget() );
  m_month_button->setObjectName( QStringLiteral( "calendarMonth" ) );
  m_month_button->setToolTip( QStringLiteral( "选择月份" ) );
  m_month_button->setAccessibleName( QStringLiteral( "选择月份" ) );
  m_month_button->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
  m_month_button->setStyleSheet( QStringLiteral( "QToolButton{font-weight:600;color:#1F2D44;padding:4px 10px;}" ) );
  header->addWidget( m_month_button );

  m_year_button = new QToolButton( contentWidget() );
  m_year_button->setObjectName( QStringLiteral( "calendarYear" ) );
  m_year_button->setToolTip( QStringLiteral( "快速选择年份" ) );
  m_year_button->setAccessibleName( QStringLiteral( "选择年份" ) );
  m_year_button->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
  m_year_button->setStyleSheet( QStringLiteral( "QToolButton{font-weight:600;color:#1F2D44;padding:4px 10px;}" ) );
  header->addWidget( m_year_button );
  header->addStretch();

  m_next = new QToolButton( contentWidget() );
  m_next->setObjectName( QStringLiteral( "calendarNext" ) );
  m_next->setText( QStringLiteral( "›" ) );
  m_next->setToolTip( QStringLiteral( "下一个月" ) );
  m_next->setAccessibleName( QStringLiteral( "下一个月" ) );
  m_next->setFixedSize( 28, 28 );
  header->addWidget( m_next );
  layout->addLayout( header );

  m_stack = new QStackedWidget( contentWidget() );
  m_stack->setObjectName( QStringLiteral( "calendarViews" ) );
  m_stack->setFixedHeight( 236 );

  m_day_page = new QWidget( m_stack );
  m_day_grid = new QGridLayout( m_day_page );
  m_day_grid->setContentsMargins( 0, 0, 0, 0 );
  m_day_grid->setHorizontalSpacing( 2 );
  m_day_grid->setVerticalSpacing( 2 );
  const QStringList weekdays = QStringList() << QStringLiteral( "一" ) << QStringLiteral( "二" ) <<
    QStringLiteral( "三" ) << QStringLiteral( "四" ) << QStringLiteral( "五" ) <<
    QStringLiteral( "六" ) << QStringLiteral( "日" );
  for( int column = 0; column < kDayColumns; ++column )
  {
    QLabel* label = new QLabel( weekdays.at( column ), m_day_page );
    label->setAlignment( Qt::AlignCenter );
    label->setFixedHeight( 22 );
    label->setProperty( "calendarWeekday", true );
    label->setStyleSheet( QStringLiteral( "QLabel{color:#8794A8;font-size:11px;}" ) );
    m_day_grid->addWidget( label, 0, column );
    m_weekday_labels.append( label );
  }
  for( int index = 0; index < kDayRows * kDayColumns; ++index )
  {
    QToolButton* day = createGridButton( m_day_page, QStringLiteral( "calendarDay" ), 34, 30 );
    day->setProperty( "calendarDay", true );
    day->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    day->setFixedHeight( 31 );
    connect( day, &QToolButton::clicked, this, [this, day]() {
      const QVariant value = day->property( "calendarDate" );
      if( value.isValid() )
        choose( value.toDate() );
    } );
    m_day_buttons.append( day );
    m_day_grid->addWidget( day, 1 + index / kDayColumns, index % kDayColumns );
  }
  for( int row = 0; row < kDayRows; ++row )
    m_day_grid->setRowStretch( 1 + row, 1 );
  m_stack->addWidget( m_day_page );

  m_month_page = new QWidget( m_stack );
  m_month_grid = new QGridLayout( m_month_page );
  m_month_grid->setContentsMargins( 8, 8, 8, 8 );
  m_month_grid->setSpacing( 6 );
  for( int index = 0; index < 12; ++index )
  {
    QToolButton* button = createGridButton( m_month_page, QStringLiteral( "calendarMonthCell" ), 52, 36 );
    button->setProperty( "calendarMonthCell", true );
    connect( button, &QToolButton::clicked, this, [this, index]() {
      const QDate candidate( m_view_date.year(), index + 1, 1 );
      if( !candidate.isValid() )
        return;
      m_view_date = candidate;
      m_view = DayView;
      refresh();
    } );
    m_month_buttons.append( button );
    m_month_grid->addWidget( button, index / 4, index % 4 );
  }
  m_stack->addWidget( m_month_page );

  m_year_page = new QWidget( m_stack );
  m_year_grid = new QGridLayout( m_year_page );
  m_year_grid->setContentsMargins( 6, 8, 6, 8 );
  m_year_grid->setSpacing( 5 );
  for( int index = 0; index < kYearPageSize; ++index )
  {
    QToolButton* button = createGridButton( m_year_page, QStringLiteral( "calendarYearCell" ), 52, 32 );
    button->setProperty( "calendarYearCell", true );
    connect( button, &QToolButton::clicked, this, [this, button]() {
      const QVariant value = button->property( "calendarYearValue" );
      if( !value.isValid() )
        return;
      m_view_date = QDate( value.toInt(), qBound( 1, m_view_date.month(), 12 ), 1 );
      m_view = MonthView;
      refresh();
    } );
    m_year_buttons.append( button );
    m_year_grid->addWidget( button, index / 4, index % 4 );
  }
  m_stack->addWidget( m_year_page );
  layout->addWidget( m_stack );

  QHBoxLayout* footer = new QHBoxLayout;
  footer->setSpacing( 6 );
  QToolButton* clear = new QToolButton( contentWidget() );
  clear->setObjectName( QStringLiteral( "calendarClear" ) );
  clear->setText( QStringLiteral( "清除生日" ) );
  clear->setToolTip( QStringLiteral( "清除已填写的生日" ) );
  clear->setAccessibleName( QStringLiteral( "清除生日" ) );
  clear->setStyleSheet( QStringLiteral( "QToolButton{color:#697B94;padding:3px 6px;}" ) );
  connect( clear, &QToolButton::clicked, this, [this]() {
    m_selected = QDate();
    hide();
    emit cleared();
  } );
  footer->addWidget( clear );
  footer->addStretch();
  QToolButton* today = new QToolButton( contentWidget() );
  today->setObjectName( QStringLiteral( "calendarToday" ) );
  today->setText( QStringLiteral( "今天" ) );
  today->setToolTip( QStringLiteral( "选择今天" ) );
  today->setAccessibleName( QStringLiteral( "选择今天" ) );
  today->setStyleSheet( QStringLiteral( "QToolButton{color:#2457B8;font-weight:600;padding:3px 6px;}" ) );
  connect( today, &QToolButton::clicked, this, [this]() {
    choose( QDate::currentDate() <= m_maximum ? QDate::currentDate() : m_maximum );
  } );
  footer->addWidget( today );
  layout->addLayout( footer );

  connect( m_previous, &QToolButton::clicked, this, [this]() {
    if( m_view == DayView )
      shiftMonth( -1 );
    else if( m_view == MonthView )
      shiftYear( -1 );
    else
      shiftYearPage( -1 );
  } );
  connect( m_next, &QToolButton::clicked, this, [this]() {
    if( m_view == DayView )
      shiftMonth( 1 );
    else if( m_view == MonthView )
      shiftYear( 1 );
    else
      shiftYearPage( 1 );
  } );
  connect( m_month_button, &QToolButton::clicked, this, [this]() {
    m_view = m_view == MonthView ? DayView : MonthView;
    refresh();
  } );
  connect( m_year_button, &QToolButton::clicked, this, [this]() {
    m_view = YearView;
    refresh();
  } );

  setStyleSheet( styleSheet() + QStringLiteral(
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true]{background:transparent;border:1px solid transparent;border-radius:7px;color:#34445E;font-size:12px;padding:0;}"
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true]:hover{background:#EDF3FC;color:#1D4ED8;}"
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true]:focus{border-color:#2463EB;}"
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true][weekend=true]{color:#C05A5A;}"
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true][today=true]{border-color:#8CB4FF;color:#2457B8;font-weight:600;}"
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true][selected=true]{background:#2463EB;border-color:#2463EB;color:#FFFFFF;font-weight:600;}"
    "QWidget#ShadowPopupContent QToolButton[calendarDay=true]:disabled{color:#C7CFDA;background:transparent;border-color:transparent;}"
    "QWidget#ShadowPopupContent QToolButton[calendarMonthCell=true],QWidget#ShadowPopupContent QToolButton[calendarYearCell=true]{border-radius:7px;color:#34445E;font-size:12px;}"
    "QWidget#ShadowPopupContent QToolButton[calendarMonthCell=true]:hover,QWidget#ShadowPopupContent QToolButton[calendarYearCell=true]:hover{background:#EDF3FC;color:#1D4ED8;}"
    "QWidget#ShadowPopupContent QToolButton[calendarMonthCell=true][selected=true],QWidget#ShadowPopupContent QToolButton[calendarYearCell=true][selected=true]{background:#2463EB;color:#FFFFFF;}"
    "QWidget#ShadowPopupContent QToolButton[calendarMonthCell=true]:disabled,QWidget#ShadowPopupContent QToolButton[calendarYearCell=true]:disabled{color:#C7CFDA;}" ) );

  refresh();
}

void ShadowCalendarPopup::setCalendarDate( const QDate& selected, const QDate& maximum )
{
  m_maximum = maximum.isValid() ? maximum : QDate::currentDate();
  m_selected = selected.isValid() && selected <= m_maximum ? selected : QDate();
  m_view_date = m_selected.isValid() ? m_selected : m_maximum;
  if( m_view_date.year() < kMinimumYear )
    m_view_date = QDate( kMinimumYear, 1, 1 );
  m_view = DayView;
  refresh();
}

QDate ShadowCalendarPopup::selectedDate() const
{
  return m_selected;
}

QDate ShadowCalendarPopup::maximumDate() const
{
  return m_maximum;
}

QToolButton* ShadowCalendarPopup::monthButton() const
{
  return m_month_button;
}

QToolButton* ShadowCalendarPopup::yearButton() const
{
  return m_year_button;
}

QToolButton* ShadowCalendarPopup::dayButton( const QDate& date ) const
{
  foreach( QToolButton* button, m_day_buttons )
    if( button->property( "calendarDate" ).toDate() == date )
      return button;
  return Q_NULLPTR;
}

QStackedWidget* ShadowCalendarPopup::viewStack() const
{
  return m_stack;
}

void ShadowCalendarPopup::showEvent( QShowEvent* event )
{
  ShadowPopup::showEvent( event );
  QToolButton* focus_button = dayButton( m_selected );
  if( !focus_button && m_view == DayView )
    focus_button = dayButton( QDate::currentDate() );
  if( focus_button && focus_button->isEnabled() )
    focus_button->setFocus( Qt::PopupFocusReason );
}

void ShadowCalendarPopup::choose( const QDate& date )
{
  if( !date.isValid() || date > m_maximum )
    return;
  m_selected = date;
  hide();
  emit dateSelected( date );
}

void ShadowCalendarPopup::shiftMonth( int months )
{
  QDate candidate = m_view_date.addMonths( months );
  if( candidate.year() < kMinimumYear )
    candidate = QDate( kMinimumYear, 1, 1 );
  if( candidate > m_maximum )
    candidate = QDate( m_maximum.year(), m_maximum.month(), 1 );
  m_view_date = candidate;
  refresh();
}

void ShadowCalendarPopup::shiftYear( int years )
{
  int year = qBound( kMinimumYear, m_view_date.year() + years, m_maximum.year() );
  m_view_date = QDate( year, m_view_date.month(), 1 );
  refresh();
}

void ShadowCalendarPopup::shiftYearPage( int pages )
{
  const int current_start = kMinimumYear + ( ( m_view_date.year() - kMinimumYear ) / kYearPageSize ) * kYearPageSize;
  int start = current_start + pages * kYearPageSize;
  const int last_start = kMinimumYear + ( ( m_maximum.year() - kMinimumYear ) / kYearPageSize ) * kYearPageSize;
  start = qBound( kMinimumYear, start, qMax( kMinimumYear, last_start ) );
  m_view_date = QDate( start, 1, 1 );
  refresh();
}

void ShadowCalendarPopup::refresh()
{
  switch( m_view )
  {
  case DayView:
    m_stack->setCurrentWidget( m_day_page );
    break;
  case MonthView:
    m_stack->setCurrentWidget( m_month_page );
    break;
  default:
    m_stack->setCurrentWidget( m_year_page );
    break;
  }
  refreshHeader();
  refreshDays();
  refreshMonths();
  refreshYears();
  m_previous->setEnabled( m_view == YearView ? m_view_date.year() > kMinimumYear
    : m_view == MonthView ? m_view_date.year() > kMinimumYear
    : !( m_view_date.year() <= kMinimumYear && m_view_date.month() <= 1 ) );
  m_previous->setToolTip( m_view == DayView ? QStringLiteral( "上一个月" ) :
    m_view == MonthView ? QStringLiteral( "上一年" ) : QStringLiteral( "上一组年份" ) );
  m_previous->setAccessibleName( m_previous->toolTip() );
  if( m_view == DayView )
  {
    const QDate next( m_view_date.year(), m_view_date.month(), 1 );
    m_next->setEnabled( next.addMonths( 1 ) <= QDate( m_maximum.year(), m_maximum.month(), 1 ) );
    m_next->setToolTip( QStringLiteral( "下一个月" ) );
  }
  else if( m_view == MonthView )
  {
    m_next->setEnabled( m_view_date.year() < m_maximum.year() );
    m_next->setToolTip( QStringLiteral( "下一年" ) );
  }
  else
  {
    const int start = kMinimumYear + ( ( m_view_date.year() - kMinimumYear ) / kYearPageSize ) * kYearPageSize;
    m_next->setEnabled( start + kYearPageSize <= m_maximum.year() );
    m_next->setToolTip( QStringLiteral( "下一组年份" ) );
  }
  m_next->setAccessibleName( m_next->toolTip() );
  m_month_button->setVisible( m_view != YearView );
  m_year_button->setVisible( true );
}

void ShadowCalendarPopup::refreshHeader()
{
  if( m_view == DayView )
  {
    m_month_button->setText( QStringLiteral( "%1月" ).arg( m_view_date.month() ) );
    m_year_button->setText( QStringLiteral( "%1年" ).arg( m_view_date.year() ) );
    m_year_button->setToolTip( QStringLiteral( "快速选择年份" ) );
  }
  else if( m_view == MonthView )
  {
    m_month_button->setText( QStringLiteral( "返回日期" ) );
    m_year_button->setText( QStringLiteral( "%1年" ).arg( m_view_date.year() ) );
    m_year_button->setToolTip( QStringLiteral( "快速选择年份" ) );
  }
  else
  {
    const int start = kMinimumYear + ( ( m_view_date.year() - kMinimumYear ) / kYearPageSize ) * kYearPageSize;
    m_year_button->setText( QStringLiteral( "%1 – %2" ).arg( start ).arg( start + kYearPageSize - 1 ) );
    m_year_button->setToolTip( QStringLiteral( "选择年份" ) );
  }
}

void ShadowCalendarPopup::updateButtonState( QToolButton* button, bool selected, bool today,
                                             bool weekend, bool enabled )
{
  button->setEnabled( enabled );
  button->setProperty( "selected", selected );
  button->setProperty( "today", today );
  button->setProperty( "weekend", weekend );
  button->style()->unpolish( button );
  button->style()->polish( button );
  button->update();
}

void ShadowCalendarPopup::refreshDays()
{
  const QDate first( m_view_date.year(), m_view_date.month(), 1 );
  const int offset = first.dayOfWeek() - Qt::Monday;
  const int days = first.daysInMonth();
  const QDate today = QDate::currentDate();
  for( int index = 0; index < m_day_buttons.size(); ++index )
  {
    QToolButton* button = m_day_buttons.at( index );
    const int day = index - offset + 1;
    if( day < 1 || day > days )
    {
      button->setText( QString() );
      button->setProperty( "calendarDate", QVariant() );
      button->setToolTip( QString() );
      button->setAccessibleName( QString() );
      updateButtonState( button, false, false, false, false );
      continue;
    }
    const QDate date( m_view_date.year(), m_view_date.month(), day );
    const int column = index % kDayColumns;
    const bool weekend = column >= 5;
    const bool enabled = date <= m_maximum;
    button->setText( QString::number( day ) );
    button->setProperty( "calendarDate", date );
    button->setToolTip( date.toString( QStringLiteral( "yyyy-MM-dd" ) ) );
    button->setAccessibleName( date.toString( QStringLiteral( "yyyy年M月d日" ) ) );
    updateButtonState( button, date == m_selected, date == today, weekend, enabled );
  }
}

void ShadowCalendarPopup::refreshMonths()
{
  const QStringList names = QStringList() << QStringLiteral( "一月" ) << QStringLiteral( "二月" ) <<
    QStringLiteral( "三月" ) << QStringLiteral( "四月" ) << QStringLiteral( "五月" ) <<
    QStringLiteral( "六月" ) << QStringLiteral( "七月" ) << QStringLiteral( "八月" ) <<
    QStringLiteral( "九月" ) << QStringLiteral( "十月" ) << QStringLiteral( "十一月" ) <<
    QStringLiteral( "十二月" );
  for( int index = 0; index < m_month_buttons.size(); ++index )
  {
    QToolButton* button = m_month_buttons.at( index );
    const bool enabled = QDate( m_view_date.year(), index + 1, 1 ) <= m_maximum;
    button->setText( names.at( index ) );
    updateButtonState( button, m_view_date.year() == m_selected.year() && index + 1 == m_selected.month(),
                       false, false, enabled );
  }
}

void ShadowCalendarPopup::refreshYears()
{
  const int start = kMinimumYear + ( ( m_view_date.year() - kMinimumYear ) / kYearPageSize ) * kYearPageSize;
  for( int index = 0; index < m_year_buttons.size(); ++index )
  {
    QToolButton* button = m_year_buttons.at( index );
    const int year = start + index;
    const bool visible = year >= kMinimumYear && year <= m_maximum.year();
    button->setVisible( visible );
    button->setText( QString::number( year ) );
    button->setProperty( "calendarYearValue", year );
    updateButtonState( button, year == m_selected.year(), false, false, visible );
  }
}

void ShadowCalendarPopup::setView( View view )
{
  m_view = view;
  refresh();
}
