//////////////////////////////////////////////////////////////////////
//
// 影聊 portable workspace archive.
//
//////////////////////////////////////////////////////////////////////

#ifndef SHADOWWORKSPACEARCHIVE_H
#define SHADOWWORKSPACEARCHIVE_H

#include "Config.h"


class ShadowWorkspaceArchive
{
public:
  static QString fileExtension();
  static QString pendingFilePath( const QString& data_folder );
  static bool exportWorkspace( const QString& data_folder, const QString& archive_path, QString* error_message );
  static bool stageImport( const QString& data_folder, const QString& archive_path, QString* error_message );
  static bool applyPendingImport( const QString& data_folder, QString* restore_folder, QString* error_message );

private:
  struct Entry
  {
    QString name;
    QByteArray data;
  };

  static QStringList allowedEntryNames();
  static bool readArchive( const QString& archive_path, QList<Entry>* entries, QString* error_message );
};

#endif // SHADOWWORKSPACEARCHIVE_H
