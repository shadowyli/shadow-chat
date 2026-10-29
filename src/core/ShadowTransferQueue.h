#ifndef SHADOW_TRANSFER_QUEUE_H
#define SHADOW_TRANSFER_QUEUE_H
#include <QtCore>

// Metadata only: source materials are never copied into the outbox.
class ShadowTransferQueue
{
public:
  struct Task {
    QString id, batch, recipient, chat, source, state, error;
    bool group = false;
    QJsonArray files;
    QStringList completed;
    QStringList directories;
  };
  bool open( const QString& accountFolder );
  bool enqueue( const QString& recipient, const QString& chat, bool group, const QString& source, QString* error, const QString& batch = QString() );
  QList<Task> tasks() const { return m_tasks; }
  bool update( const Task&, bool persist = true );
  bool remove( const QString& id );
  static bool sourceUnchanged( const Task&, QString* error );
  static QString canonicalPath( const QString& );
private:
  bool save();
  QString m_path;
  QList<Task> m_tasks;
};
#endif
