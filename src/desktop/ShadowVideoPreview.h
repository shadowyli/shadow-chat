#ifndef SHADOWVIDEOPREVIEW_H
#define SHADOWVIDEOPREVIEW_H

#include <QDialog>
#include <QUrl>

class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class ShadowVlcPlayback;

class ShadowVideoPreview : public QDialog
{
  Q_OBJECT

public:
  enum PlaybackState { Loading, Playing, Paused, Ended, Failed, Stopped };
  explicit ShadowVideoPreview( const QUrl&, QWidget* parent = Q_NULLPTR );
  ~ShadowVideoPreview() override;
  static bool canPreview( const QUrl& );
  void openFile( const QUrl& );
  PlaybackState playbackState() const;
  qint64 duration() const;
  qint64 position() const;
  bool isSeekable() const;
  bool captureFrame( const QString& ) const;

public slots:
  void done( int ) override;

protected:
  void showEvent( QShowEvent* ) override;
  void closeEvent( QCloseEvent* ) override;

private:
  void fitToScreen();
  void createPlayer();
  void releasePlayer();
  void fail( const QString& );
  void updateControls();
  void updatePosition( qint64 );
  void seekToSlider();
  QString timeText( qint64 ) const;
  QUrl m_url;
  ShadowVlcPlayback* m_player = Q_NULLPTR;
  QWidget* m_canvas;
  QLabel* m_fileName;
  QLabel* m_feedback;
  QLabel* m_time;
  QPushButton* m_play;
  QPushButton* m_retry;
  QSlider* m_progress;
  QTimer* m_loadTimeout;
  QTimer* m_refresh;
  bool m_failed = false;
  bool m_resumeAfterSeek = false;
};

#endif
