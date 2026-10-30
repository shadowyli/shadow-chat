#ifndef SHADOWDEVPREVIEW_H
#define SHADOWDEVPREVIEW_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QRect>
#include <QTimer>
class GuiMain;
class Core;
class Chat;
class QImage;
class ShadowPreviewTransport;

class ShadowDevPreview : public QObject
{
public:
  static bool checkArguments( int argc, char** argv, QString* error );
  static bool configure( const QStringList& arguments, QString* error );
  static bool seedAccount( QString* error );
  static void prepareSettings();
  static bool active();
  static bool canTestChat( const Chat& );
  static bool testPeerReady();
  static bool sendTestText( quint64 chatId, const QString& message, bool sourceCode, QString* feedback );
  static bool sendTestImage( quint64 chatId, const QImage&, QString* feedback );
  static bool sendTestFiles( quint64 chatId, const QStringList& paths, QString* feedback );
  static bool validateStyle( const QByteArray& data, QString* error );
  static bool validateLayout( const QByteArray& data, QJsonObject* values, QString* error );
  ShadowDevPreview( GuiMain* window, Core* core );
  void reload();
  void restoreBaseline();
protected:
  bool eventFilter( QObject* watched, QEvent* event ) override;
private:
  static ShadowDevPreview* testSession( quint64 chatId, QString* error );
  bool receiveTestData( quint64 chatId, const QByteArray&, const QString& name, const QString& kind,
                        QString* receivedPath, QString* feedback );
  void watchFiles();
  void seedFixtures( Core* core );
  void clearFixtureHistory();
  void saveSession();
  void restoreSession();
  void report( const QString& event, const QString& diagnostic = QString() );
  void pollControl();
  GuiMain* m_window;
  Core* m_core;
  ShadowPreviewTransport* m_testTransport = nullptr;
  QFileSystemWatcher m_watcher;
  QTimer m_debounce;
  QTimer m_poll;
  QString m_config;
  QString m_profileRoot;
  QString m_output;
  QString m_instance;
  QString m_baselineStyle;
  QRect m_baselineGeometry;
  int m_baselineListMin;
  int m_baselineListMax;
  QByteArray m_styleData;
  QByteArray m_layoutData;
  QJsonObject m_layout;
  int m_revision;
  bool m_started;
  bool m_stopping;
};
#endif
