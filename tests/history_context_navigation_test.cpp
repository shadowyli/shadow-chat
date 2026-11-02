#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "GuiChatMessage.h"
#include "HistoryRecord.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowStyle.h"
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

static int anchorPosition(QTextDocument* document,const QString& name)
{
  for(QTextBlock block=document->begin();block.isValid();block=block.next())
  {
    if(block.charFormat().anchorNames().contains(name))return block.position();
    for(QTextBlock::iterator iterator=block.begin();!iterator.atEnd();++iterator)
      if(iterator.fragment().charFormat().anchorNames().contains(name))
        return iterator.fragment().position();
  }
  return -1;
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
  settings.createLocalUser("history-context-fixture");
  settings.setAccountDataFolder(folder.path());
  settings.load();
  Core core;
  Group group;
  group.setId(98117);
  group.setName("History context fixture");
  group.setChatType(Group::PrivateChat);
  group.addUser(ID_LOCAL_USER);
  Chat conversation;
  conversation.setGroup(group);
  Message live=Protocol::instance().chatMessage(conversation,"live tail retained");
  conversation.addMessage(ChatMessage(ID_LOCAL_USER,live,ChatMessage::Chat,false));
  ChatManager::instance().setChat(conversation);
  const QString archiveName="Archived-only context fixture";
  QString archive;
  for(int index=0;index<90;++index)
  {
    HistoryRecord record;
    record.sender=settings.localUser().name();
    record.timestamp=QDateTime(QDate(2026,11,1),QTime(10,index/60,index%60));
    record.type=ChatMessage::Chat;
    record.hasDirection=true;
    record.outgoing=true;
    record.html=QString("context message %1").arg(index);
    archive+=shadowHistoryRecordHtml(record,QString());
  }
  HistoryRecord duplicate;
  duplicate.sender=settings.localUser().name();
  duplicate.timestamp=QDateTime(QDate(2026,11,1),QTime(11,10));
  duplicate.type=ChatMessage::Chat;
  duplicate.hasDirection=true;
  duplicate.outgoing=true;
  duplicate.html="identical context target";
  archive+=shadowHistoryRecordHtml(duplicate,QString());
  archive+=shadowHistoryRecordHtml(duplicate,QString());
  QMap<QString,QString> saved;
  saved.insert(conversation.name(),archive);
  saved.insert(archiveName,archive);
  ChatManager::instance().addSavedChats(saved);
  const QString original=ChatManager::instance().chatSavedText(conversation.name());
  const QString combined=original+GuiChatMessage::chatToHtml(conversation,false,true,true,true,false,true);
  const auto records=shadowChatHistoryRecords(combined);
  const QStringList keys=shadowHistoryRecordKeys(records);
  CHECK(records.size()==93&&keys.at(90)!=keys.at(91));
  QToolBar toolbar;
  GuiChat chat;
  chat.setupToolBar(&toolbar);
  chat.setModernMode(true);
  chat.resize(900,700);
  CHECK(chat.setChat(conversation));
  chat.show();
  chat.addText("draft stays while browsing history");
  CHECK(chat.showHistoryRecord(conversation.name(),keys.at(91)));
  QTest::qWait(180);
  auto* transcript=dynamic_cast<ShadowChatTranscript*>(chat.findChild<QTextBrowser*>("GuiChatViewer"));
  CHECK(transcript);
  const QString anchor="shadow-context-"+keys.at(91);
  const int position=anchorPosition(transcript->document(),anchor);
  CHECK(position>=0);
  QTextCursor cursor(transcript->document());
  cursor.setPosition(position);
  CHECK(transcript->cursorRect(cursor).top()>=-20);
  CHECK(transcript->cursorRect(cursor).top()<transcript->viewport()->height());
  CHECK(transcript->verticalScrollBar()->value()>1000);
  CHECK(transcript->toPlainText().contains("live tail retained"));
  CHECK(chat.findChild<QTextEdit*>("GuiMessageEdit")->toPlainText()=="draft stays while browsing history");
  CHECK(ChatManager::instance().chatSavedText(conversation.name())==original);
  const QString visibleBefore=transcript->toHtml();
  CHECK(!chat.showHistoryRecord(conversation.name(),"missing-record"));
  CHECK(transcript->toHtml()==visibleBefore);
  const int countBefore=ChatManager::instance().constChatList().size();
  QToolBar archivedTools;
  GuiChat archived;
  archived.setupToolBar(&archivedTools);
  archived.setModernMode(true);
  archived.resize(900,700);
  archived.show();
  CHECK(archived.showHistoryRecord(archiveName,keys.at(91),true));
  QTest::qWait(180);
  CHECK(archived.findChild<QTextEdit*>("GuiMessageEdit")->isHidden()||
    !archived.findChild<QTextEdit*>("GuiMessageEdit")->isVisible());
  CHECK(ChatManager::instance().constChatList().size()==countBefore);
  CHECK(ChatManager::instance().chatSavedText(archiveName)==archive);
  std::puts("PASS context navigation: exact duplicate occurrence, visible target anchor, archived and live context, draft and source preserved, unknown key unchanged, archived-only read-only view without fake chat");
  return 0;
}
