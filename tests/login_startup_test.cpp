#include "BeeApplication.h"
#include "ShadowLocalAccounts.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include "Settings.h"
#include "Random.h"
#include <QtWidgets>
#include <cstdio>

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
    QLabel* title = dialog->findChild<QLabel*>("LoginTitle");
    for(QLineEdit* field : dialog->findChildren<QLineEdit*>())
    {
      if(!email && field->echoMode() == QLineEdit::Normal) email = field;
      if(!password && field->echoMode() == QLineEdit::Password) password = field;
    }
    for(QPushButton* button : dialog->findChildren<QPushButton*>())
      if(!create && button->text().contains(QStringLiteral("创建本地账号"))) create = button;
    check(email && password && signIn && create && title, "all first-use login controls exist");
    if(!email || !password || !signIn || !create || !title) { dialog->reject(); return; }
    for(QWidget* widget : QList<QWidget*>() << title << email << password << signIn << create)
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
    check(!dialog->findChild<QLabel*>("LoginError")->text().isEmpty(), "invalid login keeps the form open with feedback");
    email->setText("synthetic-login@example.test");
    password->setText("synthetic-login-only");
    create->click();
  });
  QString email, folder;
  const bool accepted = ShadowLocalAccounts::login(&email, &folder, temporary.path() + "/accounts");
  check(inspected && earlyPolish.triggered, "first-use flow is exercised with early widget polish");
  check(accepted && email == "synthetic-login@example.test" && folder.startsWith(temporary.path()),
    "new account creation completes inside an isolated temporary profile");
  ShadowLocalAccounts accounts(temporary.path() + "/accounts");
  check(accounts.authenticate("synthetic-login@example.test", "synthetic-login-only"),
    "created account can authenticate after the login window closes");
  return failures ? 1 : 0;
}
