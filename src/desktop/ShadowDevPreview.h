#ifndef SHADOWDEVPREVIEW_H
#define SHADOWDEVPREVIEW_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QRect>
#include <QTimer>
class GuiMain;
class Core;

class ShadowDevPreview : public QObject
{
public:
  static bool checkArguments( int argc, char** argv, QString* error );
  static bool configure( const QStringList& arguments, QString* error );
  static bool seedAccount( QString* error );
  static void prepareSettings();
  static bool active();
  static bool validateStyle( const QByteArray& data, QString* error );
  static bool validateLayout( const QByteArray& data, QJsonObject* values, QString* error );
  ShadowDevPreview( GuiMain* window, Core* core );
  void reload();
  void restoreBaseline();
protected:
  bool eventFilter( QObject* watched, QEvent* event ) override;
private:
  void watchFiles();
  void seedFixtures( Core* core );
  void clearFixtureHistory();
  void saveSession();
  void restoreSession();
  void report( const QString& event, const QString& diagnostic = QString() );
  void pollControl();
  GuiMain* m_window;
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
