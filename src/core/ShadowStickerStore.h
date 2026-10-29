#ifndef SHADOWSTICKERSTORE_H
#define SHADOWSTICKERSTORE_H
#include <QtCore>

// Account-local original image library. ZIP contents are never extracted by
// their supplied paths, and imported bytes are never re-encoded.
class ShadowStickerStore
{
public:
  struct Sticker { QString id, path, name, format; };
  explicit ShadowStickerStore( const QString& accountFolder );
  QList<Sticker> list() const;
  bool importZip( const QString&, QString* error = Q_NULLPTR, int* added = Q_NULLPTR, int* duplicates = Q_NULLPTR );
  bool addFile( const QString&, QString* error = Q_NULLPTR );
  bool remove( const QString& id, QString* error = Q_NULLPTR );
private:
  QString m_folder;
  bool load( QList<Sticker>*, QString* ) const;
  bool save( const QList<Sticker>&, QString* ) const;
  bool addImages( const QList<QPair<QString,QByteArray> >&, QString*, int*, int* );
};
#endif
