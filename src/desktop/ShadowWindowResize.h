#ifndef SHADOWWINDOWRESIZE_H
#define SHADOWWINDOWRESIZE_H

#include <QObject>
#include <QMap>
#include <QPointer>
#include <QRect>

class QWidget;

class ShadowWindowResize final : public QObject
{
public:
  explicit ShadowWindowResize( QWidget* window, QWidget* surface = nullptr );
  static Qt::Edges edgesAt( const QWidget* window, const QPoint& position );

protected:
  bool eventFilter( QObject*, QEvent* ) override;

private:
  void updateHandles();
  void resizeTo( const QPoint& globalPosition );
  void finishResize();
  Qt::Edges allowedEdges( Qt::Edges ) const;
  QPointer<QWidget> m_window;
  QPointer<QWidget> m_surface;
  QMap<int, QWidget*> m_handles;
  Qt::Edges m_resizing;
  QPoint m_pressPosition;
  QRect m_originalGeometry;
};

#endif
