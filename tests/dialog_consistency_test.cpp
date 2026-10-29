#include "GuiVCard.h"
#include "GuiCreateGroup.h"
#include "Settings.h"
#include "IconManager.h"
#include "UserManager.h"
#include "BeeApplication.h"
#include "Core.h"
#include "Random.h"
#include <cstdio>
int main(int argc,char** argv)
{
  BeeApplication app(argc,argv); Random::init(); QTemporaryDir folder;
  Settings& settings=Settings::instance();settings.setDefaultFolders();settings.createLocalUser("Lin");settings.setAccountDataFolder(folder.path());settings.load();IconManager::instance();UserManager::instance();
  Core core;
  User colleague=settings.localUser();colleague.setId(42);colleague.setName(QStringLiteral("林言 · 美术"));colleague.setStatusDescription(QStringLiteral("角色与场景设计"));
  VCard data;data.setNickName(QStringLiteral("林言 · 美术"));data.setEmail("lin@example.test");data.setRoomLocation(QStringLiteral("内容制作中心 · A12"));data.setInfo(QStringLiteral("让好想法变成看得见的作品"));QPixmap avatar(96,96);avatar.fill(QColor("#609bea"));data.setPhoto(avatar);colleague.setVCard(data);
  int failures=0;auto check=[&](bool result,const char* name){std::printf("%s %s\n",result?"PASS":"FAIL",name);if(!result)++failures;};
  GuiVCard* card=new GuiVCard;card->setVCard(colleague,42,true);card->show();app.processEvents();
  check(card->property("shadowDialog").toBool(),"colleague card uses shared dialog tokens");
  QPushButton* chat=card->findChild<QPushButton*>("mp_pbChat");check(chat&&chat->isVisible()&&chat->text()==QStringLiteral("发消息")&&chat->height()>=36,"colleague chat has readable full-size primary action");
  check(card->findChild<QLabel*>("mp_lEmail")->isVisible(),"reparented profile details remain visible");
  check(card->findChild<QLabel*>("mp_lName")->isVisible()&&card->findChild<QLabel*>("mp_lName")->text()==QStringLiteral("林言 · 美术"),"colleague nickname explicit and nonempty");
  check(!card->findChild<QLabel*>("mp_lPath")->isVisible(),"network details collapsed initially");
  card->grab().save("E:/ShadowChat/delivery/colleague-card-proof.png");delete card;
  UserManager::instance().setUser(colleague);
  User designer=colleague;designer.setId(43);designer.setName(QStringLiteral("陈可 · 设计"));UserManager::instance().setUser(designer);
  GuiCreateGroup group;group.loadData();group.show();app.processEvents();
  check(group.property("shadowDialog").toBool(),"group dialog uses shared tokens");
  check(group.findChild<QPushButton*>("mp_pbOk")->text()==QStringLiteral("创建群聊"),"new group has explicit primary label");
  check(!group.findChild<QPushButton*>("mp_pbLeave")->isVisible(),"new group hides irrelevant leave action");
  check(group.findChild<QLineEdit*>("mp_leName")->height()>=36,"group name input readable height");
  check(group.findChild<QTreeWidget*>("mp_twUsers")->topLevelItemCount()>=2,"group lists actual named fixture peers");
  group.grab().save("E:/ShadowChat/delivery/create-group-proof.png");
  return failures?1:0;
}
