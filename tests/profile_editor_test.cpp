#include "GuiEditVCard.h"
#include "Settings.h"
#include "IconManager.h"
#include "UserManager.h"
#include "BeeApplication.h"
#include "Random.h"
#include "ShadowBirthdayEdit.h"
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include <QtTest>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

static QRect geometryIn( QWidget* widget, QWidget* target )
{
  return QRect( widget->mapTo( target, QPoint() ), widget->size() );
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication app( argc, argv );
  QTemporaryDir folder;
  CHECK( folder.isValid() );
  app.setProperty( "shadowAccountRoot", folder.path() );
  app.setFont( ShadowStyle::applicationFont() );
  ShadowDialogChrome::install( &app );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "synthetic-inline-profile" );
  settings.setAccountDataFolder( folder.path() );
  settings.load();
  IconManager::instance();
  UserManager::instance();
  User user = settings.localUser();
  user.setAccountName( "inline@example.test" );
  user.setName( QStringLiteral( "林言" ) );
  VCard card;
  card.setNickName( QStringLiteral( "林言" ) );
  card.setEmail( "inline@example.test" );
  card.setInfo( QStringLiteral( "角色与场景设计" ) );
  card.setPhoneNumber( "020-12345678" );
  card.setFirstName( "Yan" );
  card.setLastName( "Lin" );
  card.setRoomLocation( QStringLiteral( "A12" ) );
  card.setBirthday( QDate( 1998, 6, 17 ) );
  QPixmap photo( 96, 96 );
  photo.fill( QColor( "#609BEA" ) );
  card.setPhoto( photo );
  user.setVCard( card );
  user.setWorkgroups( QStringList{ QStringLiteral( "美术组" ) } );
  settings.setLocalUser( user );
  UserManager::instance().setUser( user );
  QWidget host;
  host.resize( 800, 600 );
  host.show();
  GuiEditVCard dialog( &host );
  dialog.setUser( user );
  dialog.show();
  QTest::qWait( 40 );
  QStackedWidget* pages = dialog.findChild<QStackedWidget*>( "ShadowProfilePages" );
  QLabel* nickname = dialog.findChild<QLabel*>( "profileCardNickname" );
  QPushButton* edit = dialog.findChild<QPushButton*>( "editMyProfile" );
  CHECK( pages && pages->currentIndex() == 0 && edit && edit->isVisible() );
  CHECK( qAbs( geometryIn( nickname, &dialog ).center().y() - geometryIn( edit, &dialog ).center().y() ) <= 2 );
  CHECK( dialog.findChild<QLabel*>( "profileCardGroups" )->text() == QStringLiteral( "美术组" ) );
  CHECK( dialog.findChild<QLabel*>( "profileCardEmail" )->text() == "inline@example.test" );
  CHECK( !dialog.findChild<QPushButton*>( "mp_pbRegenerateHash" )->isVisible() );
  CHECK( !dialog.findChild<QWidget*>( "profileEditor" )->isVisible() );
  QToolButton* avatar = dialog.findChild<QToolButton*>( "profileCardAvatar" );
  CHECK( avatar && !avatar->icon().isNull() && avatar->focusPolicy() != Qt::NoFocus );
  CHECK( avatar->accessibleName() == QStringLiteral( "更换头像" ) && avatar->cursor().shape() == Qt::PointingHandCursor );
  CHECK( !dialog.findChild<QToolButton*>( "profileEditPhoto" ) );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR", QDir::currentPath() );
  QDir().mkpath( proof );
  CHECK( dialog.grab().save( QDir( proof ).filePath( "inline-profile-card.png" ) ) );
  const int cardWidth = dialog.width();
  edit->click();
  QTest::qWait( 30 );
  QLineEdit* nickEdit = dialog.findChild<QLineEdit*>( "mp_leNickname" );
  CHECK( nickEdit->isVisible() && pages->currentIndex() == 0 && dialog.width() == cardWidth );
  CHECK( !dialog.findChild<QWidget*>( "profileEditor" )->isVisible() );
  nickEdit->setText( QStringLiteral( "林言 · 美术" ) );
  QTest::keyClick( nickEdit, Qt::Key_Return );
  QTest::qWait( 20 );
  CHECK( !nickEdit->isVisible() && nickname->text() == QStringLiteral( "林言 · 美术" ) );
  CHECK( dialog.vCard().nickName() == user.vCard().nickName() );
  dialog.findChild<QToolButton*>( "profileEditInfo" )->click();
  QPlainTextEdit* info = dialog.findChild<QPlainTextEdit*>( "mp_teInfo" );
  CHECK( info->isVisible() && pages->currentIndex() == 0 );
  info->setPlainText( QStringLiteral( "角色设计\n不跳转编辑" ) );
  dialog.findChild<QToolButton*>( "profileEditPhone" )->click();
  QLineEdit* phone = dialog.findChild<QLineEdit*>( "mp_lePhone" );
  CHECK( phone->isVisible() && pages->currentIndex() == 0 );
  phone->setText( "8888" );
  dialog.findChild<QToolButton*>( "profileEditWorkgroups" )->click();
  QTest::qWait( 30 );
  QLineEdit* groups = dialog.findChild<QLineEdit*>( "profileWorkgroupsEdit" );
  std::printf( "group editor visible=%d focus=%d page=%d active=%d focus-object=%s\n",
    groups->isVisible(), groups->hasFocus(), pages->currentIndex(), dialog.isActiveWindow(),
    QApplication::focusWidget() ? qPrintable( QApplication::focusWidget()->objectName() ) : "(none)" );
  dialog.grab().save( QDir( proof ).filePath( "inline-profile-before-group-save.png" ) );
  CHECK( groups->isVisible() && groups->hasFocus() && pages->currentIndex() == 0 );
  groups->setText( QStringLiteral( " 美术组、程序组, 美术组 " ) );
  CHECK( dialog.grab().save( QDir( proof ).filePath( "inline-profile-editing.png" ) ) );
  QPushButton* save = dialog.findChild<QPushButton*>( "closeMyProfile" );
  CHECK( save->text() == QStringLiteral( "保存" ) && dialog.rect().contains( geometryIn( save, &dialog ) ) );
  CHECK( !dialog.findChild<QPushButton*>( "mp_pbOk" )->isVisible() );
  dialog.findChild<QLineEdit*>( "mp_leEmail" )->setText( "tampered@example.test" );
  QSignalSpy accepted( &dialog, &QDialog::accepted );
  save->click();
  CHECK( accepted.count() == 1 && dialog.result() == QDialog::Accepted );
  CHECK( dialog.vCard().nickName() == QStringLiteral( "林言 · 美术" ) &&
    dialog.vCard().info() == QStringLiteral( "角色设计\n不跳转编辑" ) && dialog.vCard().phoneNumber() == "8888" );
  CHECK( dialog.workgroups() == QStringList( { QStringLiteral( "美术组" ), QStringLiteral( "程序组" ) } ) );
  CHECK( dialog.vCard().email() == card.email() && dialog.vCard().birthday() == card.birthday() &&
    dialog.vCard().firstName() == card.firstName() && dialog.vCard().lastName() == card.lastName() &&
    dialog.vCard().roomLocation() == card.roomLocation() && dialog.vCard().photo().toImage() == card.photo().toImage() );
  CHECK( !dialog.regenerateUserHash() && settings.localUser().vCard().nickName() == card.nickName() );
  std::puts( "PASS inline nickname/signature/phone/workgroups save as one draft without switching to the large form" );
  for( const QString& closing : QStringList{ "cancel", "escape", "chrome" } )
  {
    GuiEditVCard canceled( &host );
    canceled.setUser( user );
    canceled.show();
    QTest::qWait( 20 );
    canceled.findChild<QToolButton*>( "profileEditWorkgroups" )->click();
    canceled.findChild<QLineEdit*>( "profileWorkgroupsEdit" )->setText( "discard-groups" );
    canceled.findChild<QToolButton*>( "profileEditPhone" )->click();
    canceled.findChild<QLineEdit*>( "mp_lePhone" )->setText( "discard-phone" );
    QSignalSpy noSave( &canceled, &QDialog::accepted );
    if( closing == "cancel" ) canceled.findChild<QPushButton*>( "cancelInlineProfile" )->click();
    else if( closing == "escape" ) QTest::keyClick( &canceled, Qt::Key_Escape );
    else canceled.findChild<QToolButton*>( "shadowDialogClose" )->click();
    CHECK( canceled.result() == QDialog::Rejected && noSave.isEmpty() );
    CHECK( canceled.workgroups() == user.workgroups() && canceled.vCard() == card );
  }
  std::puts( "PASS cancel, Escape and title close discard all inline drafts and preserve stored profile data" );
  GuiEditVCard invalid( &host );
  invalid.setUser( user );
  invalid.show();
  QTest::qWait( 20 );
  invalid.findChild<QPushButton*>( "editMyProfile" )->click();
  invalid.findChild<QLineEdit*>( "mp_leNickname" )->clear();
  invalid.findChild<QPushButton*>( "closeMyProfile" )->click();
  CHECK( invalid.isVisible() && invalid.findChild<QLabel*>( "profileNicknameError" )->isVisible() &&
    !QApplication::activeModalWidget() && invalid.vCard() == card );
  invalid.findChild<QLineEdit*>( "mp_leNickname" )->setText( card.nickName() );
  invalid.findChild<QToolButton*>( "profileEditWorkgroups" )->click();
  invalid.findChild<QLineEdit*>( "profileWorkgroupsEdit" )->setText( QString( 129, QLatin1Char( 'A' ) ) );
  invalid.findChild<QPushButton*>( "closeMyProfile" )->click();
  CHECK( invalid.isVisible() && invalid.findChild<QLabel*>( "profileGroupsRowError" )->isVisible() &&
    !QApplication::activeModalWidget() && invalid.workgroups() == user.workgroups() );
  invalid.reject();
  GuiEditVCard noDate( &host );
  User missingDate = user;
  VCard noDateCard = card;
  noDateCard.setBirthday( QDate() );
  missingDate.setVCard( noDateCard );
  noDate.setUser( missingDate );
  noDate.show();
  noDate.findChild<QToolButton*>( "profileEditPhone" )->click();
  noDate.findChild<QLineEdit*>( "mp_lePhone" )->setText( "7777" );
  noDate.findChild<QPushButton*>( "closeMyProfile" )->click();
  CHECK( !noDate.vCard().birthday().isValid() && noDate.vCard().phoneNumber() == "7777" );
  std::puts( "PASS field validation stays inline, workgroup errors keep original data, missing birthday stays missing" );
  const QString replacementPath = folder.filePath( "replacement.png" );
  QPixmap replacement( 96, 96 );
  replacement.fill( QColor( "#32B891" ) );
  CHECK( replacement.save( replacementPath ) );
  const bool nativeDisabled = QApplication::testAttribute( Qt::AA_DontUseNativeDialogs );
  const bool nativeDialogs = settings.useNativeDialogs();
  settings.setUseNativeDialogs( false );
  QApplication::setAttribute( Qt::AA_DontUseNativeDialogs, true );
  for( const QString& operation : QStringList{ "picker-cancel", "draft-cancel", "save" } )
  {
    GuiEditVCard changing( &host );
    changing.setUser( user );
    changing.show();
    changing.raise();
    changing.activateWindow();
    QTest::qWait( 80 );
    QToolButton* target = changing.findChild<QToolButton*>( "profileCardAvatar" );
    CHECK( target );
    CHECK( target->isVisible() && target->isEnabled() );
    QSignalSpy clicks( target, &QToolButton::clicked );
    const QImage previousIcon = target->icon().pixmap( QSize( 60, 60 ) ).toImage();
    bool handled = false;
    QPointer<QFileDialog> openedPicker;
    QTimer driver;
    QObject::connect( &driver, &QTimer::timeout, &changing, [&]() {
      QFileDialog* picker = changing.findChild<QFileDialog*>();
      if( !picker )
        for( QWidget* widget : QApplication::topLevelWidgets() )
          if( QFileDialog* candidate = qobject_cast<QFileDialog*>( widget ) )
            if( candidate->isVisible() ) picker = candidate;
      if( !picker ) return;
      openedPicker = picker;
      if( operation == "picker-cancel" )
      {
        driver.stop();
        picker->reject();
        handled = true;
      }
      else
      {
        QLineEdit* filename = picker->findChild<QLineEdit*>( "fileNameEdit" );
        if( !filename ) return;
        driver.stop();
        filename->setText( QDir::toNativeSeparators( replacementPath ) );
        handled = QMetaObject::invokeMethod( picker, "accept", Qt::DirectConnection ) &&
          picker->result() == QDialog::Accepted;
      }
    } );
    QTimer guard;
    guard.setSingleShot( true );
    QObject::connect( &guard, &QTimer::timeout, &changing, [&]() {
      QWidget* modal = QApplication::activeModalWidget();
      std::fprintf( stderr, "FAIL avatar picker timed out operation=%s clicks=%d handled=%d picker=%d modal=%s\n",
        qPrintable( operation ), clicks.count(), handled, !openedPicker.isNull(), modal ? modal->metaObject()->className() : "(none)" );
      std::exit( 1 );
    } );
    driver.start( 25 );
    guard.start( 5000 );
    if( operation == "save" ) QTest::keyClick( target, Qt::Key_Space );
    else if( operation == "draft-cancel" ) target->click();
    else QTest::mouseClick( target, Qt::LeftButton );
    driver.stop();
    guard.stop();
    CHECK( handled && changing.isVisible() && changing.vCard().photo().toImage() == card.photo().toImage() );
    CHECK( changing.findChild<QStackedWidget*>( "ShadowProfilePages" )->currentIndex() == 0 );
    if( operation == "picker-cancel" )
    {
      CHECK( target->icon().pixmap( QSize( 60, 60 ) ).toImage() == previousIcon );
      CHECK( changing.findChild<QPushButton*>( "closeMyProfile" )->text() == QStringLiteral( "完成" ) );
      changing.reject();
    }
    else if( operation == "draft-cancel" )
    {
      CHECK( target->icon().pixmap( QSize( 60, 60 ) ).toImage() != previousIcon );
      CHECK( changing.findChild<QPushButton*>( "closeMyProfile" )->text() == QStringLiteral( "保存" ) );
      CHECK( changing.grab().save( QDir( proof ).filePath( "clickable-avatar-draft.png" ) ) );
      changing.findChild<QPushButton*>( "cancelInlineProfile" )->click();
      CHECK( changing.vCard().photo().toImage() == card.photo().toImage() );
    }
    else
    {
      changing.findChild<QPushButton*>( "closeMyProfile" )->click();
      CHECK( changing.result() == QDialog::Accepted && changing.vCard().photo().toImage() == replacement.toImage() );
      CHECK( changing.vCard().info() == card.info() && changing.vCard().birthday() == card.birthday() );
    }
    CHECK( settings.localUser().vCard().photo().toImage() == card.photo().toImage() );
  }
  QApplication::setAttribute( Qt::AA_DontUseNativeDialogs, nativeDisabled );
  settings.setUseNativeDialogs( nativeDialogs );
  std::puts( "PASS avatar click/Space opens picker without a pencil; picker cancel, draft cancel and save preserve profile boundaries" );
  std::puts( "ALL INLINE PROFILE CHECKS PASSED" );
  return 0;
}
