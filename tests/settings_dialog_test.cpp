#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include <QtTest>
#include <cstdio>

static bool verify( bool condition, const char* description )
{
  std::printf( "%s %s\n", condition ? "PASS" : "FAIL", description );
  return condition;
}

static QRect boundsIn( QWidget* widget, QWidget* ancestor )
{
  return QRect( widget->mapTo( ancestor, QPoint() ), widget->size() );
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  ShadowDialogChrome::install( &application );
  Random::init();
  QTemporaryDir temporary;
  if( !temporary.isValid() ) return 1;
  application.setProperty( "shadowAccountRoot", temporary.path() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "settings-dialog-synthetic" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  settings.setBeepOnNewMessageArrived( true );
  const QString folder = QDir::toNativeSeparators( temporary.path() +
    "/synthetic-team/long-folder-for-path-overflow-verification/received-files" );
  QDir().mkpath( folder );
  settings.setDownloadDirectory( folder );
  Core core;
  GuiMain main;
  main.loadStyle();
  main.resize( 1000, 700 );
  main.show();
  QTest::qWait( 100 );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR" );
  if( !proof.isEmpty() ) QDir().mkpath( proof );
  bool passed = true;
  QTimer guard;
  guard.setSingleShot( true );
  QObject::connect( &guard, &QTimer::timeout, []() {
    std::fprintf( stderr, "FAIL dialog did not close\n" );
    std::exit( 1 );
  } );
  for( bool save : { false, true } )
  {
    QTimer::singleShot( 150, &main, [&]() {
      auto* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
      if( !dialog ) { passed = false; return; }
      auto* surface = dialog->findChild<QWidget*>( "shadowDialogSurface" );
      auto* scroll = dialog->findChild<QScrollArea*>();
      auto* footer = dialog->findChild<QWidget*>( "ShadowSettingsFooter" );
      auto* buttons = dialog->findChild<QDialogButtonBox*>();
      auto* path = dialog->findChild<QLineEdit*>( "ShadowSettingsDownloadFolder" );
      auto* notes = dialog->findChild<QPlainTextEdit*>( "ShadowUpdateSettingsNotes" );
      passed &= verify( surface && scroll && footer && buttons && path && notes,
        "settings has a unified surface, scroll body, fixed footer and path field" );
      if( !(surface && scroll && footer && buttons && path && notes) )
      {
        dialog->reject();
        return;
      }
      for( const QSize& size : { QSize( 650, 670 ), QSize( 480, 380 ) } )
      {
        dialog->resize( size );
        QTest::qWait( 80 );
        passed &= verify( dialog->size() == size, "settings can shrink to requested size" );
        passed &= verify( surface->rect().contains( boundsIn( footer, surface ) ),
          "footer remains inside rounded surface" );
        passed &= verify( !scroll->isAncestorOf( buttons ), "save and cancel stay outside scrolling content" );
        for( QAbstractButton* button : buttons->buttons() )
          passed &= verify( button->isVisible() && surface->rect().contains( boundsIn( button, surface ) ),
            "footer action fully visible" );
        const QImage image = dialog->grab().toImage();
        const qreal ratio = image.devicePixelRatio();
        for( const QPoint& corner : { QPoint( 15, 15 ), QPoint( dialog->width() - 16, 15 ),
          QPoint( 15, dialog->height() - 16 ), QPoint( dialog->width() - 16, dialog->height() - 16 ) } )
          passed &= verify( image.pixelColor( qRound( corner.x() * ratio ),
            qRound( corner.y() * ratio ) ).alpha() < 160, "corner has no opaque rectangular overflow" );
        const QImage background = scroll->viewport()->grab().toImage();
        const QColor gutter = background.pixelColor( qRound( 8 * ratio ), qRound( 8 * ratio ) );
        passed &= verify( gutter.rgb() == QColor( "#FFFFFF" ).rgb(), "scroll gutter matches white dialog surface" );
        passed &= verify( path->text() == folder && path->toolTip() == folder &&
          path->cursorPosition() == 0, "long path preserved with full tooltip and visible beginning" );
        const QRect buttonsRect = boundsIn( buttons, surface );
        passed &= verify( buttonsRect.center().x() > surface->width() / 2,
          "footer actions align to the right" );
        if( !save && !proof.isEmpty() )
          passed &= verify( dialog->grab().save( proof + QString( "/settings-%1x%2.png" )
            .arg( size.width() ).arg( size.height() ) ), "settings screenshot saved" );
      }
      const QPoint before = buttons->mapTo( dialog, QPoint() );
      notes->setPlainText( QString( 600, QChar( 0x66F4 ) ) );
      notes->show();
      QTest::qWait( 60 );
      scroll->verticalScrollBar()->setValue( scroll->verticalScrollBar()->maximum() );
      QTest::qWait( 60 );
      passed &= verify( scroll->verticalScrollBar()->maximum() > 0 &&
        before == buttons->mapTo( dialog, QPoint() ), "update notes scroll without moving action buttons" );
      for( QCheckBox* check : dialog->findChildren<QCheckBox*>() )
        if( check->text().contains( QStringLiteral( "提示音" ) ) ) check->setChecked( false );
      buttons->button( save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel )->click();
    } );
    guard.start( 10000 );
    passed &= QMetaObject::invokeMethod( &main, "showShadowSettings", Qt::DirectConnection );
    guard.stop();
    passed &= verify( settings.beepOnNewMessageArrived() == !save,
      save ? "save persists changed preference" : "cancel retains original preference" );
  }
  QTimer::singleShot( 150, &main, [&]() {
    auto* about = main.findChild<QDialog*>( "ShadowAboutDialog" );
    if( !about ) { passed = false; return; }
    passed &= verify( !about->findChild<QLabel*>( "ShadowAboutAttribution" ),
      "about omits the requested attribution text block" );
    auto* description = about->findChild<QLabel*>( "ShadowAboutDescription" );
    passed &= verify( description && description->text().contains( "shadowyli" ) &&
      description->text().contains( settings.version( false, false, false ) ),
      "about retains maintainer and actual build version" );
    if( !proof.isEmpty() ) passed &= about->grab().save( proof + "/about-compact.png" );
    about->reject();
  } );
  guard.start( 10000 );
  passed &= QMetaObject::invokeMethod( &main, "showAbout", Qt::DirectConnection );
  guard.stop();
  std::printf( "%s settings and about, platform=%s DPR=%.2f\n", passed ? "PASS" : "FAIL",
    qPrintable( QApplication::platformName() ), main.devicePixelRatioF() );
  return passed ? 0 : 1;
}
