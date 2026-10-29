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
#include "IconManager.h"
#include "PluginManager.h"
#include "Settings.h"
#include "ShadowDialogStyle.h"
#include "ShadowStyle.h"
#include "UserManager.h"


GuiVCard::GuiVCard( QWidget *parent )
 : QFrame( parent )
{
  setupUi( this );
  setObjectName( "GuiVCard" );

  setWindowFlags( Qt::Popup );
  setAttribute( Qt::WA_DeleteOnClose, true );
  setFrameShape( QFrame::NoFrame );
  setMinimumWidth( 480 );
  setMaximumWidth( 560 );
  delete layout();
  QVBoxLayout* card = new QVBoxLayout( this );
  card->setContentsMargins( 24, 24, 24, 24 );
  card->setSpacing( 12 );
  QFrame* headerPanel = new QFrame(this);
  headerPanel->setStyleSheet(QStringLiteral("QFrame { background: #EEF4FF; border-radius: 12px; }"));
  QHBoxLayout* header = new QHBoxLayout(headerPanel);
  header->setContentsMargins(16,16,16,16);
  header->setSpacing( 18 );
  header->addWidget( mp_lPhoto );
  QVBoxLayout* identity = new QVBoxLayout;
  QLabel* cardTitle=new QLabel(QStringLiteral("同事资料"),headerPanel);cardTitle->setProperty("role","hint");identity->addWidget(cardTitle);
  mp_lName->setProperty( "role", "title" );
  mp_lName->setMinimumHeight(32);
  mp_lName->setWordWrap( true );
  mp_lStatusDescription->setProperty( "role", "hint" );
  identity->addWidget( mp_lName );
  identity->addWidget( mp_lStatusDescription );
  identity->addWidget( mp_lStatus );
  header->addLayout( identity, 1 );
  card->addWidget( headerPanel );
  foreach( QLabel* detail, QList<QLabel*>() << mp_lEmail << mp_lPhone << mp_lRoom << mp_lBirthday << mp_lWorkgroups )
  {
    detail->setWordWrap( true );
    card->addWidget( detail );
  }
  mp_teInfo->setMinimumHeight( 70 );
  mp_teInfo->setMaximumHeight( 110 );
  card->addWidget( mp_teInfo );
  mp_lPath->setProperty( "role", "hint" );
  mp_lPath->setWordWrap( true );
  QPushButton* networkDetails = new QPushButton(QStringLiteral("连接信息"),this);
  networkDetails->setCheckable(true);networkDetails->setMaximumWidth(120);
  card->addWidget(networkDetails,0,Qt::AlignLeft);
  card->addWidget( mp_lPath );
  mp_lPath->hide();connect(networkDetails,&QPushButton::toggled,mp_lPath,&QWidget::setVisible);
  QHBoxLayout* primaryActions = new QHBoxLayout;
  QHBoxLayout* secondaryActions = new QHBoxLayout;
  foreach( QPushButton* action, QList<QPushButton*>() << mp_pbChat << mp_pbFile << mp_pbFavorite << mp_pbBuzz << mp_pbColor << mp_pbRemove << mp_pbHelp )
  {
    action->setMinimumSize( 0, 36 );
    action->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
    action->setIcon( QIcon() );
  }
  primaryActions->addWidget( mp_pbChat );
  primaryActions->addWidget( mp_pbFile );
  primaryActions->addWidget( mp_pbFavorite );
  secondaryActions->addWidget( mp_pbBuzz );
  secondaryActions->addWidget( mp_pbColor );
  secondaryActions->addWidget( mp_pbRemove );
  secondaryActions->addWidget( mp_pbHelp );
  card->addLayout( primaryActions );
  card->addLayout( secondaryActions );
  mp_frame->hide();
  mp_frameBottom->hide();
  mp_pbChat->setText( QStringLiteral( "发消息" ) );
  mp_pbChat->setProperty( "role", "primary" );
  mp_pbFile->setText( QStringLiteral( "发文件" ) );
  mp_pbFavorite->setText( QStringLiteral( "收藏同事" ) );
  mp_pbBuzz->setText( QStringLiteral( "提醒" ) );
  mp_pbColor->setText( QStringLiteral( "昵称颜色" ) );
  mp_pbRemove->setText( QStringLiteral( "移除同事" ) );
  mp_pbHelp->setText( QStringLiteral( "求助" ) );

  QPalette custom_palette = palette();
  if( Settings::instance().useDarkStyle() )
  {
    custom_palette.setColor( QPalette::Window, Bee::colorDarkGrey() );
    custom_palette.setColor( QPalette::Base, Bee::colorGrey() );
  }
  else
  {
#if QT_VERSION > 0x050000
    custom_palette.setColor( QPalette::Window, Bee::colorWhite().lighter() );
    custom_palette.setColor( QPalette::Base, Bee::colorWhite() );
#else
    custom_palette.setColor( QPalette::Window, Bee::colorWhite() );
    custom_palette.setColor( QPalette::Base, Bee::colorWhite().lighter() );
#endif
  }
  setPalette( custom_palette );

  ShadowDialogStyle::apply( this );

  connect( mp_pbChat, SIGNAL( clicked() ), this, SLOT( showPrivateChat() ) );
  connect( mp_pbFile, SIGNAL( clicked() ), this, SLOT( sendFile() ) );
  connect( mp_pbColor, SIGNAL( clicked() ), this, SLOT( changeColor() ) );
  connect( mp_pbFavorite, SIGNAL( clicked() ), this, SLOT( favoriteClicked() ) );
  connect( mp_pbRemove, SIGNAL( clicked() ), this, SLOT( removeUserClicked() ) );
  connect( mp_pbBuzz, SIGNAL( clicked() ), this, SLOT( sendBuzz() ) );
}

void GuiVCard::setVCard( const User& u, VNumber chat_id, bool core_is_connected )
{
  m_userId = u.id();
  m_chatId = chat_id;
  QString user_path = u.isStatusConnected() ? QString( "%1: <b>%2</b>&nbsp;&nbsp;&nbsp;%3: <b>%4</b>" )
                                                .arg( QStringLiteral( "内网地址" ) ).arg( u.networkAddress().hostAddress().toString() )
                                                .arg( QStringLiteral( "端口" ) ).arg( u.networkAddress().hostPort() )
                                            : Bee::replaceHtmlSpecialCharacters( u.path() );
  mp_lPath->setText( user_path );
  m_userColor = u.color();

  QString name_txt = "";
  if( u.vCard().hasFullName() && u.vCard().fullName( Settings::instance().useUserFirstNameFirstInFullName() ) != u.name() )
    name_txt = QString( "<font color=""%1"">%2</font> %3 " ).arg( u.color(), u.vCard().fullName( Settings::instance().useUserFirstNameFirstInFullName() ), tr( "is" ) );
  name_txt += QString( "<font color=""%1""><b>%2</b></font>" ).arg( u.color(), Bee::replaceHtmlSpecialCharacters( u.name() ) );
  if( !u.localHostName().isEmpty() )
    name_txt.append( QString( " %1" ).arg( tr( "from %1" ).arg( u.localHostName().toLower() ) ) );
  if( !u.domainName().isEmpty() )
    name_txt.append( QString( ".%1" ).arg( u.domainName().toLower() ) );
  mp_lName->setText( Bee::replaceHtmlSpecialCharacters( u.name() ) );

  if( !u.statusDescription().isEmpty() )
    mp_lStatusDescription->setText( Bee::replaceHtmlSpecialCharacters( u.statusDescription() ) );
  else
    mp_lStatusDescription->hide();

  if( u.vCard().birthday().isValid() )
  {
//: This date format refers to a birthday. Date format: do not change letters. More info in https://doc.qt.io/qt-5/qdatetime.html#toString
    QString s_birth_day = tr( "Birthday: %1" ).arg( u.vCard().birthday().year() == 1900 ? u.vCard().birthday().toString( tr( "d MMMM" ) ) : u.vCard().birthday().toString( tr("d MMMM yyyy" ) ) );
    if( u.isBirthDay() )
      s_birth_day += QString( " - <font color=red><b>%1!!!</b></font>" ).arg( tr( "Happy Birthday" ) );
    mp_lBirthday->setText( s_birth_day );
  }
  else
    mp_lBirthday->hide();

  if( !u.vCard().email().isEmpty() )
    mp_lEmail->setText( QStringLiteral("邮箱　")+u.vCard().email() );
  else
    mp_lEmail->hide();

  const QPixmap original=u.vCard().photo().isNull()?Bee::avatarForUser(u,QSize(96,96),true):u.vCard().photo();
  QPixmap rounded(96,96);rounded.fill(Qt::transparent);QPainter portrait(&rounded);portrait.setRenderHint(QPainter::Antialiasing);QPainterPath circle;circle.addEllipse(QRectF(0,0,96,96));portrait.setClipPath(circle);portrait.drawPixmap(0,0,original.scaled(96,96,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));portrait.end();mp_lPhoto->setPixmap(rounded);

  mp_lPhoto->setToolTip( Bee::toolTipForUser( u, false ) );

  if( !u.vCard().phoneNumber().isEmpty() )
    mp_lPhone->setText( QStringLiteral("电话　")+u.vCard().phoneNumber() );
  else
    mp_lPhone->hide();

  if( !u.vCard().roomLocation().isEmpty() )
    mp_lRoom->setText( QStringLiteral("位置　")+u.vCard().roomLocation() );
  else
    mp_lRoom->hide();

  QString workgroups_txt = "";
  if( !u.workgroups().isEmpty() )
  {
    workgroups_txt = Bee::stringListToTextString( u.workgroups(), true, 9 );
    if( u.workgroups().size() > 9 )
      mp_lWorkgroups->setToolTip( Bee::stringListToTextString( u.workgroups(), true ) );
    else
      mp_lWorkgroups->setToolTip( "" );
  }
  else
  {
    workgroups_txt = tr( "none" );
    mp_lWorkgroups->setToolTip( "" );
  }
  mp_lWorkgroups->setText( QString( "<b>%1</b>: %2" ).arg( QStringLiteral( "工作组" ), workgroups_txt ) );
  mp_lWorkgroups->setVisible(!u.workgroups().isEmpty());

  if( !u.vCard().info().isEmpty() )
    mp_teInfo->setPlainText( u.vCard().info() );
  else
    mp_teInfo->setPlainText( tr( "No shared information." ) );

  QString user_version;
  if( u.version().isEmpty() )
    user_version = "";
  else if( u.version() < Settings::instance().version( false, false, false ) )
    user_version = tr( "old %1" ).arg( u.version() );
  else if( u.version() > Settings::instance().version( false, false, false ) )
    user_version = tr( "new %1" ).arg( u.version() );
  else if( u.qtVersion() != Settings::instance().localUser().qtVersion() )
    user_version = QString( "v%1-qt%2" ).arg( u.version(), u.qtVersion() );
  else
    user_version = "";

  if( !user_version.isEmpty() )
  {
    mp_lStatus->setText( QString( " (%1) " ).arg( user_version ) );
    mp_lStatus->setToolTip( tr( "BeeBEEP version" ) );
  }
  else
  {
    mp_lStatus->setText( "" );
    mp_lStatus->setToolTip( "" );
  }

  if( u.isLocal() )
  {
    mp_pbChat->hide();
    mp_pbFile->hide();
    mp_pbFavorite->hide();
    mp_pbBuzz->hide();
    mp_pbRemove->hide();
    mp_pbHelp->hide();
  }
  else
  {
    mp_pbChat->show();
    mp_pbChat->setToolTip( tr( "Open chat" ) );
    if( Settings::instance().enableFileTransfer() && u.isStatusConnected() && core_is_connected )
      mp_pbFile->show();
    else
      mp_pbFile->hide();

    if( Settings::instance().saveUserList() )
    {
      mp_pbFavorite->show();
      if( u.isFavorite() )
      {
        mp_pbFavorite->setText( QStringLiteral( "取消收藏" ) );
        mp_pbFavorite->setToolTip( tr( "Remove from favorites" ) );
      }
      else
      {
        mp_pbFavorite->setText( QStringLiteral( "收藏同事" ) );
        mp_pbFavorite->setToolTip( tr( "Add to favorites" ) );
      }
    }
    else
      mp_pbFavorite->hide();

    if( u.isStatusConnected() )
    {
      mp_pbBuzz->show();
      if( u.isHelper() )
        mp_pbHelp->show();
      else
        mp_pbHelp->hide();
    }
    else
    {
      mp_pbBuzz->hide();
      mp_pbHelp->hide();
    }

    bool remove_is_enabled;
    QString remove_tooltip;

    if( u.isStatusConnected() )
    {
      remove_is_enabled = false;
      remove_tooltip = tr( "You cannot remove an user who is connected" );
    }
    else if( ChatManager::instance().userIsInGroupChat( u.id() ) )
    {
      remove_is_enabled = false;
      remove_tooltip = tr( "You cannot remove an user who is in group chat" );
    }
    else
    {
      remove_is_enabled = true;
      remove_tooltip = tr( "Remove this user" );
    }

    mp_pbRemove->setToolTip( remove_tooltip );
    mp_pbRemove->setEnabled( remove_is_enabled );
  }
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
