#include "GuiEditVCard.h"
#include "Settings.h"
#include "IconManager.h"
#include "UserManager.h"
#include "BeeApplication.h"
#include "Random.h"
#include "ShadowBirthdayEdit.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include <QHBoxLayout>
#include <QGridLayout>
#include <QtTest>
#include <cstdio>

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication app( argc, argv ); QTemporaryDir folder; Random::init();
  std::printf( "profile scale=%s platform=%s dpr=%.2f\n",
    qPrintable( qEnvironmentVariable( "QT_SCALE_FACTOR" ) ),
    qPrintable( QGuiApplication::platformName() ), app.devicePixelRatio() );
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
  const QMargins shadowMargins = dialog.layout()->contentsMargins();
  check( surface && shadow && dialog.testAttribute( Qt::WA_TranslucentBackground ) &&
    shadow->isVisible() && shadow->testAttribute( Qt::WA_TransparentForMouseEvents ),
    "profile has a translucent outer shadow that does not intercept mouse input" );
  check( surface && !surface->mask().contains( QPoint() ) && dialog.mask().isEmpty() &&
    shadowMargins == QMargins( 14, 14, 14, 14 ) &&
    surface->geometry().left() >= shadowMargins.left() && surface->geometry().top() >= shadowMargins.top() &&
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
  QTest::qWait( 30 ); app.processEvents();
  check( pages->currentIndex() == 1 && dialog.findChild<QLineEdit*>( "mp_lePhone" )->isVisible() &&
    dialog.findChild<QDateEdit*>( "mp_deBirthday" )->isVisible(),
    "editor shows birthday and phone directly in one form" );
  check( !dialog.findChild<QWidget*>( "mp_advanced" ) && !dialog.findChild<QPushButton*>( "mp_pbAdvanced" ) &&
    !dialog.findChild<QPushButton*>( "mp_pbRemovePhoto" ) && !dialog.findChild<QLabel*>( "mp_lEmailHint" ),
    "legacy advanced toggle remove-avatar button and email hint are gone" );
  check( !dialog.findChild<QLineEdit*>( "mp_leFirstName" ) && !dialog.findChild<QLineEdit*>( "mp_leLastName" ) &&
    !dialog.findChild<QLineEdit*>( "mp_leRoom" ), "first name surname and location fields removed" );
  QList<QLabel*> alignedLabels = QList<QLabel*>() << dialog.findChild<QLabel*>( "mp_lNickname" ) <<
    dialog.findChild<QLabel*>( "mp_lEmail" ) << dialog.findChild<QLabel*>( "mp_lInfo" ) <<
    dialog.findChild<QLabel*>( "mp_lBirthday" ) << dialog.findChild<QLabel*>( "mp_lPhone" ) <<
    dialog.findChild<QLabel*>( "mp_lColor" );
  bool labels_aligned = true;
  foreach( QLabel* label, alignedLabels )
    labels_aligned = labels_aligned && label->alignment().testFlag( Qt::AlignRight ) &&
      label->width() >= 64 && label->mapTo( &dialog, QPoint() ).x() ==
        alignedLabels.first()->mapTo( &dialog, QPoint() ).x();
  QLineEdit* alignedNickname = dialog.findChild<QLineEdit*>( "mp_leNickname" );
  QLineEdit* alignedPhone = dialog.findChild<QLineEdit*>( "mp_lePhone" );
  QDateEdit* alignedBirthday = dialog.findChild<QDateEdit*>( "mp_deBirthday" );
  check( labels_aligned && alignedNickname->width() == alignedPhone->width() &&
    alignedNickname->width() == alignedBirthday->width() && alignedNickname->height() == alignedPhone->height(),
    "all field labels share one right-aligned column and controls share width and height" );
  QPushButton* saveAction = dialog.findChild<QPushButton*>( "mp_pbOk" );
  check( saveAction->isVisible() && dialog.rect().contains( QRect( saveAction->mapTo( &dialog, QPoint() ), saveAction->size() ) ),
    "save button fully visible immediately after entering editor" );
  QScrollArea* editorScroll = dialog.findChild<QScrollArea*>( "profileEditorScroll" );
  check( editorScroll && editorScroll->horizontalScrollBar()->maximum() == 0, "editor never clips horizontally" );
  dialog.grab().save( QDir::currentPath() + "/profile-editor-first-open.png" );
  dialog.resize( 580, 360 ); QTest::qWait( 30 ); app.processEvents();
  check( dialog.rect().contains( QRect( saveAction->mapTo( &dialog, QPoint() ), saveAction->size() ) ) &&
    editorScroll->verticalScrollBar()->maximum() > 0, "small window scrolls form while keeping save footer visible" );
  check( shadow && shadow->geometry() == dialog.rect() && surface &&
    surface->geometry().left() >= shadowMargins.left() && surface->mask().contains( surface->rect().center() ),
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
  check( dialog.vCard().firstName() == "Yan" && dialog.vCard().lastName() == "Lin" && dialog.vCard().birthday() == QDate( 1998, 6, 17 ) && dialog.vCard().phoneNumber() == "020-12345678" && dialog.vCard().roomLocation() == QStringLiteral( "内容制作中心 · A12" ) && !dialog.vCard().photo().isNull(), "removed fields and avatar preserve existing data when saving other edits" );
  GuiEditVCard optional; optional.setUser( user ); optional.show(); optional.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  ShadowBirthdayEdit* optionalBirthday = optional.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" );
  check( optional.findChild<QDateEdit*>( "mp_deBirthday" )->isVisible() &&
    optional.findChild<QLineEdit*>( "mp_lePhone" )->isVisible() &&
    optional.findChild<QPushButton*>( "mp_pbRegenerateHash" )->isVisible(),
    "birthday phone and identity action reachable without an advanced toggle" );
  check( optionalBirthday && optionalBirthday->hasBirthday() && optionalBirthday->birthday() == QDate( 1998, 6, 17 ),
    "existing birthday is loaded unchanged" );
  optional.findChild<QLineEdit*>( "mp_lePhone" )->setText( "8888" );
  optional.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( optional.vCard().phoneNumber() == "8888" && optional.vCard().birthday() == QDate( 1998, 6, 17 ) &&
    !optional.regenerateUserHash(), "untouched existing birthday is preserved when saving other fields" );

  User noBirthdayUser = user; VCard noBirthdayCard = noBirthdayUser.vCard(); noBirthdayCard.setBirthday( QDate() );
  noBirthdayUser.setVCard( noBirthdayCard );
  GuiEditVCard noBirthday; noBirthday.setUser( noBirthdayUser ); noBirthday.show();
  noBirthday.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  ShadowBirthdayEdit* emptyBirthdayEdit = noBirthday.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" );
  check( emptyBirthdayEdit->text() == QStringLiteral( "未填写" ) && emptyBirthdayEdit->isEmpty() &&
    emptyBirthdayEdit->date() != QDate() && emptyBirthdayEdit->property( "empty" ).toBool(),
    "missing birthday shows an explicit empty state instead of 1900-01-01" );
  noBirthday.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( !noBirthday.vCard().birthday().isValid(),
    "saving without choosing a birthday writes no date and never fabricates 1900" );

  User legacyUser = user; VCard legacyCard = legacyUser.vCard(); legacyCard.setBirthday( QDate( 1900, 1, 1 ) );
  legacyUser.setVCard( legacyCard );
  GuiEditVCard legacy; legacy.setUser( legacyUser ); legacy.show();
  legacy.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  ShadowBirthdayEdit* legacyBirthdayEdit = legacy.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" );
  check( legacyBirthdayEdit->hasBirthday() && legacyBirthdayEdit->birthday() == QDate( 1900, 1, 1 ),
    "legacy 1900-01-01 is treated as stored data rather than an empty marker" );
  legacy.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( legacy.vCard().birthday() == QDate( 1900, 1, 1 ),
    "existing 1900-01-01 birthday is preserved without silent rewriting" );
  const QList<QDate> storedDates = { QDate( 1900, 7, 19 ), QDate( 1200, 2, 3 ),
    QDate( 1, 1, 1 ), QDate::currentDate().addYears( 20 ) };
  for( const QDate& storedDate : storedDates )
  {
    User storedUser = user;
    VCard storedCard = storedUser.vCard();
    storedCard.setBirthday( storedDate ); storedUser.setVCard( storedCard );
    GuiEditVCard stored; stored.setUser( storedUser ); stored.show();
    stored.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
    ShadowBirthdayEdit* storedEdit = stored.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" );
    check( storedEdit->birthday() == storedDate && storedEdit->hasBirthday() &&
      storedEdit->text() != QStringLiteral( "未填写" ),
      "stored 1900 early or future birthday remains an explicit valid value" );
    stored.findChild<QLineEdit*>( "mp_lePhone" )->setText( "changed-other-field" );
    stored.findChild<QPushButton*>( "mp_pbOk" )->click();
    check( stored.vCard().birthday() == storedDate && stored.vCard().phoneNumber() == "changed-other-field",
      "saving another field leaves stored 1900 early or future birthday unchanged" );
  }

  GuiEditVCard picked; picked.setUser( noBirthdayUser ); picked.show();
  picked.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  ShadowBirthdayEdit* pickedBirthday = picked.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" );
  pickedBirthday->setBirthday( QDate( 2001, 2, 3 ) );
  picked.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( picked.vCard().birthday() == QDate( 2001, 2, 3 ), "explicit birthday selection is written on save" );
  GuiEditVCard cleared; cleared.setUser( user ); cleared.show();
  cleared.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  cleared.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" )->clearBirthday();
  cleared.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( !cleared.vCard().birthday().isValid(), "explicit clear removes the birthday on save" );

  GuiEditVCard canceled; canceled.setUser( user ); canceled.show(); canceled.findChild<QPushButton*>( "editMyProfile" )->click();
  canceled.findChild<QLineEdit*>( "mp_leNickname" )->setText( "discard" );
  canceled.findChild<QPlainTextEdit*>( "mp_teInfo" )->setPlainText( "discard info" );
  canceled.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" )->setBirthday( QDate( 2003, 4, 5 ) );
  canceled.findChild<QPushButton*>( "mp_pbCancel" )->click();
  check( canceled.result() == QDialog::Rejected && user.vCard().nickName() == QStringLiteral( "林言" ) &&
    user.vCard().info() == QStringLiteral( "角色与场景设计 · 让好想法变成看得见的作品" ) &&
    user.vCard().birthday() == QDate( 1998, 6, 17 ) && !user.vCard().photo().isNull() &&
    canceled.vCard().birthday() == QDate( 1998, 6, 17 ),
    "cancel writes nothing including a newly selected birthday and keeps nickname info and avatar" );
  check( !canceled.findChild<QPushButton*>( "mp_pbRemovePhoto" ),
    "no remove-avatar control exists on the cancel path" );
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
  User legacyAccount = user;
  legacyAccount.setAccountName( QString() );
  GuiEditVCard account; account.setUser( legacyAccount ); account.show();
  account.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  QLineEdit* accountField = account.findChild<QLineEdit*>( "mp_leEmail" );
  check( accountField->isReadOnly() && accountField->focusPolicy() == Qt::StrongFocus,
    "legacy account is keyboard focusable for copying and remains read only" );
  accountField->setText( "must-not-save@example.test" );
  account.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( account.vCard().email() == user.vCard().email(),
    "read-only account cannot rewrite stored email even without accountName" );
  GuiEditVCard escape; escape.setUser( user ); escape.show();
  escape.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  escape.findChild<QLineEdit*>( "mp_lePhone" )->setText( "discard phone" );
  escape.findChild<ShadowBirthdayEdit*>( "mp_deBirthday" )->clearBirthday();
  QTest::keyClick( &escape, Qt::Key_Escape ); app.processEvents();
  check( escape.result() == QDialog::Rejected && escape.vCard().phoneNumber() == user.vCard().phoneNumber() &&
    escape.vCard().birthday() == user.vCard().birthday() && escape.vCard().email() == user.vCard().email() &&
    escape.vCard().firstName() == user.vCard().firstName() &&
    escape.vCard().lastName() == user.vCard().lastName() &&
    escape.vCard().roomLocation() == user.vCard().roomLocation() &&
    escape.vCard().photo().toImage() == user.vCard().photo().toImage(),
    "Escape discards edits and preserves every stored profile field" );
  std::printf( "PROFILE RESULT failures=%d\n", failures );
  return failures ? 1 : 0;
}
