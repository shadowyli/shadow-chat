#ifndef SHADOWCHAT_HISTORYRECORD_H
#define SHADOWCHAT_HISTORYRECORD_H
#include "Config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextDocument>

struct HistoryRecord
{
  QString html, text, sender, album;
  QDateTime timestamp;
  QStringList files, media, links;
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
        if(QStringList({"jpg","jpeg","png","gif","bmp","webp","mp4","mov","avi","mkv","wmv"}).contains(extension)) r.media << address;
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
#endif
