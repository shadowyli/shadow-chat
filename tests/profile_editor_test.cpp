#include "GuiEditVCard.h"
#include "Settings.h"
#include "IconManager.h"
#include "UserManager.h"
#include "BeeApplication.h"
#include "Random.h"
#include <cstdio>

int main( int argc, char** argv )
{
  BeeApplication app( argc, argv ); QTemporaryDir folder; Random::init();
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
  int failures = 0;
  auto check = [&]( bool value, const char* text ) { std::printf( "%s %s\n", value ? "PASS" : "FAIL", text ); if( !value ) ++failures; };
  std::puts( "Fixture user ready" );
  GuiEditVCard dialog; std::puts( "Fixture dialog constructed" ); dialog.setUser( user ); std::puts( "Fixture user populated" ); dialog.show(); app.processEvents();
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
  check( dialog.findChild<QLabel*>( "profileCardAccount" )->text().contains( "lin@example.test" ), "card identifies currently signed-in account" );
  check( !dialog.findChild<QPushButton*>( "mp_pbRegenerateHash" )->isVisible(), "device identity reset absent from initial profile card" );
  QDir().mkpath( "E:/ShadowChat/delivery" ); dialog.grab().save( "E:/ShadowChat/delivery/profile-card-proof.png" );
  dialog.findChild<QPushButton*>( "editMyProfile" )->click(); app.processEvents();
  check( pages->currentIndex() == 1 && !dialog.findChild<QWidget*>( "mp_advanced" )->isVisible() && dialog.grab().toImage().pixelColor( 4, 4 ) == QColor( Qt::white ), "edit entrance reveals white concise form with optional fields collapsed" );
  check( dialog.findChild<QLineEdit*>( "mp_leEmail" )->isReadOnly(), "account email cannot be renamed through profile editor" );
  dialog.findChild<QLineEdit*>( "mp_leNickname" )->setText( QStringLiteral( "林言 · 美术" ) );
  dialog.findChild<QPlainTextEdit*>( "mp_teInfo" )->setPlainText( QStringLiteral( "角色美术与场景设计" ) );
  dialog.grab().save( "E:/ShadowChat/delivery/profile-editor-proof.png" );
  dialog.findChild<QLineEdit*>( "mp_leEmail" )->setText( "tampered@example.test" );
  dialog.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( dialog.result() == QDialog::Accepted && dialog.vCard().nickName() == QStringLiteral( "林言 · 美术" ), "save accepts edited nickname" );
  check( dialog.vCard().email() == "lin@example.test", "save preserves account email despite programmatic field change" );
  check( dialog.vCard().firstName() == "Yan" && dialog.vCard().lastName() == "Lin" && dialog.vCard().birthday() == QDate( 1998, 6, 17 ) && dialog.vCard().phoneNumber() == "020-12345678" && dialog.vCard().roomLocation() == QStringLiteral( "内容制作中心 · A12" ), "collapsed optional data survives save" );
  GuiEditVCard optional; optional.setUser( user ); optional.show(); optional.findChild<QPushButton*>( "editMyProfile" )->click(); optional.findChild<QPushButton*>( "mp_pbAdvanced" )->click(); app.processEvents();
  check( optional.findChild<QLineEdit*>( "mp_leFirstName" )->isVisible() && optional.findChild<QPushButton*>( "mp_pbRegenerateHash" )->isVisible(), "advanced details retain optional editing and identity action" );
  optional.findChild<QLineEdit*>( "mp_lePhone" )->setText( "8888" ); optional.findChild<QPushButton*>( "mp_pbOk" )->click();
  check( optional.vCard().phoneNumber() == "8888" && !optional.regenerateUserHash(), "optional field edits save without resetting identity" );
  GuiEditVCard canceled; canceled.setUser( user ); canceled.show(); canceled.findChild<QPushButton*>( "editMyProfile" )->click();
  canceled.findChild<QLineEdit*>( "mp_leNickname" )->setText( "discard" ); canceled.findChild<QPushButton*>( "mp_pbRemovePhoto" )->click(); canceled.findChild<QPushButton*>( "mp_pbCancel" )->click();
  check( canceled.result() == QDialog::Rejected && user.vCard().nickName() == QStringLiteral( "林言" ) && !user.vCard().photo().isNull(), "cancel leaves source user nickname and avatar untouched" );
  GuiEditVCard closed; closed.setUser( user ); closed.findChild<QPushButton*>( "closeMyProfile" )->click();
  check( closed.result() == QDialog::Rejected && !closed.regenerateUserHash(), "closing default card never applies profile changes" );
  return failures ? 1 : 0;
}
