#ifndef SHADOWRICHTEXT_H
#define SHADOWRICHTEXT_H

#include <QtGui>

namespace ShadowRichText
{
inline QString plainText(const QString& html)
{
  QTextDocument document;
  document.setHtml(html);
  QString text=document.toPlainText();
  text.remove(QChar::ObjectReplacementCharacter);
  return text.simplified();
}

inline QString messageText(const QString& html)
{
  QTextDocument document;
  document.setHtml(html);
  QString text=document.toPlainText();
  text.remove(QChar::ObjectReplacementCharacter);
  text.replace(QChar::Nbsp,QLatin1Char(' '));
  QStringList lines=text.split(QLatin1Char('\n'));
  for(QString& line:lines)line=line.trimmed();
  return lines.join(QLatin1Char('\n'));
}

inline QString serialize(const QTextDocument* document,bool* formatted=Q_NULLPTR)
{
  bool rich=false;
  QString html;
  QTextList* currentList=Q_NULLPTR;
  for(QTextBlock block=document->begin();block.isValid();block=block.next())
  {
    QTextList* list=block.textList();
    if(currentList!=list)
    {
      if(currentList)html+=currentList->format().style()==QTextListFormat::ListDecimal?"</ol>":"</ul>";
      currentList=list;
      if(list){html+=list->format().style()==QTextListFormat::ListDecimal?"<ol>":"<ul>";rich=true;}
    }
    const int heading=qBound(0,block.blockFormat().headingLevel(),6);
    const bool quote=!list&&(block.blockFormat().indent()>0||block.blockFormat().leftMargin()>0);
    if(quote){html+="<blockquote>";rich=true;}
    const QString tag=list?"li":heading?QString("h%1").arg(heading):"p";
    if(heading)rich=true;
    html+="<"+tag+">";
    for(QTextBlock::iterator iterator=block.begin();!iterator.atEnd();++iterator)
    {
      const QTextFragment fragment=iterator.fragment();
      const QTextCharFormat format=fragment.charFormat();
      if(format.isImageFormat())continue;
      QString text=fragment.text().toHtmlEscaped();
      text.replace(QChar::LineSeparator,"<br>");
      const bool bold=format.fontWeight()>=QFont::Bold;
      if(bold){text="<b>"+text+"</b>";rich=true;}
      if(format.fontItalic()){text="<i>"+text+"</i>";rich=true;}
      if(format.fontUnderline()){text="<u>"+text+"</u>";rich=true;}
      if(format.fontStrikeOut()){text="<s>"+text+"</s>";rich=true;}
      if(format.fontFixedPitch()||format.fontFamily().contains("Consolas")||format.fontFamily().contains("monospace",Qt::CaseInsensitive))
      {text="<code>"+text+"</code>";rich=true;}
      QString style;
      if(format.foreground().style()!=Qt::NoBrush&&format.foreground().color()!=QColor(Qt::black))
        style+="color:"+format.foreground().color().name()+";";
      if(format.background().style()!=Qt::NoBrush&&format.background().color().alpha()>0)
        style+="background-color:"+format.background().color().name()+";";
      if(!style.isEmpty()){text="<span style=\""+style+"\">"+text+"</span>";rich=true;}
      if(format.isAnchor())
      {
        const QUrl url(format.anchorHref());
        if(url.isValid()&&(url.scheme()=="http"||url.scheme()=="https"||url.scheme()=="mailto"))
        {text="<a href=\""+url.toString(QUrl::FullyEncoded).toHtmlEscaped()+"\">"+text+"</a>";rich=true;}
      }
      html+=text;
    }
    html+="</"+tag+">";
    if(quote)html+="</blockquote>";
  }
  if(currentList)html+=currentList->format().style()==QTextListFormat::ListDecimal?"</ol>":"</ul>";
  if(formatted)*formatted=rich;
  return html;
}

inline QString sanitize(const QString& html)
{
  if(html.size()>256*1024)return QString();
  QTextDocument document;
  document.setHtml(html);
  return serialize(&document);
}
}
#endif
