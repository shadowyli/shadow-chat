#include "ShadowLocalAccounts.h"
#include "ShadowWorkspaceArchive.h"
#include <QCoreApplication>
#include <QtWidgets>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <cstdio>

#define VERIFY(condition) do { if(!(condition)) { qCritical() << "FAIL line" << __LINE__ << #condition; return 1; } } while(false)
int main(int argc,char** argv)
{
  qputenv("QT_QPA_PLATFORM","offscreen");
  QApplication app(argc,argv);
  QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
  QTemporaryDir temporary;
  VERIFY(temporary.isValid());
  app.setProperty("shadowAccountRoot", temporary.path());
  ShadowLocalAccounts selected;
  VERIFY(selected.root()==temporary.path());
  app.setProperty("shadowAccountRoot", QVariant());
  ShadowLocalAccounts accounts(temporary.path());
  QString error;
  VERIFY(ShadowLocalAccounts::normalizeEmail(" Alice@Example.com ")=="alice@example.com");
  VERIFY(accounts.profileFolder("../escape").isEmpty());
  VERIFY(accounts.profileFolder("a@foo.com/escape").isEmpty());
  VERIFY(!accounts.create("alice@example.com","123",&error));
  VERIFY(accounts.create("alice@example.com","secret-a",&error));
  VERIFY(!accounts.create("alice@example.com","changed",&error));
  VERIFY(accounts.create("bob@example.com","secret-b",&error));
  VERIFY(accounts.authenticate("ALICE@example.com","secret-a"));
  VERIFY(!accounts.authenticate("alice@example.com","secret-b"));
  VERIFY(!accounts.authenticate("missing@example.com","secret-a"));
  VERIFY(accounts.profileFolder("alice@example.com")!=accounts.profileFolder("bob@example.com"));
  QFile history(QDir(accounts.profileFolder("alice@example.com")).filePath("beebeep.his"));
  VERIFY(history.open(QIODevice::WriteOnly)); history.write("alice-only"); history.close();
  VERIFY(!QFile::exists(QDir(accounts.profileFolder("bob@example.com")).filePath("beebeep.his")));
  accounts.rememberLogin("alice@example.com");
  VERIFY(accounts.autoLoginEmail()=="alice@example.com");
  accounts.setAutoLoginEnabled(false); VERIFY(accounts.autoLoginEmail().isEmpty());
  accounts.setAutoLoginEnabled(true); VERIFY(accounts.autoLoginEmail()=="alice@example.com");
  accounts.logout(); VERIFY(accounts.autoLoginEmail().isEmpty());
  VERIFY(!accounts.hasRecoveryPassword());
  VERIFY(!accounts.setRecoveryPassword("short"));
  VERIFY(accounts.setRecoveryPassword("owner-recovery"));
  VERIFY(accounts.verifyRecoveryPassword("owner-recovery"));
  VERIFY(!accounts.verifyRecoveryPassword("wrong"));
  const QString recoverySeed=QDir(temporary.path()).filePath("shadowchat-recovery.ini");
  VERIFY(accounts.exportRecoveryConfiguration(recoverySeed));
  QFile seed(recoverySeed); VERIFY(seed.open(QIODevice::ReadOnly)); VERIFY(!seed.readAll().contains("owner-recovery")); seed.close();
  VERIFY(accounts.setPassword("alice@example.com","new-secret",&error));
  VERIFY(!accounts.authenticate("alice@example.com","secret-a"));
  VERIFY(accounts.authenticate("alice@example.com","new-secret"));
  VERIFY(history.open(QIODevice::ReadOnly)); VERIFY(history.readAll()=="alice-only"); history.close();
  QFile metadata(QDir(accounts.profileFolder("alice@example.com")).filePath(".shadow-account.ini"));
  VERIFY(metadata.open(QIODevice::ReadOnly)); VERIFY(!metadata.readAll().contains("new-secret"));
  metadata.close();
  const QString backup = QDir(temporary.path()).filePath("migration.beebeep-workspace");
  VERIFY(ShadowWorkspaceArchive::exportWorkspace(accounts.profileFolder("alice@example.com"),backup,&error));
  QString archivedEmail;
  VERIFY(ShadowWorkspaceArchive::archiveEmail(backup,&archivedEmail,&error));
  VERIFY(archivedEmail=="alice@example.com");
  ShadowLocalAccounts migrated(QDir(temporary.path()).filePath("new-computer"));
  const QString migratedFolder=migrated.profileFolder("alice@example.com");
  QString restorePoint;
  VERIFY(ShadowWorkspaceArchive::stageImport(migratedFolder,backup,&error));
  VERIFY(ShadowWorkspaceArchive::applyPendingImport(migratedFolder,&restorePoint,&error));
  // A backup owner may choose a new password without knowing the old one.
  VERIFY(migrated.setPassword("alice@example.com","restored-secret",&error));
  VERIFY(migrated.authenticate("alice@example.com","restored-secret"));
  VERIFY(!migrated.authenticate("alice@example.com","new-secret"));
  VERIFY(!migrated.hasRecoveryPassword()); // Owner recovery secret never travels with a user backup.
  accounts.logout();
  QString loggedEmail,loggedFolder;
  QTimer::singleShot(0,[&](){
    QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if(!dialog)qFatal("Login dialog missing");
    QDir().mkpath("E:/ShadowChat/delivery");
    if(!dialog->grab().save("E:/ShadowChat/delivery/login-proof.png"))qFatal("Unable to save synthetic login proof");
    const QList<QLineEdit*> edits=dialog->findChildren<QLineEdit*>();
    if(edits.size()!=2)qFatal("Login fields missing");
    edits.at(0)->setText("alice@example.com"); edits.at(1)->setText("new-secret");
    foreach(QPushButton* button,dialog->findChildren<QPushButton*>()) if(button->text()==QString::fromUtf8("登录")){button->click();return;}
    qFatal("Login button missing");
  });
  VERIFY(ShadowLocalAccounts::login(&loggedEmail,&loggedFolder,temporary.path()));
  VERIFY(loggedEmail=="alice@example.com"); VERIFY(loggedFolder==accounts.profileFolder(loggedEmail));
  // A subsequent launch uses the remembered local profile without prompting.
  VERIFY(ShadowLocalAccounts::login(&loggedEmail,&loggedFolder,temporary.path()));
  accounts.logout();
  QTimer::singleShot(0,[](){QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); if(!dialog)qFatal("Login dialog missing after logout");dialog->reject();});
  VERIFY(!ShadowLocalAccounts::login(&loggedEmail,&loggedFolder,temporary.path()));
  VERIFY(accounts.autoLoginEmail().isEmpty());
  std::puts("PASS: Local account isolation, authentication, recovery and logout");
  return 0;
}
