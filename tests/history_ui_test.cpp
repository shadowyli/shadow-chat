#include "Config.h"
#include "GuiSavedChat.h"
#include "BeeApplication.h"
#include "GuiChat.h"
#include "GuiImageBatchPreview.h"
#include "ShadowImageAlbum.h"
#include "Protocol.h"
#include "UserManager.h"
#include "Core.h"
#include "ShadowStyle.h"
#include "ChatManager.h"
#include "Settings.h"
#include "GuiVCard.h"
#include "GuiCreateGroup.h"
#include "GuiFileTransfer.h"
#include "GuiMain.h"
#include "Listener.h"
#include "NetworkManager.h"
#include "Random.h"
#include <cstdio>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
#endif

static void require(bool ok,const char* message) {if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
class QaGuiChat:public GuiChat {public:explicit QaGuiChat(QWidget* parent):GuiChat(parent){}using GuiChat::dragEnterEvent;using GuiChat::dropEvent;};
static QString message(const QString& sender,const QString& html) {
  QJsonObject metadata;metadata.insert("sender",sender);metadata.insert("time",QDateTime::currentDateTime().toString(Qt::ISODate));
  return "<!--shadow-record:"+QString::fromLatin1(QJsonDocument(metadata).toJson(QJsonDocument::Compact).toBase64())+"-->"+html+"<br>";
}
int main(int argc,char** argv)
{
#ifdef Q_OS_WIN
  SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* fault)->LONG {
    HANDLE process=GetCurrentProcess();SymInitialize(process,NULL,TRUE);CONTEXT context=*fault->ContextRecord;STACKFRAME64 frame={};frame.AddrPC.Offset=context.Eip;frame.AddrPC.Mode=AddrModeFlat;frame.AddrStack.Offset=context.Esp;frame.AddrStack.Mode=AddrModeFlat;frame.AddrFrame.Offset=context.Ebp;frame.AddrFrame.Mode=AddrModeFlat;
    std::fprintf(stderr,"EXCEPTION %lx\n",fault->ExceptionRecord->ExceptionCode);
    for(int i=0;i<15;++i){char buffer[sizeof(SYMBOL_INFO)+512]={};SYMBOL_INFO* symbol=reinterpret_cast<SYMBOL_INFO*>(buffer);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=511;DWORD64 displacement=0;if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol))std::fprintf(stderr,"STACK %s+%llu\n",symbol->Name,displacement);else std::fprintf(stderr,"STACK %llx\n",frame.AddrPC.Offset);if(!StackWalk64(IMAGE_FILE_MACHINE_I386,process,GetCurrentThread(),&frame,&context,NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL))break;}return EXCEPTION_EXECUTE_HANDLER;
  });
#endif
  BeeApplication app(argc,argv);Random::init();QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");app.setFont(QFont("Microsoft YaHei",10));app.setStyleSheet(ShadowStyle::applicationStyleSheet());
  QTranslator chinese;require(chinese.load("../locale/beebeep_cn.qm"),"installed Chinese translation fixture");app.installTranslator(&chinese);
  QTemporaryDir temporary;require(temporary.isValid(),"temporary profile");
  Settings::instance().setDefaultFolders();Settings::instance().createLocalUser("HistoryTest");Settings::instance().setAccountDataFolder(temporary.path());
  Settings::instance().load();Settings::instance().setChatMessagesToShow(-1);
  Core core; // Construct services for normal modal/timer callbacks; never start networking.
  const QString imagePath=temporary.filePath("参考图.png");QImage image(320,240,QImage::Format_RGB32);image.fill(QColor("#578dde"));require(image.save(imagePath),"fixture image");
  const QString sourceFile=temporary.filePath("角色.psd");QFile source(sourceFile);require(source.open(QIODevice::WriteOnly),"file fixture");source.write("synthetic PSD filename fixture");source.close();
  require(!GuiImageBatchPreview::isImageBatch(QStringList()<<sourceFile<<sourceFile),"PSD materials use file flow rather than raster image preview");require(GuiImageBatchPreview::isImageBatch(QStringList()<<imagePath<<imagePath),"PNG multi-select uses image confirmation");
  QString html=message("Alice",QString::fromUtf8("<b>Alice</b>：请确认这版角色配色。"));
  html+=message("Bob",QString("<a href=\"%1\">角色.psd</a>").arg(QUrl::fromLocalFile(sourceFile).toString()));
  html+=message("Alice",QString("<a href=\"%1\"><img src=\"%1\" width=\"160\"></a>").arg(QUrl::fromLocalFile(imagePath).toString()));
  html+=message("Bob","<a href=\"https://flowx.gz/tools\">FlowX 工具站</a>");
  QMap<QString,QString> history;history.insert("项目讨论",html);ChatManager::instance().addSavedChats(history);
  GuiSavedChat window;window.showSavedChat("项目讨论");window.show();app.processEvents();
  QListWidget* results=window.findChild<QListWidget*>();QTabBar* tabs=window.findChild<QTabBar*>();QLineEdit* keyword=window.findChild<QLineEdit*>("GuiLineEditFilterHistory");
  require(results && tabs && keyword,"history controls exist");require(results->count()==4,"all message records");
  tabs->setCurrentIndex(1);require(results->count()==1 && results->item(0)->text().contains("角色.psd"),"file classification");
  QUrl openedFile;QObject::connect(&window,&GuiSavedChat::openUrl,[&](const QUrl& url){openedFile=url;});results->itemDoubleClicked(results->item(0));require(openedFile.toLocalFile()==sourceFile,"file result opens exact original path");
  tabs->setCurrentIndex(2);require(results->count()==1 && !results->item(0)->icon().isNull(),"media classification and thumbnail");
  require(results->viewMode()==QListView::IconMode,"media thumbnail grid");
  tabs->setCurrentIndex(3);require(results->count()==1 && results->item(0)->text().contains("flowx.gz"),"link classification");
  tabs->setCurrentIndex(0);keyword->setText("配色");require(results->count()==1,"keyword filtering");keyword->clear();
  QComboBox* sender=Q_NULLPTR;foreach(QComboBox* combo,window.findChildren<QComboBox*>())if(combo->accessibleName()==QString::fromUtf8("发送者"))sender=combo;
  require(sender,"sender filter");sender->setCurrentIndex(sender->findText("Bob"));require(results->count()==2,"sender filtering");sender->setCurrentIndex(0);
  results->setCurrentRow(0);app.processEvents();QPixmap proof(window.size());window.render(&proof);require(proof.save("E:/ShadowChat/delivery/history-ui-proof.png"),"history screenshot");
  QString longHistory;for(int i=0;i<60;++i)longHistory+=message("Alice",QString("<p>source message %1</p>").arg(i));
  history.clear();history.insert("定位测试",longHistory);ChatManager::instance().addSavedChats(history);window.showSavedChat("定位测试");app.processEvents();
  results->setCurrentRow(0);app.processEvents();require(window.findChild<QTextBrowser*>()->verticalScrollBar()->value()>0,"source anchor actually scrolls to newest record");
  std::fprintf(stderr,"QA chat construct\n");
  QMainWindow chatWindow;QaGuiChat* chatWidget=new QaGuiChat(&chatWindow);std::fprintf(stderr,"QA chat toolbar\n");QToolBar* bar=new QToolBar(&chatWindow);chatWindow.addToolBar(bar);chatWidget->setupToolBar(bar);std::fprintf(stderr,"QA chat modern\n");chatWidget->setModernMode(true);chatWindow.setCentralWidget(chatWidget);chatWindow.resize(1040,760);
  User teammate(3333);teammate.setName("Bob");UserManager::instance().setUser(teammate);
  {
    User colleague(3334);VCard details;details.setNickName(QStringLiteral("林言 · 美术"));details.setEmail("lin@example.test");details.setRoomLocation(QStringLiteral("内容制作中心 · A12"));details.setInfo(QStringLiteral("角色与场景设计"));QPixmap portrait(96,96);portrait.fill(QColor("#609BEA"));details.setPhoto(portrait);colleague.setVCard(details);UserManager::instance().setUser(colleague);
    GuiVCard* card=new GuiVCard;card->setVCard(colleague,3334,true);card->show();app.processEvents();require(card->findChild<QLabel*>("mp_lName")->text()==details.nickName(),"colleague card has explicit nickname");require(!card->findChild<QLabel*>("mp_lPath")->isVisible(),"colleague network metadata collapsed");require(card->grab().save("E:/ShadowChat/delivery/colleague-card-proof.png"),"colleague card screenshot");delete card;
    GuiCreateGroup create;create.loadData();create.show();app.processEvents();QTreeWidget* members=create.findChild<QTreeWidget*>("mp_twUsers");require(members->topLevelItemCount()>=2,"create group named colleagues");require(!create.findChild<QPushButton*>("mp_pbLeave")->isVisible(),"create group no leave action");QPushButton* createAction=create.findChild<QPushButton*>("mp_pbOk");require(createAction->isDefault()&&createAction->x()>create.findChild<QPushButton*>("mp_pbCancel")->x(),"group primary right of cancel and default");create.findChild<QLineEdit*>("mp_leName")->setText(QStringLiteral("美术项目组"));members->topLevelItem(0)->setCheckState(0,Qt::Checked);const VNumber selectedMember=members->topLevelItem(0)->data(0,Qt::UserRole+1).toULongLong();require(create.grab().save("E:/ShadowChat/delivery/create-group-proof.png"),"group screenshot");createAction->click();require(create.result()==QDialog::Accepted&&create.group().hasUser(selectedMember),"checked colleague saved into group");create.hide();
    GuiFileTransfer transfer;transfer.resize(900,360);transfer.show();app.processEvents();require(!transfer.header()->isHidden(),"transfer columns have visible labels");require(transfer.grab().save("E:/ShadowChat/delivery/transfer-empty-proof.png"),"transfer empty screenshot");FileInfo material;material.setId(77);material.setNameAndSuffix(QStringLiteral("角色场景_v03.psd"));material.setPath(sourceFile);material.setSize(128*1024*1024);transfer.setProgress(77,colleague,material,48*1024*1024,5);app.processEvents();require(transfer.topLevelItemCount()==1,"transfer populated through production progress");require(transfer.grab().save("E:/ShadowChat/delivery/transfer-progress-proof.png"),"transfer progress screenshot");
  }
  Group group;group.setId(98765);group.setName("batch-preview-test");group.setChatType(Group::GroupChat);group.addUser(ID_LOCAL_USER);group.addUser(teammate.id());Chat chat;chat.setGroup(group);
  for(int i=0;i<50;++i) {
    const QString file=temporary.filePath(QString("batch-a-%1.png").arg(i));require(QFile::copy(imagePath,file),"batch source");
    const QString url=QUrl::fromLocalFile(file).toString();
    chat.addMessage(ChatMessage(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(QString("<!--shadow-album:{11111111-1111-1111-1111-111111111111}--><a href=\"%1\"><img src=\"%1\"></a>").arg(url)),ChatMessage::ImagePreview,true));
    if(i<2) {
      const QString second=temporary.filePath(QString("batch-b-%1.png").arg(i));require(QFile::copy(imagePath,second),"second batch source");const QString secondUrl=QUrl::fromLocalFile(second).toString();
      chat.addMessage(ChatMessage(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(QString("<!--shadow-album:{22222222-2222-2222-2222-222222222222}--><a href=\"%1\"><img src=\"%1\"></a>").arg(secondUrl)),ChatMessage::ImagePreview,true));
    }
    if(i==25)chat.addMessage(ChatMessage(ID_LOCAL_USER,Protocol::instance().systemMessage("interleaved-text-kept"),ChatMessage::Chat,true));
  }
  std::fprintf(stderr,"QA chat set\n");ChatManager::instance().setChat(chat);require(chatWidget->setChat(chat),"set batch chat");std::fprintf(stderr,"QA chat show\n");chatWindow.show();app.processEvents();
  QTextBrowser* transcript=chatWidget->findChild<QTextBrowser*>("GuiChatViewer");require(transcript,"chat transcript");
  const QString rendered=transcript->toPlainText();require(rendered.count(QString::fromUtf8("50 张图片"))==1 && rendered.count(QString::fromUtf8("2 张图片"))==1,"UUID batches group exactly despite interleaving");require(rendered.contains("interleaved-text-kept"),"interleaved text retained");require(rendered.contains(QString::fromUtf8("还有 41 张")),"large batch compact 9 plus remainder");
  QSet<int> groupSizes;
  for(QTextBlock block=transcript->document()->begin();block.isValid();block=block.next())for(QTextBlock::iterator fragment=block.begin();!fragment.atEnd();++fragment){const QUrl url(fragment.fragment().charFormat().anchorHref());if(url.scheme()=="shadowimage")groupSizes.insert(shadowAlbumPaths(url).size());}
  require(groupSizes.contains(50)&&groupSizes.contains(2),"viewer original scope contains 50 and 2 separately");
  int largeRemovals=0;QObject::connect(transcript->document(),&QTextDocument::contentsChange,[&](int,int removed,int){if(removed>64)++largeRemovals;});QElapsedTimer appendTimer;appendTimer.start();
  for(int i=0;i<500;++i){const ChatMessage fileMessage(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(QString("file-completed-%1").arg(i)),ChatMessage::FileTransfer,true);chat.addMessage(fileMessage);require(chatWidget->appendChatMessage(chat,fileMessage),"file notification append");}
  require(largeRemovals==0,"file notifications do not clear and rerender existing transcript");require(transcript->toPlainText().contains("file-completed-499"),"all file notifications retained");std::fprintf(stderr,"QA 500 file notifications appended in %lld ms\n",appendTimer.elapsed());
  appendTimer.restart();
  for(int i=0;i<500;++i){const ChatMessage preview(ID_SYSTEM_MESSAGE,Protocol::instance().systemMessage(QString("<a href=\"%1\"><img src=\"%1\"></a>").arg(QUrl::fromLocalFile(imagePath).toString())),ChatMessage::ImagePreview,true);chat.addMessage(preview);require(chatWidget->appendChatMessage(chat,preview),"standalone image append");}
  require(largeRemovals==0,"standalone image previews never rerender the transcript");
  std::fprintf(stderr,"QA 500 standalone images appended in %lld ms\n",appendTimer.elapsed());
  QString capturedImage;QStringList droppedFiles;
  QObject::connect(chatWidget,&GuiChat::sendFileFromChatRequest,[&](VNumber,const QString& path){capturedImage=path;std::fprintf(stderr,"QA image captured: %s\n",qPrintable(path));});
  QObject::connect(chatWidget,&GuiChat::sendFilesFromChatRequest,[&](VNumber,const QStringList& paths){droppedFiles=paths;});
  Settings::instance().setDownloadDirectory(temporary.filePath("received"));QDir().mkpath(Settings::instance().downloadDirectory());
  QMimeData* previousClipboard=new QMimeData;const QMimeData* clipboard=QApplication::clipboard()->mimeData();if(clipboard)foreach(const QString& format,clipboard->formats())previousClipboard->setData(format,clipboard->data(format));
  QApplication::clipboard()->setImage(image);QTextEdit* composer=chatWidget->findChild<QTextEdit*>("GuiMessageEdit");require(composer,"composer");composer->paste();app.processEvents();
  require(!capturedImage.isEmpty()&&QFileInfo::exists(capturedImage),"actual clipboard paste enters send pipeline");require(QDir::fromNativeSeparators(capturedImage).startsWith(QDir::fromNativeSeparators(Settings::instance().downloadDirectory())),"generated originals survive cache cleanup");require(QImage(capturedImage)==image,"pasted original pixels preserved");
  QApplication::clipboard()->setMimeData(previousClipboard);
  std::fprintf(stderr,"QA paste verified, drag start\n");
  const QString folder=temporary.filePath("工程目录");QDir().mkpath(folder);QMimeData dropData;dropData.setUrls(QList<QUrl>()<<QUrl::fromLocalFile(imagePath)<<QUrl::fromLocalFile(folder));
  std::fprintf(stderr,"QA drag data ready\n");
  // Invoke the production handlers directly: offscreen Qt has no OS drag manager.
  QDragEnterEvent enter(QPoint(20,20),Qt::CopyAction,&dropData,Qt::LeftButton,Qt::NoModifier);chatWidget->dragEnterEvent(&enter);require(enter.isAccepted(),"file folder drag accepted");
  std::fprintf(stderr,"QA drag entered\n");
  QTimer::singleShot(200,&app,[](){QMessageBox* confirmation=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(confirmation)confirmation->done(0);});
  QDropEvent drop(QPointF(20,20),Qt::CopyAction,&dropData,Qt::LeftButton,Qt::NoModifier);chatWidget->dropEvent(&drop);require(droppedFiles.size()==2,"actual drop emits file and folder");
  std::fprintf(stderr,"QA drag verified, quote start\n");
  composer->clear();require(transcript->find("interleaved-text-kept"),"quote source found");require(QMetaObject::invokeMethod(chatWidget,"quoteSelectedText",Qt::DirectConnection),"quote action invoked");require(composer->toPlainText().contains("interleaved-text-kept"),"quote reply inserted");
  composer->clear();bool memberDialog=false;QTimer::singleShot(200,&app,[&](){QInputDialog* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());if(dialog){memberDialog=true;dialog->setTextValue("Bob");dialog->accept();}});
  std::fprintf(stderr,"QA quote verified, mention start\n");
  require(QMetaObject::invokeMethod(chatWidget,"mentionChatMember",Qt::DirectConnection),"mention action invoked");require(memberDialog&&composer->toPlainText().contains("@Bob"),"actual member chooser inserts mention");
  composer->clear();transcript->moveCursor(QTextCursor::Start);app.processEvents();
  const QRect composerBounds(composer->mapTo(chatWidget,QPoint()),composer->size());
  foreach(QToolButton* tool,chatWidget->findChildren<QToolButton*>()) {
    if(tool->objectName()=="ShadowComposerAction"&&tool->isVisible())require(!QRect(tool->mapTo(chatWidget,QPoint()),tool->size()).intersects(composerBounds),"composer actions do not overlap text input");
  }
  QPixmap chatProof(chatWindow.size());chatWindow.render(&chatProof);require(chatProof.save("E:/ShadowChat/delivery/chat-ui-proof.png"),"chat screenshot");
  {
    ChatManager::instance().removeChat(chat);
    teammate.setName(QStringLiteral("陈可 · 设计"));teammate.setStatus(User::Online);VCard teammateProfile=teammate.vCard();QPixmap teammatePhoto(96,96);teammatePhoto.fill(QColor("#B58D90"));teammateProfile.setPhoto(teammatePhoto);teammate.setVCard(teammateProfile);UserManager::instance().setUser(teammate);
    User local=Settings::instance().localUser();local.setName(QStringLiteral("陆川"));VCard localProfile=local.vCard();QPixmap localPhoto(96,96);localPhoto.fill(QColor("#7B91B0"));localProfile.setPhoto(localPhoto);local.setVCard(localProfile);Settings::instance().setLocalUser(local);
    User artist=UserManager::instance().findUser(3334);artist.setStatus(User::Online);UserManager::instance().setUser(artist);
    Group project;project.setId(99881);project.setPrivateId("synthetic-ui-project");project.setName(QStringLiteral("美术项目组"));project.setChatType(Group::GroupChat);project.addUser(ID_LOCAL_USER);project.addUser(teammate.id());project.addUser(artist.id());Chat projectChat;projectChat.setGroup(project);
    projectChat.addMessage(ChatMessage(teammate.id(),Protocol::instance().systemMessage(QStringLiteral("这版角色配色已经整理好了，大家看一下。")),ChatMessage::Chat,true));
    projectChat.addMessage(ChatMessage(ID_LOCAL_USER,Protocol::instance().systemMessage(QStringLiteral("收到，工程素材也一起放到这里吧。")),ChatMessage::Chat,true));
    projectChat.addMessage(ChatMessage(artist.id(),Protocol::instance().systemMessage(QStringLiteral("好的，文件发出后我再确认一遍。")),ChatMessage::Chat,true));ChatManager::instance().setChat(projectChat);
    // Bind only a loopback listener: exercise connected UI without discovery,
    // broadcasts, outgoing connections or messages to other people.
    Listener* uiListener=core.findChild<Listener*>();require(uiListener&&uiListener->listen(QHostAddress::LocalHost,0),"isolated loopback listener for UI state");NetworkManager::instance().searchLocalHostAddress();
    GuiMain main;main.resize(1280,820);main.show();QMetaObject::invokeMethod(&main,"onCoreConnected",Qt::DirectConnection);app.processEvents();
    QMetaObject::invokeMethod(&main,"onUserChanged",Qt::DirectConnection,Q_ARG(User,teammate));QMetaObject::invokeMethod(&main,"onUserChanged",Qt::DirectConnection,Q_ARG(User,artist));
    QMetaObject::invokeMethod(&main,"onChatChanged",Qt::DirectConnection,Q_ARG(Chat,projectChat));QMetaObject::invokeMethod(&main,"showChat",Qt::DirectConnection,Q_ARG(VNumber,projectChat.id()));app.processEvents();require(main.grab().save("E:/ShadowChat/delivery/main-shell-final.png"),"full production main screenshot");
    QMetaObject::invokeMethod(&main,"showModernContacts",Qt::DirectConnection);app.processEvents();require(main.grab().save("E:/ShadowChat/delivery/contacts-final.png"),"contacts screenshot");
    bool settingsSaved=false;QTimer::singleShot(200,&app,[&](){QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(dialog){settingsSaved=dialog->grab().save("E:/ShadowChat/delivery/settings-final.png");dialog->reject();}});require(QMetaObject::invokeMethod(&main,"showShadowSettings",Qt::DirectConnection),"open actual settings");require(settingsSaved,"settings screenshot");main.hide();uiListener->close();
  }
  std::puts("PASS history/chat UI: categories, media grid, keyword, sender, source scroll; interleaved 50+2 UUID batches and scoped originals; real paste/drop/quote/member-picker actions; colleague/group/transfer/main/contacts/settings");
  std::fflush(stdout);
  if(app.arguments().contains("--interactive")) {
    window.hide();
    QWidget backdrop;backdrop.setWindowTitle(QString::fromUtf8("截图验收背景"));backdrop.setStyleSheet("background:#4079d5;color:white;");QVBoxLayout layout(&backdrop);QLabel label(QString::fromUtf8("影聊截图验收\n此背景为本地合成数据\n点击聊天窗口“截图”，框选蓝色区域即可。"));label.setAlignment(Qt::AlignCenter);label.setFont(QFont("Microsoft YaHei",28));layout.addWidget(&label);backdrop.showFullScreen();
    chatWindow.setWindowTitle(QString::fromUtf8("影聊本地交互验收（合成数据，无网络）"));chatWindow.statusBar()->showMessage(QString::fromUtf8("合成测试会话：所有发送仅捕获本机信号，不连接网络。"));chatWindow.show();chatWindow.raise();chatWindow.activateWindow();return app.exec();
  }
  return 0;
}
