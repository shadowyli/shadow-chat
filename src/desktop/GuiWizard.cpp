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
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"


GuiWizard::GuiWizard( QWidget *parent )
  : QDialog( parent ), mp_avatarButton( Q_NULLPTR )
{
  setProperty( "shadowDialogChromeSkip", true );
  setupUi( this );
  setObjectName( "ShadowOnboardingDialog" );
  setWindowTitle( QStringLiteral( "欢迎使用影聊" ) );
  Bee::removeContextHelpButton( this );
  setMinimumSize( 520, 320 );
  resize( 560, 350 );
  setStyleSheet( ShadowStyle::applicationStyleSheet() );

  delete layout();
  QVBoxLayout* root_layout = new QVBoxLayout( this );
  root_layout->setContentsMargins( 36, 30, 36, 26 );
  root_layout->setSpacing( 14 );

  mp_lWelcome->setText( QStringLiteral( "欢迎使用影聊" ) );
  mp_lWelcome->setObjectName( "ShadowOnboardingTitle" );
  QLabel* subtitle = new QLabel( QStringLiteral( "设置名字和头像，开始与同事聊天" ), this );
  subtitle->setObjectName( "ShadowOnboardingSubtitle" );
  root_layout->addWidget( mp_lWelcome );
  root_layout->addWidget( subtitle );

  QFrame* identity_card = new QFrame( this );
  identity_card->setObjectName( "ShadowOnboardingCard" );
  QGridLayout* identity_layout = new QGridLayout( identity_card );
  identity_layout->setContentsMargins( 18, 16, 18, 16 );
  identity_layout->setHorizontalSpacing( 14 );
  identity_layout->setVerticalSpacing( 8 );
  mp_lPix->hide();
  mp_avatarButton = new QToolButton(identity_card);
  mp_avatarButton->setObjectName("ShadowOnboardingAvatar");
  mp_avatarButton->setFixedSize(72,72);
  mp_avatarButton->setIconSize(QSize(64,64));
  mp_avatarButton->setCursor(Qt::PointingHandCursor);
  mp_avatarButton->setAccessibleName(QStringLiteral("点击上传头像"));
  mp_avatarButton->setToolTip(QStringLiteral("点击上传头像"));
  connect(mp_avatarButton,&QToolButton::clicked,this,&GuiWizard::chooseAvatar);
  identity_layout->addWidget( mp_avatarButton, 0, 0, 3, 1 );
  QLabel* name_label = new QLabel( QStringLiteral( "设置你的名字和头像" ), identity_card );
  name_label->setObjectName( "ShadowOnboardingStep" );
  identity_layout->addWidget( name_label, 0, 1 );
  mp_leName->setObjectName( "ShadowOnboardingName" );
  mp_leName->setPlaceholderText( QStringLiteral( "例如：李明" ) );
  mp_leName->setAccessibleName( QStringLiteral( "你的名字" ) );
  identity_layout->addWidget( mp_leName, 1, 1 );
  mp_lAccount->setObjectName( "ShadowOnboardingAccount" );
  mp_lAccount->setWordWrap(true);
  identity_layout->addWidget( mp_lAccount, 2, 1 );
  identity_layout->setColumnStretch(1,1);
  root_layout->addWidget( identity_card );
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
  setProperty( "shadowDialogChromeSkip", false );
  ShadowDialogChrome::decorate( this );
  layout()->activate();
  adjustSize();
}

void GuiWizard::loadSettings()
{
  const QString email = Settings::instance().localUser().vCard().email();
  mp_lAccount->setText( QStringLiteral( "本机账号：%1" ).arg(
    email.isEmpty() ? Settings::instance().localUser().accountName() : email ) );
  m_avatar = Settings::instance().localUser().vCard().photo();
  if( m_avatar.isNull() )
    mp_avatarButton->setIcon( QIcon( QStringLiteral( ":/images/shadowchat/shadowchat-mark-source.png" ) ) );
  else
    mp_avatarButton->setIcon( QIcon( ShadowStyle::avatarPixmap(m_avatar,QSize(64,64)) ) );

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
  mp_avatarButton->setIcon( QIcon( ShadowStyle::avatarPixmap(avatar,QSize(64,64)) ) );
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

