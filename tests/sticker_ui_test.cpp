#include "BeeApplication.h"
#include "Core.h"
#include "Random.h"
#include "Settings.h"
#include "ChatManager.h"
#include "GuiChat.h"
#include "GuiStickerPicker.h"
#include "ShadowStickerStore.h"
#include <QtTest>
#include <QProcess>
#include <cstdio>
#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);return 1;}}while(false)
static QByteArray bytes(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return QByteArray();return file.readAll();}
static bool childReload(const QString& folder,int count)
{
  QProcess child;child.setProcessChannelMode(QProcess::ForwardedChannels);child.start(QCoreApplication::applicationFilePath(),QStringList()<<"--reload"<<folder<<QString::number(count));return child.waitForFinished(30000)&&child.exitCode()==0;
}
int main(int argc,char** argv)
{
  if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");BeeApplication app(argc,argv);
  if(app.arguments().value(1)=="--reload"){
    const QString folder=app.arguments().value(2);const int count=app.arguments().value(3).toInt();GuiStickerPicker picker(folder);CHECK(picker.findChild<QListWidget*>("StickerGrid")->count()==count);CHECK(ShadowStickerStore(folder).list().size()==count);
    QTemporaryDir different;GuiStickerPicker other(different.path());CHECK(other.findChild<QListWidget*>("StickerGrid")->count()==0);std::puts("PASS sticker fresh-process grid/manifest reload and account isolation");return 0;
  }
  Random::init();
  QTemporaryDir workspace;CHECK(workspace.isValid());Settings& settings=Settings::instance();settings.setDefaultFolders();settings.createLocalUser("sticker-fixture");settings.setAccountDataFolder(workspace.path());settings.load();
  Core core;
  Group group;group.setId(9200);group.setName("sticker-fixture-chat");group.setChatType(Group::PrivateChat);group.addUser(ID_LOCAL_USER);group.addUser(9201);Chat chat;chat.setGroup(group);ChatManager::instance().setChat(chat);
  QToolBar toolbar;GuiChat widget;widget.setupToolBar(&toolbar);widget.setModernMode(true);widget.resize(940,700);CHECK(widget.setChat(chat));widget.show();QCoreApplication::processEvents();QToolButton* button=Q_NULLPTR;foreach(QToolButton* candidate,widget.findChildren<QToolButton*>())if(candidate->property("stickerPicker").toBool())button=candidate;CHECK(button);
  QString sent;QObject::connect(&widget,&GuiChat::sendFileFromChatRequest,[&](VNumber chatId,const QString& path){if(chatId==9200)sent=path;});
  const QString sourceDirectory=workspace.filePath("synthetic-zip-source");CHECK(QDir().mkpath(sourceDirectory));
  for(int imageIndex=0;imageIndex<512;++imageIndex)
  {
    QImage synthetic(12,12,QImage::Format_ARGB32);
    synthetic.fill(QColor(imageIndex%256,imageIndex/256,(imageIndex*31)%256));
    CHECK(synthetic.save(QDir(sourceDirectory).filePath(QString("sticker-%1.png").arg(imageIndex))));
  }
  const QString zip=workspace.filePath("synthetic-stickers.zip");
  QProcess zipProcess;
  zipProcess.start("python",QStringList()<<"-c"
    <<"import pathlib,sys,zipfile;archive=zipfile.ZipFile(sys.argv[2],'w',compression=zipfile.ZIP_DEFLATED);[archive.write(path,path.name) for path in sorted(pathlib.Path(sys.argv[1]).glob('*.png'))];archive.close()"
    <<sourceDirectory<<zip);
  CHECK(zipProcess.waitForStarted(5000)&&zipProcess.waitForFinished(30000)&&zipProcess.exitCode()==0);
  CHECK(QFileInfo::exists(zip));bool imported=false,clicked=false;
  QTimer::singleShot(30,[&](){
    GuiStickerPicker* picker=widget.findChild<GuiStickerPicker*>();if(!picker)return;
    imported=picker->importArchive(zip);QListWidget* grid=picker->findChild<QListWidget*>("StickerGrid");imported=imported&&grid&&grid->count()>500;
    if(imported){grid->scrollToTop();QCoreApplication::processEvents();QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(0)).center());clicked=true;}else{std::fprintf(stderr,"IMPORT count=%d status=%s\n",grid?grid->count():-1,qPrintable(picker->findChild<QLabel*>("StickerStatus")->text()));picker->reject();}
  });
  QTimer::singleShot(30000,&widget,[&](){foreach(QDialog* dialog,widget.findChildren<QDialog*>())dialog->reject();});button->click();CHECK(imported&&clicked);CHECK(!sent.isEmpty());const QByteArray sentBytes=bytes(sent);CHECK(!sentBytes.isEmpty());const auto importedItems=ShadowStickerStore(workspace.path()).list();CHECK(importedItems.size()>500);CHECK(sent==importedItems.first().path);CHECK(QCryptographicHash::hash(sentBytes,QCryptographicHash::Sha256).toHex()==QFileInfo(sent).completeBaseName().toLatin1());CHECK(childReload(workspace.path(),importedItems.size()));
  // Real received file anchor -> production context action -> independent favorite copy.
  const QString received=workspace.filePath("received-original.png");QImage original(79,61,QImage::Format_ARGB32);original.fill(QColor("#fd8b51"));CHECK(original.save(received));const QByteArray originalBytes=bytes(received);QTextBrowser* transcript=widget.findChild<QTextBrowser*>("GuiChatViewer");CHECK(transcript);transcript->setHtml(QString("<a href=\"%1\"><img src=\"%1\" width=\"79\" height=\"61\"></a>").arg(QUrl::fromLocalFile(received).toString()));QCoreApplication::processEvents();QPoint anchor;
  for(int y=0;y<100&&anchor.isNull();++y)for(int x=0;x<150;++x)if(!transcript->anchorAt(QPoint(x,y)).isEmpty()){anchor=QPoint(x,y);break;}CHECK(!anchor.isNull());bool favorited=false;
  QTimer::singleShot(20,[&](){foreach(QMenu* menu,widget.findChildren<QMenu*>())foreach(QAction* action,menu->actions())if(action->objectName()=="FavoriteStickerAction"){action->trigger();favorited=true;menu->close();}});
  CHECK(QMetaObject::invokeMethod(&widget,"customContextMenu",Qt::DirectConnection,Q_ARG(QPoint,anchor)));CHECK(favorited);ShadowStickerStore store(workspace.path());CHECK(store.list().size()==importedItems.size()+1);CHECK(store.addFile(received));CHECK(store.list().size()==importedItems.size()+1);CHECK(bytes(received)==originalBytes);
  GuiStickerPicker management(workspace.path());management.show();QListWidget* grid=management.findChild<QListWidget*>("StickerGrid");CHECK(grid);const QString favoriteId=QString::fromLatin1(QCryptographicHash::hash(originalBytes,QCryptographicHash::Sha256).toHex());QListWidgetItem* favorite=Q_NULLPTR;for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole).toString()==favoriteId)favorite=grid->item(i);CHECK(favorite);const QString favoritePath=favorite->data(Qt::UserRole+1).toString();CHECK(favoritePath!=received);CHECK(bytes(favoritePath)==originalBytes);grid->setCurrentItem(favorite);management.findChild<QPushButton*>("StickerRemove")->click();CHECK(!QFileInfo::exists(favoritePath));CHECK(bytes(received)==originalBytes);CHECK(store.list().size()==importedItems.size());CHECK(childReload(workspace.path(),importedItems.size()));
  std::puts("PASS actual composer button/production ZIP import/grid click sends original bytes; received-image context favorite deduplicates; remove preserves received original; independent process restart/isolation");return 0;
}
