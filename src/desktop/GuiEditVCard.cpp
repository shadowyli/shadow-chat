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
// $Id: GuiEditVCard.cpp 1550 2023-02-01 08:50:11Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "Avatar.h"
#include "BeeUtils.h"
#include "FileDialog.h"
#include "GuiEditVCard.h"
#include "IconManager.h"
#include "Settings.h"
#include "UserManager.h"
#include "ShadowDialogStyle.h"


GuiEditVCard::GuiEditVCard( QWidget *parent )
  : QDialog( parent ), m_vCard(), m_userColor( "#000000" ), m_regenerateUserHash( false ), m_profilePages( Q_NULLPTR )
{
  setupUi( this );
  setObjectName( "GuiEditVCard" );
  setWindowTitle( QStringLiteral( "我的资料 · 影聊" ) );
  setWindowIcon( IconManager::instance().icon( "profile-edit.png" ) );
  Bee::removeContextHelpButton( this );

  mp_lTitle->setProperty( "role", "title" );
  mp_lIdentity->setProperty( "role", "hint" );
  mp_lEmailHint->setProperty( "role", "hint" );
  mp_pbOk->setProperty( "role", "primary" );
  ShadowDialogStyle::apply( this );
  setMinimumWidth( 560 );
  mp_advanced->hide();
  connect( mp_pbAdvanced, &QPushButton::toggled, this, [this]( bool expanded ) {
    mp_advanced->setVisible( expanded );
    mp_pbAdvanced->setText( expanded ? QStringLiteral( "收起更多资料 ▴" ) : QStringLiteral( "更多资料与设置 ▾" ) );
    resizeProfilePage();
  } );
  mp_deBirthday->setToolTip( tr( "If you don't want to show the year to other users use 1900. To remove your birthday use 01/01/1900" ) );

  connect( mp_pbOk, SIGNAL( clicked() ), this, SLOT( onOkClicked() ) );
  connect( mp_pbCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
  connect( mp_pbChangePhoto, SIGNAL( clicked() ), this, SLOT( changePhoto() ) );
  connect( mp_pbRemovePhoto, SIGNAL( clicked() ), this, SLOT( removePhoto() ) );
  connect( mp_pbColor, SIGNAL( clicked() ), this, SLOT( changeUserColor() ) );
  connect( mp_pbRegenerateHash, SIGNAL( clicked() ), this, SLOT( regenerateHash() ) );

  // The initial view is a profile card. Editing is an explicit second step;
  // closing the card never applies changes to the current account.
  QWidget* editor = new QWidget;
  editor->setObjectName( "profileEditor" );
  editor->setProperty( "shadowDialog", true );
  editor->setLayout( mainLayout );
  m_profilePages = new QStackedWidget( this );
  QWidget* card = new QWidget;
  card->setObjectName( "myProfileCard" );
  QVBoxLayout* cardLayout = new QVBoxLayout( card );
  cardLayout->setContentsMargins( 24, 24, 24, 20 ); cardLayout->setSpacing( 22 );
  QFrame* header = new QFrame;
  header->setObjectName( "profileCardHeader" );
  header->setStyleSheet( "QFrame#profileCardHeader{background:#eef4ff;border-radius:12px;}" );
  QHBoxLayout* headerLayout = new QHBoxLayout( header );
  headerLayout->setContentsMargins( 22, 24, 22, 24 ); headerLayout->setSpacing( 18 );
  m_cardPhoto = new QLabel; m_cardPhoto->setObjectName( "profileCardAvatar" ); m_cardPhoto->setFixedSize( 80, 80 );
  headerLayout->addWidget( m_cardPhoto );
  QVBoxLayout* identity = new QVBoxLayout;
  QLabel* ownLabel = new QLabel( QStringLiteral( "我的资料" ) ); ownLabel->setProperty( "role", "hint" );
  m_cardNickname = new QLabel; m_cardNickname->setObjectName( "profileCardNickname" ); m_cardNickname->setProperty( "role", "title" );
  m_cardAccount = new QLabel; m_cardAccount->setObjectName( "profileCardAccount" ); m_cardAccount->setProperty( "role", "hint" ); m_cardAccount->setWordWrap( true );
  identity->addWidget( ownLabel ); identity->addWidget( m_cardNickname ); identity->addWidget( m_cardAccount );
  headerLayout->addLayout( identity, 1 );
  QPushButton* edit = new QPushButton( QStringLiteral( "编辑资料" ) ); edit->setObjectName( "editMyProfile" ); edit->setAutoDefault( false );
  headerLayout->addWidget( edit, 0, Qt::AlignTop );
  cardLayout->addWidget( header );
  QLabel* infoTitle = new QLabel( QStringLiteral( "个人简介" ) ); infoTitle->setProperty( "role", "hint" );
  m_cardInfo = new QLabel; m_cardInfo->setObjectName( "profileCardInfo" ); m_cardInfo->setWordWrap( true ); m_cardInfo->setTextInteractionFlags( Qt::TextSelectableByMouse );
  cardLayout->addWidget( infoTitle ); cardLayout->addWidget( m_cardInfo );
  QFormLayout* details = new QFormLayout; details->setHorizontalSpacing( 24 ); details->setVerticalSpacing( 18 );
  m_cardEmail = new QLabel; m_cardEmail->setWordWrap( true ); m_cardEmail->setTextInteractionFlags( Qt::TextSelectableByMouse );
  m_cardPhone = new QLabel; m_cardLocation = new QLabel;
  details->addRow( QStringLiteral( "邮箱" ), m_cardEmail ); details->addRow( QStringLiteral( "电话" ), m_cardPhone ); details->addRow( QStringLiteral( "位置" ), m_cardLocation );
  cardLayout->addLayout( details ); cardLayout->addStretch();
  QPushButton* close = new QPushButton( QStringLiteral( "完成" ) ); close->setObjectName( "closeMyProfile" ); close->setAutoDefault( false );
  QHBoxLayout* footer = new QHBoxLayout; footer->addStretch(); footer->addWidget( close ); cardLayout->addLayout( footer );
  QScrollArea* editorScroll = new QScrollArea; editorScroll->setObjectName( "profileEditorScroll" );
  editorScroll->setWidget( editor ); editorScroll->setWidgetResizable( true ); editorScroll->setFrameShape( QFrame::NoFrame );
  editorScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  m_profilePages->addWidget( card ); m_profilePages->addWidget( editorScroll );
  QVBoxLayout* outer = new QVBoxLayout( this ); outer->setContentsMargins( 0, 0, 0, 0 ); outer->addWidget( m_profilePages );
  connect( edit, &QPushButton::clicked, this, [this, editorScroll]() { m_profilePages->setCurrentWidget( editorScroll ); resizeProfilePage(); mp_leNickname->setFocus(); } );
  connect( close, &QPushButton::clicked, this, &QDialog::reject );
  setStyleSheet( styleSheet() + QStringLiteral( "QWidget{font-family:'Microsoft YaHei UI';font-size:13px;} QLabel[role=title]{font-size:22px;} QLabel[role=hint]{font-size:12px;}" ) );
}

void GuiEditVCard::resizeProfilePage()
{
  if( !m_profilePages ) return;
  QWidget* page = m_profilePages->currentWidget();
  QScrollArea* scroll = qobject_cast<QScrollArea*>( page );
  const int contentHeight = scroll ? scroll->widget()->sizeHint().height() : page->sizeHint().height();
  const QScreen* display = QGuiApplication::primaryScreen();
  const int available = display ? qMax( 360, display->availableGeometry().height() - 100 ) : 700;
  m_profilePages->setFixedHeight( qMin( contentHeight, available ) );
  adjustSize();
}

QString GuiEditVCard::currentAvatarName() const
{
  QString current_avatar_name = "";
  if( Settings::instance().useUserFullName() )
    current_avatar_name = Settings::instance().useUserFirstNameFirstInFullName() ?
      QString( "%1 %2" ).arg( mp_leFirstName->text().simplified(), mp_leLastName->text().simplified() ).trimmed() :
      QString( "%1 %2" ).arg( mp_leLastName->text().simplified(), mp_leFirstName->text().simplified() ).trimmed();

  if( current_avatar_name.isEmpty() )
    current_avatar_name = mp_leNickname->text().simplified();
  return current_avatar_name.isEmpty() ? QLatin1String( "??" ) : current_avatar_name;
}

void GuiEditVCard::setUserColor( const QString& new_value )
{
  m_userColor = new_value;
  QColor c( m_userColor );
  QPalette palette = mp_leNickname->palette();
  palette.setColor( QPalette::Text, c );
  mp_leNickname->setPalette( palette );
  palette.setColor( QPalette::Foreground, c );
  mp_lNickname->setPalette( palette );
  if( m_vCard.photo().isNull() )
    mp_lPhoto->setPixmap( Avatar::create( currentAvatarName(), m_userColor, QSize( 96, 96 ) ) );
}

void GuiEditVCard::setUser( const User& u )
{
  m_vCard = u.vCard();
  m_accountEmail = u.accountName().trimmed();
  setUserColor( u.color() );
  m_regenerateUserHash = false;
  loadVCard();
  mp_lIdentity->setText( QStringLiteral( "正在编辑自己的资料，同事会看到你的昵称和头像。" ) );
  mp_leEmail->setReadOnly( !m_accountEmail.isEmpty() );
  if( !m_accountEmail.isEmpty() ) mp_leEmail->setText( m_accountEmail );
  mp_leEmail->setToolTip( QStringLiteral( "切换账号请使用主菜单的退出登录；修改资料不会更换账号或聊天记录。" ) );
  m_cardNickname->setText( m_vCard.nickName().isEmpty() ? u.name() : m_vCard.nickName() );
  m_cardAccount->setText( QStringLiteral( "当前登录 · %1" ).arg( m_accountEmail.isEmpty() ? m_vCard.email() : m_accountEmail ) );
  m_cardEmail->setText( m_accountEmail.isEmpty() ? m_vCard.email() : m_accountEmail );
  m_cardInfo->setText( m_vCard.info().isEmpty() ? QStringLiteral( "还没有填写个人简介" ) : m_vCard.info() );
  m_cardPhone->setText( m_vCard.phoneNumber().isEmpty() ? QStringLiteral( "未填写" ) : m_vCard.phoneNumber() );
  m_cardLocation->setText( m_vCard.roomLocation().isEmpty() ? QStringLiteral( "未填写" ) : m_vCard.roomLocation() );
  QPixmap source = m_vCard.photo().isNull() ? Avatar::create( currentAvatarName(), m_userColor, QSize( 80, 80 ) ) : m_vCard.photo();
  QPixmap circle( 80, 80 ); circle.fill( Qt::transparent );
  QPainter painter( &circle ); painter.setRenderHint( QPainter::Antialiasing );
  QPainterPath clip; clip.addEllipse( 0, 0, 80, 80 ); painter.setClipPath( clip );
  QPixmap scaled = source.scaled( 80, 80, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
  painter.drawPixmap( (80 - scaled.width()) / 2, (80 - scaled.height()) / 2, scaled ); painter.end();
  m_cardPhoto->setPixmap( circle );
  foreach( QLabel* label, QList<QLabel*>() << m_cardNickname << m_cardAccount << m_cardEmail << m_cardInfo << m_cardPhone << m_cardLocation )
    label->setTextFormat( Qt::PlainText );
  m_profilePages->setCurrentIndex( 0 );
  resizeProfilePage();
}

void GuiEditVCard::loadVCard()
{
  mp_leNickname->setText( m_vCard.nickName() );
  if( Settings::instance().allowEditNickname() )
  {
    mp_leNickname->setEnabled( true  );
    mp_leNickname->setReadOnly( false );
    mp_leNickname->setToolTip( "" );
  }
  else
  {
    mp_leNickname->setEnabled( false  );
    mp_leNickname->setReadOnly( true  );
    mp_leNickname->setToolTip( tr( "Disabled by system administrator" ) );
  }
  mp_leFirstName->setText( m_vCard.firstName() );
  mp_leLastName->setText( m_vCard.lastName() );

  if( m_vCard.birthday().isValid() )
    mp_deBirthday->setDate( m_vCard.birthday() );
  else
    mp_deBirthday->setDate( QDate( 1900, 1, 1 ) );

  mp_leEmail->setText( m_vCard.email() );

  if( m_vCard.photo().isNull() )
    mp_lPhoto->setPixmap( Avatar::create( currentAvatarName(), m_userColor, QSize( 96, 96 ) ) );
  else
    mp_lPhoto->setPixmap( m_vCard.photo().scaled( 96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );

  mp_lePhone->setText( m_vCard.phoneNumber() );

  mp_leRoom->setText( m_vCard.roomLocation() );

  mp_teInfo->setPlainText( m_vCard.info() );

  mp_leNickname->setFocus();
}

void GuiEditVCard::changePhoto()
{
  QList<QByteArray> supported_formats = QImageReader::supportedImageFormats();
  QString supported_formats_string = "";

  foreach( QByteArray sf, supported_formats )
  {
    supported_formats_string += QString( " *." );
    supported_formats_string.append( sf );
  }

  QString photo_path = FileDialog::getOpenFileName( true, this, tr( "%1 - Select your profile photo" ).arg( Settings::instance().programName() ),
                                                     Settings::instance().lastDirectorySelected(), tr( "Images" ) + QString( " (%1)" ).arg( supported_formats_string.simplified() ) );
  if( photo_path.isNull() || photo_path.isEmpty() )
    return;

  Settings::instance().setLastDirectorySelectedFromFile( photo_path );

  QImage img;
  QImageReader img_reader( photo_path );
  img_reader.setAutoDetectImageFormat( true );
  if( !img_reader.read( &img ) )
  {
    QMessageBox::warning( this, Settings::instance().programName(), tr( "Unable to load image %1." ).arg( photo_path ), tr( "Ok" ) );
    qWarning() << "Can not load profile picture. Format supported:" << supported_formats_string;
    return;
  }

  if( img.width() != img.height() )
    QMessageBox::information( this, Settings::instance().programName(), tr( "It is preferable to use square images to avoid display problems." ), tr( "Ok" ) );
  QPixmap pix = QPixmap::fromImage( img.scaled( 96, 96, Qt::KeepAspectRatio ) );
  if( !pix.isNull() )
  {
    mp_lPhoto->setPixmap( pix );
    m_vCard.setPhoto( pix );
  }
}

void GuiEditVCard::removePhoto()
{
  mp_lPhoto->setPixmap( Avatar::create( currentAvatarName(), m_userColor, QSize( 96, 96 ) ) );
  m_vCard.setPhoto( QPixmap() );
}

void GuiEditVCard::changeUserColor()
{
  QColor c = QColorDialog::getColor( QColor( Settings::instance().chatFontColor() ), this );
  if( c.isValid() )
    setUserColor( c.name() );
}

bool GuiEditVCard::checkLineEdit( QLineEdit* ple, const QString& msg )
{
  QString s = ple->text().simplified();
  if( s.isEmpty() )
  {
    QMessageBox::information( this, Settings::instance().programName(), msg, tr( "Ok" ) );
    ple->setFocus();
    return false;
  }
  return true;
}

bool GuiEditVCard::checkData()
{
  if( !checkLineEdit( mp_leNickname, tr( "Please insert your nickname." ) ) )
    return false;

  QString user_nickname = mp_leNickname->text().simplified();
  User u = UserManager::instance().findUserByNickname( user_nickname );
  if( u.isValid() && !u.isLocal() )
  {
    QMessageBox::warning( this, Settings::instance().programName(), tr( "The nickname '%1' is already in use by the user %2." ).arg( user_nickname, u.path() ) );
    mp_leNickname->setFocus();
    return false;
  }

  return true;
}

void GuiEditVCard::saveVCard()
{
  m_vCard.setNickName( mp_leNickname->text().simplified() );
  m_vCard.setFirstName( mp_leFirstName->text().simplified() );
  m_vCard.setLastName( mp_leLastName->text().simplified() );
  if( mp_deBirthday->date() != QDate( 1900, 1, 1 ) )
    m_vCard.setBirthday( mp_deBirthday->date() );
  else
    m_vCard.setBirthday( QDate() );
  if( m_accountEmail.isEmpty() ) m_vCard.setEmail( mp_leEmail->text().simplified() );
  m_vCard.setPhoneNumber( mp_lePhone->text().simplified() );
  m_vCard.setRoomLocation( mp_leRoom->text().simplified() );
  m_vCard.setInfo( mp_teInfo->toPlainText().trimmed() );
}

void GuiEditVCard::onOkClicked()
{
  if( !checkData() )
    return;
  saveVCard();
  accept();
}

void GuiEditVCard::regenerateHash()
{
  if( !checkData() )
    return;

  if( QMessageBox::question( this, Settings::instance().programName(),
                 tr( "Be careful, by changing the hash code you may no longer be a member of some groups and you will have to get invited again. Do you really want to regenerate your hash code?" ),
                 tr( "Yes" ), tr( "No" ), QString(), 1, 1 ) == 0 )
  {
    m_regenerateUserHash = true;
    saveVCard();
    accept();
  }
}
