//////////////////////////////////////////////////////////////////////
//
// BeeBEEP Copyright (C) 2010-2021 Marco Mastroddi
//
// BeeBEEP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// BeeBEEP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with BeeBEEP. If not, see <http://www.gnu.org/licenses/>.
//
// Author: Marco Mastroddi <marco.mastroddi(AT)gmail.com>
//
// $Id: GuiWizard.cpp 1455 2020-12-23 10:17:53Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "BeeUtils.h"
#include "GuiWizard.h"
#include "IconManager.h"
#include "Settings.h"
#include "ShadowStyle.h"


GuiWizard::GuiWizard( QWidget *parent )
  : QDialog( parent ), mp_cbCheckForUpdates( Q_NULLPTR ), mp_pbChooseAvatar( Q_NULLPTR )
{
  setupUi( this );
  setObjectName( "ShadowOnboardingDialog" );
  setWindowTitle( QStringLiteral( "欢迎使用影聊" ) );
  Bee::removeContextHelpButton( this );
  setMinimumSize( 560, 480 );
  resize( 600, 520 );
  setStyleSheet( ShadowStyle::applicationStyleSheet() );

  delete layout();
  QVBoxLayout* root_layout = new QVBoxLayout( this );
  root_layout->setContentsMargins( 36, 30, 36, 26 );
  root_layout->setSpacing( 14 );

  mp_lWelcome->setText( QStringLiteral( "欢迎使用影聊" ) );
  mp_lWelcome->setObjectName( "ShadowOnboardingTitle" );
  QLabel* subtitle = new QLabel( QStringLiteral( "三步完成设置，然后在局域网里找到同事" ), this );
  subtitle->setObjectName( "ShadowOnboardingSubtitle" );
  root_layout->addWidget( mp_lWelcome );
  root_layout->addWidget( subtitle );

  QFrame* identity_card = new QFrame( this );
  identity_card->setObjectName( "ShadowOnboardingCard" );
  QGridLayout* identity_layout = new QGridLayout( identity_card );
  identity_layout->setContentsMargins( 18, 16, 18, 16 );
  identity_layout->setHorizontalSpacing( 14 );
  identity_layout->setVerticalSpacing( 8 );
  mp_lPix->setMinimumSize( 72, 72 );
  mp_lPix->setMaximumSize( 72, 72 );
  mp_lPix->setAlignment( Qt::AlignCenter );
  mp_lPix->setObjectName( "ShadowOnboardingAvatar" );
  identity_layout->addWidget( mp_lPix, 0, 0, 3, 1 );
  QLabel* name_label = new QLabel( QStringLiteral( "1  设置你的名字和头像" ), identity_card );
  name_label->setObjectName( "ShadowOnboardingStep" );
  identity_layout->addWidget( name_label, 0, 1, 1, 2 );
  mp_leName->setObjectName( "ShadowOnboardingName" );
  mp_leName->setPlaceholderText( QStringLiteral( "例如：李明" ) );
  mp_leName->setAccessibleName( QStringLiteral( "你的名字" ) );
  identity_layout->addWidget( mp_leName, 1, 1 );
  mp_pbChooseAvatar = new QPushButton( QStringLiteral( "选择头像" ), identity_card );
  mp_pbChooseAvatar->setAccessibleName( QStringLiteral( "选择头像" ) );
  connect( mp_pbChooseAvatar, SIGNAL( clicked() ), this, SLOT( chooseAvatar() ) );
  identity_layout->addWidget( mp_pbChooseAvatar, 1, 2 );
  mp_lAccount->setObjectName( "ShadowOnboardingAccount" );
  identity_layout->addWidget( mp_lAccount, 2, 1, 1, 2 );
  root_layout->addWidget( identity_card );

  QFrame* privacy_card = new QFrame( this );
  privacy_card->setObjectName( "ShadowOnboardingTrust" );
  QVBoxLayout* privacy_layout = new QVBoxLayout( privacy_card );
  privacy_layout->setContentsMargins( 18, 14, 18, 14 );
  privacy_layout->setSpacing( 5 );
  QLabel* privacy_title = new QLabel( QStringLiteral( "2  确认使用范围" ), privacy_card );
  privacy_title->setObjectName( "ShadowOnboardingStep" );
  QLabel* privacy_text = new QLabel( QStringLiteral( "聊天、文件和通讯录留在局域网内。影聊不使用云端中转，也不需要注册账号。" ), privacy_card );
  privacy_text->setWordWrap( true );
  privacy_text->setObjectName( "ShadowOnboardingBody" );
  privacy_layout->addWidget( privacy_title );
  privacy_layout->addWidget( privacy_text );
  root_layout->addWidget( privacy_card );

  QFrame* update_card = new QFrame( this );
  update_card->setObjectName( "ShadowOnboardingCard" );
  QVBoxLayout* update_layout = new QVBoxLayout( update_card );
  update_layout->setContentsMargins( 18, 14, 18, 14 );
  update_layout->setSpacing( 6 );
  QLabel* update_title = new QLabel( QStringLiteral( "3  选择是否接收新版本提醒" ), update_card );
  update_title->setObjectName( "ShadowOnboardingStep" );
  mp_cbCheckForUpdates = new QCheckBox( QStringLiteral( "每 7 天仅检查一次版本号（推荐）" ), update_card );
  mp_cbCheckForUpdates->setToolTip( QStringLiteral( "只请求静态版本清单；请求内容不包含姓名、设备标识、联系人或使用数据。网站仍会看到普通网络连接所需的出口 IP。" ) );
  mp_cbCheckForUpdates->setAccessibleName( QStringLiteral( "接收新版本提醒" ) );
  update_layout->addWidget( update_title );
  update_layout->addWidget( mp_cbCheckForUpdates );
  root_layout->addWidget( update_card );
  root_layout->addStretch( 1 );

  QHBoxLayout* button_layout = new QHBoxLayout;
  button_layout->addStretch( 1 );
  mp_pbCancel->setText( QStringLiteral( "取消" ) );
  mp_pbChangeUser->setText( QStringLiteral( "进入影聊" ) );
  mp_pbChangeUser->setObjectName( "ShadowPrimaryButton" );
  mp_pbChangeUser->setMinimumWidth( 104 );
  button_layout->addWidget( mp_pbCancel );
  button_layout->addWidget( mp_pbChangeUser );
  root_layout->addLayout( button_layout );

  m_userName = "";
  connect( mp_pbChangeUser, SIGNAL( clicked() ), this, SLOT( saveSettings() ) );
  connect( mp_pbCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
}

void GuiWizard::loadSettings()
{
  mp_lAccount->setText( QStringLiteral( "Windows 账号：%1；无需注册" ).arg( Settings::instance().localUser().accountName() ) );
  m_avatar = Settings::instance().localUser().vCard().photo();
  if( m_avatar.isNull() )
    mp_lPix->setPixmap( QPixmap( QStringLiteral( ":/images/shadowchat/shadowchat-mark-source.png" ) ).scaled( 58, 58, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
  else
    mp_lPix->setPixmap( m_avatar.scaled( 64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation ) );
  if( mp_cbCheckForUpdates )
    mp_cbCheckForUpdates->setChecked( Settings::instance().firstTime() || Settings::instance().checkNewVersionAtStartup() );

  if( Settings::instance().allowEditNickname() )
  {
    mp_leName->setEnabled( true );
    mp_leName->setReadOnly( false );
    mp_leName->setToolTip( "" );
    if( Settings::instance().localUser().name() == Settings::instance().localUser().accountName() )
    {
      QString display_name = Settings::instance().localUser().name();
      display_name.replace( QChar( '.' ), QChar( ' ' ) );
      display_name.replace( QChar( '_' ), QChar( ' ' ) );
      mp_leName->setText( Bee::capitalizeFirstLetter( display_name, true ) );
    }
    else
      mp_leName->setText( Settings::instance().localUser().name() );
  }
  else
  {
    mp_leName->setEnabled( false );
    mp_leName->setReadOnly( true );
    mp_leName->setToolTip( tr( "Disabled by system administrator" ) );
    mp_leName->setText( Settings::instance().localUser().name() );
  }
}

void GuiWizard::chooseAvatar()
{
  const QString file_path = QFileDialog::getOpenFileName( this, QStringLiteral( "选择头像" ), QStandardPaths::writableLocation( QStandardPaths::PicturesLocation ),
                                                          QStringLiteral( "图片 (*.png *.jpg *.jpeg *.bmp *.gif)" ) );
  if( file_path.isEmpty() )
    return;
  QPixmap avatar( file_path );
  if( avatar.isNull() )
  {
    QMessageBox::information( this, QStringLiteral( "无法使用这张图片" ), QStringLiteral( "请选择 PNG、JPG、BMP 或 GIF 图片。" ) );
    return;
  }
  m_avatar = avatar;
  mp_lPix->setPixmap( avatar.scaled( 64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation ) );
}

void GuiWizard::saveSettings()
{
  QString user_name = mp_leName->text().simplified();
  if( user_name.isEmpty() )
  {
    QMessageBox::information( this, Settings::instance().programName(), tr( "Your nickname can not be empty." ) );
    mp_leName->setFocus();
    return;
  }

  m_userName = user_name;
  accept();
}

