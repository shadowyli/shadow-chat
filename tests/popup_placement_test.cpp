#include "ShadowBirthdayEdit.h"
#include "ShadowCalendarPopup.h"
#include "ShadowColorPanel.h"
#include "ShadowPopup.h"
#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "GuiEditVCard.h"
#include "GuiVCard.h"
#include "GuiStickerPicker.h"
#include "ShadowStickerStore.h"
#include "GuiMessageEdit.h"
#include "ShadowComposerFormatBar.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include "Settings.h"
#include "IconManager.h"
#include "Random.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFontInfo>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

static void flushEvents()
{
  QApplication::processEvents();
  QApplication::sendPostedEvents( Q_NULLPTR, QEvent::DeferredDelete );
}

static QString proofDir()
{
  const QString from_env = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR" );
  return from_env.isEmpty() ? QDir::currentPath() : from_env;
}

static bool saveProof( QWidget* widget, const QString& name )
{
  QDir().mkpath( proofDir() );
  return widget && widget->grab().save( proofDir() + QLatin1Char( '/' ) + name );
}

static bool completeShadow( QWidget* widget )
{
  const QImage pixels = widget->grab().toImage();
  const qreal ratio = pixels.devicePixelRatio();
  const int margin = ShadowPopup::shadowMargin / 2;
  const QList<QPoint> samples = QList<QPoint>() << QPoint( margin, widget->height()/2 ) <<
    QPoint( widget->width()-margin-1, widget->height()/2 ) <<
    QPoint( widget->width()/2, margin ) << QPoint( widget->width()/2, widget->height()-margin-1 );
  if( pixels.pixelColor( 0, 0 ).alpha() != 0 )
    return false;
  for( const QPoint& point : samples )
  {
    const int alpha = pixels.pixelColor( qRound( point.x()*ratio ), qRound( point.y()*ratio ) ).alpha();
    if( alpha <= 0 || alpha >= 255 )
      return false;
  }
  for( const QPoint& direction : QList<QPoint>{ QPoint( 1, 0 ), QPoint( -1, 0 ),
       QPoint( 0, 1 ), QPoint( 0, -1 ) } )
  {
    const QPoint edge( direction.x() > 0 ? 0 : direction.x() < 0 ? widget->width()-1 : widget->width()/2,
      direction.y() > 0 ? 0 : direction.y() < 0 ? widget->height()-1 : widget->height()/2 );
    const QPoint outside = edge + direction * 2;
    const QPoint inside = edge + direction * (ShadowPopup::shadowMargin-2);
    const int outside_alpha = pixels.pixelColor( qRound( outside.x()*ratio ), qRound( outside.y()*ratio ) ).alpha();
    const int inside_alpha = pixels.pixelColor( qRound( inside.x()*ratio ), qRound( inside.y()*ratio ) ).alpha();
    if( outside_alpha <= 0 || inside_alpha <= outside_alpha || inside_alpha >= 255 )
      return false;
  }
  return true;
}

static bool opaqueCard( QWidget* widget, const QString& name, bool check_padding = true )
{
  const QImage pixels = widget->grab().toImage();
  const qreal ratio = pixels.devicePixelRatio();
  const int inset = ShadowPopup::shadowMargin + 14;
  const QRect inner( qCeil( inset*ratio ), qCeil( inset*ratio ),
    qFloor( (widget->width()-2*inset)*ratio ), qFloor( (widget->height()-2*inset)*ratio ) );
  if( inner.isEmpty() || !pixels.rect().contains( inner ) )
    return false;
  int transparent_pixels = 0;
  for( int vertical = inner.top(); vertical <= inner.bottom(); ++vertical )
    for( int horizontal = inner.left(); horizontal <= inner.right(); ++horizontal )
      if( pixels.pixelColor( horizontal, vertical ).alpha() != 255 )
        ++transparent_pixels;
  std::printf( "card-opacity %s scale=%.2f inner=%dx%d nonopaque=%d\n", qPrintable( name ),
    ratio, inner.width(), inner.height(), transparent_pixels );
  if( transparent_pixels )
    return false;
  if( !check_padding )
    return true;
  const QRect card = widget->rect().adjusted( ShadowPopup::shadowMargin, ShadowPopup::shadowMargin,
    -ShadowPopup::shadowMargin, -ShadowPopup::shadowMargin );
  const QList<QPoint> blank_samples = { QPoint( card.left()+card.width()/4, card.top()+3 ),
    QPoint( card.center().x(), card.top()+3 ), QPoint( card.right()-card.width()/4, card.top()+3 ),
    QPoint( card.left()+3, card.center().y() ) };
  for( const QPoint& point : blank_samples )
  {
    const QColor color = pixels.pixelColor( qRound( point.x()*ratio ), qRound( point.y()*ratio ) );
    std::printf( "card-blank %s logical=%d,%d rgba=%d,%d,%d,%d\n", qPrintable( name ),
      point.x(), point.y(), color.red(), color.green(), color.blue(), color.alpha() );
    if( color.alpha() != 255 || color.red() < 245 || color.green() < 245 || color.blue() < 245 )
      return false;
  }
  return true;
}

static int verifyMenu( QMenu* menu, QWidget* trigger, const QString& name )
{
  CHECK( menu && trigger );
  QWidget* owner = trigger->window();
  owner->raise();
  owner->activateWindow();
  QTest::qWait( 80 );
  trigger->setFocus();
  menu->popup( trigger->mapToGlobal( QPoint( 0, trigger->height() ) ) );
  QTest::qWait( 30 );
  CHECK( menu->isVisible() );
  CHECK( menu->windowFlags().testFlag( Qt::FramelessWindowHint ) );
  CHECK( menu->testAttribute( Qt::WA_TranslucentBackground ) );
#ifdef Q_OS_WIN
  CHECK( GetWindowLongPtrW( reinterpret_cast<HWND>( menu->winId() ), GWL_EXSTYLE ) & WS_EX_LAYERED );
  CHECK( !(GetWindowLongPtrW( reinterpret_cast<HWND>( menu->winId() ), GWL_STYLE ) & WS_CAPTION) );
#endif
  const QRect available = trigger->screen()->availableGeometry();
  CHECK( available.contains( menu->frameGeometry() ) );
  owner->move( owner->pos() + QPoint( -24, 15 ) );
  QTest::qWait( 30 );
  const QRect anchor( trigger->mapToGlobal( QPoint() ), trigger->size() );
  CHECK( menu->geometry() == ShadowPopup::placeGeometry( anchor, menu->sizeHint(), available ) );
  CHECK( available.contains( menu->frameGeometry() ) );
  CHECK( completeShadow( menu ) );
  CHECK( opaqueCard( menu, name ) );
  CHECK( saveProof( menu, name + QStringLiteral( ".png" ) ) );
  if( QGuiApplication::platformName() == QStringLiteral( "windows" ) )
  {
    QTest::qWait( 200 );
    CHECK( menu->isVisible() && QApplication::activePopupWidget() == menu );
    const QRect rectangle = menu->frameGeometry();
    const QPixmap native = menu->screen()->grabWindow( 0, rectangle.x(), rectangle.y(), rectangle.width(), rectangle.height() );
    CHECK( !native.isNull() && native.save( proofDir() + "/" + name + "-native-screen.png" ) );
    const QImage pixels = native.toImage();
    const QColor edge = pixels.pixelColor( pixels.width()/2, 1 );
    bool capturedContent = false;
    for( int vertical = 0; vertical < pixels.height() && !capturedContent; ++vertical )
      for( int horizontal = 0; horizontal < pixels.width(); ++horizontal )
      {
        const QColor sample = pixels.pixelColor( horizontal, vertical );
        if( sample.red() > 30 || sample.green() > 30 || sample.blue() > 30 )
        { capturedContent = true; break; }
      }
    if( capturedContent )
      CHECK( edge.red() > 30 || edge.green() > 30 || edge.blue() > 30 );
    else
      std::printf( "NATIVE_SCREEN_CAPTURE_UNAVAILABLE %s: whole desktop crop is black; WGC/manual review required\n", qPrintable( name ) );
  }
  QTest::keyClick( menu, Qt::Key_Escape );
  flushEvents();
  CHECK( !menu->isVisible() && owner->isVisible() );
  std::printf( "PASS actual %s follows its parent, keeps frame on screen and closes on Escape\n", qPrintable( name ) );
  return 0;
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication app( argc, argv );
  ShadowDialogChrome::install( &app );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  std::setvbuf( stderr, Q_NULLPTR, _IONBF, 0 );
  QTemporaryDir fixture;
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "synthetic-popup-test" );
  settings.setAccountDataFolder( fixture.path() );
  settings.load();
  IconManager::instance();
  Core core;
  ShadowPopup::installMenuTracking();
  app.setQuitOnLastWindowClosed( false );
  const QString scale = qEnvironmentVariable( "QT_SCALE_FACTOR", "1" );
  std::printf( "popup placement scale=%s platform=%s dpr=%.2f\n", qPrintable( scale ),
               qPrintable( QGuiApplication::platformName() ), app.devicePixelRatio() );
  QScreen* screen = QGuiApplication::primaryScreen();
  CHECK( screen );
  const QRect available = screen->availableGeometry();
  CHECK( available.width() > 200 && available.height() > 200 );

  const QSize popup_size( qMin( 300, available.width() / 2 ), qMin( 340, available.height() / 2 ) );
  const QList<QPoint> anchors = QList<QPoint>() << available.topLeft() <<
    QPoint( available.right() - 1, available.top() ) << QPoint( available.left(), available.bottom() - 1 ) <<
    QPoint( available.right() - 1, available.bottom() - 1 );
  for( int index = 0; index < anchors.size(); ++index )
  {
    const QPoint anchor = anchors.at( index );
    const QRect geometry = ShadowPopup::placeGeometry( QRect( anchor, QSize( 1, 1 ) ), popup_size,
      available, ShadowPopup::Automatic, 6 );
    CHECK( available.contains( geometry ) );
    CHECK( geometry.size() == popup_size );
    const bool top = index < 2;
    if( top )
      CHECK( geometry.top() > anchor.y() );
    else
      CHECK( geometry.bottom() < anchor.y() );
  }
  std::puts( "PASS placeGeometry keeps all four corner anchors inside available geometry and flips at the bottom" );

  QWidget owner;
  owner.setGeometry( QRect( available.center() - QPoint( 260, 190 ), QSize( 520, 380 ) ) );
  owner.show();
  owner.raise();
  owner.activateWindow();
  QTest::qWait( 100 );
  owner.activateWindow();
  flushEvents();
  ShadowPopup popup( &owner );
  popup.setObjectName( QStringLiteral( "PlacementProbePopup" ) );
  QLabel* label = new QLabel( QStringLiteral( "弹层定位检查" ), popup.contentWidget() );
  label->setAlignment( Qt::AlignCenter );
  popup.contentLayout()->addWidget( label );
  popup.setFixedWidth( 220 );
  const QPoint bottom_anchor( available.right() - 4, available.bottom() - 4 );
  popup.setAnchorRect( QRect( bottom_anchor, QSize( 1, 1 ) ) );
  popup.showPopup();
  QTest::qWait( 30 );
  flushEvents();
  CHECK( popup.isVisible() );
  CHECK( completeShadow( &popup ) );
  CHECK( opaqueCard( &popup, "placement-probe" ) );
  CHECK( available.contains( popup.frameGeometry() ) );
  CHECK( popup.geometry().bottom() < bottom_anchor.y() );
  CHECK( popup.geometry().right() <= available.right() );
  const QRect bottom_expected = ShadowPopup::placeGeometry( QRect( bottom_anchor, QSize( 1, 1 ) ),
    popup.size(), available );
  std::printf( "bottom-right actual=%d,%d %dx%d expected=%d,%d %dx%d available=%d,%d %dx%d\n",
    popup.x(), popup.y(), popup.width(), popup.height(), bottom_expected.x(), bottom_expected.y(),
    bottom_expected.width(), bottom_expected.height(), available.x(), available.y(), available.width(), available.height() );
  CHECK( popup.frameGeometry() == bottom_expected );
  saveProof( &popup, QStringLiteral( "popup-bottom-right-%1.png" ).arg( scale ) );

  QTest::keyClick( &popup, Qt::Key_Escape );
  flushEvents();
  CHECK( !popup.isVisible() );
  std::puts( "PASS real popup opens above a bottom-right anchor, stays on screen and closes on Escape" );

  QWidget moving_owner;
  moving_owner.setGeometry( QRect( available.center() - QPoint( 200, 150 ), QSize( 400, 300 ) ) );
  QPushButton* moving_anchor = new QPushButton( QStringLiteral( "anchor" ), &moving_owner );
  moving_anchor->setGeometry( 140, 120, 100, 32 );
  moving_owner.show();
  moving_owner.activateWindow();
  flushEvents();
  ShadowPopup follow_popup( &moving_owner );
  follow_popup.setAnchor( moving_anchor );
  QLabel* follow_label = new QLabel( QStringLiteral( "跟随窗口" ), follow_popup.contentWidget() );
  follow_label->setAlignment( Qt::AlignCenter );
  follow_popup.contentLayout()->addWidget( follow_label );
  follow_popup.showPopup();
  QTest::qWait( 30 );
  flushEvents();
  const QPoint before = follow_popup.pos();
  const QPoint owner_before = moving_owner.pos();
  moving_owner.move( owner_before + QPoint( 70, 45 ) );
  QTest::qWait( 40 );
  flushEvents();
  const QPoint delta = follow_popup.pos() - before;
  CHECK( delta == QPoint( 70, 45 ) );
  CHECK( available.contains( follow_popup.frameGeometry() ) );
  saveProof( &follow_popup, QStringLiteral( "popup-follows-owner-%1.png" ).arg( scale ) );
  moving_anchor->setFocus();
  follow_popup.hide();
  QTest::qWait( 30 );
  flushEvents();
  CHECK( moving_anchor->hasFocus() );
  std::puts( "PASS popup follows owner window movement and restores focus to its anchor" );
  follow_popup.setAnchorRect( QRect( moving_anchor->mapToGlobal( QPoint() ), moving_anchor->size() ) );
  follow_popup.showPopup(); QTest::qWait( 20 );
  const QPoint rect_before = follow_popup.pos();
  moving_owner.move( moving_owner.pos() + QPoint( -35, -20 ) );
  QTest::qWait( 20 );
  CHECK( follow_popup.pos() - rect_before == QPoint( -35, -20 ) );
  follow_popup.hide();
  std::puts( "PASS global rectangle anchors track owner movement too" );

  QDialog host;
  host.setObjectName( QStringLiteral( "PopupHostDialog" ) );
  host.setWindowTitle( QStringLiteral( "生日与颜色弹层检查" ) );
  QVBoxLayout* host_layout = new QVBoxLayout( &host );
  ShadowBirthdayEdit* birthday = new ShadowBirthdayEdit( &host );
  birthday->setObjectName( QStringLiteral( "mp_deBirthday" ) );
  birthday->setBirthday( QDate( 2000, 1, 1 ) );
  host_layout->addWidget( birthday );
  QPushButton* color_anchor = new QPushButton( QStringLiteral( "选择颜色" ), &host );
  host_layout->addWidget( color_anchor );
  host.resize( 360, 180 );
  host.move( available.center() - QPoint( 180, 90 ) );
  host.show();
  host.activateWindow();
  flushEvents();
  const QDate today = QDate::currentDate();
  birthday->setBirthday( QDate( today.year(), today.month(), 1 ) );
  birthday->showCalendar();
  QTest::qWait( 30 );
  flushEvents();
  ShadowCalendarPopup* calendar = birthday->findChild<ShadowCalendarPopup*>();
  CHECK( calendar && calendar->isVisible() );
  const QString popup_font_family = QFontInfo( calendar->font() ).family();
  CHECK( !popup_font_family.isEmpty() );
  CHECK( QFontInfo( calendar->yearButton()->font() ).family() == popup_font_family );
  foreach( QToolButton* day, calendar->findChildren<QToolButton*>( QStringLiteral( "calendarDay" ) ) )
    CHECK( QFontInfo( day->font() ).family() == popup_font_family );
  CHECK( available.contains( calendar->geometry() ) );
  CHECK( qAbs( calendar->geometry().center().x() - birthday->mapToGlobal( birthday->rect().center() ).x() ) < available.width() / 2 );
  calendar->yearButton()->click();
  QTest::qWait( 20 );
  CHECK( calendar->viewStack()->currentIndex() == 2 );
  QToolButton* current_year = Q_NULLPTR;
  foreach( QToolButton* button, calendar->findChildren<QToolButton*>( QStringLiteral( "calendarYearCell" ) ) )
    if( button->isVisible() && button->text() == QString::number( today.year() ) )
      current_year = button;
  CHECK( current_year );
  current_year->click();
  QTest::qWait( 20 );
  CHECK( calendar->viewStack()->currentIndex() == 1 );
  calendar->monthButton()->click();
  QTest::qWait( 20 );
  CHECK( calendar->viewStack()->currentIndex() == 0 );
  int future_count = 0;
  foreach( QToolButton* button, calendar->findChildren<QToolButton*>( QStringLiteral( "calendarDay" ) ) )
    if( button->property( "calendarDate" ).toDate() > today )
    {
      ++future_count;
      CHECK( !button->isEnabled() );
    }
  CHECK( future_count > 0 || today.day() == today.daysInMonth() );
  CHECK( opaqueCard( calendar, "calendar-days" ) && completeShadow( calendar ) );
  saveProof( calendar, QStringLiteral( "calendar-popup-%1.png" ).arg( scale ) );
  QTest::keyClick( calendar, Qt::Key_Escape );
  QTest::qWait( 20 );
  flushEvents();
  CHECK( !calendar->isVisible() );
  CHECK( host.isVisible() );
  CHECK( birthday->hasFocus() );
  std::puts( "PASS birthday calendar uses quick year/month views, disables future dates and Escape closes only the topmost layer" );
  birthday->clearBirthday();
  birthday->showCalendar(); QTest::qWait( 20 );
  CHECK( !birthday->hasBirthday() && !calendar->selectedDate().isValid() );
  QTest::keyClick( calendar, Qt::Key_Escape );
  CHECK( !birthday->hasBirthday() );
  birthday->showCalendar(); QTest::qWait( 20 );
  calendar->yearButton()->click();
  QToolButton* previous = calendar->findChild<QToolButton*>( "calendarPrevious" );
  QToolButton* next = calendar->findChild<QToolButton*>( "calendarNext" );
  previous->click();
  next->click();
  bool last_year_present = false;
  for( QToolButton* cell : calendar->findChildren<QToolButton*>( "calendarYearCell" ) )
    last_year_present = last_year_present || (cell->isVisible() && cell->text() == QString::number( today.year() ));
  CHECK( last_year_present );
  for( int page = 0; page < 20 && previous->isEnabled(); ++page )
    previous->click();
  QToolButton* first_year = Q_NULLPTR;
  for( QToolButton* cell : calendar->findChildren<QToolButton*>( "calendarYearCell" ) )
    if( cell->isVisible() && cell->text() == "1900" ) first_year = cell;
  CHECK( first_year );
  CHECK( opaqueCard( calendar, "calendar-years" ) && completeShadow( calendar ) );
  CHECK( saveProof( calendar, QStringLiteral( "calendar-year-grid.png" ) ) );
  first_year->click();
  calendar->findChildren<QToolButton*>( "calendarMonthCell" ).first()->click();
  CHECK( calendar->dayButton( QDate( 1900, 1, 1 ) ) );
  calendar->dayButton( QDate( 1900, 1, 1 ) )->click();
  CHECK( birthday->birthday() == QDate( 1900, 1, 1 ) );
  std::puts( "PASS empty calendar never commits today, final year page is reachable, actual 1900 birthday can be selected" );

  ShadowColorPanel color_panel( &host );
  color_panel.setAnchor( color_anchor );
  color_panel.setFocusOnClose( color_anchor );
  color_panel.setColor( QColor( "#2563EB" ) );
  color_panel.showPopup();
  QTest::qWait( 30 );
  flushEvents();
  CHECK( color_panel.isVisible() );
  CHECK( opaqueCard( &color_panel, "color-panel" ) && completeShadow( &color_panel ) );
  CHECK( available.contains( color_panel.frameGeometry() ) );
  CHECK( qAbs( color_panel.geometry().center().x() - color_anchor->mapToGlobal( color_anchor->rect().center() ).x() ) < available.width() / 2 );
  saveProof( &color_panel, QStringLiteral( "color-panel-%1.png" ).arg( scale ) );
  QTest::keyClick( &color_panel, Qt::Key_Escape );
  flushEvents();
  CHECK( !color_panel.isVisible() );
  std::puts( "PASS color panel opens next to its trigger, stays on screen and closes on Escape" );
  color_panel.showPopup(); QTest::qWait( 20 );
  QLineEdit* hex = color_panel.findChild<QLineEdit*>( "colorPanelHex" );
  QPushButton* accept = color_panel.findChild<QPushButton*>( "colorPanelAccept" );
  hex->setFocus(); hex->selectAll();
  QTest::keyClicks( hex, "#12" );
  CHECK( !accept->isEnabled() && !color_panel.findChild<QLabel*>( "colorPanelError" )->text().isEmpty() );
  QTest::keyClick( hex, Qt::Key_Return );
  CHECK( color_panel.isVisible() );
  hex->selectAll(); QTest::keyClicks( hex, "#123456" );
  CHECK( hex->text() == "#123456" && accept->isEnabled() && color_panel.color() == QColor( "#123456" ) );
  QSignalSpy accepted( &color_panel, &ShadowColorPanel::colorAccepted );
  QTest::keyClick( hex, Qt::Key_Return );
  CHECK( accepted.count() == 1 && !color_panel.isVisible() );
  color_panel.showPopup(); QTest::qWait( 20 );
  QTest::mouseClick( &host, Qt::LeftButton, Qt::NoModifier, QPoint( 3, 3 ) );
  flushEvents();
  CHECK( !color_panel.isVisible() );
  std::puts( "PASS custom hex typing is not rewritten mid-entry, invalid values block acceptance, outside click closes" );

  const QList<ShadowPopup*> narrowPanels = { calendar, &color_panel };
  for( ShadowPopup* panel : narrowPanels )
  {
    panel->setMaximumSize( 210, 160 );
    panel->showPopup(); QTest::qWait( 30 );
    CHECK( available.contains( panel->frameGeometry() ) && panel->width() <= 210 && panel->height() <= 160 );
    QScrollArea* scroll = panel->findChild<QScrollArea*>( "ShadowPopupScroll" );
    CHECK( scroll && (scroll->verticalScrollBar()->maximum() > 0 || scroll->horizontalScrollBar()->maximum() > 0) );
    QWidget* target = panel == calendar ? static_cast<QWidget*>(calendar->findChild<QToolButton*>( "calendarToday" )) :
      static_cast<QWidget*>(accept);
    target->setFocus();
    scroll->ensureWidgetVisible( target ); QTest::qWait( 20 );
    CHECK( scroll->viewport()->rect().intersects( QRect( target->mapTo( scroll->viewport(), QPoint() ), target->size() ) ) );
    CHECK( opaqueCard( panel, panel == calendar ? "calendar-narrow" : "color-narrow", false ) &&
      completeShadow( panel ) );
    CHECK( saveProof( panel, panel == calendar ? "calendar-narrow.png" : "color-narrow.png" ) );
    QTest::keyClick( target, Qt::Key_Escape );
    CHECK( !panel->isVisible() && host.isVisible() );
    panel->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
  }
  std::puts( "PASS real calendar and color frames compress to 210x160 and scroll focused footer controls into view" );

  QMenu menu;
  menu.addAction( QStringLiteral( "打开会话" ) );
  menu.addAction( QStringLiteral( "查看资料" ) );
  CHECK( ShadowPopup::showMenuAtGlobal( &menu, QRect( QPoint( available.right() - 2, available.bottom() - 2 ), QSize( 1, 1 ) ) ) );
  QTest::qWait( 30 );
  flushEvents();
  CHECK( menu.isVisible() );
  CHECK( available.contains( menu.geometry() ) );
  menu.close();
  flushEvents();
  CHECK( !menu.isVisible() );
  std::puts( "PASS custom menus opened near a screen corner stay inside available geometry" );
  QMenu tall_menu( color_anchor );
  for( int index = 0; index < 50; ++index )
    tall_menu.addAction( QStringLiteral( "可达菜单项 %1" ).arg( index + 1 ) );
  tall_menu.popup( color_anchor->mapToGlobal( QPoint() ) ); QTest::qWait( 30 );
  CHECK( available.contains( tall_menu.frameGeometry() ) );
  QTest::keyClick( &tall_menu, Qt::Key_End ); QTest::qWait( 30 );
  CHECK( tall_menu.activeAction() == tall_menu.actions().last() );
  CHECK( tall_menu.rect().intersects( tall_menu.actionGeometry( tall_menu.actions().last() ) ) );
  CHECK( opaqueCard( &tall_menu, "tall-menu" ) && completeShadow( &tall_menu ) );
  CHECK( saveProof( &tall_menu, "tall-menu-last-action.png" ) );
  QTest::keyClick( &tall_menu, Qt::Key_Escape );
  std::puts( "PASS a real 50-item menu stays on screen and its final action is keyboard reachable" );
  GuiEditVCard profile;
  profile.setUser( settings.localUser() );
  profile.move( available.center() - QPoint( 180, 220 ) );
  profile.show(); QTest::qWait( 30 );
  QPushButton* status = profile.findChild<QPushButton*>( "profileCardStatus" );
  CHECK( verifyMenu( status->menu(), status, "profile-status-menu" ) == 0 );
  profile.reject();

  GuiMessageEdit editor( Q_NULLPTR );
  QWidget format_host;
  QVBoxLayout* format_layout = new QVBoxLayout( &format_host );
  QToolBar* format = shadowComposerFormatBar( &editor, &format_host );
  format_layout->addWidget( format );
  format_host.resize( qMin( 600, available.width() - 50 ), 120 );
  format_host.move( available.center() - QPoint( format_host.width()/2, 60 ) );
  format_host.show(); QTest::qWait( 20 );
  const QStringList formatControls = { "FormatColor", "FormatHighlight", "FormatHeading" };
  for( const QString& control : formatControls )
  {
    QToolButton* button = format->findChild<QToolButton*>( control );
    CHECK( verifyMenu( button->menu(), button, control ) == 0 );
  }
  format_host.close();

  QImage sticker_image( 12, 12, QImage::Format_RGB32 );
  sticker_image.fill( QColor( "#4381CA" ) );
  const QString sticker_source = fixture.filePath( "sticker-cell.png" );
  CHECK( sticker_image.save( sticker_source ) );
  CHECK( ShadowStickerStore( fixture.path() ).addFile( sticker_source ) );
  GuiStickerPicker stickers( fixture.path(), &host );
  host.activateWindow();
  host.raise();
  QTest::qWait( 30 );
  stickers.showAbove( color_anchor ); QTest::qWait( 30 );
  CHECK( stickers.isVisible() );
  CHECK( available.contains( stickers.frameGeometry() ) );
  QListWidget* sticker_grid = stickers.findChild<QListWidget*>( "StickerGrid" );
  CHECK( sticker_grid && sticker_grid->count() == 2 );
  const QRect sticker_cell = sticker_grid->visualItemRect( sticker_grid->item( 1 ) );
  CHECK( sticker_cell.width() >= sticker_grid->iconSize().width() &&
    sticker_cell.height() >= sticker_grid->iconSize().height() );
  CHECK( sticker_grid->viewport()->rect().contains( sticker_cell.center() ) &&
    sticker_grid->viewport()->visibleRegion().contains( sticker_cell.center() ) &&
    sticker_grid->itemAt( sticker_cell.center() ) == sticker_grid->item( 1 ) );
  CHECK( saveProof( &stickers, "sticker-before-send.png" ) );
  QSignalSpy sticker_sent( &stickers, &GuiStickerPicker::stickerSelected );
  QTest::mouseClick( sticker_grid->viewport(), Qt::LeftButton, Qt::NoModifier, sticker_cell.center() );
  CHECK( !stickers.isVisible() && sticker_sent.count() == 1 &&
    sticker_sent.first().first().toString() == ShadowStickerStore( fixture.path() ).list().first().path );
  stickers.showAbove( color_anchor ); QTest::qWait( 30 );
  std::puts( "PASS lazy sticker thumbnails reserve visible hit targets and an actual grid click selects the original file" );
  const QPoint sticker_before = stickers.pos();
  host.move( host.pos() + QPoint( -20, -15 ) ); QTest::qWait( 20 );
  std::printf( "sticker-follow before=%d,%d actual=%d,%d expected=%d,%d size=%dx%d visible=%d active-owner=%d\n",
    sticker_before.x(), sticker_before.y(), stickers.x(), stickers.y(),
    stickers.placementGeometryFor( stickers.sizeHint() ).x(), stickers.placementGeometryFor( stickers.sizeHint() ).y(),
    stickers.width(), stickers.height(), stickers.isVisible(), host.isActiveWindow() );
  CHECK( stickers.isVisible() );
  CHECK( stickers.pos() - sticker_before == QPoint( -20, -15 ) );
  QToolButton* add = stickers.findChild<QToolButton*>( "StickerAddButton" );
  add->menu()->popup( add->mapToGlobal( QPoint( 0, add->height() ) ) ); QTest::qWait( 20 );
  CHECK( add->menu()->isVisible() && stickers.isVisible() );
  QTest::keyClick( add->menu(), Qt::Key_Escape ); QTest::qWait( 20 );
  CHECK( stickers.isVisible() );
  CHECK( opaqueCard( &stickers, "stickers" ) && completeShadow( &stickers ) );
  CHECK( saveProof( &stickers, "sticker-popup.png" ) );
  stickers.setMaximumSize( 230, 150 );
  stickers.hide(); stickers.showAbove( color_anchor ); QTest::qWait( 20 );
  CHECK( stickers.width() <= 230 && stickers.height() <= 150 && available.contains( stickers.frameGeometry() ) );
  QScrollArea* sticker_scroll = stickers.findChild<QScrollArea*>( "ShadowPopupScroll" );
  sticker_scroll->ensureWidgetVisible( add );
  CHECK( opaqueCard( &stickers, "stickers-narrow", false ) && completeShadow( &stickers ) );
  CHECK( saveProof( &stickers, "sticker-narrow.png" ) );
  QTest::keyClick( add, Qt::Key_Escape ); QTest::qWait( 20 );
  CHECK( !stickers.isVisible() );
  std::puts( "PASS actual sticker popup follows parent, nests a menu, keeps first Escape local and scrolls in narrow frames" );

  GuiMain window;
  window.loadStyle();
  window.resize( qMin( 1000, available.width() - 60 ), qMin( 650, available.height() - 60 ) );
  window.move( available.topLeft() + QPoint( 35, 20 ) );
  window.show(); QTest::qWait( 50 );
  QWidget* rail = window.findChild<QWidget*>( "ShadowNavRail" );
  CHECK( rail );
  const QImage railPixels = rail->grab().toImage();
  const qreal railRatio = railPixels.devicePixelRatio();
  CHECK( railPixels.pixelColor( qRound( 2*railRatio ), qRound( (rail->height()-3)*railRatio ) ).rgb() ==
    QColor( "#030a0a" ).rgb() );
  bool checkedNavigation = false;
  for( QToolButton* button : rail->findChildren<QToolButton*>( "ShadowNavButton" ) )
    if( button->isChecked() )
    {
      const QImage pixels = button->grab().toImage();
      const qreal ratio = pixels.devicePixelRatio();
      CHECK( pixels.pixelColor( qRound( button->width()/2*ratio ), qRound( (button->height()-4)*ratio ) ).rgb() ==
        QColor( "#e93d34" ).rgb() );
      checkedNavigation = true;
    }
  CHECK( checkedNavigation );
  QMenu* main_menu = window.findChild<QMenu*>( "ShadowMainMenu" );
  CHECK( main_menu );
  QWidget* main_trigger = main_menu->parentWidget();
  CHECK( verifyMenu( main_menu, main_trigger, "main-menu" ) == 0 );
  main_menu->popup( main_trigger->mapToGlobal( QPoint( 0, main_trigger->height() ) ) );
  QTest::qWait( 20 );
  QMenu* submenu = Q_NULLPTR;
  for( QAction* action : main_menu->actions() )
    if( action->menu() ) { submenu = action->menu(); break; }
  CHECK( submenu );
  submenu->popup( main_menu->mapToGlobal( main_menu->actionGeometry( submenu->menuAction() ).topRight() ) );
  QTest::qWait( 20 );
  const QPoint root_before = main_menu->pos();
  const QPoint sub_before = submenu->pos();
  window.move( window.pos() + QPoint( 16, 12 ) ); QTest::qWait( 30 );
  CHECK( submenu->pos().y() - sub_before.y() == main_menu->pos().y() - root_before.y() );
  CHECK( available.contains( submenu->frameGeometry() ) && completeShadow( submenu ) );
  CHECK( opaqueCard( submenu, "workspace-submenu" ) );
  QTest::keyClick( submenu, Qt::Key_Escape ); QTest::qWait( 20 );
  CHECK( !submenu->isVisible() && main_menu->isVisible() );
  QTest::keyClick( main_menu, Qt::Key_Escape );
  CHECK( saveProof( &window, "actual-main-window.png" ) );
  QToolButton* self_avatar = window.findChild<QToolButton*>( "ShadowTitleProfile" );
  CHECK( self_avatar );
  self_avatar->click(); QTest::qWait( 30 );
  QPointer<GuiEditVCard> self_card = window.findChild<GuiEditVCard*>();
  CHECK( self_card && self_card->isVisible() && !self_card->isModal() );
  const auto self_geometry = [&]() {
    return ShadowPopup::placeGeometry( QRect( self_avatar->mapToGlobal( QPoint() ), self_avatar->size() ),
      self_card->size(), available );
  };
  CHECK( self_card->geometry() == self_geometry() && available.contains( self_card->frameGeometry() ) );
  window.move( window.pos() + QPoint( -18, -12 ) ); QTest::qWait( 20 );
  CHECK( self_card->geometry() == self_geometry() );
  CHECK( saveProof( self_card, "self-avatar-card.png" ) );
  self_card->findChild<QPushButton*>( "editMyProfile" )->click(); QTest::qWait( 20 );
  QPushButton* self_save = self_card->findChild<QPushButton*>( "mp_pbOk" );
  CHECK( self_save->isVisible() && self_card->rect().contains(
    QRect( self_save->mapTo( self_card, QPoint() ), self_save->size() ) ) );
  const QPoint editor_before = self_card->pos();
  window.move( window.pos() + QPoint( 12, 8 ) ); QTest::qWait( 20 );
  CHECK( self_card->pos() == editor_before );
  self_card->reject(); flushEvents();
  CHECK( !self_card );
  self_avatar->click(); QTest::qWait( 20 );
  self_card = window.findChild<GuiEditVCard*>();
  CHECK( self_card && self_card->isVisible() );
  QTest::mouseClick( &window, Qt::LeftButton, Qt::NoModifier, QPoint( 10, window.height()-10 ) );
  flushEvents();
  CHECK( !self_card );
  bool statusPassed = false;
  QTimer::singleShot( 100, &window, [&]() {
    QDialog* dialog = qobject_cast<QDialog*>( QApplication::activeModalWidget() );
    if( dialog && dialog->objectName() == "ShadowConnectionStatusDialog" )
    {
      QLabel* summary = dialog->findChild<QLabel*>( "ConnectionStatusSummary" );
      QPlainTextEdit* logs = dialog->findChild<QPlainTextEdit*>( "ConnectionDiagnosticLog" );
      statusPassed = summary && logs && logs->isHidden() &&
        summary->text().contains( QStringLiteral( "已验证在线联系人" ) ) &&
        !summary->text().contains( "BeeBEEP" ) && saveProof( dialog, "connection-status.png" );
      dialog->reject();
    }
  } );
  CHECK( QMetaObject::invokeMethod( &window, "showModernActivities", Qt::DirectConnection ) );
  CHECK( statusPassed );
  window.hide();
  std::puts( "PASS actual GuiMain menu and nested workspace menu follow the main window; Escape preserves the parent menu" );
  std::puts( "PASS actual self avatar opens an anchored card, follows the main window, edits in a reachable dialog and dismisses outside" );

  host.activateWindow();
  host.raise();
  QTest::qWait( 30 );
  QPointer<GuiVCard> public_card = new GuiVCard( &host );
  public_card->setVCard( settings.localUser(), ID_INVALID, false );
  public_card->showAt( color_anchor->mapToGlobal( QPoint() ) ); QTest::qWait( 20 );
  CHECK( public_card && public_card->isVisible() );
  CHECK( available.contains( public_card->frameGeometry() ) );
  CHECK( opaqueCard( public_card, "public-profile" ) && completeShadow( public_card ) );
  const QPoint public_before = public_card->pos();
  host.move( host.pos() + QPoint( 15, 10 ) ); QTest::qWait( 20 );
  CHECK( public_card->pos() - public_before == QPoint( 15, 10 ) );
  CHECK( saveProof( public_card, "public-profile-popup.png" ) );
  QTest::keyClick( public_card->findChild<QToolButton*>( "closePublicProfile" ), Qt::Key_Escape );
  flushEvents();
  std::puts( "PASS actual public profile follows owner, remains on screen and closes from a focused child" );

  qputenv( "SHADOWCHAT_OPAQUE_MENUS", "1" );
  host.activateWindow();
  QMenu fallback( &host );
  fallback.addAction( QStringLiteral( "兼容菜单测试" ) );
  fallback.popup( host.mapToGlobal( QPoint( 30, 50 ) ) );
  QTest::qWait( 40 );
  CHECK( fallback.property( "shadowMenuOpaque" ).toBool() );
  CHECK( !fallback.testAttribute( Qt::WA_TranslucentBackground ) && !fallback.mask().isEmpty() );
  const QImage fallbackPixels = fallback.grab().toImage();
  const QColor fallbackCenter = fallbackPixels.pixelColor( fallbackPixels.width()/2, fallbackPixels.height()/2 );
  CHECK( fallbackCenter.alpha() == 255 );
  CHECK( saveProof( &fallback, "menu-opaque-fallback.png" ) );
  QTest::keyClick( &fallback, Qt::Key_Escape );
  CHECK( !fallback.isVisible() );
  qunsetenv( "SHADOWCHAT_OPAQUE_MENUS" );
  std::puts( "PASS forced compositor fallback uses a masked white card without transparent margins" );

  if( qEnvironmentVariableIsSet( "SHADOWCHAT_NATIVE_QA_HOLD" ) )
  {
    window.setWindowTitle( "ShadowChat 1.2.2 native QA" );
    window.show();
    window.raise();
    window.activateWindow();
    QTimer::singleShot( 250, &window, [main_menu,main_trigger]() {
      main_menu->popup( main_trigger->mapToGlobal( QPoint( 0, main_trigger->height() ) ) );
    } );
    QTimer::singleShot( 120000, &app, &QCoreApplication::quit );
    app.exec();
  }
  host.close();
  moving_owner.close();
  owner.close();
  flushEvents();
  std::puts( "ALL POPUP PLACEMENT CHECKS PASSED" );
  return 0;
}
