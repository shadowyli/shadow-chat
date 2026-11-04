#ifndef SHADOWCHAT_SHADOWBIRTHDAYEDIT_H
#define SHADOWCHAT_SHADOWBIRTHDAYEDIT_H

#include <QDate>
#include <QDateEdit>
#include <QIcon>

class QAction;
class QEvent;
class QKeyEvent;
class QWheelEvent;
class ShadowCalendarPopup;

class ShadowBirthdayEdit : public QDateEdit
{
  Q_OBJECT

public:
  explicit ShadowBirthdayEdit( QWidget* parent = Q_NULLPTR );

  void setBirthday( const QDate& date );
  QDate birthday() const;
  bool hasBirthday() const;
  bool isEmpty() const;

  int minimumSelectableYear() const;
  static QDate emptyDate();

public slots:
  void clearBirthday();
  void showCalendar();

signals:
  void birthdayChanged( const QDate& );

protected:
  void changeEvent( QEvent* ) override;
  void wheelEvent( QWheelEvent* ) override;
  void stepBy( int ) override;
  void keyPressEvent( QKeyEvent* ) override;

private:
  void updateAppearance();
  QIcon calendarIcon( const QColor& color ) const;

  QDate m_date;
  bool m_has_date;
  QAction* m_calendar_action;
  QAction* m_clear_action;
  ShadowCalendarPopup* m_calendar;
};

#endif
