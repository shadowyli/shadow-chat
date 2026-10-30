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
#include "ShadowStyle.h"
#include <QActionGroup>
#include <QClipboard>
#include <QMenu>
#include <QScopedValueRollback>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>

namespace
{
QToolButton* profileAction( QWidget* parent, const QString& name, const QString& icon, const QString& label )
{
  QToolButton* action = new QToolButton( parent );
  action->setObjectName( name );
  action->setIcon( ShadowStyle::actionIcon( icon ) );
  action->setIconSize( QSize( 16, 16 ) );
  action->setFixedSize( 30, 30 );
  action->setAccessibleName( label );
  action->setToolTip( label );
  return action;
}
}


GuiEditVCard::GuiEditVCard( QWidget *parent )
  : QDialog( parent ), m_vCard(), m_userColor( "#000000" ), m_regenerateUserHash( false ), m_profilePages( Q_NULLPTR )
{
  setupUi( this );
  setObjectName( "GuiEditVCard" );
  setProperty( "shadowDialogTitleHidden", true );
  setWindowTitle( QStringLiteral( "我的资料 · 影聊" ) );
  setWindowIcon( IconManager::instance().icon( "profile-edit.png" ) );
  Bee::removeContextHelpButton( this );

  mp_lTitle->setProperty( "role", "title" );
  mp_lIdentity->setProperty( "role", "hint" );
  mp_lEmailHint->setProperty( "role", "hint" );
  mp_pbOk->setProperty( "role", "primary" );
  ShadowDialogStyle::apply( this );
  setMinimumSize( 360, 320 );
  mp_advanced->show();
  connect( mp_pbAdvanced, &QPushButton::toggled, this, [this]( bool expanded ) {
    mp_advanced->setVisible( expanded );
    mp_pbAdvanced->setText( expanded ? QStringLiteral( "收起更多资料" ) : QStringLiteral( "展开更多资料" ) );
    resizeProfilePage();
    QTimer::singleShot( 0, this, [this]() { resizeProfilePage(); } );
  } );
  mp_deBirthday->setToolTip( tr( "If you don't want to show the year to other users use 1900. To remove your birthday use 01/01/1900" ) );

  connect( mp_pbOk, SIGNAL( clicked() ), this, SLOT( onOkClicked() ) );
  connect( mp_pbCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
  connect( mp_pbChangePhoto, SIGNAL( clicked() ), this, SLOT( changePhoto() ) );
  connect( mp_pbRemovePhoto, SIGNAL( clicked() ), this, SLOT( removePhoto() ) );
  connect( mp_pbColor, SIGNAL( clicked() ), this, SLOT( changeUserColor() ) );
  connect( mp_pbRegenerateHash, SIGNAL( clicked() ), this, SLOT( regenerateHash() ) );

  mainLayout->removeItem( buttonLayout );
  delete buttonLayout;
  QWidget* editor = new QWidget;
  editor->setObjectName( "profileEditor" );
  editor->setProperty( "shadowDialog", true );
  editor->setLayout( mainLayout );
  mainLayout->setSizeConstraint( QLayout::SetMinAndMaxSize );
  m_profilePages = new QStackedWidget( this );
  m_profilePages->setObjectName( QStringLiteral( "ShadowProfilePages" ) );
  m_profilePages->setMinimumSize( 0, 0 );
  m_profilePages->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
  QWidget* cardPage = new QWidget( this );
  cardPage->setObjectName( QStringLiteral( "ShadowProfileCardPage" ) );
  QVBoxLayout* cardPageLayout = new QVBoxLayout( cardPage );
  cardPageLayout->setContentsMargins( 0, 0, 0, 0 );
  cardPageLayout->setSpacing( 0 );
  QWidget* card = new QWidget;
  card->setObjectName( "myProfileCard" );
  card->setProperty( "shadowDialog", true );
  QVBoxLayout* cardLayout = new QVBoxLayout( card );
  cardLayout->setContentsMargins( 0, 0, 0, 0 ); cardLayout->setSpacing( 0 );
  QFrame* header = new QFrame;
  header->setObjectName( "profileCardHeader" );
  header->setStyleSheet( "QFrame#profileCardHeader{background:#EAF2FF;border:0;}" );
  QVBoxLayout* headerLayout = new QVBoxLayout( header );
  headerLayout->setContentsMargins( 18, 4, 18, 18 ); headerLayout->setSpacing( 14 );
  QHBoxLayout* avatarRow = new QHBoxLayout;
  m_cardPhoto = new QLabel; m_cardPhoto->setObjectName( "profileCardAvatar" ); m_cardPhoto->setFixedSize( 64, 64 );
  avatarRow->addWidget( m_cardPhoto );
  avatarRow->addStretch();
  m_cardStatus = new QPushButton;
  m_cardStatus->setObjectName( QStringLiteral( "profileCardStatus" ) );
  m_cardStatus->setAutoDefault( false );
  m_cardStatus->setAccessibleName( QStringLiteral( "切换工作状态" ) );
  QMenu* statusMenu = new QMenu( m_cardStatus );
  QActionGroup* statuses = new QActionGroup( statusMenu );
  for( int status : QList<int>() << User::Online << User::Busy << User::Away )
  {
    QAction* action = statusMenu->addAction( Bee::userStatusToString( status ) );
    action->setData( status );
    action->setCheckable( true );
    statuses->addAction( action );
    connect( action, &QAction::triggered, this, [this, status]() { emit userStatusSelected( status ); } );
  }
  m_cardStatus->setMenu( statusMenu );
  avatarRow->addWidget( m_cardStatus, 0, Qt::AlignTop );
  headerLayout->addLayout( avatarRow );
  QHBoxLayout* identity = new QHBoxLayout;
  identity->setSpacing( 4 );
  m_cardNickname = new QLabel; m_cardNickname->setObjectName( "profileCardNickname" ); m_cardNickname->setProperty( "role", "title" ); m_cardNickname->setWordWrap( true );
  m_cardNickname->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
  identity->addWidget( m_cardNickname );
  QPushButton* edit = new QPushButton;
  edit->setObjectName( "editMyProfile" ); edit->setAutoDefault( false );
  edit->setIcon( ShadowStyle::actionIcon( QStringLiteral( "pencil-simple" ) ) );
  edit->setIconSize( QSize( 16, 16 ) ); edit->setFixedSize( 30, 30 );
  edit->setAccessibleName( QStringLiteral( "编辑资料" ) ); edit->setToolTip( QStringLiteral( "编辑资料" ) );
  identity->addWidget( edit, 0, Qt::AlignTop );
  identity->addStretch();
  headerLayout->addLayout( identity );
  cardLayout->addWidget( header );
  QWidget* details = new QWidget;
  details->setObjectName( QStringLiteral( "profileCardDetails" ) );
  QVBoxLayout* detailsLayout = new QVBoxLayout( details );
  detailsLayout->setContentsMargins( 16, 8, 12, 10 ); detailsLayout->setSpacing( 0 );
  auto addRow = [&]( const QString& name, const QString& icon, QLabel* value, QToolButton* action, bool separator ) {
    QWidget* row = new QWidget( details );
    row->setObjectName( name );
    QHBoxLayout* line = new QHBoxLayout( row );
    line->setContentsMargins( 0, 9, 0, 9 ); line->setSpacing( 10 );
    QLabel* symbol = new QLabel( row );
    symbol->setFixedSize( 18, 18 );
    symbol->setPixmap( ShadowStyle::tintedPixmap( QStringLiteral( ":/images/shadowchat/icons/%1.svg" ).arg( icon ),
      QSize( 18, 18 ), QColor( "#8A96A8" ) ) );
    line->addWidget( symbol, 0, Qt::AlignTop );
    value->setMinimumWidth( 0 );
    value->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    value->setWordWrap( true );
    value->setTextFormat( Qt::PlainText );
    line->addWidget( value, 1 );
    if( action ) line->addWidget( action, 0, Qt::AlignTop );
    detailsLayout->addWidget( row );
    if( separator )
    {
      QFrame* divider = new QFrame( details );
      divider->setFixedHeight( 1 );
      divider->setStyleSheet( QStringLiteral( "background:#EDF0F4;border:0;" ) );
      detailsLayout->addWidget( divider );
    }
  };
  m_cardClock = new QLabel;
  m_cardClock->setObjectName( QStringLiteral( "profileCardClock" ) );
  addRow( "profileClockRow", "clock", m_cardClock, Q_NULLPTR, true );
  m_cardInfo = new QLabel; m_cardInfo->setObjectName( "profileCardInfo" ); m_cardInfo->setTextInteractionFlags( Qt::TextSelectableByMouse );
  QToolButton* editInfo = profileAction( details, "profileEditInfo", "pencil-simple", QStringLiteral( "编辑个性签名" ) );
  addRow( "profileInfoRow", "chats-circle", m_cardInfo, editInfo, true );
  m_cardEmail = new QLabel; m_cardEmail->setObjectName( "profileCardEmail" ); m_cardEmail->setTextInteractionFlags( Qt::TextSelectableByMouse );
  QToolButton* copyEmail = profileAction( details, "profileCopyEmail", "copy", QStringLiteral( "复制邮箱" ) );
  addRow( "profileEmailRow", "envelope-simple", m_cardEmail, copyEmail, false );
  m_cardPhone = new QLabel; m_cardPhone->setObjectName( "profileCardPhone" );
  QToolButton* editPhone = profileAction( details, "profileEditPhone", "pencil-simple", QStringLiteral( "编辑电话号码" ) );
  addRow( "profilePhoneRow", "phone", m_cardPhone, editPhone, true );
  m_cardGroups = new QLabel; m_cardGroups->setObjectName( "profileCardGroups" );
  addRow( "profileGroupsRow", "users-three", m_cardGroups, Q_NULLPTR, false );
  cardLayout->addWidget( details );
  QScrollArea* cardScroll = new QScrollArea( cardPage );
  cardScroll->setObjectName( QStringLiteral( "profileCardScroll" ) );
  cardScroll->setFrameShape( QFrame::NoFrame );
  cardScroll->setWidgetResizable( true );
  cardScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  cardScroll->setWidget( card );
  cardPageLayout->addWidget( cardScroll, 1 );
  QWidget* cardFooter = new QWidget( cardPage );
  cardFooter->setObjectName( QStringLiteral( "ShadowProfileCardFooter" ) );
  QPushButton* close = new QPushButton( QStringLiteral( "完成" ) ); close->setObjectName( "closeMyProfile" ); close->setAutoDefault( false );
  QHBoxLayout* footer = new QHBoxLayout( cardFooter );
  footer->setContentsMargins( 16, 4, 16, 14 );
  footer->addStretch(); footer->addWidget( close );
  cardPageLayout->addWidget( cardFooter );
  QWidget* editorPage = new QWidget( this );
  editorPage->setObjectName( QStringLiteral( "ShadowProfileEditorPage" ) );
  QVBoxLayout* editorPageLayout = new QVBoxLayout( editorPage );
  editorPageLayout->setContentsMargins( 0, 0, 0, 0 );
  editorPageLayout->setSpacing( 0 );
  QScrollArea* editorScroll = new QScrollArea( editorPage ); editorScroll->setObjectName( "profileEditorScroll" );
  editorScroll->setWidget( editor ); editorScroll->setWidgetResizable( true ); editorScroll->setFrameShape( QFrame::NoFrame );
  editorScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  editorPageLayout->addWidget( editorScroll, 1 );
  QWidget* editorFooter = new QWidget( editorPage );
  editorFooter->setObjectName( QStringLiteral( "ShadowProfileEditorFooter" ) );
  QHBoxLayout* editorButtons = new QHBoxLayout( editorFooter );
  editorButtons->setContentsMargins( 28, 12, 28, 18 );
  editorButtons->setSpacing( 8 );
  editorButtons->addStretch();
  editorButtons->addWidget( mp_pbCancel );
  editorButtons->addWidget( mp_pbOk );
  editorPageLayout->addWidget( editorFooter );
  m_profilePages->addWidget( cardPage ); m_profilePages->addWidget( editorPage );
  QVBoxLayout* outer = new QVBoxLayout( this ); outer->setContentsMargins( 0, 0, 0, 0 ); outer->addWidget( m_profilePages );
  connect( edit, &QPushButton::clicked, this, [this]() { showProfileEditor( mp_leNickname ); } );
  connect( editInfo, &QToolButton::clicked, this, [this]() { showProfileEditor( mp_teInfo ); } );
  connect( editPhone, &QToolButton::clicked, this, [this]() { showProfileEditor( mp_lePhone ); } );
  connect( copyEmail, &QToolButton::clicked, this, [this, copyEmail]() {
    QApplication::clipboard()->setText( m_cardEmail->text() );
    copyEmail->setToolTip( QStringLiteral( "邮箱已复制" ) );
    copyEmail->setAccessibleDescription( QStringLiteral( "邮箱已复制到剪贴板" ) );
  } );
  connect( close, &QPushButton::clicked, this, &QDialog::reject );
  QTimer* clock = new QTimer( this );
  clock->setInterval( 60000 );
  connect( clock, &QTimer::timeout, this, &GuiEditVCard::updateCardClock );
  clock->start();
  updateCardClock();
  setStyleSheet( styleSheet() + QStringLiteral(
    "QWidget{font-family:'Microsoft YaHei UI';font-size:13px;}"
    "QLabel[role=title]{font-size:22px;} QLabel[role=hint]{font-size:12px;}"
    "QLabel#profileCardNickname{font-size:20px;color:#17243B;}"
    "QLabel[empty=true]{color:#94A3B8;}"
    "QPushButton#editMyProfile{background:transparent;border:0;padding:0;min-height:0;}"
    "QPushButton#editMyProfile:hover{background:#D7E6FC;}"
    "QPushButton#editMyProfile:focus{border:1px solid #2463EB;}"
    "QPushButton#profileCardStatus{background:#DAE8FC;border:1px solid transparent;border-radius:5px;padding:3px 10px;min-height:22px;color:#476486;font-size:12px;}"
    "QPushButton#profileCardStatus:hover{background:#CEDFF8;}"
    "QPushButton#profileCardStatus:focus{border-color:#2463EB;}"
    "QPushButton#profileCardStatus::menu-indicator{image:none;}"
    "QToolButton#profileEditInfo,QToolButton#profileEditPhone,QToolButton#profileCopyEmail{padding:0;}"
    "QScrollArea#profileEditorScroll,QScrollArea#profileCardScroll{background:#FFFFFF;border:0;}"
    "QWidget#qt_scrollarea_viewport{background:#FFFFFF;}"
    "QWidget#ShadowProfileEditorFooter,QWidget#ShadowProfileCardFooter{background:#FFFFFF;}" ) );
}

void GuiEditVCard::showProfileEditor( QWidget* target )
{
  m_profilePages->setCurrentIndex( 1 );
  resizeProfilePage();
  target->setFocus( Qt::OtherFocusReason );
  QTimer::singleShot( 0, this, [this, target]() {
    resizeProfilePage();
    QScrollArea* scroll = m_profilePages->currentWidget()->findChild<QScrollArea*>();
    if( scroll ) scroll->ensureWidgetVisible( target );
  } );
}

void GuiEditVCard::setProfileStatus( int status )
{
  m_cardStatus->setText( Bee::userStatusToString( status ) );
  for( QAction* action : m_cardStatus->menu()->actions() )
    action->setChecked( action->data().toInt() == status );
}

void GuiEditVCard::updateCardClock()
{
  const QDateTime now = QDateTime::currentDateTime();
  const int minutes = qAbs( now.offsetFromUtc() ) / 60;
  m_cardClock->setText( QStringLiteral( "%1\nUTC%2%3:%4 · 本地时间" )
    .arg( now.toString( QStringLiteral( "MM/dd HH:mm" ) ) )
    .arg( now.offsetFromUtc() < 0 ? "-" : "+" )
    .arg( minutes / 60, 2, 10, QLatin1Char( '0' ) )
    .arg( minutes % 60, 2, 10, QLatin1Char( '0' ) ) );
}

void GuiEditVCard::resizeProfilePage()
{
  if( !m_profilePages || m_resizingProfile ) return;
  QScopedValueRollback<bool> resizing( m_resizingProfile, true );
  ensurePolished();
  QWidget* page = m_profilePages->currentWidget();
  page->ensurePolished();
  QScrollArea* scroll = page->findChild<QScrollArea*>();
  if( !scroll || !scroll->widget() ) return;
  QWidget* content = scroll->widget();
  content->ensurePolished();
  content->layout()->invalidate();
  content->layout()->activate();
  const QScreen* display = parentWidget() ? parentWidget()->screen() : screen();
  const QRect available = display ? display->availableGeometry() : QRect( 0, 0, 1280, 800 );
  const QSize bounds = available.size() - QSize( 32, 32 );
  const QMargins margins = layout() ? layout()->contentsMargins() : QMargins();
  const bool hasSurface = findChild<QWidget*>( QStringLiteral( "shadowDialogSurface" ) );
  const int frameWidth = margins.left() + margins.right() + (hasSurface ? 2 : 0);
  const int frameHeight = margins.top() + margins.bottom() + (hasSurface ? 2 : 0);
  const bool cardVisible = m_profilePages->currentIndex() == 0;
  const int targetWidth = qMin( (cardVisible ? 360 : 620) + frameWidth, bounds.width() );
  const int contentWidth = qMax( 1, targetWidth - frameWidth - 20 );
  const int contentHeight = content->layout()->hasHeightForWidth()
    ? content->layout()->heightForWidth( contentWidth ) : content->layout()->sizeHint().height();
  QWidget* footer = page->findChild<QWidget*>( m_profilePages->currentIndex() == 0
    ? QStringLiteral( "ShadowProfileCardFooter" ) : QStringLiteral( "ShadowProfileEditorFooter" ) );
  const int footerHeight = footer ? footer->sizeHint().height() : 68;
  QWidget* header = findChild<QWidget*>( QStringLiteral( "shadowDialogHeader" ) );
  if( header )
    header->setStyleSheet( cardVisible ? QStringLiteral( "QWidget#shadowDialogHeader{background:#EAF2FF;border:0;}" )
      : QStringLiteral( "QWidget#shadowDialogHeader{background:#FFFFFF;border:0;border-bottom:1px solid #E8EDF2;}" ) );
  const int chromeHeight = header ? header->height() : 0;
  setMinimumSize( qMin( (cardVisible ? 320 : 560) + frameWidth, bounds.width() ), qMin( 320 + frameHeight, bounds.height() ) );
  setMaximumSize( bounds );
  resize( targetWidth, qMin( bounds.height(), qMax( 360, contentHeight + footerHeight + chromeHeight + frameHeight + 8 ) ) );
  if( layout() ) { layout()->invalidate(); layout()->activate(); }
  page->layout()->activate();
  scroll->viewport()->update();
  if( isVisible() )
  {
    const int left = available.left() + 16;
    const int top = available.top() + 16;
    move( qBound( left, x(), qMax( left, available.right() - width() - 15 ) ),
          qBound( top, y(), qMax( top, available.bottom() - height() - 15 ) ) );
  }
}

void GuiEditVCard::showEvent( QShowEvent* event )
{
  QDialog::showEvent( event );
  resizeProfilePage();
  QTimer::singleShot( 0, this, [this]() { resizeProfilePage(); } );
}

QString GuiEditVCard::currentAvatarName() const
{
  const QString current_avatar_name = mp_leNickname->text().simplified();
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
  m_cardEmail->setText( m_accountEmail.isEmpty() ? m_vCard.email() : m_accountEmail );
  m_cardInfo->setText( m_vCard.info().isEmpty() ? QStringLiteral( "输入个性签名" ) : m_vCard.info() );
  m_cardInfo->setProperty( "empty", m_vCard.info().isEmpty() );
  m_cardPhone->setText( m_vCard.phoneNumber().isEmpty() ? QStringLiteral( "添加电话号码" ) : m_vCard.phoneNumber() );
  m_cardPhone->setProperty( "empty", m_vCard.phoneNumber().isEmpty() );
  m_cardGroups->setText( u.workgroups().isEmpty() ? QStringLiteral( "未设置工作组" ) : u.workgroups().join( QStringLiteral( " · " ) ) );
  m_cardGroups->setProperty( "empty", u.workgroups().isEmpty() );
  m_cardGroups->setToolTip( m_cardGroups->text() );
  findChild<QToolButton*>( QStringLiteral( "profileCopyEmail" ) )->setEnabled( !m_cardEmail->text().isEmpty() );
  setProfileStatus( u.status() );
  m_cardStatus->setToolTip( u.statusDescription().isEmpty() ? QStringLiteral( "切换工作状态" ) : u.statusDescription() );
  QPixmap source = m_vCard.photo().isNull() ? Avatar::create( currentAvatarName(), m_userColor, QSize( 64, 64 ) ) : m_vCard.photo();
  QPixmap circle( 64, 64 ); circle.fill( Qt::transparent );
  QPainter painter( &circle ); painter.setRenderHint( QPainter::Antialiasing );
  QPainterPath clip; clip.addEllipse( 0, 0, 64, 64 ); painter.setClipPath( clip );
  QPixmap scaled = source.scaled( 64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
  painter.drawPixmap( (64 - scaled.width()) / 2, (64 - scaled.height()) / 2, scaled ); painter.end();
  m_cardPhoto->setPixmap( circle );
  foreach( QLabel* label, QList<QLabel*>() << m_cardNickname << m_cardEmail << m_cardInfo << m_cardPhone )
    label->setTextFormat( Qt::PlainText );
  m_profilePages->setCurrentIndex( 0 );
  mp_pbAdvanced->setChecked( true );
  mp_advanced->show();
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
  if( mp_deBirthday->date() != QDate( 1900, 1, 1 ) )
    m_vCard.setBirthday( mp_deBirthday->date() );
  else
    m_vCard.setBirthday( QDate() );
  if( m_accountEmail.isEmpty() ) m_vCard.setEmail( mp_leEmail->text().simplified() );
  m_vCard.setPhoneNumber( mp_lePhone->text().simplified() );
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
