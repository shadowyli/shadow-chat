#ifndef SHADOWCHAT_SHADOWCALENDARPOPUP_H
#define SHADOWCHAT_SHADOWCALENDARPOPUP_H

#include <QDate>
#include <QVector>

#include "ShadowPopup.h"

class QLabel;
class QStackedWidget;
class QToolButton;
class QGridLayout;

class ShadowCalendarPopup : public ShadowPopup
{
  Q_OBJECT

public:
  explicit ShadowCalendarPopup( QWidget* parent = Q_NULLPTR );

  void setCalendarDate( const QDate& selected, const QDate& maximum = QDate::currentDate() );
  QDate selectedDate() const;
  QDate maximumDate() const;

  QToolButton* monthButton() const;
  QToolButton* yearButton() const;
  QToolButton* dayButton( const QDate& date ) const;
  QStackedWidget* viewStack() const;

signals:
  void dateSelected( const QDate& );
  void cleared();

protected:
  void showEvent( QShowEvent* ) override;

private:
  enum View { DayView, MonthView, YearView };

  void buildUi();
  void setView( View );
  void refresh();
  void refreshHeader();
  void refreshDays();
  void refreshMonths();
  void refreshYears();
  void choose( const QDate& );
  void shiftMonth( int months );
  void shiftYear( int years );
  void shiftYearPage( int pages );
  void updateButtonState( QToolButton* button, bool selected, bool today, bool weekend, bool enabled );

  QDate m_selected;
  QDate m_maximum;
  QDate m_view_date;
  View m_view;

  QToolButton* m_previous;
  QToolButton* m_next;
  QToolButton* m_month_button;
  QToolButton* m_year_button;
  QStackedWidget* m_stack;
  QWidget* m_day_page;
  QWidget* m_month_page;
  QWidget* m_year_page;
  QGridLayout* m_day_grid;
  QGridLayout* m_month_grid;
  QGridLayout* m_year_grid;
  QVector<QToolButton*> m_day_buttons;
  QVector<QToolButton*> m_month_buttons;
  QVector<QToolButton*> m_year_buttons;
  QVector<QLabel*> m_weekday_labels;
};

#endif
