#ifndef SHADOWSCREENSHOTPREVIEW_H
#define SHADOWSCREENSHOTPREVIEW_H

#include <QDialog>
#include <QPixmap>
#include <QVector>

class QLabel;
class QGridLayout;
class QPushButton;
class QKeyEvent;
class ShadowScreenshotCanvas;

class ShadowScreenshotPreview : public QDialog
{
  Q_OBJECT

public:
  explicit ShadowScreenshotPreview( const QPixmap& image, QWidget* parent = Q_NULLPTR );
  ShadowScreenshotPreview( const QPixmap& image, const QRect& desktopGeometry, const QRect& selection, QWidget* parent );
  QPixmap selectedImage() const;
  bool saveImage( const QString& path );
  void showSendError( const QString& message );

signals:
  void sendRequested();

protected:
  void keyPressEvent( QKeyEvent* ) override;

private:
  void positionControls();
  void showFeedback( const QString&, bool error );
  ShadowScreenshotCanvas* m_canvas;
  QWidget* m_toolbar = Q_NULLPTR;
  QGridLayout* m_toolbarLayout;
  int m_toolbarColumns = 0;
  QVector<QPushButton*> m_buttons;
  QLabel* m_dimensions = Q_NULLPTR;
  QPushButton* m_undo;
  QPushButton* m_redo;
  QLabel* mp_feedback;
};

#endif
