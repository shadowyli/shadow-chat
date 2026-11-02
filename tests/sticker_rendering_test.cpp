#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

class StickerRenderProbe : public GuiChat
{
public:
  using GuiChat::modernImageAlbumHtml;
  using GuiChat::modernHistoryHtml;
};

static QString mediaHtml(const QString& original,const QString& preview,bool sticker)
{
  return QString(sticker?"<!--shadow-sticker--><a href=\"%1\"><img src=\"%2\"></a>":
    "<a href=\"%1\"><img src=\"%2\"></a>").arg(
      QUrl::fromLocalFile(original).toString().toHtmlEscaped(),
      QUrl::fromLocalFile(preview).toString().toHtmlEscaped());
}

static QList<QTextImageFormat> mediaFormats(QTextDocument* document)
{
  QList<QTextImageFormat> result;
  for(QTextBlock block=document->begin();block.isValid();block=block.next())
    for(QTextBlock::iterator iterator=block.begin();!iterator.atEnd();++iterator)
    {
      const QTextFragment fragment=iterator.fragment();
      if(!fragment.charFormat().isImageFormat())continue;
      const QTextImageFormat image=fragment.charFormat().toImageFormat();
      if(!image.name().contains("chat-avatar-")&&!image.name().startsWith("shadowtransfer:"))
        result.append(image);
    }
  return result;
}

int main(int argc,char** argv)
{
  BeeApplication application(argc,argv);
  application.setFont(ShadowStyle::applicationFont());
  Random::init();
  QTemporaryDir folder;
  CHECK(folder.isValid());
  Settings& settings=Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser("sticker-render-fixture");
  settings.setAccountDataFolder(folder.path());
  settings.load();
  Core core;
  QImage image(480,240,QImage::Format_RGB32);
  image.fill(Qt::red);
  const QString ordinary=folder.filePath("photo.png");
  const QString largeSticker=folder.filePath(QString(64,'a')+".png");
  const QString tinySticker=folder.filePath(QString(64,'b')+".png");
  const QString cachedPreview=folder.filePath("old-static-preview.png");
  CHECK(image.save(ordinary)&&image.save(largeSticker)&&image.save(cachedPreview));
  CHECK(image.scaled(24,12).save(tinySticker));
  const QString animated=folder.filePath(QString(64,'c')+".gif");
  const QByteArray gifBytes=QByteArray::fromBase64(
    "R0lGODlhGAAMAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQACgAAACwAAAAAGAAMAAAIHgABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFiwwDAgAh+QQBCgABACwAAAAAGAAMAIEAAP8AAAAAAAAAAAAIHgABCBxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFiwwDAgA7");
  QFile gifFile(animated);
  CHECK(gifFile.open(QIODevice::WriteOnly)&&gifFile.write(gifBytes)==gifBytes.size());
  gifFile.close();
  QToolBar toolbar;
  StickerRenderProbe chat;
  chat.setupToolBar(&toolbar);
  chat.setModernMode(true);
  chat.resize(1050,760);
  chat.show();
  application.processEvents();
  auto* transcript=chat.findChild<ShadowChatTranscript*>("GuiChatViewer");
  CHECK(transcript);
  const ChatMessage message(ID_LOCAL_USER,Protocol::instance().systemMessage(QString()),ChatMessage::ImagePreview,true);
  const QStringList bodies{
    mediaHtml(ordinary,ordinary,false),
    mediaHtml(largeSticker,largeSticker,true),
    mediaHtml(tinySticker,tinySticker,true),
    mediaHtml(animated,cachedPreview,true)
  };
  QString combined;
  for(const QString& body:bodies)combined+=chat.modernImageAlbumHtml(QStringList{body},message);
  transcript->setHtml(combined);
  QTest::qWait(80);
  QList<QTextImageFormat> formats=mediaFormats(transcript->document());
  CHECK(formats.size()==4);
  CHECK(formats.at(0).width()>120&&formats.at(0).width()<=360);
  CHECK(formats.at(1).width()<=120&&formats.at(1).height()<=120);
  CHECK(formats.at(2).width()==24&&formats.at(2).height()==12);
  CHECK(QUrl(formats.at(3).name()).toLocalFile()==animated);
  CHECK(formats.at(3).width()==24&&formats.at(3).height()==12);
  CHECK(!transcript->toPlainText().contains(QString(64,'a')));
  QJsonObject metadata;
  metadata.insert("type",int(ChatMessage::ImagePreview));
  metadata.insert("outgoing",true);
  metadata.insert("time","2026-11-02T16:23:00");
  const QString archived=QString("<!--shadow-record:%1-->%2").arg(
    QString::fromLatin1(QJsonDocument(metadata).toJson(QJsonDocument::Compact).toBase64()),
    mediaHtml(animated,cachedPreview,true));
  transcript->setHtml(chat.modernHistoryHtml(archived));
  QTest::qWait(80);
  formats=mediaFormats(transcript->document());
  CHECK(formats.size()==1&&QUrl(formats.first().name()).toLocalFile()==animated);
  CHECK(formats.first().width()<=120&&formats.first().height()<=120);
  CHECK(gifFile.open(QIODevice::ReadOnly)&&gifFile.readAll()==gifBytes);
  std::puts("PASS sticker rendering: 120px limit, aspect ratio, tiny images not enlarged, ordinary photo independent, legacy static GIF source recovered, history restored, no filename and unchanged source");
  return 0;
}
