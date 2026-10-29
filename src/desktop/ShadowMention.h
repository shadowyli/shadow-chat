#ifndef SHADOWMENTION_H
#define SHADOWMENTION_H
#include <QtCore>
inline bool shadowMentionsUser(const QString& text,const QString& name) {
 if(name.trimmed().isEmpty())return false;
 return QRegularExpression(QStringLiteral("(?:^|\\s)@%1(?=$|\\s|[，。！？、,:;!?])").arg(QRegularExpression::escape(name))).match(text).hasMatch();
}
#endif
