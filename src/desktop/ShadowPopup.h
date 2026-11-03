#ifndef SHADOWCHAT_SHADOWPOPUP_H
#define SHADOWCHAT_SHADOWPOPUP_H

#include <QColor>
#include <QPointer>
#include <QRect>
#include <QWidget>

class QEvent;
class QHideEvent;
class QKeyEvent;
class QMenu;
class QPaintEvent;
class QPainter;
class QScreen;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;

class ShadowPopup : public QWidget
{
  Q_OBJECT

public:
  enum Placement { Automatic, Above, Below };

  explicit ShadowPopup( QWidget* parent = Q_NULLPTR );
  ~ShadowPopup();

  void setAnchor( QWidget* anchor, const QPoint& anchor_offset = QPoint() );
  QWidget* anchor() const;
  void setAnchorRect( const QRect& global_anchor_rect );
  QRect anchorRect() const;

  void setPlacement( Placement placement );
  Placement placement() const;
  void setGap( int gap );
  void setFocusOnClose( QWidget* widget );
  void setPreferredWidth( int width );
  QSize sizeHint() const override;

  QWidget* contentWidget() const;
  QVBoxLayout* contentLayout() const;

  void showPopup();
  QRect placementGeometryFor( const QSize& size ) const;

  static QRect placeGeometry( const QRect& anchor_rect, const QSize& popup_size,
                              const QRect& available_geometry, Placement placement = Automatic, int gap = 6 );
  static QRect availableGeometryFor( const QPoint& global_point, const QScreen* preferred_screen = Q_NULLPTR );
  static bool showMenuAt( QMenu* menu, QWidget* anchor, const QPoint& local_anchor = QPoint() );
  static bool showMenuAtGlobal( QMenu* menu, const QRect& global_anchor_rect );
  static void installMenuTracking();
  static void paintCard( QPainter* painter, const QRect& card_rect, int radius = 12,
                         const QColor& background = QColor( "#FFFFFF" ), const QColor& border = QColor( "#DCE4F0" ) );

  static const int shadowMargin = 10;

signals:
  void popupHidden();

protected:
  void paintEvent( QPaintEvent* ) override;
  void showEvent( QShowEvent* ) override;
  void keyPressEvent( QKeyEvent* ) override;
  void hideEvent( QHideEvent* ) override;
  bool eventFilter( QObject* watched, QEvent* event ) override;

private:
  void place();
  void trackAnchor( QWidget* anchor );

  QWidget* m_content;
  QVBoxLayout* m_contentLayout;
  QScrollArea* m_scroll;
  QPointer<QWidget> m_anchor;
  QPointer<QWidget> m_anchorWindow;
  QPointer<QWidget> m_focusOnClose;
  QRect m_anchorRectOverride;
  QPoint m_anchorOffset;
  Placement m_placement;
  int m_gap;
  int m_preferredWidth;
};

#endif
