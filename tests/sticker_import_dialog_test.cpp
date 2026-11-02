#include "GuiStickerPicker.h"
#include "ShadowStyle.h"
#include <QtTest>
#include <QtWidgets>
#include <QtGui/private/qzipwriter_p.h>
#include <functional>

#ifndef SHADOWCHAT_VERIFY_PRODUCTION_OBJECTS
namespace ShadowStyle
{
QIcon actionIcon(const QString&) { return QIcon(); }
}
#endif

class TestStickerPicker : public GuiStickerPicker
{
public:
  explicit TestStickerPicker(const QString& folder):GuiStickerPicker(folder){}
  std::function<int(QFileDialog&)> execute;
protected:
  int execArchiveDialog(QFileDialog& dialog) override { return execute(dialog); }
  QString defaultArchiveDirectory() const override { return accountFolder(); }
};

class StickerImportDialogTest : public QObject
{
  Q_OBJECT
private slots:
  void preferredDirectories()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    QDir folder(workspace.path());
    const QStringList directories=QStringList()<<"previous"<<"Downloads"<<"Documents"<<"home";
    for(const QString& name:directories)
      QVERIFY(folder.mkdir(name));
    const QString previous=folder.filePath("previous");
    const QString downloads=folder.filePath("Downloads");
    const QString documents=folder.filePath("Documents");
    const QString home=folder.filePath("home");
    const QString missing=folder.filePath("missing");
    QFile regularFile(folder.filePath("regular-file"));
    QVERIFY(regularFile.open(QIODevice::WriteOnly));
    regularFile.close();
    QCOMPARE(GuiStickerPicker::preferredArchiveDirectory(previous,downloads,documents,home),previous);
    QCOMPARE(GuiStickerPicker::preferredArchiveDirectory(QString(),downloads,documents,home),downloads);
    QCOMPARE(GuiStickerPicker::preferredArchiveDirectory(missing,missing,documents,home),documents);
    QCOMPARE(GuiStickerPicker::preferredArchiveDirectory(regularFile.fileName(),missing,documents,home),documents);
    QCOMPARE(GuiStickerPicker::preferredArchiveDirectory(QString(),QString(),QString(),home),home);
    QVERIFY(GuiStickerPicker::preferredArchiveDirectory(missing,missing,missing,missing).isEmpty());
    QVERIFY(folder.rmdir("previous"));
    QCOMPARE(GuiStickerPicker::preferredArchiveDirectory(previous,downloads,documents,home),downloads);
  }

  void dialogPolicyAndCancel()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    const QString settingsPath=workspace.filePath("sticker-panel.ini");
    QSettings preferences(settingsPath,QSettings::IniFormat);
    preferences.setValue("importDirectory",workspace.path());
    preferences.setValue("recent",QStringList()<<"existing-favorite");
    preferences.sync();
    QFile settingsFile(settingsPath);
    QVERIFY(settingsFile.open(QIODevice::ReadOnly));
    const QByteArray before=settingsFile.readAll();
    settingsFile.close();
    TestStickerPicker picker(workspace.path());
    bool called=false;
    picker.execute=[&](QFileDialog& dialog) {
      called=true;
      const int result=inspectDialog(dialog,workspace.path());
      QDir().mkpath(workspace.filePath("cancelled-navigation"));
      dialog.setDirectory(workspace.filePath("cancelled-navigation"));
      return result;
    };
    picker.chooseArchive();
    QVERIFY(called);
    QVERIFY(settingsFile.open(QIODevice::ReadOnly));
    QCOMPARE(settingsFile.readAll(),before);
    QVERIFY(!QFileInfo::exists(workspace.filePath("stickers")));
  }

  void selectionPersistsAcrossPickers()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    const QString directory=workspace.filePath(QStringLiteral("中文 ZIP 文件夹"));
    QVERIFY(QDir().mkpath(directory));
    const QString archive=QDir(directory).filePath(QStringLiteral("合成表情.zip"));
    QFile fixture(archive);
    QVERIFY(fixture.open(QIODevice::WriteOnly));
    QCOMPARE(fixture.write("invalid ZIP fixture"),qint64(19));
    fixture.close();
    {
      TestStickerPicker picker(workspace.path());
      picker.execute=[&](QFileDialog& dialog) {
        dialog.setOption(QFileDialog::DontUseNativeDialog,true);
        dialog.selectFile(archive);
        return QDialog::Accepted;
      };
      picker.chooseArchive();
      QVERIFY(picker.findChild<QLabel*>("StickerStatus")->text().contains(QStringLiteral("ZIP")));
      QVERIFY(!QFileInfo::exists(workspace.filePath("stickers")));
    }
    QSettings preferences(workspace.filePath("sticker-panel.ini"),QSettings::IniFormat);
    QCOMPARE(preferences.value("importDirectory").toString(),directory);
    TestStickerPicker reopened(workspace.path());
    bool called=false;
    reopened.execute=[&](QFileDialog& dialog) {
      called=true;
      return inspectDialog(dialog,directory);
    };
    reopened.chooseArchive();
    QVERIFY(called);
    QCOMPARE(preferences.value("importDirectory").toString(),directory);
  }

  void missingSelectionDoesNotReplaceDirectory()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    QSettings preferences(workspace.filePath("sticker-panel.ini"),QSettings::IniFormat);
    preferences.setValue("importDirectory",workspace.path());
    preferences.sync();
    TestStickerPicker picker(workspace.path());
    picker.execute=[&](QFileDialog& dialog) {
      dialog.setOption(QFileDialog::DontUseNativeDialog,true);
      dialog.selectFile(workspace.filePath("missing/archive.zip"));
      return QDialog::Accepted;
    };
    picker.chooseArchive();
    preferences.sync();
    QCOMPARE(preferences.value("importDirectory").toString(),workspace.path());
  }

  void firstCancelDoesNotCreatePreferences()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    TestStickerPicker picker(workspace.path());
    picker.execute=[](QFileDialog&) { return QDialog::Rejected; };
    picker.chooseArchive();
    QVERIFY(!QFileInfo::exists(workspace.filePath("sticker-panel.ini")));
    QVERIFY(!QFileInfo::exists(workspace.filePath("stickers")));
  }

  void anchoredPopupAndDismissal()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    QWidget owner;
    owner.resize(700,500);
    owner.move(20,20);
    QToolButton anchor(&owner);
    anchor.setGeometry(200,440,30,30);
    owner.show();
    QCoreApplication::processEvents();
    GuiStickerPicker picker(workspace.path(),&owner);
    picker.showAbove(&anchor);
    QCoreApplication::processEvents();
    QVERIFY(picker.isVisible());
    QVERIFY(!picker.isModal());
    QVERIFY(picker.windowFlags().testFlag(Qt::Popup));
    QCOMPARE(picker.x(),anchor.mapToGlobal(QPoint()).x());
    QCOMPARE(picker.y()+picker.height()+6,anchor.mapToGlobal(QPoint()).y());
    QVERIFY(!picker.findChild<QLabel*>("StickerHeading"));
    QTest::keyClick(picker.findChild<QListWidget*>("StickerGrid"),Qt::Key_Escape);
    QVERIFY(!picker.isVisible());
    picker.showAbove(&anchor);
    QTest::mouseClick(&owner,Qt::LeftButton,Qt::NoModifier,QPoint(5,5));
    QVERIFY(!picker.isVisible());
    picker.showAbove(&anchor);
    anchor.move(220,450);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(picker.x(),anchor.mapToGlobal(QPoint()).x());
    QCOMPARE(picker.y()+picker.height()+6,anchor.mapToGlobal(QPoint()).y());
    anchor.move(owner.width()-anchor.width()-5,450);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    const QRect screen=anchor.screen()->availableGeometry();
    QVERIFY(picker.x()>=screen.left()+8);
    QVERIFY(picker.geometry().right()<=screen.right()-8);
    owner.hide();
    QVERIFY(!picker.isVisible());
  }

  void zipSendRemovePreservesOriginal()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    const QByteArray image=animatedGif();
    const QString original=workspace.filePath("original.gif");
    QFile fixture(original);
    QVERIFY(fixture.open(QIODevice::WriteOnly));
    QCOMPARE(fixture.write(image),qint64(image.size()));
    fixture.close();
    const QString archive=workspace.filePath("synthetic.zip");
    QZipWriter writer(archive);
    writer.addFile("pack/sticker.gif",image);
    writer.close();
    QCOMPARE(writer.status(),QZipWriter::NoError);
    TestStickerPicker picker(workspace.path());
    picker.execute=[&](QFileDialog& dialog) {
      dialog.setOption(QFileDialog::DontUseNativeDialog,true);
      dialog.selectFile(archive);
      return QDialog::Accepted;
    };
    picker.chooseArchive();
    QListWidget* grid=picker.findChild<QListWidget*>("StickerGrid");
    QCOMPARE(grid->count(),2);
    QVERIFY(grid->item(1)->text().isEmpty());
    QVERIFY(!grid->item(1)->toolTip().contains("sticker"));
    QSignalSpy sent(&picker,&GuiStickerPicker::stickerSelected);
    const QString saved=grid->item(1)->data(Qt::UserRole+1).toString();
    QVERIFY(saved!=original);
    picker.activateSticker(grid->item(1));
    QCOMPARE(sent.count(),1);
    QCOMPARE(sent.first().first().toString(),saved);
    QVERIFY(!picker.isVisible());
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    QCOMPARE(fixture.readAll(),image);
    fixture.close();
    picker.show();
    picker.findChild<QAction*>("StickerManageFavorites")->trigger();
    grid->setCurrentItem(grid->item(1));
    picker.findChild<QPushButton*>("StickerRemove")->click();
    QCOMPARE(grid->count(),1);
    QVERIFY(!QFileInfo::exists(saved));
    QVERIFY(QFileInfo::exists(original));
    QToolButton* add=picker.findChild<QToolButton*>("StickerAddButton");
    QVERIFY(add&&add->menu());
    QVERIFY(add->menu()->actions().contains(picker.findChild<QAction*>("StickerImportZip")));
  }

  void visibleGifPlaybackAndLargeLibrary()
  {
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());
    QVERIFY(createLibrary(workspace.path(),512));
    QElapsedTimer elapsed;
    elapsed.start();
    GuiStickerPicker picker(workspace.path());
    QListWidget* grid=picker.findChild<QListWidget*>("StickerGrid");
    QCOMPARE(grid->count(),513);
    QVERIFY(picker.findChildren<QMovie*>().isEmpty());
    picker.findChild<QLineEdit*>("StickerSearch")->setText("synthetic-511");
    QCOMPARE(grid->count(),2);
    QVERIFY(grid->item(1)->text().isEmpty());
    QVERIFY(!grid->item(1)->toolTip().contains("synthetic-511"));
    picker.findChild<QLineEdit*>("StickerSearch")->clear();
    QCOMPARE(grid->count(),513);
    picker.show();
    QTest::qWait(80);
    auto movies=picker.findChildren<QMovie*>("StickerVisibleMovie");
    QVERIFY(!movies.isEmpty());
    QVERIFY(movies.size()<=12);
    QMovie* first=movies.first();
    QSignalSpy frames(first,&QMovie::frameChanged);
    QSignalSpy modelChanges(grid->model(),&QAbstractItemModel::dataChanged);
    QTRY_VERIFY(frames.count()>=2);
    QCOMPARE(modelChanges.count(),0);
    QCOMPARE(first->cacheMode(),QMovie::CacheNone);
    const QPointer<QMovie> oldMovie=first;
    grid->scrollToBottom();
    QTRY_VERIFY(oldMovie.isNull());
    QVERIFY(!picker.findChildren<QMovie*>("StickerVisibleMovie").isEmpty());
    QVERIFY(picker.findChildren<QMovie*>("StickerVisibleMovie").size()<=12);
    for(int step=0;step<40;++step)
    {
      picker.resize(280+(step%5)*70,180+(step%4)*40);
      grid->verticalScrollBar()->setValue((step%2)?grid->verticalScrollBar()->maximum():0);
      QCoreApplication::processEvents();
    }
    QTest::qWait(50);
    QVERIFY(picker.findChildren<QMovie*>("StickerVisibleMovie").size()<=12);
    QVERIFY2(elapsed.elapsed()<8000,"513-item popup/animation/scroll/resize regression exceeded eight seconds");
    qInfo("513 GIF entries: %lld ms including playback and 40 resize/scroll cycles",elapsed.elapsed());
    picker.hide();
    QVERIFY(picker.findChildren<QMovie*>().isEmpty());
  }

private:
  QByteArray animatedGif() const
  {
    return QByteArray::fromBase64(
      "R0lGODlhDAAMAIEAAP8AAAAAAAAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQACAAAACwAAAAADAAMAAAIFgABCBxIsKDBgwgTKlzIsKHDhxARBgQAIfkEAQgAAQAsAAAAAAwADACBAAD/AAAAAAAAAAAACBYAAQgcSLCgwYMIEypcyLChw4cQEQYEADs=");
  }

  bool createLibrary(const QString& accountFolder,int count)
  {
    const QString directory=QDir(accountFolder).filePath("stickers");
    if(!QDir().mkpath(directory))return false;
    QJsonArray items;
    for(int index=0;index<count;++index)
    {
      const QByteArray image=animatedGif()+QByteArray::number(index);
      const QString id=QString::fromLatin1(QCryptographicHash::hash(image,QCryptographicHash::Sha256).toHex());
      QFile file(QDir(directory).filePath(id+".gif"));
      if(!file.open(QIODevice::WriteOnly)||file.write(image)!=image.size())return false;
      QJsonObject item;
      item["id"]=id;
      item["format"]="gif";
      item["name"]=QString("synthetic-%1").arg(index);
      items.append(item);
    }
    QJsonObject manifest;
    manifest["version"]=1;
    manifest["items"]=items;
    QFile file(QDir(directory).filePath("index.json"));
    if(!file.open(QIODevice::WriteOnly))return false;
    const QByteArray data=QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    return file.write(data)==data.size();
  }

  int inspectDialog(QFileDialog& dialog,const QString& directory)
  {
    const bool nativePlatform=QGuiApplication::platformName()==QStringLiteral("windows");
    if(dialog.testOption(QFileDialog::DontUseNativeDialog)==nativePlatform)
      QTest::qFail("Dialog platform policy is incorrect",__FILE__,__LINE__);
    if(dialog.directory().absolutePath()!=directory)
      QTest::qFail("Dialog did not open the expected directory",__FILE__,__LINE__);
    if(dialog.windowTitle()!=QStringLiteral("导入表情包 ZIP")||
       dialog.nameFilters()!=(QStringList()<<QStringLiteral("ZIP 表情包 (*.zip)"))||
       dialog.acceptMode()!=QFileDialog::AcceptOpen||dialog.fileMode()!=QFileDialog::ExistingFile||
       dialog.labelText(QFileDialog::Accept)!=QStringLiteral("打开")||
       dialog.labelText(QFileDialog::Reject)!=QStringLiteral("取消")||
       dialog.labelText(QFileDialog::FileName)!=QStringLiteral("文件名")||
       dialog.labelText(QFileDialog::FileType)!=QStringLiteral("文件类型"))
      QTest::qFail("ZIP dialog purpose, filter, mode or Chinese labels are incorrect",__FILE__,__LINE__);
    return QDialog::Rejected;
  }
};

int main(int argc,char** argv)
{
  if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
  QApplication application(argc,argv);
  QStandardPaths::setTestModeEnabled(true);
  StickerImportDialogTest test;
  return QTest::qExec(&test,argc,argv);
}

#include "sticker_import_dialog_test.moc"
