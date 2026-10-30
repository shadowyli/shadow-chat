#include "GuiEditVCard.h"
#include "Settings.h"
#include "IconManager.h"
#include "UserManager.h"
#include "BeeApplication.h"
#include "Random.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include <cstdio>

int main( int argc, char** argv )
{
  BeeApplication app( argc, argv ); QTemporaryDir folder; Random::init();
  app.setFont( ShadowStyle::applicationFont() );
  ShadowDialogChrome::install( &app );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 ); std::puts( "Fixture QApplication ready" );
  Settings& settings = Settings::instance(); settings.setDefaultFolders(); settings.createLocalUser( "Lin" ); settings.setAccountDataFolder( folder.path() ); settings.load();
  IconManager::instance(); UserManager::instance();
  std::puts( "Fixture settings ready" );
  User user = settings.localUser(); user.setAccountName( "lin@example.test" ); user.setName( QStringLiteral( "林言" ) );
  VCard vcard; vcard.setNickName( QStringLiteral( "林言" ) ); vcard.setEmail( "lin@example.test" );
  vcard.setInfo( QStringLiteral( "角色与场景设计 · 让好想法变成看得见的作品" ) );
  vcard.setFirstName( "Yan" ); vcard.setLastName( "Lin" ); vcard.setBirthday( QDate( 1998, 6, 17 ) );
  vcard.setPhoneNumber( "020-12345678" ); vcard.setRoomLocation( QStringLiteral( "内容制作中心 · A12" ) );
  QPixmap photo( 96, 96 ); photo.fill( QColor( "#609bea" ) );
  QPainter avatarPainter( &photo ); avatarPainter.setPen( Qt::white ); avatarPainter.setFont( QFont( "Microsoft YaHei UI", 32 ) ); avatarPainter.drawText( photo.rect(), Qt::AlignCenter, QStringLiteral( "林" ) ); avatarPainter.end();
  vcard.setPhoto( photo ); user.setVCard( vcard ); settings.setLocalUser( user );
  user.setWorkgroups( QStringList() << QStringLiteral( "内容制作中心" ) << QStringLiteral( "美术组" ) );
  settings.setLocalUser( user );
  int failures = 0;
  auto check = [&]( bool value, const char* text ) { std::printf( "%s %s\n", value ? "PASS" : "FAIL", text ); if( !value ) ++failures; };
  std::puts( "Fixture user ready" );
  QWidget host; host.resize( 800, 600 ); host.show();
  GuiEditVCard dialog( &host ); dialog.setModal( true ); std::puts( "Fixture dialog constructed" ); dialog.setUser( user ); dialog.setSizeGripEnabled( true ); std::puts( "Fixture user populated" ); dialog.show(); app.processEvents();
  foreach( const QString& family, QFontDatabase().families() )
    if( family.contains( "YaHei", Qt::CaseInsensitive ) || family.contains( "Segoe", Qt::CaseInsensitive ) || family.contains( "SimSun", Qt::CaseInsensitive ) )
      std::printf( "Installed font: %s\n", qPrintable( family ) );
  foreach( QLabel* label, dialog.findChildren<QLabel*>() )
    if( label->objectName() == "profileCardInfo" || label->objectName() == "profileCardNickname" )
      std::printf( "Font %s requested=%s resolved=%s point=%g pixel=%d style=%d\n", qPrintable( label->objectName() ), qPrintable( label->font().family() ), qPrintable( QFontInfo( label->font() ).family() ), label->font().pointSizeF(), label->font().pixelSize(), int( label->font().styleHint() ) );
  QLabel* bodyFontProbe = dialog.findChild<QLabel*>( "profileCardInfo" );
  check( QFontInfo( bodyFontProbe->font() ).family() == "Microsoft YaHei UI" && bodyFontProbe->font().pixelSize() == 13,
    "profile body resolves to installed sans YaHei UI at13px" );
  QPushButton* editFontProbe = dialog.findChild<QPushButton*>( "editMyProfile" );
  check( QFontInfo( editFontProbe->font() ).family() == "Microsoft YaHei UI" && editFontProbe->font().pixelSize() == 13,
    "profile edit action resolves to installed sans YaHei UI at13px" );
  QStackedWidget* pages = dialog.findChild<QStackedWidget*>();
  check( pages && pages->currentIndex() == 0 && dialog.findChild<QPushButton*>( "editMyProfile" )->isVisible(), "own profile opens as card with explicit edit entrance" );
  check( dialog.findChild<QLabel*>( "profileCardEmail" )->text() == "lin@example.test" &&
    !dialog.findChild<QLabel*>( "profileCardAccount" ), "account appears once in the email row" );
  check( dialog.width() <= 430 && dialog.findChild<QLabel*>( "profileCardAvatar" )->width() == 64,
    "profile opens as a compact portrait card with a 64px avatar" );
  QLabel* nickname = dialog.findChild<QLabel*>( "profileCardNickname" );
  QLabel* avatar = dialog.findChild<QLabel*>( "profileCardAvatar" );
  check( nickname->mapTo( &dialog, QPoint() ).y() >=
    avatar->mapTo( &dialog, QPoint( 0, avatar->height() ) ).y(),
    "nickname sits below avatar like the reference card" );
  check( dialog.findChild<QLabel*>( "profileCardClock" )->text().contains( "UTC" ) &&
    dialog.findChild<QLabel*>( "profileCardGroups" )->text().contains( QStringLiteral( "美术组" ) ),
    "time zone and workgroups show real local data" );
  check( !dialog.findChild<QToolButton*>( "profileEditInfo" )->icon().pixmap( 16, 16 ).isNull() &&
    !dialog.findChild<QToolButton*>( "profileCopyEmail" )->icon().pixmap( 16, 16 ).isNull() &&
    !ShadowStyle::tintedPixmap( ":/images/shadowchat/icons/clock.svg", QSize( 18, 18 ), Qt::gray ).isNull(),
    "profile action and information icons render from the deployed SVG plugin" );
  dialog.findChild<QToolButton*>( "profileCopyEmail" )->click();
  check( QApplication::clipboard()->text() == "lin@example.test", "email copy action writes the current account email" );
  int selectedStatus = -1;
  QObject::connect( &dialog, &GuiEditVCard::userStatusSelected, &dialog, [&]( int status ) {
    selectedStatus = status;
    dialog.setProfileStatus( status );
  } );
  QPushButton* statusButton = dialog.findChild<QPushButton*>( "profileCardStatus" );
  for( QAction* action : statusButton->menu()->actions() )
    if( action->data().toInt() == User::Busy ) action->trigger();
  check( selectedStatus == User::Busy &&
    statusButton->menu()->actions().at( 1 )->isChecked(),
    "work status menu emits the selected status and reflects the resulting state" );
  check( !dialog.findChild<QPushButton*>( "mp_pbRegenerateHash" )->isVisible(), "device identity reset absent from initial profile card" );
  check( dialog.findChild<QLabel*>( "shadowDialogTitle" ) &&
    dialog.findChild<QLabel*>( "shadowDialogTitle" )->text().isEmpty() &&
    !dialog.findChild<QLabel*>( "shadowDialogTitle" )->isVisible(), "profile chrome duplicate title removed" );
  QPushButton* cardClose = dialog.findChild<QPushButton*>( "closeMyProfile" );
  check( dialog.rect().contains( QRect( cardClose->mapTo( &dialog, QPoint() ), cardClose->size() ) ),
    "card footer fully inside initial window without manual resize" );
  check( dialog.screen()->availableGeometry().contains( dialog.frameGeometry() ),
    "profile first-open geometry stays inside the owning screen" );
  QWidget* surface = dialog.findChild<QWidget*>( "shadowDialogSurface" );
  QWidget* shadow = dialog.findChild<QWidget*>( "shadowDialogShadow" );
  check( surface && shadow && dialog.testAttribute( Qt::WA_TranslucentBackground ) &&
    shadow->isVisible() && shadow->testAttribute( Qt::WA_TransparentForMouseEvents ),
    "profile has a translucent outer shadow that does not intercept mouse input" );
  check( surface && !surface->mask().contains( QPoint() ) && dialog.mask().isEmpty() &&
    surface->geometry().left() >= 20 && surface->geometry().top() >= 20 &&
    shadow->geometry() == dialog.rect(), "rounded content leaves room for all sides of the shadow" );
  if( surface && shadow )
  {
    const QImage pixels = dialog.grab().toImage();
    const qreal scale = pixels.devicePixelRatio();
    auto pixel = [&]( int horizontal, int vertical ) {
      return pixels.pixelColor( qRound( horizontal * scale ), qRound( vertical * scale ) );
    };
    const QRect bounds = surface->geometry();
    const QColor leftShadow = pixel( bounds.left() - 8, bounds.center().y() );
    const QColor topShadow = pixel( bounds.center().x(), bounds.top() - 8 );
    const QColor rightShadow = pixel( bounds.right() + 8, bounds.center().y() );
    const QColor bottomShadow = pixel( bounds.center().x(), bounds.bottom() + 8 );
    check( leftShadow.alpha() > 0 && topShadow.alpha() > 0 &&
      rightShadow.alpha() > 0 && bottomShadow.alpha() > 0 &&
      leftShadow.alpha() < 180 && bottomShadow.alpha() < 180,
      "native raster contains soft translucent shadow pixels on all four edges" );
    check( pixel( 0, 0 ).alpha() == 0 &&
      pixel( bounds.center().x(), bounds.top() - 8 ).alpha() <
        pixel( bounds.center().x(), bounds.top() - 2 ).alpha(),
      "shadow fades outward and outer corners remain transparent" );
    QImage whiteBackground( pixels.size(), QImage::Format_ARGB32_Premultiplied );
    whiteBackground.fill( Qt::white );
    QPainter compositing( &whiteBackground );
    compositing.drawImage( QPoint(), pixels );
    compositing.end();
    whiteBackground.save( QDir::currentPath() + "/profile-shadow-on-white.png" );
    for( QSizeGrip* grip : dialog.findChildren<QSizeGrip*>( QString(), Qt::FindDirectChildrenOnly ) )
      check( bounds.contains( grip->geometry() ), "resize grip stays inside content surface instead of shadow" );
  }
  dialog.grab().save( QDir::currentPath() + "/profile-card-proof.png" );
  dialog.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  app.processEvents();
  check( pages->currentIndex() == 1 && dialog.findChild<QWidget*>( "mp_advanced" )->isVisible() &&
    dialog.findChild<QPushButton*>( "mp_pbAdvanced" )->isChecked(), "editor opens with remaining optional details expanded" );
  check( !dialog.findChild<QLineEdit*>( "mp_leFirstName" ) && !dialog.findChild<QLineEdit*>( "mp_leLastName" ) &&
    !dialog.findChild<QLineEdit*>( "mp_leRoom" ), "first name surname and location fields removed" );
  QPushButton* saveAction = dialog.findChild<QPushButton*>( "mp_pbOk" );
  check( saveAction->isVisible() && dialog.rect().contains( QRect( saveAction->mapTo( &dialog, QPoint() ), saveAction->size() ) ),
    "save button fully visible immediately after entering editor" );
  QScrollArea* editorScroll = dialog.findChild<QScrollArea*>( "profileEditorScroll" );
  check( editorScroll && editorScroll->horizontalScrollBar()->maximum() == 0, "editor never clips horizontally" );
  dialog.grab().save( QDir::currentPath() + "/profile-editor-first-open.png" );
  dialog.resize( 580, 360 ); app.processEvents();
  check( dialog.rect().contains( QRect( saveAction->mapTo( &dialog, QPoint() ), saveAction->size() ) ) &&
    editorScroll->verticalScrollBar()->maximum() > 0, "small window scrolls form while keeping save footer visible" );
  check( shadow && shadow->geometry() == dialog.rect() && surface &&
    surface->geometry().left() >= 20 && surface->mask().contains( surface->rect().center() ),
    "shadow and rounded surface follow small-window resizing" );
  editorScroll->ensureWidgetVisible( dialog.findChild<QLineEdit*>( "mp_lePhone" ) ); app.processEvents();
  QLineEdit* phoneAction = dialog.findChild<QLineEdit*>( "mp_lePhone" );
  check( editorScroll->viewport()->rect().contains( phoneAction->mapTo( editorScroll->viewport(), phoneAction->rect().center() ) ),
    "small-window optional fields reachable without stretching dialog" );
  dialog.grab().save( QDir::currentPath() + "/profile-editor-small-window.png" );
  check( dialog.findChild<QLineEdit*>( "mp_leEmail" )->isReadOnly(), "account email cannot be renamed through profile editor" );
  dialog.findChild<QLineEdit*>( "mp_leNickname" )->setText( QStringLiteral( "林言 · 美术" ) );
  dialog.findChild<QPlainTextEdit*>( "mp_teInfo" )->setPlainText( QStringLiteral( "角色美术与场景设计" ) );
  dialog.grab().save( QDir::currentPath() + "/profile-editor-proof.png" );
  dialog.findChild<QLineEdit*>( "mp_leEmail" )->setText( "tampered@example.test" );
  dialog.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( dialog.result() == QDialog::Accepted && dialog.vCard().nickName() == QStringLiteral( "林言 · 美术" ), "save accepts edited nickname" );
  check( dialog.vCard().email() == "lin@example.test", "save preserves account email despite programmatic field change" );
  check( dialog.vCard().firstName() == "Yan" && dialog.vCard().lastName() == "Lin" && dialog.vCard().birthday() == QDate( 1998, 6, 17 ) && dialog.vCard().phoneNumber() == "020-12345678" && dialog.vCard().roomLocation() == QStringLiteral( "内容制作中心 · A12" ), "removed fields preserve existing data when saving other edits" );
  GuiEditVCard optional; optional.setUser( user ); optional.show(); optional.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  check( optional.findChild<QDateEdit*>( "mp_deBirthday" )->isVisible() &&
    optional.findChild<QLineEdit*>( "mp_lePhone" )->isVisible() &&
    optional.findChild<QPushButton*>( "mp_pbRegenerateHash" )->isVisible(), "birthday phone and identity action retained in expanded details" );
  optional.findChild<QPushButton*>( "mp_pbAdvanced" )->click(); app.processEvents();
  check( !optional.findChild<QWidget*>( "mp_advanced" )->isVisible(), "expanded section still collapses on request" );
  optional.findChild<QPushButton*>( "mp_pbAdvanced" )->click(); app.processEvents();
  check( optional.findChild<QWidget*>( "mp_advanced" )->isVisible(), "details expand again without manual window resize" );
  optional.findChild<QLineEdit*>( "mp_lePhone" )->setText( "8888" ); optional.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( optional.vCard().phoneNumber() == "8888" && !optional.regenerateUserHash(), "optional field edits save without resetting identity" );
  GuiEditVCard canceled; canceled.setUser( user ); canceled.show(); canceled.findChild<QPushButton*>( "editMyProfile" )->click();
  canceled.findChild<QLineEdit*>( "mp_leNickname" )->setText( "discard" ); canceled.findChild<QPushButton*>( "mp_pbRemovePhoto" )->click(); canceled.findChild<QPushButton*>( "mp_pbCancel" )->click();
  check( canceled.result() == QDialog::Rejected && user.vCard().nickName() == QStringLiteral( "林言" ) && !user.vCard().photo().isNull(), "cancel leaves source user nickname and avatar untouched" );
  GuiEditVCard closed; closed.setUser( user ); closed.findChild<QPushButton*>( "closeMyProfile" )->click();
  check( closed.result() == QDialog::Rejected && !closed.regenerateUserHash(), "closing default card never applies profile changes" );
  GuiEditVCard chromeClosed; chromeClosed.setUser( user ); chromeClosed.show(); app.processEvents();
  chromeClosed.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  chromeClosed.findChild<QLineEdit*>( "mp_leNickname" )->setText( "discard-from-close" );
  chromeClosed.findChild<QToolButton*>( "shadowDialogClose" )->click();
  check( chromeClosed.result() == QDialog::Rejected && chromeClosed.vCard().nickName() == user.name(),
    "titlebar close rejects editor changes" );
  GuiEditVCard signature; signature.setUser( user ); signature.show(); app.processEvents();
  signature.findChild<QToolButton*>( "profileEditInfo" )->click(); app.processEvents();
  check( signature.findChild<QStackedWidget*>()->currentIndex() == 1 &&
    signature.findChild<QPlainTextEdit*>( "mp_teInfo" )->hasFocus(),
    "signature row opens the real editor with signature focused" );
  signature.reject();
  GuiEditVCard phone; phone.setUser( user ); phone.show(); app.processEvents();
  phone.findChild<QToolButton*>( "profileEditPhone" )->click(); app.processEvents();
  check( phone.findChild<QStackedWidget*>()->currentIndex() == 1 &&
    phone.findChild<QLineEdit*>( "mp_lePhone" )->hasFocus(),
    "phone row opens the real editor with phone focused" );
  phone.reject();
  User emptyDetails = user; VCard emptyCard = emptyDetails.vCard();
  emptyCard.setInfo( QString() ); emptyCard.setPhoneNumber( QString() );
  emptyDetails.setVCard( emptyCard ); emptyDetails.setWorkgroups( QStringList() );
  GuiEditVCard empty; empty.setUser( emptyDetails ); empty.show(); app.processEvents();
  check( empty.findChild<QLabel*>( "profileCardInfo" )->text() == QStringLiteral( "输入个性签名" ) &&
    empty.findChild<QLabel*>( "profileCardPhone" )->text() == QStringLiteral( "添加电话号码" ) &&
    empty.findChild<QLabel*>( "profileCardGroups" )->text() == QStringLiteral( "未设置工作组" ),
    "missing optional details show honest prompts without reference data" );
  empty.reject();
  return failures ? 1 : 0;
}
