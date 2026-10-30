#include "HistoryRecord.h"
#include "ShadowImageViewer.h"
#include "ShadowRegionCapture.h"
#include "ShadowImageAlbum.h"
#include <QApplication>
#undef NDEBUG
#include <cassert>

static QString record(const QString& sender,const QString& time,const QString& html)
{
  QJsonObject value;value.insert("sender",sender);value.insert("time",time);
  return "<!--shadow-record:"+QString::fromLatin1(QJsonDocument(value).toJson(QJsonDocument::Compact).toBase64())+"-->"+html;
}
int main(int argc,char** argv)
{
  QApplication app(argc,argv);
  QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
  app.setFont(QFont("Microsoft YaHei",10));
  QString html=record("Alice","2026-10-28T12:30:00","<b>Alice</b>: hello<br>second line<br>");
  html+=record("Bob","2026-10-27T11:20:00","<a href=\"file:///C:/素材/test.psd\">test.psd</a><br>");
  html+=record("Bob","2026-10-27T11:21:00","<a href=\"file:///C:/素材/test.png\"><img src=\"file:///C:/素材/test.png\"></a><br>");
  html+=record("Alice","2026-10-28T12:31:00","<a href=\"https://example.test/tools?a=1&amp;b=2\">链接示例</a><br>");
  QList<HistoryRecord> records=shadowChatHistoryRecords(html);
  assert(records.size()==4);
  assert(records[0].sender=="Alice" && records[0].timestamp.date()==QDate(2026,10,28));
  assert(records[0].text.contains("second line"));
  assert(records[1].files.size()==1 && records[1].media.isEmpty());
  assert(records[2].media.size()==1 && records[2].files.isEmpty());
  assert(records[3].links.size()==1 && records[3].links[0].contains("&b=2"));
  QList<HistoryRecord> legacy=shadowChatHistoryRecords("<b>Old</b>: legacy<br>"+html);
  assert(legacy.size()==5 && legacy[0].sender=="Old" && !legacy[0].timestamp.isValid());
  assert(shadowChatHistoryRecords("<br><br>").isEmpty());
  const QString albumHtml="<a href=\"file:///C:/art/original.png\"><img src=\"file:///C:/cache/thumbnail.png\"></a><a href=\"file:///C:/art/second.jpg\"><img src=\"file:///C:/cache/second-thumb.jpg\"></a>";
  const QStringList originals=shadowAlbumOriginals(albumHtml);
  assert(originals.size()==2 && !originals.join(" ").contains("thumbnail"));
  const QUrl albumUrl=shadowAlbumUrl(originals,1);
  assert(shadowAlbumPaths(albumUrl)==originals && albumUrl.fragment()=="1");
  assert(ShadowImageViewer::supports("image.PNG"));
  assert(!ShadowImageViewer::supports("image.psd"));
  QTemporaryDir temporary;assert(temporary.isValid());
  const QString imagePath=temporary.filePath("preview.png");
  QImage image(320,240,QImage::Format_ARGB32);image.fill(QColor(50,130,230,180));assert(image.save(imagePath));
  ShadowImageViewer viewer(QStringList()<<imagePath,imagePath);
  viewer.show();app.processEvents();
  assert(viewer.findChildren<QPushButton*>().size()==7);
  assert(viewer.findChildren<QGraphicsView*>().size()==1);
  assert(viewer.findChild<QLabel*>()->text().contains("320"));
  QPixmap proof(viewer.size());viewer.render(&proof);
  assert(proof.save("E:/ShadowChat/delivery/image-viewer-proof.png"));
  ShadowRegionCapture capture(QPixmap::fromImage(image));
  QMouseEvent press(QEvent::MouseButtonPress,QPointF(10,20),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
  QMouseEvent move(QEvent::MouseMove,QPointF(110,100),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease,QPointF(110,100),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
  QApplication::sendEvent(&capture,&press);QApplication::sendEvent(&capture,&move);QApplication::sendEvent(&capture,&release);
  assert(capture.result()==QDialog::Accepted && capture.selectedImage().size()==QSize(101,81));
  assert(capture.selectedImage().toImage()==QPixmap::fromImage(image).toImage().copy(QRect(QPoint(10,20),QPoint(110,100))));
  ShadowRegionCapture canceled(QPixmap::fromImage(image));QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&canceled,&escape);assert(canceled.result()==QDialog::Rejected);
  qInfo("History record tests passed: modern boundaries, metadata, file/media/link classification, legacy compatibility.");
  return 0;
}
