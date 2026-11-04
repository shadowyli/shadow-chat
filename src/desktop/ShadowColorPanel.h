#ifndef SHADOWCHAT_SHADOWCOLORPANEL_H
#define SHADOWCHAT_SHADOWCOLORPANEL_H

#include <QColor>
#include <QList>

#include "ShadowPopup.h"

class QAbstractButton;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

class ShadowColorPanel : public ShadowPopup
{
  Q_OBJECT

public:
  explicit ShadowColorPanel( QWidget* parent = Q_NULLPTR );

  void setColor( const QColor& color );
  QColor color() const;
  void setTitle( const QString& title );

  static QColor pick( QWidget* anchor, const QColor& initial, QWidget* parent = Q_NULLPTR,
                      const QString& title = QString() );
  static QColor pickAtPoint( const QPoint& global_point, const QColor& initial, QWidget* parent = Q_NULLPTR,
                             const QString& title = QString() );
  static QColor pickAtRect( const QRect& global_anchor_rect, const QColor& initial,
                            QWidget* parent = Q_NULLPTR, const QString& title = QString() );
  static QColor pickForAction( QObject* source, const QColor& initial, QWidget* parent,
                               const QString& title = QString() );

signals:
  void colorAccepted( const QColor& );

private slots:
  void chooseColor( const QColor& );
  void acceptColor();

private:
  void buildSwatches();
  void updatePreview( bool update_text = true );

  QColor m_color;
  QLabel* m_title;
  QLineEdit* m_hex;
  QLabel* m_preview;
  QLabel* m_error;
  QPushButton* m_accept;
  QButtonGroup* m_swatch_group;
  QList<QAbstractButton*> m_swatches;
};

#endif
