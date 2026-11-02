#include "ShadowLocalAccounts.h"
#include <QtTest>
#include <QtWidgets>
#include <functional>

namespace {
bool activateDomain( QComboBox* domains, const QString& domain )
{
  const int index=domains->findData(domain);
  if(index<0) return false;
  domains->setCurrentIndex(index);
  return QMetaObject::invokeMethod(domains,"activated",Qt::DirectConnection,Q_ARG(int,index));
}

bool runLogin( const QString& root, const std::function<void(QDialog*)>& interaction,
               QString* selected, QString* folder )
{
  ShadowLocalAccounts(root).logout();
  QTimer::singleShot(100,[&](){
    QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if(!dialog) {QTest::qFail("Local login dialog missing",__FILE__,__LINE__);return;}
    QTimer::singleShot(3000,dialog,[](){
      for(QWidget* widget : QApplication::topLevelWidgets())
        if(QDialog* pending=qobject_cast<QDialog*>(widget)) pending->reject();
    });
    interaction(dialog);
  });
  return ShadowLocalAccounts::login(selected,folder,root);
}
}

class LoginEmailDomainsTest : public QObject
{
  Q_OBJECT
private slots:
  void suffixSelection_data()
  {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("domain");
    QTest::addColumn<QString>("expected");
    QTest::newRow("bare-name") << "alice" << "corp.netease.com" << "alice";
    QTest::newRow("complete-email") << "alice@qq.com" << "corp.netease.com" << "alice";
    QTest::newRow("switch-from-corp") << "alice@corp.netease.com" << "163.com" << "alice";
    QTest::newRow("reselect-same-domain") << "alice@qq.com" << "qq.com" << "alice";
    QTest::newRow("trim-input") << " Alice+team@qq.com " << "gmail.com" << "Alice+team";
    QTest::newRow("empty-input") << "" << "corp.netease.com" << "";
  }

  void suffixSelection()
  {
    QFETCH(QString,input);
    QFETCH(QString,domain);
    QFETCH(QString,expected);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString selected,folder;
    QVERIFY(!runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      address->setText(input);
      QVERIFY(activateDomain(domains,domain));
      QCOMPARE(address->text(),expected);
      QCOMPARE(address->text().count('@'),0);
      QCOMPARE(domains->currentData().toString(),domain);
      QCOMPARE(address->cursorPosition(),input.isEmpty() ? 0 : expected.size());
      dialog->reject();
    },&selected,&folder));
  }

  void keyboardAndPresentation()
  {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString selected,folder;
    QVERIFY(!runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      QCOMPARE(dialog->findChildren<QLineEdit*>().size(),2);
      QVERIFY(!address->isReadOnly());
      QVERIFY(qobject_cast<QListView*>(domains->view()));
      QVERIFY(!QIcon(":/images/shadowchat/icons/chevron-down.svg").pixmap(14,14).isNull());
      QCOMPARE(domains->count(),19);
      QVERIFY(!domains->isEditable());
      QCOMPARE(domains->findData(QString()),-1);
      QCOMPARE(domains->itemData(domains->findData("corp.netease.com"),Qt::ToolTipRole).toString(),QString::fromUtf8("corp 邮箱"));
      QVERIFY(!dialog->findChild<QPushButton*>("LoginCreateAccount"));
      QVERIFY(!dialog->findChild<QLabel*>("LoginHint"));
      QPushButton* signIn=dialog->findChild<QPushButton*>("LoginPrimary");
      QLabel* autoCreateHint=dialog->findChild<QLabel*>("LoginAutoCreateHint");
      QVERIFY(signIn);
      QVERIFY(autoCreateHint);
      QCOMPARE(signIn->text(),QString::fromUtf8("一键登录"));
      QCOMPARE(autoCreateHint->text(),QString::fromUtf8("未创建的账号将自动创建"));
      QVERIFY(autoCreateHint->mapTo(dialog,QPoint()).y()>=signIn->mapTo(dialog,QPoint(0,signIn->height())).y());
      QCOMPARE(address->mapTo(dialog,QPoint()).y(),domains->mapTo(dialog,QPoint()).y());
      QVERIFY(address->width()>=120);
      QVERIFY(domains->mapTo(dialog,QPoint()).x()>address->mapTo(dialog,QPoint(address->width(),0)).x());
      QStyleOptionComboBox domainStyle;
      domainStyle.initFrom(domains);
      const QRect domainText=domains->style()->subControlRect(QStyle::CC_ComboBox,&domainStyle,QStyle::SC_ComboBoxEditField,domains);
      QVERIFY2(domainText.width()>=domains->fontMetrics().horizontalAdvance(QStringLiteral("@corp.netease.com")),
        qPrintable(QString("combo=%1 textRect=%2 text=%3").arg(domains->width()).arg(domainText.width()).arg(domains->fontMetrics().horizontalAdvance(QStringLiteral("@corp.netease.com")))));
      const QStringList common=QStringList() << "163.com" << "126.com" << "qq.com" << "foxmail.com" << "yeah.net"
        << "netease.com" << "gmail.com" << "outlook.com" << "hotmail.com" << "icloud.com"
        << "vip.qq.com" << "yahoo.com" << "live.com" << "sina.com" << "sohu.com" << "139.com" << "189.cn" << "aliyun.com";
      for(const QString& domain : common)
      {
        const int index=domains->findData(domain);
        QVERIFY(index>0);
        QCOMPARE(domains->itemText(index),"@"+domain);
        address->setText("alice");
        QVERIFY(activateDomain(domains,domain));
        QCOMPARE(address->text(),QString("alice"));
        QCOMPARE(address->text().count('@'),0);
        address->setText("bob@"+domain.toUpper());
        QCOMPARE(domains->currentData().toString(),domain);
      }
      QVERIFY(!dialog->styleSheet().contains("gradient",Qt::CaseInsensitive));
      QVERIFY(dialog->rect().contains(QRect(domains->mapTo(dialog,QPoint()),domains->size())));
      address->clear();
      domains->setCurrentIndex(0);
      domains->setFocus();
      QTest::keyClick(domains,Qt::Key_Down);
      QCOMPARE(domains->currentData().toString(),QString("163.com"));
      QVERIFY(address->text().isEmpty());
      QCOMPARE(address->cursorPosition(),0);
      QTest::keyClicks(address,"alice");
      QCOMPARE(address->text(),QString("alice"));
      domains->setCurrentIndex(0);
      const QString proofs=qEnvironmentVariable("SHADOWCHAT_EMAIL_TEST_ARTIFACTS");
      if(!proofs.isEmpty())
      {
        QVERIFY(dialog->grab().save(QDir(proofs).filePath("login-email-domains.png")));
        domains->showPopup();
        QApplication::processEvents();
        QVERIFY(domains->view()->window()->grab().save(QDir(proofs).filePath("login-email-domains-popup.png")));
        domains->hidePopup();
      }
      dialog->reject();
    },&selected,&folder));
  }

  void rejectsCustomAddresses()
  {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString selected,folder;
    QVERIFY(!runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      address->setText(" Alice@CORP.NETEASE.COM ");
      QCOMPARE(address->text(),QString("Alice"));
      QCOMPARE(domains->currentData().toString(),QString("corp.netease.com"));
      QLineEdit* password=dialog->findChild<QLineEdit*>("LoginPassword");
      QPushButton* signIn=dialog->findChild<QPushButton*>("LoginPrimary");
      QVERIFY(password);
      QVERIFY(signIn);
      password->setText("local-secret");
      ShadowLocalAccounts accounts(temporary.path());
      const QStringList rejected=QStringList()<<"alice@studio.example.org"<<"alice@@qq.com"<<"alice@"<<"../alice";
      for(const QString& value : rejected)
      {
        address->setText(value);
        signIn->click();
        QVERIFY(dialog->isVisible());
        QVERIFY(!dialog->findChild<QLabel*>("LoginError")->text().isEmpty());
        QVERIFY(!accounts.exists("alice@studio.example.org"));
        QVERIFY(accounts.autoLoginEmail().isEmpty());
      }
      address->selectAll();
      QApplication::clipboard()->setText("other@qq.com");
      address->paste();
      QCOMPARE(address->text(),QString("other"));
      QCOMPARE(domains->currentData().toString(),QString("qq.com"));
      address->clear();
      dialog->findChild<QPushButton*>("LoginPrimary")->click();
      QVERIFY(dialog->isVisible());
      QVERIFY(!dialog->findChild<QLabel*>("LoginError")->text().isEmpty());
      dialog->reject();
    },&selected,&folder));
  }

  void localRegistrationAndLogin_data()
  {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("domain");
    QTest::addColumn<QString>("expected");
    QTest::newRow("corp") << "Alice" << "corp.netease.com" << "alice@corp.netease.com";
    QTest::newRow("common") << "Alice" << "qq.com" << "alice@qq.com";
    QTest::newRow("complete-supported") << " Alice@Gmail.com " << "gmail.com" << "alice@gmail.com";
  }

  void localRegistrationAndLogin()
  {
    QFETCH(QString,input);
    QFETCH(QString,domain);
    QFETCH(QString,expected);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ShadowLocalAccounts accounts(temporary.path());
    QString selected,folder;
    QVERIFY(runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      address->setText(input);
      if(!domain.isEmpty()) QVERIFY(activateDomain(domains,domain));
      dialog->findChild<QLineEdit*>("LoginPassword")->setText("local-secret");
      dialog->findChild<QPushButton*>("LoginPrimary")->click();
    },&selected,&folder));
    QCOMPARE(selected,expected);
    QCOMPARE(folder,accounts.profileFolder(expected));
    QVERIFY(accounts.exists(expected));
    QVERIFY(accounts.authenticate(expected,"local-secret"));
    QVERIFY(!accounts.authenticate(expected,"wrong-secret"));
    QCOMPARE(accounts.autoLoginEmail(),expected);
    QVERIFY(runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      address->setText(domain.isEmpty() ? expected : "Alice");
      if(!domain.isEmpty()) QVERIFY(activateDomain(domains,domain));
      QLineEdit* password=dialog->findChild<QLineEdit*>("LoginPassword");
      password->setText("wrong-secret");
      dialog->findChild<QPushButton*>("LoginPrimary")->click();
      QVERIFY(dialog->isVisible());
      password->setText("local-secret");
      QTest::keyClick(password,Qt::Key_Return);
    },&selected,&folder));
    QCOMPARE(selected,expected);
    QCOMPARE(folder,accounts.profileFolder(expected));
    QString automaticEmail,automaticFolder;
    QVERIFY(ShadowLocalAccounts::login(&automaticEmail,&automaticFolder,temporary.path()));
    QCOMPARE(automaticEmail,expected);
    QCOMPARE(automaticFolder,folder);
  }

  void passwordRecoveryUsesSelectedAddress()
  {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ShadowLocalAccounts accounts(temporary.path());
    QString error;
    QVERIFY(accounts.create("alice@corp.netease.com","old-local-secret",&error));
    QVERIFY(accounts.setRecoveryPassword("fixture-owner-recovery"));
    QString selected,folder;
    QVERIFY(!runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      address->setText("alice@qq.com");
      QVERIFY(activateDomain(domains,"corp.netease.com"));
      QTimer::singleShot(0,dialog,[&](){
        QInputDialog* recovery=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        QVERIFY(recovery);
        recovery->setTextValue("fixture-owner-recovery");
        QTimer::singleShot(0,dialog,[&](){
          QInputDialog* replacement=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
          QVERIFY(replacement);
          replacement->setTextValue("new-local-secret");
          replacement->accept();
        });
        recovery->accept();
      });
      dialog->findChild<QPushButton*>("LoginRecoverPassword")->click();
      QCOMPARE(dialog->findChild<QLabel*>("LoginError")->text(),QString::fromUtf8("密码已重设，请登录。"));
      QVERIFY(accounts.authenticate("alice@corp.netease.com","new-local-secret"));
      QVERIFY(!accounts.authenticate("alice@corp.netease.com","old-local-secret"));
      QVERIFY(!accounts.exists("alice@qq.com"));
      dialog->reject();
    },&selected,&folder));
  }

  void backupRecoveryUsesSelectedAddress()
  {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ShadowLocalAccounts accounts(temporary.path());
    QString error;
    QVERIFY(accounts.create("alice@corp.netease.com","local-secret",&error));
    QString selected,folder;
    QVERIFY(!runLogin(temporary.path(),[&](QDialog* dialog){
      QLineEdit* address=dialog->findChild<QLineEdit*>("LoginEmail");
      QComboBox* domains=dialog->findChild<QComboBox*>("LoginEmailDomain");
      QVERIFY(address);
      QVERIFY(domains);
      address->setText("alice@qq.com");
      QVERIFY(activateDomain(domains,"corp.netease.com"));
      dialog->findChild<QLineEdit*>("LoginPassword")->setText("local-secret");
      dialog->findChild<QPushButton*>("LoginRestoreBackup")->click();
      QCOMPARE(dialog->findChild<QLabel*>("LoginError")->text(),
        QString::fromUtf8("该账号已存在，请先登录后使用工作区导入，避免覆盖。"));
      QVERIFY(accounts.authenticate("alice@corp.netease.com","local-secret"));
      QVERIFY(!accounts.exists("alice@qq.com"));
      dialog->reject();
    },&selected,&folder));
  }
};

int main( int argc, char** argv )
{
  if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
  QApplication app(argc,argv);
#ifdef Q_OS_WIN
  QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR","C:/Windows")).filePath("Fonts/msyh.ttc"));
#endif
  if(app.arguments().contains("--preview-login"))
  {
    QTemporaryDir temporary;
    if(!temporary.isValid())return 1;
    QTimer::singleShot(0,[](){
      if(QDialog* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))
        dialog->setWindowTitle(QString::fromUtf8("影聊 · 登录预览"));
    });
    QString email,folder;
    ShadowLocalAccounts::login(&email,&folder,temporary.path());
    return 0;
  }
  LoginEmailDomainsTest test;
  return QTest::qExec(&test,argc,argv);
}

#include "login_email_domains_test.moc"
