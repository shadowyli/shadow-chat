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
// $Id: GuiChatList.cpp 1546 2022-09-29 10:45:33Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "GuiChatList.h"
#include "GuiConfig.h"
#include "ChatManager.h"
#include "IconManager.h"
#include "MessageManager.h"
#include "Settings.h"
#include "UserManager.h"
#include "ShadowConversationPreferences.h"
#include "ShadowConversationDelegate.h"
#include <QDataStream>
#include <QScrollArea>


GuiChatList::GuiChatList( QWidget* parent )
  : QWidget( parent ), m_activeChat( ID_INVALID ), m_quickRefresh( this ), m_quickExpanded( false )
{
  setObjectName( "GuiChatList" );
  setupUi( this );

  mp_menuContext = new QMenu( parent );
  mp_menuSettings = new QMenu( parent );

  mp_twChatList->setColumnCount( 1 );
  mp_twChatList->setRootIsDecorated( false );
  mp_twChatList->setSortingEnabled( true );
  mp_twChatList->sortItems(0,Qt::DescendingOrder);
  mp_twChatList->setIconSize( QSize( 46, 46 ) );
  mp_twChatList->setContextMenuPolicy( Qt::CustomContextMenu );
  mp_twChatList->setMouseTracking( true );
  mp_twChatList->setHeaderHidden( true );
  mp_twChatList->setObjectName( "ShadowConversationList" );
  mp_twChatList->setItemDelegate( new ShadowConversationDelegate( mp_twChatList ) );
  mp_twChatList->setFrameShape( QFrame::NoFrame );
  mp_twChatList->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mp_twChatList->setUniformRowHeights( true );
  mp_twChatList->setAccessibleName( QStringLiteral( "会话列表，显示最近消息和未读状态" ) );
  mp_leFilter->hide();
  mp_pbClearFilter->hide();
  mp_pbSettings->hide();
  gridLayout->removeItem( horizontalLayout );
  delete horizontalLayout;
  gridLayout->removeWidget( mp_twChatList );
  gridLayout->setContentsMargins( 0, 0, 0, 0 );
  gridLayout->setSpacing( 0 );
  setStyleSheet( QStringLiteral(
    "QWidget#GuiChatList{background:#F2F7FA;border:0;}"
    "QWidget#ShadowQuickArea{background:#F2F7FA;border:0;}"
    "QWidget#ShadowQuickItems{background:#F2F7FA;border:0;}"
    "QToolButton[quickChat=true]{background:transparent;border:1px solid transparent;border-radius:7px;padding:3px 0;color:#526174;font-size:11px;}"
    "QToolButton[quickChat=true]:hover{background:#E5EEF9;}"
    "QToolButton[quickChat=true]:focus{border-color:#9BBCEB;}"
    "QToolButton#ShadowQuickExpand{background:#FFFFFF;border:0;border-radius:7px;padding:0;color:#8994A1;}"
    "QTabBar#ShadowConversationFilters{background:#F2F7FA;border-bottom:1px solid #E4ECF2;}"
    "QTabBar#ShadowConversationFilters::tab{background:transparent;border:0;border-radius:5px;color:#3E4D60;padding:7px 9px;margin:5px 1px;font-size:13px;}"
    "QTabBar#ShadowConversationFilters::tab:selected{background:#DCEAFE;color:#2878F0;}"
    "QTabBar#ShadowConversationFilters::tab:focus{background:#E5EFFF;}"
    "QTreeWidget#ShadowConversationList{background:#F2F7FA;border:0;outline:0;padding:2px 0;}"
    "QLabel#ShadowConversationEmpty{color:#8A96A4;background:#F2F7FA;padding:24px;font-size:12px;}" ) );
  mp_quickArea = new QWidget( this );
  mp_quickArea->setObjectName( QStringLiteral( "ShadowQuickArea" ) );
  mp_quickArea->setAccessibleName( QStringLiteral( "置顶会话" ) );
  mp_quickArea->setContextMenuPolicy( Qt::PreventContextMenu );
  QVBoxLayout* quickLayout = new QVBoxLayout( mp_quickArea );
  quickLayout->setContentsMargins( 6, 8, 6, 8 );
  quickLayout->setSpacing( 3 );
  QWidget* quickItems = new QWidget( mp_quickArea );
  quickItems->setObjectName( QStringLiteral( "ShadowQuickItems" ) );
  quickItems->setAttribute( Qt::WA_StyledBackground );
  mp_quickGrid = new QGridLayout( quickItems );
  mp_quickGrid->setContentsMargins( 0, 0, 0, 0 );
  mp_quickGrid->setHorizontalSpacing( 2 );
  mp_quickGrid->setVerticalSpacing( 4 );
  for( int column = 0; column < 5; ++column ) mp_quickGrid->setColumnStretch( column, 1 );
  mp_quickScroll = new QScrollArea( mp_quickArea );
  mp_quickScroll->setObjectName( QStringLiteral( "ShadowPinnedScroll" ) );
  mp_quickScroll->setFrameShape( QFrame::NoFrame );
  mp_quickScroll->setWidgetResizable( true );
  mp_quickScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mp_quickScroll->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  mp_quickScroll->setStyleSheet( QStringLiteral(
    "QScrollArea#ShadowPinnedScroll,QWidget#qt_scrollarea_viewport{background:#F2F7FA;border:0;}" ) );
  mp_quickScroll->setWidget( quickItems );
  quickLayout->addWidget( mp_quickScroll );
  mp_quickToggle = new QToolButton( mp_quickArea );
  mp_quickToggle->setObjectName( QStringLiteral( "ShadowQuickExpand" ) );
  mp_quickToggle->setFixedSize( 70, 17 );
  mp_quickToggle->setText( QStringLiteral( "⌄" ) );
  mp_quickToggle->setAccessibleName( QStringLiteral( "展开置顶会话" ) );
  connect( mp_quickToggle, &QToolButton::clicked, this, [this]() {
    m_quickExpanded = !m_quickExpanded;
    m_quickSnapshot.clear();
    refreshQuickAccess();
  } );
  quickLayout->addWidget( mp_quickToggle, 0, Qt::AlignHCenter );
  gridLayout->addWidget( mp_quickArea, 0, 0 );
  mp_filters = new QTabBar( this );
  mp_filters->setObjectName( QStringLiteral( "ShadowConversationFilters" ) );
  mp_filters->setAccessibleName( QStringLiteral( "会话筛选" ) );
  mp_filters->setExpanding( true );
  mp_filters->setDrawBase( false );
  mp_filters->addTab( QStringLiteral( "最近" ) );
  mp_filters->addTab( QStringLiteral( "未读" ) );
  mp_filters->addTab( QStringLiteral( "@我" ) );
  mp_filters->addTab( QStringLiteral( "待处理" ) );
  mp_filters->setTabToolTip( 2, QStringLiteral( "显示尚有未读消息并提及你的会话" ) );
  mp_filters->setTabToolTip( 3, QStringLiteral( "显示手动标记待处理或仍有待发送消息的会话" ) );
  connect( mp_filters, &QTabBar::currentChanged, this, [this]() {
    for( int index = 0; index < mp_twChatList->topLevelItemCount(); ++index )
      updateItemVisibility( static_cast<GuiChatItem*>( mp_twChatList->topLevelItem( index ) ) );
    updateEmptyState();
  } );
  gridLayout->addWidget( mp_filters, 1, 0 );
  gridLayout->addWidget( mp_twChatList, 2, 0 );
  mp_empty = new QLabel( this );
  mp_empty->setObjectName( QStringLiteral( "ShadowConversationEmpty" ) );
  mp_empty->setAlignment( Qt::AlignCenter );
  mp_empty->setWordWrap( true );
  mp_empty->hide();
  gridLayout->addWidget( mp_empty, 3, 0 );
  gridLayout->setRowStretch( 2, 1 );
  gridLayout->setRowStretch( 3, 0 );
  m_quickRefresh.setSingleShot( true );
  m_quickRefresh.setInterval( 0 );
  connect( &m_quickRefresh, &QTimer::timeout, this, &GuiChatList::refreshQuickAccess );
  refreshQuickAccess();

  m_chatSelected = ID_INVALID;
  m_blockShowChatRequest = false;
  m_filter = "";

#if QT_VERSION >= 0x040700
  mp_leFilter->setPlaceholderText( tr( "Search chat" ) );
#endif
  mp_pbClearFilter->setIcon( IconManager::instance().icon( "clear.png" ) );

  setContextMenuPolicy( Qt::CustomContextMenu );
  connect( this, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( showChatMenu( const QPoint& ) ) );
  connect( mp_twChatList, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( showChatMenu( const QPoint& ) ) );
  connect( mp_twChatList, SIGNAL( itemClicked( QTreeWidgetItem*, int ) ), this, SLOT( chatClicked( QTreeWidgetItem*, int ) ) );
  connect( mp_twChatList, &QTreeWidget::itemActivated, this, &GuiChatList::chatClicked );
  connect( mp_leFilter, SIGNAL( textChanged( const QString& ) ), this, SLOT( filterText( const QString& ) ) );
  connect( mp_pbClearFilter, SIGNAL( clicked() ), this, SLOT( clearFilter() ) );
  connect( mp_pbSettings, SIGNAL( clicked() ), this, SLOT( showMenuSettings() ) );
}

void GuiChatList::keyReleaseEvent( QKeyEvent* e )
{
  if( e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space )
  {
     QList<QTreeWidgetItem*> selected_items = mp_twChatList->selectedItems();
     if( !selected_items.isEmpty() )
       chatClicked( selected_items.first(), 0 );
  }
}

void GuiChatList::updateChats()
{
  mp_twChatList->setUpdatesEnabled( false );
  mp_twChatList->setSortingEnabled( false );
  const int scroll = mp_twChatList->verticalScrollBar()->value();
  mp_twChatList->clear();
  mp_twChatList->setIconSize( QSize( 46, 46 ) );
  foreach( Chat c, ChatManager::instance().constChatList() )
    updateChat( c );
  mp_twChatList->setSortingEnabled( true );
  mp_twChatList->sortItems( 0, Qt::DescendingOrder );
  mp_twChatList->verticalScrollBar()->setValue( scroll );
  mp_twChatList->setUpdatesEnabled( true );
  updateEmptyState();
  m_quickRefresh.start();
}

GuiChatItem* GuiChatList::itemFromChatId( VNumber chat_id )
{
  GuiChatItem* item;
  QTreeWidgetItemIterator it( mp_twChatList );
  while( *it )
  {
    item = dynamic_cast<GuiChatItem*>( *it );
    if( item->chatId() == chat_id )
      return item;
    ++it;
  }
  return Q_NULLPTR;
}

void GuiChatList::updateChat( const Chat& c )
{
  if( !c.isValid() )
    return;

  GuiChatItem* item = itemFromChatId( c.id() );
  if( c.isDefault() )
  {
    delete item;
    return;
  }
  if( !item )
  {
    item = new GuiChatItem( mp_twChatList );
    item->setChatId( c.id() );
  }
  item->updateItem( c );

  item->setData( 0, GuiChatItem::ChatActive, c.id() == m_activeChat );
  if( c.id() == m_activeChat ) mp_twChatList->setCurrentItem( item );
  updateItemVisibility( item );
  updateEmptyState();
  m_quickRefresh.start();
}

void GuiChatList::updateItemVisibility( GuiChatItem* item )
{
  const Chat c = ChatManager::instance().chat( item->chatId() );
  bool hide_item = !c.isValid() || c.isDefault() || ShadowConversationPreferences().hidden( c );
  if( !hide_item && Settings::instance().hideEmptyChatsInList() )
    hide_item = ChatManager::instance().isChatEmpty( c, true );

  if( !hide_item && !m_filter.isEmpty() )
  {
    if( c.isDefault() )
      hide_item = !GuiChatItem::defaultChatName().contains( m_filter, Qt::CaseInsensitive );
    else
      hide_item = !c.name().contains( m_filter, Qt::CaseInsensitive );
  }
  if( mp_filters->currentIndex() == 1 ) hide_item = hide_item || item->unreadMessages() == 0;
  else if( mp_filters->currentIndex() == 2 ) hide_item = hide_item || !item->data( 0, GuiChatItem::ChatMentioned ).toBool();
  else if( mp_filters->currentIndex() == 3 ) hide_item = hide_item ||
    (!item->data( 0, GuiChatItem::ChatPending ).toBool() && item->data( 0, GuiChatItem::ChatQueued ).toInt() == 0);
  item->setHidden( hide_item );
}

void GuiChatList::updateEmptyState()
{
  bool visible = false;
  for( int index = 0; index < mp_twChatList->topLevelItemCount(); ++index )
    visible = visible || !mp_twChatList->topLevelItem( index )->isHidden();
  const QStringList empty = QStringList() << QStringLiteral( "还没有会话，可从通讯录开始聊天。" )
    << QStringLiteral( "没有未读会话" ) << QStringLiteral( "没有未读的 @我 消息" )
    << QStringLiteral( "暂无待处理会话\n右键会话可标记为待处理。" );
  mp_empty->setText( empty.value( mp_filters->currentIndex() ) );
  mp_empty->setVisible( !visible );
  mp_twChatList->setVisible( visible );
  gridLayout->setRowStretch( 2, visible ? 1 : 0 );
  gridLayout->setRowStretch( 3, visible ? 0 : 1 );
}

void GuiChatList::setActiveChat( VNumber id )
{
  m_activeChat = id;
  for( int index = 0; index < mp_twChatList->topLevelItemCount(); ++index )
  {
    GuiChatItem* item = static_cast<GuiChatItem*>( mp_twChatList->topLevelItem( index ) );
    item->setData( 0, GuiChatItem::ChatActive, item->chatId() == id );
    if( item->chatId() == id ) mp_twChatList->setCurrentItem( item );
  }
}

void GuiChatList::refreshQuickAccess()
{
  QList<GuiChatItem*> chats;
  const ShadowConversationPreferences preferences;
  for( int index = 0; index < mp_twChatList->topLevelItemCount(); ++index )
  {
    GuiChatItem* item = static_cast<GuiChatItem*>( mp_twChatList->topLevelItem( index ) );
    const Chat chat = ChatManager::instance().chat( item->chatId() );
    if( chat.isValid() && !chat.isDefault() && preferences.pinned( chat ) &&
        !preferences.hidden( chat ) ) chats << item;
  }
  if( chats.size() <= 10 ) m_quickExpanded = false;
  mp_quickArea->setVisible( !chats.isEmpty() );
  QByteArray snapshot;
  QDataStream data( &snapshot, QIODevice::WriteOnly );
  data << m_quickExpanded << chats.size();
  const int displayed = m_quickExpanded ? chats.size() : qMin( chats.size(), 10 );
  for( int index = 0; index < displayed; ++index )
  {
    const GuiChatItem* item = chats.at( index );
    data << static_cast<quint64>( item->chatId() ) << item->data( 0, GuiChatItem::ChatName ).toString()
      << item->data( 0, GuiChatItem::ChatPinned ).toBool();
    const Chat chat = ChatManager::instance().chat( item->chatId() );
    if( chat.isPrivate() )
    {
      const User user = UserManager::instance().findUser( chat.privateUserId() );
      data << user.vCard().photoHash() << user.color();
    }
  }
  if( snapshot == m_quickSnapshot ) return;
  m_quickSnapshot = snapshot;
  while( QLayoutItem* item = mp_quickGrid->takeAt( 0 ) )
  {
    if( item->widget() ) { item->widget()->hide(); item->widget()->deleteLater(); }
    delete item;
  }
  for( int index = 0; index < displayed; ++index )
  {
    QToolButton* button = new QToolButton( mp_quickArea );
    button->setProperty( "quickChat", true );
    button->setContextMenuPolicy( Qt::PreventContextMenu );
    button->setIconSize( QSize( 40, 40 ) );
    button->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    button->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
    button->setFixedHeight( 68 );
    button->setMinimumWidth( 0 );
    const GuiChatItem* item = chats.at( index );
    const VNumber chatId = item->chatId();
    const QString name = item->data( 0, GuiChatItem::ChatName ).toString();
    button->setObjectName( QStringLiteral( "ShadowQuickChat" ) );
    button->setProperty( "chatId", QVariant::fromValue<qulonglong>( chatId ) );
    button->setIcon( QIcon( ShadowStyle::avatarPixmap( item->icon( 0 ), QSize( 40, 40 ) ) ) );
    QFont labelFont = ShadowStyle::applicationFont();
    labelFont.setPixelSize( 11 );
    button->setText( QFontMetrics( labelFont ).elidedText( name, Qt::ElideRight, 50 ) );
    button->setAccessibleName( name );
    button->setToolTip( name + QStringLiteral( "\n置顶会话 · 右键取消置顶" ) );
    button->setContextMenuPolicy( Qt::CustomContextMenu );
    QMenu* pinMenu = new QMenu( button );
    QAction* unpin = pinMenu->addAction( QStringLiteral( "取消置顶" ) );
    connect( unpin, &QAction::triggered, this, [this, chatId]() {
      m_chatSelected = chatId;
      togglePinSelected();
    } );
    connect( button, &QWidget::customContextMenuRequested, this, [button, pinMenu]( const QPoint& point ) {
      pinMenu->popup( button->mapToGlobal( point ) );
    } );
    connect( button, &QToolButton::clicked, this, [this, chatId]() {
      setActiveChat( chatId );
      emit chatSelected( chatId );
    } );
    mp_quickGrid->addWidget( button, index / 5, index % 5 );
  }
  const int rows = qMin( 3, (displayed + 4) / 5 );
  mp_quickScroll->setFixedHeight( rows * 68 + qMax( 0, rows - 1 ) * 4 );
  mp_quickToggle->setVisible( chats.size() > 10 );
  QPixmap chevron( 18, 12 );
  chevron.fill( Qt::transparent );
  QPainter chevronPainter( &chevron );
  chevronPainter.setRenderHint( QPainter::Antialiasing );
  chevronPainter.setPen( QPen( QColor( "#8994A1" ), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
  const int outerY = m_quickExpanded ? 8 : 4;
  const int centerY = m_quickExpanded ? 4 : 8;
  chevronPainter.drawLine( QPointF( 4, outerY ), QPointF( 9, centerY ) );
  chevronPainter.drawLine( QPointF( 9, centerY ), QPointF( 14, outerY ) );
  chevronPainter.end();
  mp_quickToggle->setText( QString() );
  mp_quickToggle->setIcon( QIcon( chevron ) );
  mp_quickToggle->setIconSize( chevron.size() );
  mp_quickToggle->setToolTip( m_quickExpanded ? QStringLiteral( "收起置顶会话" ) : QStringLiteral( "展开置顶会话" ) );
  mp_quickToggle->setAccessibleName( mp_quickToggle->toolTip() );
}

void GuiChatList::updateUser( const User& u )
{
  QList<Chat> chat_list = ChatManager::instance().chatsWithUser( u.id() );
  foreach( Chat c, chat_list )
    updateChat( c );
}

void GuiChatList::chatClicked( QTreeWidgetItem* item, int )
{
  if( m_blockShowChatRequest )
  {
    m_blockShowChatRequest = false;
    return;
  }

  if( !item )
    return;

  GuiChatItem* user_item = dynamic_cast<GuiChatItem*>( item );
  if( !user_item ) return;
  setActiveChat( user_item->chatId() );
  emit chatSelected( user_item->chatId() );
}

void GuiChatList::showChatMenu( const QPoint& p )
{
  mp_menuContext->clear();
  QAction* act;

  QTreeWidgetItem* item = mp_twChatList->itemAt( p );
  if( !item )
  {
    bool create_chat_is_enabled = true;
    if( UserManager::instance().userList().toList().size() < 2 )
    {
      QAction* unavailable = mp_menuContext->addAction( QStringLiteral( "发现同事后可创建群聊" ) );
      unavailable->setEnabled( false );
      create_chat_is_enabled = false;
      mp_menuContext->addSeparator();
    }
    act = mp_menuContext->addAction( IconManager::instance().icon( "message-create.png" ), QStringLiteral( "发起私聊" ), this, SIGNAL( createNewMessageRequest() ) );
    act->setEnabled( !Settings::instance().disableCreateMessage() );
    act = mp_menuContext->addAction( IconManager::instance().icon( "group-create.png" ), QStringLiteral( "新建群聊" ), this, SIGNAL( createNewChatRequest() ) );
    act->setEnabled( create_chat_is_enabled );
  }
  else
  {
    GuiChatItem* chat_item = dynamic_cast<GuiChatItem*>( item );
    m_chatSelected = chat_item->chatId();
    Chat c = ChatManager::instance().chat( m_chatSelected );
    if( !c.isValid() )
    {
      mp_twChatList->clearSelection();
      qWarning() << "ChatListView is unable to find chat id" << m_chatSelected << " and item is hidden";
      item->setHidden( true );
      return;
    }
    act = mp_menuContext->addAction( IconManager::instance().icon( "chat.png" ), QStringLiteral( "打开会话" ), this, SLOT( openChatSelected() ) );
    mp_menuContext->setDefaultAction( act );
    mp_menuContext->addSeparator();
    const ShadowConversationPreferences preferences;
    mp_menuContext->addAction(preferences.pinned(c)?QStringLiteral("取消置顶"):QStringLiteral("置顶会话"),this,SLOT(togglePinSelected()));
    act=mp_menuContext->addAction(QStringLiteral("消息免打扰"),this,SLOT(toggleMuteSelected()));act->setCheckable(true);act->setChecked(preferences.muted(c));
    mp_menuContext->addAction( preferences.pending( c ) ? QStringLiteral( "完成待处理" ) : QStringLiteral( "标为待处理" ),
      this, SLOT( togglePendingSelected() ) );
    mp_menuContext->addAction(QStringLiteral("移除会话"),this,SLOT(hideChatSelected()));
    mp_menuContext->addSeparator();
    act = mp_menuContext->addAction( IconManager::instance().icon( "clear.png" ), QStringLiteral( "清空聊天记录" ), this, SLOT( clearChatSelected() ) );
    act->setToolTip( tr( "Clear all chat messages" ) );
    act->setDisabled( ChatManager::instance().isChatEmpty( c, true ) );
    if( c.isGroup() )
    {
      mp_menuContext->addSeparator();
      mp_menuContext->addAction( IconManager::instance().icon( "group-edit.png" ), QStringLiteral( "管理群聊" ), this, SLOT( editChatSelected() ) );
    }
    mp_menuContext->addSeparator();
    act = mp_menuContext->addAction( IconManager::instance().icon( "unsent-message.png" ), QStringLiteral( "清除未发送消息" ), this, SLOT( removeUnsentMessagesInChatSelected() ) );
    act->setToolTip( tr( "Clear all unsent messages" ) );
    act->setDisabled( MessageManager::instance().countUnsentMessages( ID_INVALID, m_chatSelected ) <= 0 );
  }

  mp_menuContext->exec( QCursor::pos() );
}

void GuiChatList::openChatSelected()
{
  ShadowConversationPreferences().reveal(ChatManager::instance().chat(m_chatSelected));
  emit chatSelected( m_chatSelected );
}

void GuiChatList::togglePinSelected()
{
  const Chat chat=ChatManager::instance().chat(m_chatSelected);ShadowConversationPreferences preferences;
  if(!preferences.setPinned(chat,!preferences.pinned(chat)))QMessageBox::warning(this,QStringLiteral("会话设置"),QStringLiteral("无法保存置顶设置，请检查磁盘空间与权限。"));
  updateChat(chat);mp_twChatList->sortItems(0,Qt::DescendingOrder);
}
void GuiChatList::toggleMuteSelected()
{
  const Chat chat=ChatManager::instance().chat(m_chatSelected);ShadowConversationPreferences preferences;
  if(!preferences.setMuted(chat,!preferences.muted(chat)))QMessageBox::warning(this,QStringLiteral("会话设置"),QStringLiteral("无法保存免打扰设置，请检查磁盘空间与权限。"));
  updateChat(chat);
}
void GuiChatList::hideChatSelected()
{
  const Chat chat=ChatManager::instance().chat(m_chatSelected);
  if(!ShadowConversationPreferences().setHidden(chat,true))QMessageBox::warning(this,QStringLiteral("会话设置"),QStringLiteral("无法保存会话设置，请检查磁盘空间与权限。"));
  updateChat(chat);
}

void GuiChatList::togglePendingSelected()
{
  const Chat chat = ChatManager::instance().chat( m_chatSelected );
  const ShadowConversationPreferences preferences;
  if( !preferences.setPending( chat, !preferences.pending( chat ) ) )
    QMessageBox::warning( this, QStringLiteral( "会话设置" ), QStringLiteral( "无法保存待处理标记，请检查文件夹权限。" ) );
  updateChat( chat );
}

void GuiChatList::clearChatSelected()
{
  emit chatToClear( m_chatSelected );
}

void GuiChatList::editChatSelected()
{
  emit chatToEdit( m_chatSelected );
}

void GuiChatList::removeUnsentMessagesInChatSelected()
{
  emit removeUnsentMessagesInChat( m_chatSelected );
}

void GuiChatList::filterText( const QString& txt )
{
  QString new_filter = txt.trimmed().toLower();
  if( m_filter == new_filter )
    return;

  m_filter = new_filter;
  updateChats();
}

void GuiChatList::clearFilter()
{
  mp_leFilter->setText( "" );
  mp_leFilter->setFocus();
}

void GuiChatList::selectBackgroundColor()
{
  QColor c = Bee::selectColor( this, Settings::instance().chatListBackgroundColor() );
  if( c.isValid() )
  {
    Settings::instance().setChatListBackgroundColor( c.name() );
    updateBackground();
  }
}

void GuiChatList::updateBackground()
{
  mp_twChatList->setStyleSheet( QStringLiteral(
    "QTreeWidget#ShadowConversationList{background:#F2F7FA;border:0;outline:0;padding:2px 0;}" ) );
}

void GuiChatList::showMenuSettings()
{
  bool create_chat_is_enabled = UserManager::instance().userList().toList().size() >= 2;
  QAction* act;
  mp_menuSettings->clear();
  act = mp_menuSettings->addAction( IconManager::instance().icon( "message-create.png" ), tr( "Write a message" ), this, SIGNAL( createNewMessageRequest() ) );
  act->setEnabled( !Settings::instance().disableCreateMessage() );
  act = mp_menuSettings->addAction( IconManager::instance().icon( "group-create.png" ), tr( "Create new group chat" ), this, SIGNAL( createNewChatRequest() ) );
  act->setEnabled( create_chat_is_enabled );
  mp_menuSettings->addSeparator();
  mp_menuSettings->addAction( IconManager::instance().icon( "background-color.png" ), tr( "Change background color" ) + QString("..."), this, SLOT( selectBackgroundColor() ) );
  mp_menuSettings->addSeparator();
  act = mp_menuSettings->addAction( tr( "Hide empty chats" ), this, SIGNAL( hideEmptyChatsRequest() ) );
  act->setCheckable( true );
  act->setChecked( Settings::instance().hideEmptyChatsInList() );
  mp_menuSettings->exec( QCursor::pos() );
}

void GuiChatList::selectFirstChat()
{
  mp_twChatList->clearSelection();
  for(int index=0;index<mp_twChatList->topLevelItemCount();++index)
  {
    QTreeWidgetItem* item = mp_twChatList->topLevelItem(index);
    if( item && !item->isHidden() )
    {
      item->setSelected( true );
      mp_twChatList->setFocus();
      return;
    }
  }
}
