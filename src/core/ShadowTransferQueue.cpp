#include "ShadowTransferQueue.h"

QString ShadowTransferQueue::canonicalPath( const QString& path )
{
  return QDir::fromNativeSeparators( QFileInfo( path ).absoluteFilePath() );
}

static QJsonObject snapshot( const QFileInfo& info )
{
  QJsonObject object;
  object["path"] = ShadowTransferQueue::canonicalPath( info.absoluteFilePath() );
  object["size"] = QString::number( info.size() );
  object["modified"] = QString::number( info.lastModified().toMSecsSinceEpoch() );
  return object;
}

bool ShadowTransferQueue::open( const QString& accountFolder )
{
  const QString path = QDir( accountFolder ).filePath( "pending-transfers.json" );
  if( m_path == path ) return true;
  m_path = path;
  m_tasks.clear();
  QFile file( path );
  if( !file.exists() ) return true;
  if( !file.open( QIODevice::ReadOnly ) ) { m_path.clear(); return false; }
  QJsonParseError parse;
  const QJsonDocument document = QJsonDocument::fromJson( file.readAll(), &parse );
  if( parse.error != QJsonParseError::NoError || !document.isArray() ) { m_path.clear(); return false; }
  foreach( const QJsonValue& value, document.array() )
  {
    const QJsonObject object = value.toObject();
    Task task;
    task.id = object["id"].toString();
    task.batch = object["batch"].toString( task.id );
    task.recipient = object["recipient"].toString();
    task.chat = object["chat"].toString();
    task.source = object["source"].toString();
    task.group = object["group"].toBool();
    task.files = object["files"].toArray();
    task.state = object["state"].toString();
    task.error = object["error"].toString();
    foreach( const QJsonValue& completed, object["completed"].toArray() ) task.completed << completed.toString();
    foreach( const QJsonValue& directory, object["directories"].toArray() ) task.directories << directory.toString();
    // After restarting there is no live transfer; resend the entire task.
    if( task.state == "offered" ) { task.state = "pending"; task.completed.clear(); }
    if( !task.id.isEmpty() && !task.recipient.isEmpty() ) m_tasks << task;
  }
  return true;
}

bool ShadowTransferQueue::save()
{
  QJsonArray array;
  foreach( const Task& task, m_tasks )
  {
    QJsonObject object;
    object["id"] = task.id; object["batch"] = task.batch; object["recipient"] = task.recipient;
    object["chat"] = task.chat; object["source"] = task.source;
    object["group"] = task.group; object["files"] = task.files;
    object["state"] = task.state; object["error"] = task.error;
    object["completed"] = QJsonArray::fromStringList( task.completed );
    object["directories"] = QJsonArray::fromStringList( task.directories );
    array << object;
  }
  QSaveFile file( m_path );
  const QByteArray bytes = QJsonDocument( array ).toJson();
  return file.open( QIODevice::WriteOnly ) && file.write( bytes ) == bytes.size() && file.commit();
}

bool ShadowTransferQueue::enqueue( const QString& recipient, const QString& chat, bool group, const QString& source, QString* error, const QString& batch )
{
  Task task;
  task.id = QUuid::createUuid().toString(); task.recipient = recipient;
  task.batch = batch.isEmpty() ? task.id : batch;
  task.chat = chat; task.group = group; task.source = canonicalPath( source ); task.state = "pending";
  const QFileInfo root( source );
  if( root.isFile() && root.isReadable() ) task.files << snapshot( root );
  else if( root.isDir() )
  {
    QDirIterator directories( source, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories );
    while( directories.hasNext() )
    {
      directories.next();
      if( directories.fileInfo().isSymLink() ) { if( error ) *error = QStringLiteral( "文件夹内包含不支持的符号链接。" ); return false; }
      task.directories << canonicalPath( directories.filePath() );
    }
    QDirIterator iterator( source, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories );
    while( iterator.hasNext() )
    {
      iterator.next();
      if( !iterator.fileInfo().isReadable() || iterator.fileInfo().isSymLink() ) { if( error ) *error = QStringLiteral( "源文件无法读取，或包含不支持的符号链接。" ); return false; }
      task.files << snapshot( iterator.fileInfo() );
    }
  }
  if( task.files.isEmpty() ) { if( error ) *error = QStringLiteral( "源文件不存在、无法读取，或文件夹内没有文件。" ); return false; }
  const QList<Task> previous = m_tasks;
  for( int i = m_tasks.size() - 1; i >= 0; --i )
    if( m_tasks[i].state == "failed" && m_tasks[i].recipient == recipient &&
        m_tasks[i].chat == chat && m_tasks[i].source == task.source ) m_tasks.removeAt( i );
  m_tasks << task;
  if( save() ) return true;
  m_tasks = previous;
  if( error ) *error = QStringLiteral( "无法保存待发送任务，请检查磁盘空间和目录权限。" );
  return false;
}

bool ShadowTransferQueue::sourceUnchanged( const Task& task, QString* error )
{
  foreach( const QJsonValue& value, task.files )
  {
    const QJsonObject object = value.toObject();
    const QFileInfo info( object["path"].toString() );
    if( !info.isFile() || !info.isReadable() || info.size() != object["size"].toString().toLongLong() ||
        info.lastModified().toMSecsSinceEpoch() != object["modified"].toString().toLongLong() )
    {
      if( error ) *error = QStringLiteral( "源文件已移动、删除或修改：%1。请重新选择并发送。" ).arg( info.filePath() );
      return false;
    }
  }
  // Refuse additions too: the queued folder is the exact material selected.
  if( QFileInfo( task.source ).isDir() )
  {
    foreach( const QString& path, task.directories )
      if( !QFileInfo( path ).isDir() ) { if( error ) *error = QStringLiteral( "源文件夹结构已改变，请重新选择并发送。" ); return false; }
    QDirIterator directories( task.source, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories );
    int directoryCount = 0;
    while( directories.hasNext() ) { directories.next(); ++directoryCount; }
    if( directoryCount != task.directories.size() ) { if( error ) *error = QStringLiteral( "源文件夹结构已改变，请重新选择并发送。" ); return false; }
    int count = 0;
    QDirIterator iterator( task.source, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories );
    while( iterator.hasNext() ) { iterator.next(); ++count; }
    if( count != task.files.size() ) { if( error ) *error = QStringLiteral( "源文件夹内容已改变，请重新选择并发送。" ); return false; }
  }
  return true;
}

bool ShadowTransferQueue::update( const Task& task, bool persist )
{
  for( int i = 0; i < m_tasks.size(); ++i )
    if( m_tasks[i].id == task.id )
    {
      const Task previous = m_tasks[i]; m_tasks[i] = task;
      if( !persist ) return true;
      if( save() ) return true;
      m_tasks[i] = previous; return false;
    }
  return false;
}

bool ShadowTransferQueue::remove( const QString& id )
{
  for( int i = 0; i < m_tasks.size(); ++i )
    if( m_tasks[i].id == id )
    {
      const Task previous = m_tasks.takeAt( i );
      if( save() ) return true;
      m_tasks.insert( i, previous ); return false;
    }
  return false;
}
