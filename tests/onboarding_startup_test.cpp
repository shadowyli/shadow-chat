#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "GuiWizard.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowDialogChrome.h"
#include "ShadowDirectoryPage.h"
#include "ShadowStyle.h"
#include <QtWidgets>
#include <cstdio>

class EarlyOnboardingPolish final : public QObject
{
public:
  bool triggered = false;

protected:
  bool eventFilter( QObject* watched, QEvent* event ) override
  {
    QDialog* dialog = qobject_cast<QDialog*>( watched );
    if( !triggered && dialog &&
        (dialog->objectName() == "GuiWizardDialog" || dialog->objectName() == "ShadowOnboardingDialog") &&
        (event->type() == QEvent::ChildAdded || event->type() == QEvent::StyleChange) && dialog->layout() )
    {
      triggered = true;
      dialog->ensurePolished();
    }
    return false;
  }
};

int main( int argc, char** argv )
{
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  QCoreApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  Random::init();
  QTemporaryDir temporary;
  if( !temporary.isValid() ) return 1;
  Settings::instance().setDefaultFolders();
  Settings::instance().createLocalUser( "synthetic-onboarding" );
  Settings::instance().setAccountDataFolder( temporary.path() + "/settings" );
  Settings::instance().load();
  ShadowDialogChrome::install( &application );
  EarlyOnboardingPolish earlyPolish;
  application.installEventFilter( &earlyPolish );
  int failures = 0;
  auto check = [&]( bool condition, const char* name ) {
    std::printf( "%s %s\n", condition ? "PASS" : "FAIL", name );
    if( !condition ) ++failures;
  };
  application.setProperty( "shadowDevPreview", true );
  Core core;
  GuiMain mainWindow;
  mainWindow.loadStyle();
  mainWindow.resize( 1000, 720 );
  mainWindow.show();
  GuiWizard wizard( &mainWindow );
  wizard.setModal( true );
  wizard.loadSettings();
  bool inspected = false;
  QTimer::singleShot( 200, &application, [&]() {
    inspected = true;
    check( QApplication::activeModalWidget() == &wizard, "first-use settings is the active modal" );
    QLineEdit* name = wizard.findChild<QLineEdit*>( "ShadowOnboardingName" );
    QPushButton* enter = wizard.findChild<QPushButton*>( "ShadowPrimaryButton" );
    QToolButton* avatar = wizard.findChild<QToolButton*>( "ShadowOnboardingAvatar" );
    QLabel* title = wizard.findChild<QLabel*>( "ShadowOnboardingTitle" );
    QWidget* surface = wizard.findChild<QWidget*>( "shadowDialogSurface" );
    check( name && enter && avatar && title, "all compact onboarding controls exist" );
    check( wizard.findChildren<QCheckBox*>().isEmpty(),
      "compact onboarding does not restore removed update or privacy options" );
    check( surface && wizard.layout()->indexOf( surface ) >= 0,
      "onboarding keeps its decorated surface inside the active root layout" );
    for( QWidget* control : QList<QWidget*>() << name << enter << avatar << title )
    {
      check( control && control->isVisible() && control->width() > 0 && control->height() > 0 &&
        wizard.rect().contains( QRect( control->mapTo( &wizard, QPoint() ), control->size() ) ),
        "first-use control is fully visible without resizing" );
      if( control )
      {
        check( control->visibleRegion().boundingRect().contains( control->rect() ),
          "first-use control is not clipped by a transparent ancestor" );
        check( surface && surface->isAncestorOf( control ),
          "first-use control belongs to the complete opaque dialog surface" );
      }
    }
    const QImage image = wizard.grab().toImage();
    int opaquePixels = 0;
    for( int vertical = 0; vertical < image.height(); ++vertical )
      for( int horizontal = 0; horizontal < image.width(); ++horizontal )
        opaquePixels += image.pixelColor( horizontal, vertical ).alpha() == 255;
    check( opaquePixels > image.width() * image.height() / 2, "onboarding has a complete readable surface" );
    wizard.grab().save( QDir::currentPath() + "/onboarding-first-open.png" );
    if( name && enter )
    {
      name->setText( "Synthetic colleague" );
      enter->click();
    }
    else wizard.reject();
  } );
  QTimer::singleShot( 6000, &wizard, &QDialog::reject );
  const int outcome = wizard.exec();
  check( inspected && earlyPolish.triggered, "early polish during construction is covered" );
  check( outcome == QDialog::Accepted, "enter action closes first-use settings" );
  application.processEvents();
  check( QApplication::activeModalWidget() == Q_NULLPTR, "no invisible modal blocks the main window" );
  QToolButton* directoryButton = Q_NULLPTR;
  for( QToolButton* button : mainWindow.findChildren<QToolButton*>( "ShadowNavButton" ) )
    if( button->accessibleName() == QStringLiteral( "通讯录" ) ) directoryButton = button;
  check( directoryButton && directoryButton->isEnabled(), "actual main-window directory action is available" );
  if( directoryButton ) directoryButton->click();
  application.processEvents();
  ShadowDirectoryPage* directory = mainWindow.findChild<ShadowDirectoryPage*>( "ShadowDirectoryPage" );
  check( directory && directory->isVisible() && mainWindow.isEnabled(),
    "actual main window opens the directory after onboarding without an invisible modal" );
  mainWindow.grab().save( QDir::currentPath() + "/onboarding-directory-open.png" );
  return failures ? 1 : 0;
}
