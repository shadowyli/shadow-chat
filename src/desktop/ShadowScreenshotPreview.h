#ifndef SHADOWSCREENSHOTPREVIEW_H
#define SHADOWSCREENSHOTPREVIEW_H

#include <QDialog>
#include <QPixmap>

class QLabel;

class ShadowScreenshotPreview : public QDialog
{
  Q_OBJECT

public:
  explicit ShadowScreenshotPreview( const QPixmap& image, QWidget* parent = Q_NULLPTR );
  void showSendError( const QString& message );

signals:
  void sendRequested();

private:
  QLabel* mp_feedback;
};

#endif
