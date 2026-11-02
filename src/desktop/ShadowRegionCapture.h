#ifndef SHADOWCHAT_REGIONCAPTURE_H
#define SHADOWCHAT_REGIONCAPTURE_H
#include "Config.h"
#include <QScreen>
#include <QMouseEvent>
#include <QPainter>

class ShadowRegionCapture : public QDialog
{
public:
  // Image-backed constructor allows deterministic selection tests without
  // capturing the user's desktop or sending operating-system input.
  explicit ShadowRegionCapture(const QPixmap& image,QWidget* parent=Q_NULLPTR):QDialog(parent),m_screen(image),m_dragging(false) {
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Tool);
    resize(image.size());setCursor(Qt::CrossCursor);setMouseTracking(true);
  }
  explicit ShadowRegionCapture(QWidget* parent=Q_NULLPTR):QDialog(parent),m_dragging(false) {
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Tool);
    QRect bounds;
    foreach(QScreen* screen,QGuiApplication::screens())bounds=bounds.united(screen->geometry());
    m_screen=QPixmap(bounds.size());m_screen.fill(Qt::black);
    QPainter painter(&m_screen);
    foreach(QScreen* screen,QGuiApplication::screens())painter.drawPixmap(screen->geometry().topLeft()-bounds.topLeft(),screen->grabWindow(0).scaled(screen->geometry().size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
    painter.end();setGeometry(bounds);setCursor(Qt::CrossCursor);setMouseTracking(true);
  }
  QPixmap selectedImage() const {return m_selection.width()>2&&m_selection.height()>2?m_screen.copy(m_selection):QPixmap();}
  QPixmap screenImage() const { return m_screen; }
  QRect selectedRect() const { return m_selection; }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);painter.drawPixmap(0,0,m_screen);painter.fillRect(rect(),QColor(0,0,0,100));
    if(!m_selection.isEmpty()) {painter.drawPixmap(m_selection,m_screen,m_selection);painter.setPen(QPen(QColor("#2563eb"),2));painter.drawRect(m_selection);}
    painter.setPen(Qt::white);painter.drawText(24,32,QString::fromUtf8("拖动选择截图区域，松开确认 · Esc 取消"));
  }
  void mousePressEvent(QMouseEvent* event) override {if(event->button()!=Qt::LeftButton){reject();return;}m_origin=event->pos();m_dragging=true;m_selection=QRect();}
  void mouseMoveEvent(QMouseEvent* event) override {if(m_dragging){m_selection=QRect(m_origin,event->pos()).normalized().intersected(rect());update();}}
  void mouseReleaseEvent(QMouseEvent* event) override {if(!m_dragging)return;m_dragging=false;m_selection=QRect(m_origin,event->pos()).normalized().intersected(rect());if(m_selection.width()>2&&m_selection.height()>2)accept();else reject();}
private:
  QPixmap m_screen;QPoint m_origin;QRect m_selection;bool m_dragging;
};
#endif
