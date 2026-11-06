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
#include "ShadowColorPanel.h"
#include "ShadowWorkgroups.h"
#include <QActionGroup>
#include <QClipboard>
#include <QMenu>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>

namespace
{
class ProfileInlineStack : public QStackedWidget
{
public:
  explicit ProfileInlineStack( QWidget* parent = Q_NULLPTR ) : QStackedWidget( parent ) {}
  QSize sizeHint() const override
  {
    return currentWidget() ? currentWidget()->sizeHint().expandedTo( currentWidget()->minimumSize() )
      .boundedTo( currentWidget()->maximumSize() ) : QSize();
  }
  QSize minimumSizeHint() const override
  {
    return currentWidget() ? currentWidget()->minimumSizeHint().expandedTo( currentWidget()->minimumSize() )
      .boundedTo( currentWidget()->maximumSize() ) : QSize();
  }
};

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
  ShadowPopup::installMenuTracking();
  qApp->installEventFilter( this );
  setProperty( "shadowDialogTitleHidden", true );
  setWindowTitle( QStringLiteral( "我的资料 · 影聊" ) );
  setWindowIcon( IconManager::instance().icon( "profile-edit.png" ) );
  Bee::removeContextHelpButton( this );

  mp_lTitle->setProperty( "role", "title" );
  mp_lIdentity->setProperty( "role", "hint" );
  mp_pbOk->setProperty( "role", "primary" );
  ShadowDialogStyle::apply( this );
  setMinimumSize( 360, 320 );

  foreach( QLabel* label, QList<QLabel*>() << mp_lNickname << mp_lEmail << mp_lInfo << mp_lBirthday
                                          << mp_lPhone << mp_lColor )
  {
    label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
    label->setMinimumWidth( 64 );
    label->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
  }
  formLayout->setColumnMinimumWidth( 0, 64 );
  formLayout->setColumnStretch( 1, 1 );
  formLayout->setAlignment( mp_lInfo, Qt::AlignRight | Qt::AlignTop );
  mp_pbChangePhoto->setFixedWidth( 88 );
  mp_leEmail->setReadOnly( true );
  mp_leEmail->setFocusPolicy( Qt::StrongFocus );
  mp_leEmail->setToolTip( QStringLiteral( "登录账号只读，可选中复制；切换账号请使用主菜单的退出登录。" ) );

  connect( mp_pbOk, SIGNAL( clicked() ), this, SLOT( onOkClicked() ) );
  connect( mp_pbCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
  connect( mp_pbChangePhoto, SIGNAL( clicked() ), this, SLOT( changePhoto() ) );
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
  m_cardPhoto = new QToolButton( header );
  m_cardPhoto->setObjectName( "profileCardAvatar" );
  m_cardPhoto->setFixedSize( 64, 64 );
  m_cardPhoto->setIconSize( QSize( 60, 60 ) );
  m_cardPhoto->setCursor( Qt::PointingHandCursor );
  m_cardPhoto->setAccessibleName( QStringLiteral( "更换头像" ) );
  m_cardPhoto->setToolTip( QStringLiteral( "点击更换头像" ) );
  avatarRow->addWidget( m_cardPhoto );
  avatarRow->addStretch();
  m_cardStatus = new QPushButton;
  m_cardStatus->setObjectName( QStringLiteral( "profileCardStatus" ) );
  m_cardStatus->setAutoDefault( false );
  m_cardStatus->setAccessibleName( QStringLiteral( "切换工作状态" ) );
  QMenu* statusMenu = new QMenu( m_cardStatus );
  QActionGroup* statuses = new QActionGroup( statusMenu );
  for( int status : QList<int>{ User::Online, User::Busy, User::Away } )
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
  auto makeInlineField = [this]( QLabel* value, QWidget* input, const QString& name, QLabel** errorLabel ) {
    QWidget* field = new QWidget;
    field->setObjectName( name + QStringLiteral( "Field" ) );
    QVBoxLayout* fieldLayout = new QVBoxLayout( field );
    fieldLayout->setContentsMargins( 0, 0, 0, 0 );
    fieldLayout->setSpacing( 3 );
    ProfileInlineStack* stack = new ProfileInlineStack( field );
    stack->setObjectName( name + QStringLiteral( "Stack" ) );
    stack->setMinimumWidth( 0 );
    stack->layout()->setSizeConstraint( QLayout::SetNoConstraint );
    stack->addWidget( value );
    stack->addWidget( input );
    input->setProperty( "profileInline", true );
    input->setMinimumWidth( 0 );
    input->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    input->installEventFilter( this );
    fieldLayout->addWidget( stack );
    if( errorLabel )
    {
      *errorLabel = new QLabel( field );
      (*errorLabel)->setObjectName( name + QStringLiteral( "Error" ) );
      (*errorLabel)->setWordWrap( true );
      (*errorLabel)->setStyleSheet( QStringLiteral( "color:#C33C32;font-size:12px;" ) );
      (*errorLabel)->hide();
      fieldLayout->addWidget( *errorLabel );
    }
    input->setProperty( "profileInlineStack", QVariant::fromValue<QObject*>( stack ) );
    connect( stack, &QStackedWidget::currentChanged, this, [this, stack]() {
      if( stack->objectName() == QStringLiteral( "profileNicknameStack" ) )
        stack->parentWidget()->setMaximumWidth( stack->currentIndex() == 0 ?
          qMin( 260, m_cardNickname->fontMetrics().horizontalAdvance( m_cardNickname->text() ) + 4 ) : 220 );
      stack->updateGeometry();
      QTimer::singleShot( 0, this, [this]() { resizeProfilePage(); } );
    } );
    return field;
  };
  mp_leNickname->setAccessibleName( QStringLiteral( "昵称" ) );
  QWidget* nicknameField = makeInlineField( m_cardNickname, mp_leNickname, "profileNickname", &m_nicknameError );
  identity->addWidget( nicknameField, 0, Qt::AlignVCenter );
  QPushButton* edit = new QPushButton;
  edit->setObjectName( "editMyProfile" ); edit->setAutoDefault( false );
  edit->setIcon( ShadowStyle::actionIcon( QStringLiteral( "pencil-simple" ) ) );
  edit->setIconSize( QSize( 16, 16 ) ); edit->setFixedSize( 30, 30 );
  edit->setAccessibleName( QStringLiteral( "编辑昵称" ) ); edit->setToolTip( QStringLiteral( "编辑昵称" ) );
  identity->addWidget( edit, 0, Qt::AlignVCenter );
  identity->addStretch();
  headerLayout->addLayout( identity );
  cardLayout->addWidget( header );
  QWidget* details = new QWidget;
  details->setObjectName( QStringLiteral( "profileCardDetails" ) );
  QVBoxLayout* detailsLayout = new QVBoxLayout( details );
  detailsLayout->setContentsMargins( 16, 8, 12, 10 ); detailsLayout->setSpacing( 0 );
  auto addRow = [&]( const QString& name, const QString& icon, QLabel* value, QWidget* input, QToolButton* action, bool separator, QLabel** errorLabel = Q_NULLPTR ) {
    QWidget* row = new QWidget( details );
    row->setObjectName( name );
    QHBoxLayout* line = new QHBoxLayout( row );
    line->setContentsMargins( 0, 9, 0, 9 ); line->setSpacing( 10 );
    QLabel* symbol = new QLabel( row );
    symbol->setFixedSize( 18, 18 );
    symbol->setPixmap( ShadowStyle::tintedPixmap( QStringLiteral( ":/images/shadowchat/icons/%1.svg" ).arg( icon ),
      QSize( 18, 18 ), QColor( "#8A96A8" ) ) );
    line->addWidget( symbol, 0, Qt::AlignVCenter );
    value->setMinimumWidth( 0 );
    value->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    value->setWordWrap( true );
    value->setTextFormat( Qt::PlainText );
    line->addWidget( input ? makeInlineField( value, input, name, errorLabel ) : value, 1 );
    if( action ) line->addWidget( action, 0, Qt::AlignVCenter );
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
  addRow( "profileClockRow", "clock", m_cardClock, Q_NULLPTR, Q_NULLPTR, true );
  m_cardInfo = new QLabel; m_cardInfo->setObjectName( "profileCardInfo" ); m_cardInfo->setTextInteractionFlags( Qt::TextSelectableByMouse );
  QToolButton* editInfo = profileAction( details, "profileEditInfo", "pencil-simple", QStringLiteral( "编辑个性签名" ) );
  mp_teInfo->setAccessibleName( QStringLiteral( "个性签名" ) );
  mp_teInfo->setMinimumHeight( 64 );
  mp_teInfo->setMaximumHeight( 100 );
  addRow( "profileInfoRow", "chats-circle", m_cardInfo, mp_teInfo, editInfo, true );
  m_cardEmail = new QLabel; m_cardEmail->setObjectName( "profileCardEmail" ); m_cardEmail->setTextInteractionFlags( Qt::TextSelectableByMouse );
  QToolButton* copyEmail = profileAction( details, "profileCopyEmail", "copy", QStringLiteral( "复制邮箱" ) );
  addRow( "profileEmailRow", "envelope-simple", m_cardEmail, Q_NULLPTR, copyEmail, false );
  m_cardPhone = new QLabel; m_cardPhone->setObjectName( "profileCardPhone" );
  QToolButton* editPhone = profileAction( details, "profileEditPhone", "pencil-simple", QStringLiteral( "编辑电话号码" ) );
  mp_lePhone->setAccessibleName( QStringLiteral( "电话号码" ) );
  addRow( "profilePhoneRow", "phone", m_cardPhone, mp_lePhone, editPhone, true );
  m_cardGroups = new QLabel; m_cardGroups->setObjectName( "profileCardGroups" );
  m_workgroupsEdit = new QLineEdit;
  m_workgroupsEdit->setObjectName( "profileWorkgroupsEdit" );
  m_workgroupsEdit->setAccessibleName( QStringLiteral( "工作组" ) );
  m_workgroupsEdit->setPlaceholderText( QStringLiteral( "例如：设计组、程序组" ) );
  m_workgroupsEdit->setToolTip( QStringLiteral( "多个工作组用逗号或顿号分隔，用于组织架构分类，不开启仅同组连接。" ) );
  QToolButton* editGroups = profileAction( details, "profileEditWorkgroups", "pencil-simple", QStringLiteral( "编辑工作组" ) );
  addRow( "profileGroupsRow", "users-three", m_cardGroups, m_workgroupsEdit, editGroups, false, &m_workgroupsError );
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
  m_cardSave = close;
  m_cardCancel = new QPushButton( QStringLiteral( "取消" ), cardFooter );
  m_cardCancel->setObjectName( "cancelInlineProfile" );
  m_cardCancel->setAutoDefault( false );
  m_cardCancel->hide();
  QHBoxLayout* footer = new QHBoxLayout( cardFooter );
  footer->setContentsMargins( 16, 4, 16, 14 );
  footer->addStretch(); footer->addWidget( m_cardCancel ); footer->addWidget( close );
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
  connect( editGroups, &QToolButton::clicked, this, &GuiEditVCard::editWorkgroupsInline );
  connect( m_cardPhoto, &QToolButton::clicked, this, &GuiEditVCard::changePhoto );
  connect( mp_leNickname, &QLineEdit::returnPressed, this, [this]() { finishInlineEdit( mp_leNickname ); } );
  connect( mp_lePhone, &QLineEdit::returnPressed, this, [this]() { finishInlineEdit( mp_lePhone ); } );
  connect( m_workgroupsEdit, &QLineEdit::returnPressed, this, [this]() { finishInlineEdit( m_workgroupsEdit ); } );
  connect( copyEmail, &QToolButton::clicked, this, [this, copyEmail]() {
    QApplication::clipboard()->setText( m_cardEmail->text() );
    copyEmail->setToolTip( QStringLiteral( "邮箱已复制" ) );
    copyEmail->setAccessibleDescription( QStringLiteral( "邮箱已复制到剪贴板" ) );
  } );
  connect( close, &QPushButton::clicked, this, [this]() {
    if( m_inlineDirty ) onOkClicked(); else reject();
  } );
  connect( m_cardCancel, &QPushButton::clicked, this, &QDialog::reject );
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
    "QLineEdit,QPlainTextEdit,QDateEdit{min-height:20px;}"
    "QDateEdit QLineEdit{background:transparent;border:0;padding:0;}"
    "QDateEdit[empty=true] QLineEdit{color:#94A3B8;}"
    "QLineEdit#mp_leEmail{color:#52647f;}"
    "QPushButton#mp_pbChangePhoto,QPushButton#mp_pbColor{min-height:24px;}"
    "QPushButton#editMyProfile{background:transparent;border:0;padding:0;min-height:0;}"
    "QPushButton#editMyProfile:hover{background:#D7E6FC;}"
    "QPushButton#editMyProfile:focus{border:1px solid #2463EB;}"
    "QPushButton#profileCardStatus{background:#DAE8FC;border:1px solid transparent;border-radius:5px;padding:3px 10px;min-height:22px;color:#476486;font-size:12px;}"
    "QPushButton#profileCardStatus:hover{background:#CEDFF8;}"
    "QPushButton#profileCardStatus:focus{border-color:#2463EB;}"
    "QPushButton#profileCardStatus::menu-indicator{image:none;}"
    "QToolButton#profileCardAvatar{background:transparent;border:1px solid transparent;border-radius:32px;padding:0;}"
    "QToolButton#profileCardAvatar:hover{background:#D7E6FC;}"
    "QToolButton#profileCardAvatar:focus{border-color:#2463EB;}"
    "QToolButton#profileEditInfo,QToolButton#profileEditPhone,QToolButton#profileEditWorkgroups,QToolButton#profileCopyEmail{padding:0;}"
    "QLineEdit[profileInline=true]{background:#FFFFFF;padding:5px 8px;min-height:18px;border:1px solid #CFDAE8;border-radius:6px;}"
    "QPlainTextEdit[profileInline=true]{background:#FFFFFF;padding:5px 8px;border:1px solid #CFDAE8;border-radius:6px;}"
    "QLineEdit[profileInline=true]:focus,QPlainTextEdit[profileInline=true]:focus{border-color:#2463EB;}"
    "QScrollArea#profileEditorScroll,QScrollArea#profileCardScroll{background:#FFFFFF;border:0;}"
    "QWidget#qt_scrollarea_viewport{background:#FFFFFF;}"
    "QWidget#ShadowProfileEditorFooter,QWidget#ShadowProfileCardFooter{background:#FFFFFF;}" ) );
}

void GuiEditVCard::showProfileEditor( QWidget* target )
{
  beginInlineEdit( target );
}

void GuiEditVCard::beginInlineEdit( QWidget* input )
{
  if( !input->isEnabled() ) return;
  QStackedWidget* stack = qobject_cast<QStackedWidget*>( input->property( "profileInlineStack" ).value<QObject*>() );
  if( !stack ) return;
  if( stack->currentIndex() == 1 ) { finishInlineEdit( input ); return; }
  m_inlineDirty = true;
  if( input == m_workgroupsEdit ) m_workgroupsEdited = true;
  m_cardSave->setText( QStringLiteral( "保存" ) );
  m_cardCancel->show();
  stack->setCurrentIndex( 1 );
  resizeProfilePage();
  input->setFocus( Qt::OtherFocusReason );
  if( QLineEdit* edit = qobject_cast<QLineEdit*>( input ) ) edit->selectAll();
  if( QScrollArea* scroll = findChild<QScrollArea*>( "profileCardScroll" ) ) scroll->ensureWidgetVisible( input );
}

void GuiEditVCard::finishInlineEdit( QWidget* input )
{
  if( !validateInlineData() ) return;
  QStackedWidget* stack = qobject_cast<QStackedWidget*>( input->property( "profileInlineStack" ).value<QObject*>() );
  if( stack ) stack->setCurrentIndex( 0 );
  updateInlineLabels();
  resizeProfilePage();
}

void GuiEditVCard::editWorkgroupsInline()
{
  beginInlineEdit( m_workgroupsEdit );
}

void GuiEditVCard::updateInlineLabels()
{
  m_cardNickname->setText( mp_leNickname->text().simplified() );
  m_cardNickname->ensurePolished();
  if( findChild<QStackedWidget*>( "profileNicknameStack" )->currentIndex() == 0 )
    findChild<QWidget*>( "profileNicknameField" )->setMaximumWidth(
      qMin( 260, m_cardNickname->fontMetrics().horizontalAdvance( m_cardNickname->text() ) + 4 ) );
  const QString info = mp_teInfo->toPlainText().trimmed();
  m_cardInfo->setText( info.isEmpty() ? QStringLiteral( "输入个性签名" ) : info );
  m_cardInfo->setProperty( "empty", info.isEmpty() );
  const QString phone = mp_lePhone->text().simplified();
  m_cardPhone->setText( phone.isEmpty() ? QStringLiteral( "添加电话号码" ) : phone );
  m_cardPhone->setProperty( "empty", phone.isEmpty() );
  m_cardGroups->setText( m_workgroups.isEmpty() ? QStringLiteral( "未设置工作组" ) : m_workgroups.join( QStringLiteral( " · " ) ) );
  m_cardGroups->setProperty( "empty", m_workgroups.isEmpty() );
  for( QLabel* label : QList<QLabel*>{ m_cardInfo, m_cardPhone, m_cardGroups } )
  {
    label->style()->unpolish( label );
    label->style()->polish( label );
  }
}

bool GuiEditVCard::validateInlineData()
{
  m_nicknameError->hide();
  m_workgroupsError->hide();
  const QString nickname = mp_leNickname->text().simplified();
  const User existing = UserManager::instance().findUserByNickname( nickname );
  if( nickname.isEmpty() || (existing.isValid() && !existing.isLocal()) )
  {
    m_nicknameError->setText( nickname.isEmpty() ? QStringLiteral( "请填写昵称。" ) : QStringLiteral( "此昵称已被联系人使用，请换一个。" ) );
    m_nicknameError->show();
    qobject_cast<QStackedWidget*>( mp_leNickname->property( "profileInlineStack" ).value<QObject*>() )->setCurrentIndex( 1 );
    mp_leNickname->setAccessibleDescription( m_nicknameError->text() );
    mp_leNickname->setFocus();
    resizeProfilePage();
    return false;
  }
  QStringList normalizedGroups;
  if( m_workgroupsEdited && !ShadowWorkgroups::fromText( m_workgroupsEdit->text(), &normalizedGroups ) )
  {
    m_workgroupsError->setText( QStringLiteral( "最多 32 个工作组，每个最多 128 个字，不接受控制字符。" ) );
    m_workgroupsError->show();
    qobject_cast<QStackedWidget*>( m_workgroupsEdit->property( "profileInlineStack" ).value<QObject*>() )->setCurrentIndex( 1 );
    m_workgroupsEdit->setAccessibleDescription( m_workgroupsError->text() );
    m_workgroupsEdit->setFocus();
    resizeProfilePage();
    return false;
  }
  if( m_workgroupsEdited ) m_workgroups = normalizedGroups;
  mp_leNickname->setAccessibleDescription( QString() );
  m_workgroupsEdit->setAccessibleDescription( QString() );
  return true;
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
    if( m_cardAnchored )
    {
      placeCard();
      return;
    }
    const int left = available.left() + 16;
    const int top = available.top() + 16;
    move( qBound( left, x(), qMax( left, available.right() - width() - 15 ) ),
          qBound( top, y(), qMax( top, available.bottom() - height() - 15 ) ) );
  }
}

void GuiEditVCard::showAt( QWidget* anchor )
{
  m_cardAnchor = anchor;
  m_cardAnchored = anchor != Q_NULLPTR;
  setModal( false );
  show();
  placeCard();
  raise();
  activateWindow();
}

void GuiEditVCard::placeCard()
{
  if( !m_cardAnchored || !m_cardAnchor || m_profilePages->currentIndex() != 0 )
    return;
  const QRect anchor( m_cardAnchor->mapToGlobal( QPoint() ), m_cardAnchor->size() );
  setGeometry( ShadowPopup::placeGeometry( anchor, size(),
    ShadowPopup::availableGeometryFor( anchor.center() ), ShadowPopup::Automatic, 6 ) );
}

bool GuiEditVCard::eventFilter( QObject* watched, QEvent* event )
{
  if( !isVisible() || !m_cardAnchored || !m_cardAnchor )
    return QDialog::eventFilter( watched, event );
  QWidget* widget = qobject_cast<QWidget*>( watched );
  if( widget && (widget == m_cardAnchor || widget->isAncestorOf( m_cardAnchor )) )
  {
    if( event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest )
      placeCard();
    if( event->type() == QEvent::Hide ||
        (event->type() == QEvent::WindowStateChange && widget->isMinimized()) )
      reject();
  }
  QWidget* active_popup = QApplication::activePopupWidget();
  if( event->type() == QEvent::MouseButtonPress && widget &&
      (!active_popup || !isAncestorOf( active_popup )) && !isAncestorOf( widget ) && widget != this &&
      !frameGeometry().contains( static_cast<QMouseEvent*>( event )->globalPos() ) )
  {
    reject();
    return widget == m_cardAnchor || m_cardAnchor->isAncestorOf( widget );
  }
  return QDialog::eventFilter( watched, event );
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
  mp_lColorValue->setText( c.name().toUpper() );
  if( m_photo.isNull() )
    mp_lPhoto->setPixmap( Avatar::create( currentAvatarName(), m_userColor, mp_lPhoto->size() ) );
}

void GuiEditVCard::setUser( const User& u )
{
  m_vCard = u.vCard();
  m_photo = m_vCard.photo();
  m_savedUserColor = u.color();
  m_accountEmail = u.accountName().trimmed();
  setUserColor( u.color() );
  m_regenerateUserHash = false;
  m_savedWorkgroups = u.workgroups();
  m_workgroups = m_savedWorkgroups;
  m_workgroupsEdit->setText( m_savedWorkgroups.join( QStringLiteral( ", " ) ) );
  m_workgroupsEdited = false;
  m_inlineDirty = false;
  m_cardSave->setText( QStringLiteral( "完成" ) );
  m_cardCancel->hide();
  m_nicknameError->hide();
  m_workgroupsError->hide();
  for( QWidget* input : QList<QWidget*>{ mp_leNickname, mp_teInfo, mp_lePhone, m_workgroupsEdit } )
    qobject_cast<QStackedWidget*>( input->property( "profileInlineStack" ).value<QObject*>() )->setCurrentIndex( 0 );
  loadVCard();
  findChild<QPushButton*>( "editMyProfile" )->setEnabled( Settings::instance().allowEditNickname() );
  mp_lIdentity->setText( QStringLiteral( "正在编辑自己的资料，同事会看到你的昵称和头像。" ) );
  mp_leEmail->setReadOnly( true );
  m_cardNickname->setText( m_vCard.nickName().isEmpty() ? u.name() : m_vCard.nickName() );
  m_cardNickname->ensurePolished();
  findChild<QWidget*>( "profileNicknameField" )->setMaximumWidth(
    qMin( 260, m_cardNickname->fontMetrics().horizontalAdvance( m_cardNickname->text() ) + 4 ) );
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
  refreshCardPhoto();
  foreach( QLabel* label, QList<QLabel*>() << m_cardNickname << m_cardEmail << m_cardInfo << m_cardPhone )
    label->setTextFormat( Qt::PlainText );
  m_profilePages->setCurrentIndex( 0 );
  resizeProfilePage();
}

void GuiEditVCard::refreshCardPhoto()
{
  QPixmap source = m_photo.isNull() ? Avatar::create( currentAvatarName(), m_userColor, QSize( 64, 64 ) ) : m_photo;
  QPixmap circle( 64, 64 ); circle.fill( Qt::transparent );
  QPainter painter( &circle ); painter.setRenderHint( QPainter::Antialiasing );
  QPainterPath clip; clip.addEllipse( 0, 0, 64, 64 ); painter.setClipPath( clip );
  QPixmap scaled = source.scaled( 64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
  painter.drawPixmap( (64 - scaled.width()) / 2, (64 - scaled.height()) / 2, scaled ); painter.end();
  m_cardPhoto->setIcon( QIcon( circle ) );
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
    mp_deBirthday->setBirthday( m_vCard.birthday() );
  else
    mp_deBirthday->setBirthday( QDate() );

  mp_leEmail->setReadOnly( true );
  mp_leEmail->setText( m_accountEmail.isEmpty() ? m_vCard.email() : m_accountEmail );

  if( m_photo.isNull() )
    mp_lPhoto->setPixmap( Avatar::create( currentAvatarName(), m_userColor, mp_lPhoto->size() ) );
  else
    mp_lPhoto->setPixmap( m_photo.scaled( mp_lPhoto->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );

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
    m_photo = pix;
    mp_lPhoto->setPixmap( pix.scaled( mp_lPhoto->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    m_inlineDirty = true;
    m_cardSave->setText( QStringLiteral( "保存" ) );
    m_cardCancel->show();
    refreshCardPhoto();
  }
}

void GuiEditVCard::reject()
{
  m_photo = m_vCard.photo();
  setUserColor( m_savedUserColor );
  m_regenerateUserHash = false;
  m_workgroups = m_savedWorkgroups;
  m_workgroupsEdit->setText( m_savedWorkgroups.join( QStringLiteral( ", " ) ) );
  m_workgroupsEdited = false;
  m_inlineDirty = false;
  loadVCard();
  updateInlineLabels();
  refreshCardPhoto();
  QDialog::reject();
}

void GuiEditVCard::changeUserColor()
{
  const QColor selected = ShadowColorPanel::pick( mp_pbColor, QColor( m_userColor ), this,
                                                 QStringLiteral( "昵称颜色" ) );
  if( selected.isValid() )
    setUserColor( selected.name() );
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
  if( m_profilePages->currentIndex() == 0 )
    return validateInlineData();
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
  if( mp_deBirthday->hasBirthday() )
    m_vCard.setBirthday( mp_deBirthday->birthday() );
  else
    m_vCard.setBirthday( QDate() );
  m_vCard.setPhoto( m_photo );
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
