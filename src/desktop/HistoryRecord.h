#ifndef SHADOWCHAT_HISTORYRECORD_H
#define SHADOWCHAT_HISTORYRECORD_H
#include "Config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDataStream>
#include <QRegularExpression>
#include <QTextDocument>

struct HistoryRecord
{
  QString html, text, sender, album;
  QDateTime timestamp;
  QStringList files, media, links;
  int type = -1;
  bool hasDirection = false;
  bool outgoing = false;
};

inline QList<HistoryRecord> shadowChatHistoryRecords(const QString& html)
{
  QList<HistoryRecord> records;
  QRegularExpression marker("<!--shadow-record:([A-Za-z0-9+/=]+)-->");
  QList<int> starts, ends;
  QList<QJsonObject> metadata;
  QRegularExpressionMatchIterator it = marker.globalMatch(html);
  while(it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    starts << match.capturedStart(); ends << match.capturedEnd();
    metadata << QJsonDocument::fromJson(QByteArray::fromBase64(match.captured(1).toLatin1())).object();
  }
  // Legacy HTML has no stable message IDs. Keep every legacy block visible;
  // never invent a sender or a date when the old format omitted them.
  QStringList fragments;
  if(starts.isEmpty()) fragments = html.split(QRegularExpression("<br\\s*/?>"), QString::SkipEmptyParts);
  else {
    if(starts.first() > 0) records = shadowChatHistoryRecords(html.left(starts.first()));
    for(int i=0;i<starts.size();++i) fragments << html.mid(ends[i], (i+1<starts.size()?starts[i+1]:html.size())-ends[i]);
  }
  for(int i=0;i<fragments.size();++i) {
    HistoryRecord r; r.html=fragments[i];
    QTextDocument doc; doc.setHtml(r.html); r.text=doc.toPlainText().trimmed();
    if(r.text.isEmpty() && !r.html.contains("<img",Qt::CaseInsensitive)) continue;
    const int mi=i;
    if(!starts.isEmpty() && mi>=0 && mi<metadata.size()) {
      r.sender=metadata[mi].value("sender").toString();
      r.album=metadata[mi].value("album").toString();
      r.timestamp=QDateTime::fromString(metadata[mi].value("time").toString(), Qt::ISODate);
      r.type=metadata[mi].value("type").toInt(-1);
      r.hasDirection=metadata[mi].value("outgoing").isBool();
      r.outgoing=metadata[mi].value("outgoing").toBool();
    } else {
      QRegularExpressionMatch sender=QRegularExpression("<b>([^<]+)</b>").match(r.html);
      if(sender.hasMatch()) { QTextDocument name; name.setHtml(sender.captured(1)); r.sender=name.toPlainText(); }
      QRegularExpressionMatch date=QRegularExpression("(\\d{4}-\\d{2}-\\d{2})[ T](\\d{1,2}:\\d{2}:\\d{2})").match(r.text);
      if(date.hasMatch()) r.timestamp=QDateTime::fromString(date.captured(1)+" "+date.captured(2),"yyyy-MM-dd h:mm:ss");
    }
    if(r.album.isEmpty()) {
      const QRegularExpressionMatch album=QRegularExpression("<!--shadow-album:([{}A-Za-z0-9-]+)-->").match(r.html);
      if(album.hasMatch())r.album=album.captured(1);
    }
    QStringList imageSources;
    QRegularExpression urls("(href|src)\\s*=\\s*[\"']([^\"']+)[\"']",QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator uit=urls.globalMatch(r.html);
    while(uit.hasNext()) {
      const QRegularExpressionMatch target=uit.next();
      QTextDocument decoded; decoded.setHtml(target.captured(2));
      QString address=decoded.toPlainText(); QUrl url(address);
      if(!url.isLocalFile() && QFileInfo(address).isAbsolute()) {url=QUrl::fromLocalFile(address);address=url.toString();}
      const bool source=target.captured(1).compare("src",Qt::CaseInsensitive)==0;
      if(source) {
        if(url.isLocalFile()) imageSources<<address;
        continue;
      }
      if(url.scheme()=="http" || url.scheme()=="https") r.links << address;
      else if(url.isLocalFile()) {
        const QString extension=QFileInfo(url.toLocalFile()).suffix().toLower();
        if(QStringList({"jpg","jpeg","png","gif","bmp","webp","mp4","m4v","webm","mkv",
          "avi","mov","wmv","mpg","mpeg","m2v","m2ts","mts","ts","3gp","3g2","ogv","flv"}).contains(extension)) r.media << address;
        else r.files << address;
      }
    }
    if(r.media.isEmpty())r.media=imageSources;
    QRegularExpressionMatchIterator plain=QRegularExpression("https?://[^\\s<>\"]+").globalMatch(r.text);
    while(plain.hasNext()) r.links << plain.next().captured();
    r.links.removeDuplicates();r.files.removeDuplicates();r.media.removeDuplicates();
    records << r;
  }
  return records;
}

inline QStringList shadowHistoryRecordKeys(const QList<HistoryRecord>& records)
{
  QStringList keys;
  QHash<QString,int> occurrences;
  for(const HistoryRecord& record:records)
  {
    QByteArray identity;
    QDataStream stream(&identity,QIODevice::WriteOnly);
    stream<<record.sender<<record.timestamp<<record.album<<record.html;
    const QString digest=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    const int occurrence=occurrences.value(digest);
    occurrences.insert(digest,occurrence+1);
    keys.append(digest+QLatin1Char(':')+QString::number(occurrence));
  }
  return keys;
}

inline QString shadowHistoryRecordHtml(const HistoryRecord& record,const QString& key)
{
  QJsonObject metadata;
  metadata.insert("sender",record.sender);
  if(record.timestamp.isValid())metadata.insert("time",record.timestamp.toString(Qt::ISODate));
  if(!record.album.isEmpty())metadata.insert("album",record.album);
  if(record.type>=0)metadata.insert("type",record.type);
  if(record.hasDirection)metadata.insert("outgoing",record.outgoing);
  return QStringLiteral("<!--shadow-record:%1-->%2%3").arg(
    QString::fromLatin1(QJsonDocument(metadata).toJson(QJsonDocument::Compact).toBase64()),
    key.isEmpty()?QString():QStringLiteral("<!--shadow-context:%1-->").arg(key),record.html);
}
#endif
