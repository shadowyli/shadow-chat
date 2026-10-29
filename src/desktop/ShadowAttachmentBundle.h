#ifndef SHADOWATTACHMENTBUNDLE_H
#define SHADOWATTACHMENTBUNDLE_H
#include <QtCore>
// Chunked compressed envelope around a legacy workspace. Bounded memory even
// for multi-GB attachments; extracted paths are never supplied by the sender.
namespace ShadowAttachmentBundle {
const quint32 Magic=0x53434232;
const int Chunk=1024*1024;
inline bool isBundle(const QString& path) { QFile f(path); if(!f.open(QIODevice::ReadOnly))return false; QDataStream s(&f);quint32 m=0;s>>m;return m==Magic; }
inline bool safeName(const QString& name) {
 if(name.isEmpty() || name.size()>1000 || QDir::isAbsolutePath(name) || name.contains('\\') || name.contains(':'))return false;
 for(const QString& part:name.split('/'))if(part.isEmpty() || part=="." || part==".." || part.endsWith('.') || part.endsWith(' '))return false;
 return true;
}
inline qint64 estimate(const QString& root) {
 qint64 size=0;QDirIterator it(root,QDir::Files|QDir::NoSymLinks,QDirIterator::Subdirectories);
 while(it.hasNext()){it.next();size+=it.fileInfo().size();}return size;
}
inline bool write(const QString& core,const QString& root,const QString& target,QString* error) {
 auto fail=[&](const QString& m){if(error)*error=m;return false;};
 QFile coreFile(core);if(!coreFile.open(QIODevice::ReadOnly))return fail(QStringLiteral("无法读取账号备份。"));
 QStringList paths;QDirIterator it(root,QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDirIterator::Subdirectories);
 while(it.hasNext()){const QString p=it.next();if(QFileInfo(p).isSymLink())continue;if(QFileInfo(p).canonicalFilePath()==QFileInfo(target).canonicalFilePath())continue;paths<<p;if(paths.size()>100000)return fail(QStringLiteral("附件数量超过 100000，请分开迁移。"));}
 QSaveFile output(target);if(!output.open(QIODevice::WriteOnly))return fail(QStringLiteral("无法创建备份文件。"));
 QDataStream s(&output);s.setVersion(QDataStream::Qt_5_6);s<<Magic<<quint16(2)<<coreFile.readAll()<<QDir(root).absolutePath()<<quint32(paths.size());
 for(const QString& path:paths){
  const QFileInfo info(path);const QString name=QDir(root).relativeFilePath(path);if(!safeName(name))return fail(QStringLiteral("附件路径无法安全打包：%1").arg(name));
  s<<name<<info.isDir()<<quint64(info.isDir()?0:info.size());if(info.isDir())continue;
  QFile f(path);if(!f.open(QIODevice::ReadOnly))return fail(QStringLiteral("无法读取附件：%1").arg(name));
  QCryptographicHash hash(QCryptographicHash::Sha256);qint64 count=0;
  while(!f.atEnd()){const QByteArray data=f.read(Chunk);if(data.isEmpty())return fail(QStringLiteral("附件读取失败：%1").arg(name));count+=data.size();hash.addData(data);s<<qCompress(data,3);if(s.status()!=QDataStream::Ok)return fail(QStringLiteral("磁盘空间不足或备份写入失败。"));}
  if(count!=info.size() || QFileInfo(path).lastModified()!=info.lastModified())return fail(QStringLiteral("附件在备份过程中发生变化，请重试：%1").arg(name));
  s<<QByteArray()<<hash.result();
 }
 return s.status()==QDataStream::Ok && output.commit() ? true : fail(QStringLiteral("备份写入失败，旧备份未改变。"));
}
inline bool unpack(const QString& path,const QString& core,const QString& destination,QString* sourceRoot,QString* error) {
 auto fail=[&](const QString& m){if(error)*error=m;return false;};
 QFile f(path);if(!f.open(QIODevice::ReadOnly))return fail(QStringLiteral("无法打开备份。"));
 QDataStream s(&f);s.setVersion(QDataStream::Qt_5_6);quint32 magic;quint16 version; s>>magic>>version;
 if(magic!=Magic || version!=2)return fail(QStringLiteral("不支持的附件备份格式。"));
 // Read byte arrays with a length cap before allocating, including malformed input.
 auto bounded=[&](QByteArray* bytes,quint32 limit){quint32 size;s>>size;if(size==0xffffffffU){bytes->clear();return s.status()==QDataStream::Ok;}if(s.status()!=QDataStream::Ok || size>limit || qint64(size)>f.bytesAvailable())return false;bytes->resize(int(size));return s.readRawData(bytes->data(),int(size))==int(size);};
 QByteArray data;if(!bounded(&data,260U*1024*1024))return fail(QStringLiteral("账号备份大小异常。"));
 if(!core.isEmpty()){QSaveFile c(core);if(!c.open(QIODevice::WriteOnly)||c.write(data)!=data.size()||!c.commit())return fail(QStringLiteral("无法暂存账号备份。"));}data.clear();
 QString source;quint32 count;s>>source>>count;if(source.size()>2000 || count>100000)return fail(QStringLiteral("附件清单异常。"));if(sourceRoot)*sourceRoot=source;
 QSet<QString> seen;quint64 total=0;
 for(quint32 i=0;i<count;++i){QString name;bool directory;quint64 size;s>>name>>directory>>size;
  if(s.status()!=QDataStream::Ok || !safeName(name)||seen.contains(name.toCaseFolded())||size>1024ULL*1024*1024*1024 || total+size>1024ULL*1024*1024*1024)return fail(QStringLiteral("附件路径、数量或大小异常。"));
  seen.insert(name.toCaseFolded());total+=size;QString target;
  if(!destination.isEmpty()) {target=QDir(destination).filePath(name);if(!QDir().mkpath(directory?target:QFileInfo(target).absolutePath()))return fail(QStringLiteral("无法创建附件目录。"));}
  if(directory){if(size)return fail(QStringLiteral("附件目录数据异常。"));continue;}
  QSaveFile output(target);if(!target.isEmpty()&&!output.open(QIODevice::WriteOnly))return fail(QStringLiteral("无法恢复附件。"));
  QCryptographicHash hash(QCryptographicHash::Sha256);quint64 written=0;
  for(;;){QByteArray compressed;if(!bounded(&compressed,Chunk+65536))return fail(QStringLiteral("附件压缩块异常。"));if(compressed.isEmpty())break;
   if(compressed.size()<4)return fail(QStringLiteral("附件压缩块损坏。"));
   const uchar* b=reinterpret_cast<const uchar*>(compressed.constData());const quint32 length=(quint32(b[0])<<24)|(quint32(b[1])<<16)|(quint32(b[2])<<8)|b[3];
   if(!length||length>Chunk||written+length>size)return fail(QStringLiteral("附件解压大小异常。"));
   const QByteArray plain=qUncompress(compressed);if(plain.size()!=int(length))return fail(QStringLiteral("附件解压失败。"));
   hash.addData(plain);written+=length;if(!target.isEmpty()&&output.write(plain)!=plain.size())return fail(QStringLiteral("附件写入失败，请检查剩余空间。"));
  }
  QByteArray expected;if(!bounded(&expected,32)||expected.size()!=32||written!=size||hash.result()!=expected)return fail(QStringLiteral("附件完整性校验失败。"));
  if(!target.isEmpty()&&!output.commit())return fail(QStringLiteral("附件保存失败。"));
 }
 return s.status()==QDataStream::Ok&&f.atEnd()?true:fail(QStringLiteral("备份文件尾部异常。"));
}
}
#endif
