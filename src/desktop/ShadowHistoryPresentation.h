#ifndef SHADOW_HISTORY_PRESENTATION_H
#define SHADOW_HISTORY_PRESENTATION_H

#include "HistoryRecord.h"
#include "ChatMessage.h"

inline QString shadowHistoryPreviewText( const HistoryRecord& record )
{
  if( record.sender.isEmpty() || !record.timestamp.isValid() ) return record.text;
  const QString spacing = QStringLiteral( "(?:\\s|&nbsp;|<br\\s*/?>|<!--[^>]*-->)*" );
  const QString important = QStringLiteral( "(?<important><font color=red><b>!!</b></font>\\s*)?" );
  const QString sender = QStringLiteral(
    "(?:<font\\s+color=[^>]+>)?<b>(?<sender>[^<]*)</b>(?:</font>)?:?" );
  const QString timestamp = QStringLiteral( "<font\\s+color=[^>]+>\\((?<time>[^<]+)\\)</font>" );
  const QStringList orders = QStringList() << timestamp + spacing + sender << sender + spacing + timestamp;
  for( const QString& order : orders )
  {
    const QRegularExpression header( QStringLiteral( "\\A" ) + spacing + important + order + spacing,
      QRegularExpression::CaseInsensitiveOption );
    const QRegularExpressionMatch match = header.match( record.html );
    if( !match.hasMatch() ) continue;
    QTextDocument name;
    name.setHtml( match.captured( QStringLiteral( "sender" ) ) );
    if( name.toPlainText() != record.sender ) continue;
    QDateTime displayed = QDateTime::fromString( match.captured( QStringLiteral( "time" ) ),
      QStringLiteral( "yyyy-MM-dd h:mm:ss" ) );
    if( !displayed.isValid() )
      displayed = QLocale( QLocale::English, QLocale::UnitedStates ).toDateTime(
        match.captured( QStringLiteral( "time" ) ), QStringLiteral( "yyyy-MM-dd h:mm:ss ap" ) );
    if( displayed.date() != record.timestamp.date() || displayed.time() != record.timestamp.time() ) continue;
    QTextDocument body;
    body.setHtml( match.captured( QStringLiteral( "important" ) ) + record.html.mid( match.capturedEnd() ) );
    return body.toPlainText().trimmed();
  }
  return record.text;
}

inline QString shadowHistoryMessagePreview( const HistoryRecord& record )
{
  if( record.media.isEmpty() ) return shadowHistoryPreviewText( record );
  if( record.type == ChatMessage::FileTransfer &&
      !record.html.contains( QStringLiteral( "<!--shadow-transfer-error-->" ) ) )
    return QStringLiteral( "[图片和视频]" );
  HistoryRecord visible = record;
  const QRegularExpression anchors( QStringLiteral(
    "<a\\b[^>]*href\\s*=\\s*[\"'](?<url>[^\"']+)[\"'][^>]*>(?<label>[\\s\\S]*?)</a>" ),
    QRegularExpression::CaseInsensitiveOption );
  QList<QRegularExpressionMatch> matches;
  QRegularExpressionMatchIterator iterator = anchors.globalMatch( visible.html );
  while( iterator.hasNext() ) matches.append( iterator.next() );
  for( int index = matches.size() - 1; index >= 0; --index )
  {
    const QRegularExpressionMatch& match = matches.at( index );
    QTextDocument label;
    label.setHtml( match.captured( QStringLiteral( "label" ) ) );
    const QUrl url( QTextDocumentFragment::fromHtml( match.captured( QStringLiteral( "url" ) ) ).toPlainText() );
    if( url.isLocalFile() && record.media.contains( url.toString() ) &&
        (match.captured( QStringLiteral( "label" ) ).contains( QRegularExpression( "<img\\b",
          QRegularExpression::CaseInsensitiveOption ) ) ||
         label.toPlainText().trimmed() == QFileInfo( url.toLocalFile() ).fileName()) )
      visible.html.remove( match.capturedStart(), match.capturedLength() );
  }
  QTextDocument text;
  text.setHtml( visible.html );
  visible.text = text.toPlainText().trimmed();
  QString preview = shadowHistoryPreviewText( visible );
  preview.remove( QChar::ObjectReplacementCharacter );
  preview = preview.trimmed();
  return preview.isEmpty() ? QStringLiteral( "[图片和视频]" )
    : QStringLiteral( "[图片和视频] " ) + preview;
}

#endif
