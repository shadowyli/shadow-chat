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
// $Id: GuiVCard.cpp 1529 2021-12-09 11:30:57Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "Avatar.h"
#include "BeeUtils.h"
#include "ChatManager.h"
#include "GuiVCard.h"
#include "ShadowAvatarInteractions.h"
#include "IconManager.h"
#include "PluginManager.h"
#include "Settings.h"
#include "ShadowDialogStyle.h"
#include "ShadowStyle.h"
#include "UserManager.h"


GuiVCard::GuiVCard( QWidget *parent )
 : QFrame( parent ), m_userId( ID_INVALID ), m_chatId( ID_INVALID )
{
  setupUi( this );
  setObjectName( "GuiVCard" );
  setWindowFlags( Qt::Popup | Qt::FramelessWindowHint );
  setAttribute( Qt::WA_DeleteOnClose, true );
  setFocusPolicy( Qt::StrongFocus );
  setFrameShape( QFrame::NoFrame );
  setMinimumSize( 0, 0 );
  setMaximumWidth( 420 );
  delete layout();
  QVBoxLayout* card = new QVBoxLayout( this );
  card->setContentsMargins( 20, 16, 20, 20 );
  card->setSpacing( 12 );
  QHBoxLayout* heading = new QHBoxLayout;
  QLabel* title = new QLabel( QStringLiteral( "公开资料" ), this );
  title->setProperty( "role", "hint" );
  heading->addWidget( title );
  heading->addStretch();
  QToolButton* closeButton = new QToolButton( this );
  closeButton->setObjectName( QStringLiteral( "closePublicProfile" ) );
  closeButton->setText( QStringLiteral( "×" ) );
  closeButton->setFixedSize( 32, 32 );
  closeButton->setToolTip( QStringLiteral( "关闭资料卡（Esc）" ) );
  closeButton->setAccessibleName( QStringLiteral( "关闭资料卡" ) );
  connect( closeButton, &QToolButton::clicked, this, &QWidget::close );
  heading->addWidget( closeButton );
  card->addLayout( heading );

  mp_publicDetails = new QScrollArea( this );
  mp_publicDetails->setObjectName( QStringLiteral( "publicProfileDetails" ) );
  mp_publicDetails->setFrameShape( QFrame::NoFrame );
  mp_publicDetails->setWidgetResizable( true );
  mp_publicDetails->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  QWidget* details = new QWidget;
  details->setObjectName( QStringLiteral( "publicProfileBody" ) );
  details->setStyleSheet( QStringLiteral( "QWidget#publicProfileBody{background:#ffffff;}" ) );
  QVBoxLayout* identity = new QVBoxLayout( details );
  identity->setContentsMargins( 0, 0, 8, 0 );
  identity->setSpacing( 12 );
  mp_lPhoto->setFixedSize( 64, 64 );
  identity->addWidget( mp_lPhoto, 0, Qt::AlignLeft );
  mp_lName->setProperty( "role", "title" );
  mp_lName->setStyleSheet( QStringLiteral( "font-size:22px;font-weight:600;color:#14213a;background:transparent;" ) );
  mp_fullName = new QLabel( details );
  mp_fullName->setObjectName( QStringLiteral( "publicProfileFullName" ) );
  mp_fullName->setProperty( "role", "hint" );
  foreach( QLabel* detail, QList<QLabel*>() << mp_lName << mp_fullName << mp_lStatusDescription <<
           mp_lEmail << mp_lPhone << mp_lRoom << mp_lBirthday << mp_lWorkgroups )
  {
    detail->setMinimumSize( 0, 0 );
    detail->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
    detail->setTextFormat( Qt::PlainText );
    detail->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard );
    detail->setWordWrap( true );
    detail->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
  }
  mp_lStatusDescription->setProperty( "role", "hint" );
  identity->addWidget( mp_lName );
  identity->addWidget( mp_fullName );
  identity->addWidget( mp_lStatusDescription );
  mp_teInfo->setReadOnly( true );
  mp_teInfo->setMinimumSize( 0, 48 );
  mp_teInfo->setMaximumHeight( 100 );
  mp_teInfo->setAccessibleName( QStringLiteral( "个人介绍" ) );
  mp_teInfo->setStyleSheet( QStringLiteral( "QPlainTextEdit{background:transparent;border:0;padding:0;color:#697b94;}" ) );
  identity->addWidget( mp_teInfo );
  foreach( QLabel* detail, QList<QLabel*>() << mp_lEmail << mp_lPhone << mp_lRoom << mp_lBirthday << mp_lWorkgroups )
    identity->addWidget( detail );
  identity->addStretch();
  mp_publicDetails->setWidget( details );
  card->addWidget( mp_publicDetails, 1 );

  foreach( QPushButton* action, QList<QPushButton*>() << mp_pbChat << mp_pbFile << mp_pbFavorite << mp_pbBuzz << mp_pbColor << mp_pbRemove << mp_pbHelp )
    action->hide();
  mp_pbChat->setMinimumSize( 0, 36 );
  mp_pbChat->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
  mp_pbChat->setIcon( QIcon() );
  card->addWidget( mp_pbChat );
  mp_frame->hide();
  mp_frameBottom->hide();
  mp_lPath->clear();
  mp_lPath->hide();
  mp_lStatus->clear();
  mp_lStatus->hide();
  mp_pbChat->setText( QStringLiteral( "发消息" ) );
  mp_pbChat->setProperty( "role", "primary" );
  ShadowDialogStyle::apply( this );
  setStyleSheet( styleSheet() + QStringLiteral(
    "QFrame#GuiVCard{background:#ffffff;border:1px solid #dce4f0;border-radius:12px;}"
    "QScrollArea#publicProfileDetails,QScrollArea#publicProfileDetails>QWidget>QWidget{background:#ffffff;}"
    "QPlainTextEdit#mp_teInfo{background:transparent;border:0;padding:0;color:#697b94;}" ) );
  qApp->installEventFilter( this );
  connect( mp_pbChat, SIGNAL( clicked() ), this, SLOT( showPrivateChat() ) );
}

void GuiVCard::setVCard( const User& u, VNumber chat_id, bool core_is_connected )
{
  m_userId = u.id();
  m_chatId = chat_id;
  m_userColor = u.color();
  setProperty( "profileUserId", QVariant::fromValue<qulonglong>( m_userId ) );
  mp_lName->setText( u.name() );
  setWindowTitle( QStringLiteral( "%1的公开资料" ).arg( u.name() ) );
  const auto setDetail = []( QLabel* label, const QString& title, const QString& value ) {
    label->setText( value.trimmed().isEmpty() ? QString() : title + value );
    label->setVisible( !value.trimmed().isEmpty() );
    label->setToolTip( QString() );
  };
  const QString fullName = u.vCard().fullName( Settings::instance().useUserFirstNameFirstInFullName() );
  setDetail( mp_fullName, QStringLiteral( "姓名　" ), fullName == u.name() ? QString() : fullName );
  setDetail( mp_lStatusDescription, QString(), u.statusDescription() );
  setDetail( mp_lEmail, QStringLiteral( "邮箱　" ), u.vCard().email() );
  setDetail( mp_lPhone, QStringLiteral( "电话　" ), u.vCard().phoneNumber() );
  setDetail( mp_lRoom, QStringLiteral( "位置　" ), u.vCard().roomLocation() );
  setDetail( mp_lBirthday, QStringLiteral( "生日　" ), u.vCard().birthday().isValid() ?
    u.vCard().birthday().toString( u.vCard().birthday().year() == 1900 ? QStringLiteral( "MM-dd" ) : QStringLiteral( "yyyy-MM-dd" ) ) : QString() );
  setDetail( mp_lWorkgroups, QStringLiteral( "工作组　" ), u.workgroups().join( QStringLiteral( " / " ) ) );
  mp_teInfo->setPlainText( u.vCard().info() );
  mp_teInfo->setVisible( !u.vCard().info().trimmed().isEmpty() );
  const QPixmap original = u.vCard().photo().isNull() ? Bee::avatarForUser( u, QSize( 64, 64 ), true ) : u.vCard().photo();
  mp_lPhoto->setPixmap( ShadowStyle::avatarPixmap( QIcon( original ), QSize( 64, 64 ) ) );
  mp_lPhoto->setToolTip( u.name() );
  mp_pbChat->setVisible( !u.isLocal() && chat_id != ID_INVALID );
  Q_UNUSED( core_is_connected );
}

void GuiVCard::showAt( const QPoint& anchor )
{
  const QRect available = qApp->desktop()->availableGeometry( anchor ).adjusted( 8, 8, -8, -8 );
  const int width = qMin( 380, available.width() );
  setMaximumWidth( width );
  resize( width, qMin( 560, available.height() ) );
  ensurePolished();
  layout()->activate();
  mp_publicDetails->widget()->layout()->activate();
  const int contentHeight = mp_publicDetails->widget()->layout()->totalHeightForWidth(
    qMax( 1, width - 60 ) );
  const int desiredHeight = qMax( 240, contentHeight + (mp_pbChat->isHidden() ? 92 : 140) );
  setGeometry( ShadowAvatarInteractions::popupGeometry( anchor,
    QSize( width, qMin( qMin( desiredHeight, 600 ), available.height() ) ), available ) );
  show();
  setFocus( Qt::PopupFocusReason );
}

void GuiVCard::keyPressEvent( QKeyEvent* event )
{
  if( event->key() == Qt::Key_Escape )
  {
    close();
    event->accept();
    return;
  }
  QFrame::keyPressEvent( event );
}

bool GuiVCard::eventFilter( QObject* watched, QEvent* event )
{
  if( isVisible() && event->type() == QEvent::MouseButtonPress &&
      !frameGeometry().contains( static_cast<QMouseEvent*>( event )->globalPos() ) )
    close();
  return QFrame::eventFilter( watched, event );
}

void GuiVCard::showPrivateChat()
{
  hide();
  emit showChat( m_chatId );
  close();
}

void GuiVCard::sendFile()
{
  hide();
  emit sendFile( m_userId );
  close();
}

void GuiVCard::changeColor()
{
  hide();
  emit changeUserColor( m_userId, m_userColor );
  close();
}

void GuiVCard::favoriteClicked()
{
  hide();
  emit toggleFavorite( m_userId );
  close();
}

void GuiVCard::removeUserClicked()
{
  hide();
  emit removeUser( m_userId );
  close();
}

void GuiVCard::sendBuzz()
{
  hide();
  emit showChat( m_chatId );
  emit buzzUser( m_userId );
  close();
}

void GuiVCard::sendHelp()
{
  hide();
  emit sendHelpRequestToUser( m_userId );
  close();
}
