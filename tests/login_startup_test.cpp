#include "BeeApplication.h"
#include "ShadowLocalAccounts.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include "Settings.h"
#include "Random.h"
#include <QtWidgets>
#include <QProcess>
#include <cstdio>
#include <functional>

class EarlyLoginPolish final : public QObject
{
public:
  bool triggered = false;
protected:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    QDialog* dialog = qobject_cast<QDialog*>(watched);
    if(!triggered && dialog && dialog->objectName() == "ShadowAccountLogin" && event->type() == QEvent::ChildAdded && dialog->layout())
    {
      triggered = true;
      dialog->ensurePolished();
    }
    return false;
  }
};

int main(int argc, char** argv)
{
  std::setvbuf(stdout, Q_NULLPTR, _IONBF, 0);
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  BeeApplication app(argc, argv);
  const QString accountEmail = "synthetic-login@example.test";
  const QString accountPassword = "synthetic-login-only";
  if(app.arguments().size() == 3 && app.arguments().at(1) == "--verify-persisted-login")
  {
    const QString accountRoot = app.arguments().at(2);
    app.setProperty("shadowAccountRoot", accountRoot);
    ShadowLocalAccounts persisted;
    bool prompted = false;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &app, [&]() {
      prompted = true;
      QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if(dialog) dialog->reject();
    });
    guard.start(0);
    QString email, folder;
    const bool accepted = ShadowLocalAccounts::login(&email, &folder);
    return accepted && !prompted && persisted.root() == accountRoot &&
      persisted.autoLoginEmail() == accountEmail &&
      persisted.authenticate(accountEmail, accountPassword) &&
      email == accountEmail && folder == persisted.profileFolder(accountEmail) ? 0 : 1;
  }
  app.setFont(ShadowStyle::applicationFont());
  Random::init();
  QTemporaryDir temporary;
  if(!temporary.isValid()) return 1;
  Settings::instance().setDefaultFolders();
  Settings::instance().createLocalUser("synthetic-login");
  Settings::instance().setAccountDataFolder(temporary.path() + "/settings");
  Settings::instance().load();
  ShadowDialogChrome::install(&app);
  EarlyLoginPolish earlyPolish;
  app.installEventFilter(&earlyPolish);
  int failures = 0;
  auto check = [&](bool condition, const char* name) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if(!condition) ++failures;
  };
  const QString accountRoot = temporary.path() + "/accounts";
  ShadowLocalAccounts accounts(accountRoot);
  bool inspected = false;
  QTimer::singleShot(150, &app, [&]() {
    QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if(!dialog || dialog->objectName() != "ShadowAccountLogin") { app.exit(1); return; }
    inspected = true;
    check(!dialog->testAttribute(Qt::WA_TranslucentBackground) &&
      !dialog->windowFlags().testFlag(Qt::FramelessWindowHint),
      "first login uses an opaque normal window before global dialog decoration");
    QLineEdit* email = dialog->findChild<QLineEdit*>("LoginEmail");
    QLineEdit* password = dialog->findChild<QLineEdit*>("LoginPassword");
    QPushButton* signIn = dialog->findChild<QPushButton*>("LoginPrimary");
    QPushButton* create = dialog->findChild<QPushButton*>("LoginCreateAccount");
    QPushButton* recover = Q_NULLPTR;
    QPushButton* restore = Q_NULLPTR;
    QLabel* title = dialog->findChild<QLabel*>("LoginTitle");
    QLabel* message = dialog->findChild<QLabel*>("LoginError");
    for(QLineEdit* field : dialog->findChildren<QLineEdit*>())
    {
      if(!email && field->echoMode() == QLineEdit::Normal) email = field;
      if(!password && field->echoMode() == QLineEdit::Password) password = field;
    }
    for(QPushButton* button : dialog->findChildren<QPushButton*>())
    {
      if(!create && button->text().contains(QStringLiteral("创建本地账号"))) create = button;
      if(button->text() == QStringLiteral("忘记密码")) recover = button;
      if(button->text() == QStringLiteral("从备份恢复")) restore = button;
    }
    check(!create, "first-use login has no separate create-account button");
    check(email && password && signIn && recover && restore && title && message,
      "login, password recovery and backup restore controls exist");
    if(!email || !password || !signIn || !recover || !restore || !title || !message) { dialog->reject(); return; }
    check(signIn->isDefault(), "login is the default action for the first-use form");
    for(QWidget* widget : QList<QWidget*>() << title << email << password << signIn << recover << restore)
      check(widget->isVisible() && dialog->rect().contains(QRect(widget->mapTo(dialog, QPoint()), widget->size())),
        "login control is visible inside the initial window without resizing");
    const QImage image = dialog->grab().toImage();
    int whitePixels = 0, transparentPixels = 0;
    for(int vertical = 0; vertical < image.height(); ++vertical)
      for(int horizontal = 0; horizontal < image.width(); ++horizontal)
      {
        const QColor color = image.pixelColor(horizontal, vertical);
        whitePixels += color.red() > 245 && color.green() > 245 && color.blue() > 245 && color.alpha() == 255;
        transparentPixels += color.alpha() < 255;
      }
    check(whitePixels > image.width() * image.height() / 3 && transparentPixels == 0,
      "login raster has a complete opaque background instead of floating input fields");
    dialog->grab().save(QDir::currentPath() + "/login-first-open.png");
    signIn->click();
    check(dialog->isVisible() && message->isVisible() && message->text().contains(QStringLiteral("有效")),
      "empty login keeps the form open with visible email feedback");
    recover->click();
    check(message->text().contains(QStringLiteral("已存在")), "password recovery still validates the entered account");
    restore->click();
    check(message->text().contains(QStringLiteral("邮箱")), "backup restore still validates the entered account");
    email->setText("../invalid@example.test");
    password->setText(accountPassword);
    signIn->click();
    check(dialog->isVisible() && message->isVisible() && message->text().contains(QStringLiteral("有效")) &&
      !QFileInfo::exists(accountRoot),
      "invalid email cannot create a profile or close the login form");
    email->setText("short-login@example.test");
    password->setText("12345");
    signIn->click();
    check(dialog->isVisible() && message->isVisible() && message->text().contains(QStringLiteral("6")) &&
      !accounts.exists(email->text()) && accounts.autoLoginEmail().isEmpty(),
      "short password cannot create or remember an account");
    email->setText(" SYNTHETIC-LOGIN@EXAMPLE.TEST ");
    password->setText(accountPassword);
    signIn->click();
  });
  QString email, folder;
  const bool accepted = ShadowLocalAccounts::login(&email, &folder, accountRoot);
  check(inspected && earlyPolish.triggered, "first-use flow is exercised with early widget polish");
  check(accepted && email == accountEmail && folder == accounts.profileFolder(accountEmail),
    "login automatically creates a normalized new account inside the isolated profile");
  check(accounts.authenticate(accountEmail, accountPassword),
    "created account can authenticate after the login window closes");
  const QString metadataPath = QDir(accounts.profileFolder(accountEmail)).filePath(".shadow-account.ini");
  QFile metadata(metadataPath);
  const bool metadataOpened = metadata.open(QIODevice::ReadOnly);
  check(metadataOpened, "new account metadata is persisted on disk");
  const QByteArray originalMetadata = metadataOpened ? metadata.readAll() : QByteArray();
  metadata.close();
  check(!originalMetadata.isEmpty() && !originalMetadata.contains(accountPassword.toUtf8()),
    "persisted account metadata does not contain the plaintext password");
  QSettings session(QDir(accountRoot).filePath("session.ini"), QSettings::IniFormat);
  check(session.value("autoLoginEmail").toString() == accountEmail,
    "successful first login persists the normalized session");
  QProcess restarted;
  restarted.start(QCoreApplication::applicationFilePath(),
    QStringList() << "--verify-persisted-login" << accountRoot);
  const bool restartedFinished = restarted.waitForStarted(5000) && restarted.waitForFinished(15000);
  if(!restartedFinished)
  {
    restarted.kill();
    restarted.waitForFinished(5000);
  }
  check(restartedFinished && restarted.exitStatus() == QProcess::NormalExit && restarted.exitCode() == 0,
    "a fresh process reloads credentials and the remembered login without opening a dialog");
  if(!restartedFinished || restarted.exitCode() != 0)
    std::fprintf(stderr, "%s\n", restarted.readAllStandardError().constData());

  auto visitLogin = [&](const QString& root, const std::function<void(QDialog*)>& interact,
    QString* selectedEmail, QString* selectedFolder) {
    bool visited = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &app, [&]() {
      QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if(!dialog || dialog->objectName() != "ShadowAccountLogin") qFatal("Login dialog missing");
      visited = true;
      interact(dialog);
      if(dialog->isVisible()) dialog->reject();
    });
    timer.start(150);
    const bool result = ShadowLocalAccounts::login(selectedEmail, selectedFolder, root);
    check(visited, "login dialog is shown when no automatic session is available");
    return result;
  };
  accounts.logout();
  ShadowLocalAccounts loggedOut(accountRoot);
  check(loggedOut.exists(accountEmail) && loggedOut.autoLoginEmail().isEmpty(),
    "logout clears the persisted session while keeping the local account");
  email = "unchanged-email";
  folder = "unchanged-folder";
  check(!visitLogin(accountRoot, [](QDialog* dialog) { dialog->reject(); }, &email, &folder) &&
    email == "unchanged-email" && folder == "unchanged-folder" && loggedOut.autoLoginEmail().isEmpty(),
    "cancelling login after logout does not enter the main window or restore the session");

  const bool existingAccepted = visitLogin(accountRoot, [&](QDialog* dialog) {
    QLineEdit* address = dialog->findChild<QLineEdit*>("LoginEmail");
    QLineEdit* password = dialog->findChild<QLineEdit*>("LoginPassword");
    QPushButton* signIn = dialog->findChild<QPushButton*>("LoginPrimary");
    QLabel* message = dialog->findChild<QLabel*>("LoginError");
    if(!address || !password || !signIn || !message) qFatal("Existing account login controls missing");
    address->setText(" SYNTHETIC-LOGIN@EXAMPLE.TEST ");
    password->setText("wrong-existing-password");
    signIn->click();
    metadata.open(QIODevice::ReadOnly);
    const QByteArray afterWrongPassword = metadata.readAll();
    metadata.close();
    check(dialog->isVisible() && message->isVisible() && message->text().contains(QStringLiteral("密码不正确")) &&
      accounts.autoLoginEmail().isEmpty() && afterWrongPassword == originalMetadata &&
      accounts.authenticate(accountEmail, accountPassword) &&
      !accounts.authenticate(accountEmail, "wrong-existing-password"),
      "wrong password for an existing email neither logs in nor resets stored credentials");
    password->setText("123");
    signIn->click();
    check(dialog->isVisible() && message->text().contains(QStringLiteral("6")) &&
      accounts.authenticate(accountEmail, accountPassword),
      "short password for an existing account cannot replace its password");
    for(QPushButton* button : dialog->findChildren<QPushButton*>())
    {
      if(button->text() == QStringLiteral("忘记密码"))
      {
        button->click();
        check(message->text().contains(QStringLiteral("尚未配置")),
          "existing-account password recovery remains available");
      }
    }
    password->setText(accountPassword);
    for(QPushButton* button : dialog->findChildren<QPushButton*>())
    {
      if(button->text() == QStringLiteral("从备份恢复"))
      {
        button->click();
        check(message->text().contains(QStringLiteral("账号已存在")),
          "backup restore refuses to overwrite an existing account");
      }
    }
    QMetaObject::invokeMethod(password, "returnPressed", Qt::DirectConnection);
  }, &email, &folder);
  check(existingAccepted && email == accountEmail && folder == accounts.profileFolder(accountEmail) &&
    accounts.autoLoginEmail() == accountEmail,
    "the original password logs into an existing account through the default login action");
  metadata.open(QIODevice::ReadOnly);
  check(metadata.readAll() == originalMetadata, "successful existing-account login preserves password metadata");
  metadata.close();

  accounts.setAutoLoginEnabled(false);
  ShadowLocalAccounts automaticDisabled(accountRoot);
  check(!automaticDisabled.autoLoginEnabled() && automaticDisabled.autoLoginEmail().isEmpty(),
    "disabling automatic login persists across account instances");
  check(!visitLogin(accountRoot, [](QDialog* dialog) { dialog->reject(); }, &email, &folder),
    "a remembered account still prompts when automatic login is disabled");
  accounts.setAutoLoginEnabled(true);
  check(accounts.autoLoginEmail() == accountEmail, "re-enabling automatic login reuses the saved session");

  const QString blockedRoot = temporary.path() + "/blocked-account-root";
  QFile blockedFile(blockedRoot);
  const bool blockedFileOpened = blockedFile.open(QIODevice::WriteOnly);
  check(blockedFileOpened, "account-folder persistence failure is prepared");
  if(blockedFileOpened) blockedFile.write("block account folder creation");
  blockedFile.close();
  ShadowLocalAccounts blocked(blockedRoot);
  email = "unchanged-email";
  folder = "unchanged-folder";
  check(!visitLogin(blockedRoot, [&](QDialog* dialog) {
    QLineEdit* address = dialog->findChild<QLineEdit*>("LoginEmail");
    QLineEdit* password = dialog->findChild<QLineEdit*>("LoginPassword");
    QPushButton* signIn = dialog->findChild<QPushButton*>("LoginPrimary");
    QLabel* message = dialog->findChild<QLabel*>("LoginError");
    if(!address || !password || !signIn || !message) qFatal("Blocked-account login controls missing");
    address->setText(accountEmail);
    password->setText(accountPassword);
    signIn->click();
    check(dialog->isVisible() && message->isVisible() && message->text().contains(QStringLiteral("无法创建账号文件夹")) &&
      !blocked.exists(accountEmail) && blocked.autoLoginEmail().isEmpty(),
      "account persistence failure stays on the form with a clear error");
  }, &email, &folder) && email == "unchanged-email" && folder == "unchanged-folder",
    "failed account creation never returns an accepted profile");

  const QString blockedSessionRoot = temporary.path() + "/blocked-session";
  const QString blockedSessionPath = QDir(blockedSessionRoot).filePath("session.ini");
  check(QDir().mkpath(blockedSessionPath), "session persistence failure is prepared");
  ShadowLocalAccounts blockedSession(blockedSessionRoot);
  check(!visitLogin(blockedSessionRoot, [&](QDialog* dialog) {
    QLineEdit* address = dialog->findChild<QLineEdit*>("LoginEmail");
    QLineEdit* password = dialog->findChild<QLineEdit*>("LoginPassword");
    QPushButton* signIn = dialog->findChild<QPushButton*>("LoginPrimary");
    QLabel* message = dialog->findChild<QLabel*>("LoginError");
    if(!address || !password || !signIn || !message) qFatal("Blocked-session login controls missing");
    address->setText(accountEmail);
    password->setText(accountPassword);
    signIn->click();
    check(dialog->isVisible() && message->isVisible() && message->text().contains(QStringLiteral("无法保存登录状态")) &&
      blockedSession.authenticate(accountEmail, accountPassword) && blockedSession.autoLoginEmail().isEmpty(),
      "failed session save keeps the account but cannot accept or cache an automatic login");
  }, &email, &folder) && email == "unchanged-email" && folder == "unchanged-folder",
    "failed session persistence never returns an accepted profile");
  check(QDir().rmdir(blockedSessionPath), "session persistence is restored for a retry");
  check(visitLogin(blockedSessionRoot, [&](QDialog* dialog) {
    QLineEdit* address = dialog->findChild<QLineEdit*>("LoginEmail");
    QLineEdit* password = dialog->findChild<QLineEdit*>("LoginPassword");
    QPushButton* signIn = dialog->findChild<QPushButton*>("LoginPrimary");
    if(!address || !password || !signIn) qFatal("Session retry login controls missing");
    address->setText(accountEmail);
    password->setText(accountPassword);
    signIn->click();
  }, &email, &folder) && email == accountEmail && folder == blockedSession.profileFolder(accountEmail),
    "retrying with the original password succeeds after storage becomes writable");
  return failures ? 1 : 0;
}
