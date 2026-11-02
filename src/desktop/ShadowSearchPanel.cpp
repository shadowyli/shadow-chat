#include "ShadowSearchPanel.h"
#include "BeeUtils.h"
#include "ChatManager.h"
#include "HistoryRecord.h"
#include "Settings.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QPainterPath>

namespace
{
  const int KindRole = Qt::UserRole + 1;
  const int DetailRole = Qt::UserRole + 2;
  const int ChatNameRole = Qt::UserRole + 3;
  const int ArchivedRole = Qt::UserRole + 4;
  const int QueryRole = Qt::UserRole + 5;
  const int ResultLimit = 100;

  struct SearchResult
  {
    VNumber id;
    int kind;
    QString title;
    QString detail;
    QIcon avatar;
    QString chatName;
    bool archived;
  };

  QString highlighted( const QString& text, const QString& query )
  {
    const int position = text.indexOf( query, 0, Qt::CaseInsensitive );
    if( query.isEmpty() || position < 0 ) return text.toHtmlEscaped();
    return text.left( position ).toHtmlEscaped() + QStringLiteral( "<span style=\"color:#2878F0;\">%1</span>" )
      .arg( text.mid( position, query.size() ).toHtmlEscaped() ) +
      text.mid( position + query.size() ).toHtmlEscaped();
  }

  class SearchDelegate : public QStyledItemDelegate
  {
  public:
    explicit SearchDelegate( QObject* parent ) : QStyledItemDelegate( parent ) {}

    QSize sizeHint( const QStyleOptionViewItem&, const QModelIndex& index ) const override
    {
      return QSize( 400, index.data( KindRole ).toInt() == 0 ? 44 : 70 );
    }

    void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
    {
      painter->save();
      painter->setClipRect( option.rect );
      const QRect row = option.rect.adjusted( 8, 1, -8, -1 );
      if( index.data( KindRole ).toInt() == 0 )
      {
        QFont font = ShadowStyle::applicationFont();
        font.setPixelSize( 14 );
        font.setWeight( QFont::DemiBold );
        painter->setFont( font );
        painter->setPen( QColor( "#243044" ) );
        painter->drawText( row.adjusted( 12, 0, -12, 0 ), Qt::AlignVCenter | Qt::AlignLeft, index.data().toString() );
        painter->restore();
        return;
      }
      if( option.state.testFlag( QStyle::State_Selected ) || option.state.testFlag( QStyle::State_MouseOver ) )
      {
        painter->setRenderHint( QPainter::Antialiasing );
        painter->setPen( Qt::NoPen );
        painter->setBrush( option.state.testFlag( QStyle::State_Selected ) ? QColor( "#E7F1FF" ) : QColor( "#F4F8FD" ) );
        painter->drawRoundedRect( row, 6, 6 );
      }
      const QRect avatar( row.left() + 12, row.top() + 15, 38, 38 );
      painter->drawPixmap( avatar, ShadowStyle::avatarPixmap( index.data( Qt::DecorationRole ).value<QIcon>(), avatar.size() ) );
      const int available = qMax( 1, row.width() - 84 );
      QFont font = ShadowStyle::applicationFont();
      font.setPixelSize( 14 );
      QTextDocument title;
      title.setDefaultFont( font );
      title.setDocumentMargin( 0 );
      title.setDefaultStyleSheet( QStringLiteral( "body {color:#243044;}" ) );
      title.setHtml( highlighted( QFontMetrics( font ).elidedText( index.data().toString(), Qt::ElideRight, available ),
                                 index.data( QueryRole ).toString() ) );
      painter->save();
      painter->translate( avatar.right() + 13, row.top() + 12 );
      title.drawContents( painter, QRectF( 0, 0, available, 26 ) );
      painter->restore();
      font.setPixelSize( 12 );
      painter->setFont( font );
      painter->setPen( QColor( "#8B95A2" ) );
      painter->drawText( QRect( avatar.right() + 13, row.top() + 38, available, 22 ),
        Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics( font ).elidedText(
          index.data( DetailRole ).toString(), Qt::ElideRight, available ) );
      painter->restore();
    }
  };

  QString snippet( const QString& text, const QString& query )
  {
    const QString plain = text.simplified();
    const int position = qMax( 0, plain.indexOf( query, 0, Qt::CaseInsensitive ) );
    const int start = qMax( 0, position - 25 );
    return (start > 0 ? QStringLiteral( "…" ) : QString()) + plain.mid( start, 180 );
  }
}

ShadowSearchPanel::ShadowSearchPanel( QWidget* parent )
  : QWidget( parent ), m_debounce( this )
{
  setObjectName( QStringLiteral( "ShadowSearchPanel" ) );
  setAttribute( Qt::WA_StyledBackground );
  setStyleSheet( QStringLiteral(
    "QWidget#ShadowSearchPanel{background:#FFFFFF;border:1px solid #DCE4ED;border-radius:9px;}"
    "QWidget#ShadowSearchInputBar{background:#F7FAFC;border-bottom:1px solid #E8EDF2;}"
    "QLineEdit#ShadowSearchInput{background:transparent;border:0;padding:7px;font-size:15px;color:#172033;}"
    "QPushButton#ShadowSearchClear{background:transparent;border:0;color:#8A95A2;padding:6px 8px;}"
    "QToolButton#ShadowSearchClose{background:transparent;border:0;border-radius:5px;padding:5px;}"
    "QToolButton#ShadowSearchClose:hover{background:#E9EFF5;}"
    "QTabBar#ShadowSearchTabs{background:#FFFFFF;border-bottom:1px solid #EDF0F4;}"
    "QTabBar#ShadowSearchTabs::tab{background:transparent;border:0;border-bottom:3px solid transparent;color:#243044;padding:13px 20px;font-size:15px;}"
    "QTabBar#ShadowSearchTabs::tab:selected{color:#2878F0;border-bottom-color:#4289FF;}"
    "QTabBar#ShadowSearchTabs::tab:focus{background:#F0F6FF;}"
    "QTreeWidget#ShadowGlobalSearchResults{background:#FFFFFF;border:0;outline:0;}"
    "QLabel#ShadowSearchEmpty{background:#FFFFFF;border:0;color:#8A95A2;font-size:14px;padding:28px;}"
    "QWidget#ShadowSearchFooter{background:#F7F9FB;border:0;border-top:1px solid #EDF0F4;}"
    "QLabel#ShadowSearchKeys,QLabel#ShadowSearchScope{font-size:11px;color:#8A95A2;border:0;background:transparent;}"
    "QPushButton[searchMore=true]{background:transparent;color:#697684;border:0;padding:7px 12px;}"
    "QPushButton[searchMore=true]:hover{color:#2878F0;}" ) );
  QVBoxLayout* layout = new QVBoxLayout( this );
  layout->setContentsMargins( 1, 1, 1, 1 );
  layout->setSpacing( 0 );
  QWidget* inputBar = new QWidget( this );
  inputBar->setObjectName( QStringLiteral( "ShadowSearchInputBar" ) );
  inputBar->setFixedHeight( 46 );
  QHBoxLayout* input = new QHBoxLayout( inputBar );
  input->setContentsMargins( 10, 0, 8, 0 );
  input->setSpacing( 5 );
  mp_search = new QLineEdit( inputBar );
  mp_search->setObjectName( QStringLiteral( "ShadowSearchInput" ) );
  mp_search->setPlaceholderText( QStringLiteral( "搜索联系人、群聊或本机消息" ) );
  mp_search->setAccessibleName( QStringLiteral( "全局搜索" ) );
  mp_search->addAction( ShadowStyle::actionIcon( QStringLiteral( "magnifying-glass" ) ), QLineEdit::LeadingPosition );
  input->addWidget( mp_search, 1 );
  QPushButton* clear = new QPushButton( QStringLiteral( "清空" ), inputBar );
  clear->setObjectName( QStringLiteral( "ShadowSearchClear" ) );
  clear->setAutoDefault( false );
  input->addWidget( clear );
  connect( clear, &QPushButton::clicked, this, [this]() { mp_search->clear(); mp_search->setFocus(); } );
  QToolButton* close = new QToolButton( inputBar );
  close->setObjectName( QStringLiteral( "ShadowSearchClose" ) );
  close->setFixedSize( 30, 30 );
  close->setIcon( ShadowStyle::actionIcon( QStringLiteral( "x" ) ) );
  close->setAccessibleName( QStringLiteral( "关闭搜索" ) );
  close->setToolTip( QStringLiteral( "关闭搜索 (Esc)" ) );
  input->addWidget( close );
  connect( close, &QToolButton::clicked, this, &ShadowSearchPanel::dismiss );
  layout->addWidget( inputBar );
  mp_tabs = new QTabBar( this );
  mp_tabs->setObjectName( QStringLiteral( "ShadowSearchTabs" ) );
  mp_tabs->setExpanding( false );
  mp_tabs->setDrawBase( false );
  mp_tabs->addTab( QStringLiteral( "全部" ) );
  mp_tabs->addTab( QStringLiteral( "联系人" ) );
  mp_tabs->addTab( QStringLiteral( "群" ) );
  mp_tabs->addTab( QStringLiteral( "消息" ) );
  layout->addWidget( mp_tabs );
  mp_results = new QTreeWidget( this );
  mp_results->setObjectName( QStringLiteral( "ShadowGlobalSearchResults" ) );
  mp_results->setAccessibleName( QStringLiteral( "搜索结果" ) );
  mp_results->setHeaderHidden( true );
  mp_results->setRootIsDecorated( false );
  mp_results->setIndentation( 0 );
  mp_results->setFrameShape( QFrame::NoFrame );
  mp_results->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mp_results->setSelectionMode( QAbstractItemView::SingleSelection );
  mp_results->setMouseTracking( true );
  mp_results->setItemDelegate( new SearchDelegate( mp_results ) );
  layout->addWidget( mp_results, 1 );
  mp_empty = new QLabel( this );
  mp_empty->setObjectName( QStringLiteral( "ShadowSearchEmpty" ) );
  mp_empty->setAlignment( Qt::AlignCenter );
  mp_empty->setWordWrap( true );
  layout->addWidget( mp_empty, 1 );
  QWidget* footer = new QWidget( this );
  footer->setObjectName( QStringLiteral( "ShadowSearchFooter" ) );
  QHBoxLayout* hints = new QHBoxLayout( footer );
  hints->setContentsMargins( 16, 10, 16, 10 );
  QLabel* keys = new QLabel( QStringLiteral( "↑↓ 选择    Tab 切换分类    Enter 打开    Esc 关闭" ), footer );
  keys->setObjectName( QStringLiteral( "ShadowSearchKeys" ) );
  hints->addWidget( keys );
  hints->addStretch();
  mp_scope = new QLabel( QStringLiteral( "本机已加载内容" ), footer );
  mp_scope->setObjectName( QStringLiteral( "ShadowSearchScope" ) );
  hints->addWidget( mp_scope );
  layout->addWidget( footer );
  m_debounce.setSingleShot( true );
  m_debounce.setInterval( 120 );
  connect( &m_debounce, &QTimer::timeout, this, &ShadowSearchPanel::refresh );
  connect( mp_search, &QLineEdit::textChanged, this, [this]( const QString& text ) {
    mp_results->clear();
    mp_results->hide();
    mp_empty->setText( text.trimmed().isEmpty() ? QStringLiteral( "输入姓名、群名称或消息关键词开始搜索" )
                                              : QStringLiteral( "正在搜索本机内容…" ) );
    mp_empty->show();
    emit queryChanged( text );
    m_debounce.start();
  } );
  connect( mp_tabs, &QTabBar::currentChanged, this, [this]() { m_debounce.stop(); refresh(); } );
  connect( mp_results, &QTreeWidget::itemClicked, this, [this]( QTreeWidgetItem* item, int ) { activateItem( item ); } );
  connect( mp_results, &QTreeWidget::itemActivated, this, [this]( QTreeWidgetItem* item, int ) { activateItem( item ); } );
  qApp->installEventFilter( this );
  hide();
}

void ShadowSearchPanel::positionPanel()
{
  const int left = qBound( 88, parentWidget()->width() / 10, 144 );
  const int width = qMin( 1200, parentWidget()->width() - left - 100 );
  setGeometry( left, 7, qMax( 460, width ), qMin( 760, parentWidget()->height() - 24 ) );
  QPainterPath outline;
  outline.addRoundedRect( QRectF( rect() ), 9, 9 );
  setMask( QRegion( outline.toFillPolygon().toPolygon() ) );
}

void ShadowSearchPanel::open( const QString& query )
{
  if( !isVisible() )
  {
    const QSignalBlocker blocked( mp_tabs );
    mp_tabs->setCurrentIndex( 0 );
  }
  setQuery( query );
  positionPanel();
  show();
  raise();
  mp_search->setFocus();
  emit queryChanged( query );
}

void ShadowSearchPanel::setQuery( const QString& query )
{
  if( mp_search->text() != query )
  {
    mp_results->clear();
    mp_results->hide();
    mp_empty->setText( query.trimmed().isEmpty() ? QStringLiteral( "输入姓名、群名称或消息关键词开始搜索" )
                                               : QStringLiteral( "正在搜索本机内容…" ) );
    mp_empty->show();
    const QSignalBlocker blocked( mp_search );
    mp_search->setText( query );
  }
  m_debounce.start();
}

void ShadowSearchPanel::dismiss()
{
  m_debounce.stop();
  hide();
  emit dismissed();
}

void ShadowSearchPanel::refresh()
{
  const QString query = mp_search->text().trimmed();
  mp_results->clear();
  if( query.isEmpty() )
  {
    mp_empty->setText( QStringLiteral( "输入姓名、群名称或消息关键词开始搜索" ) );
    mp_results->hide();
    mp_empty->show();
    return;
  }
  QVector<SearchResult> contacts;
  QVector<SearchResult> groups;
  QVector<SearchResult> messages;
  for( const User& user : UserManager::instance().userList().toList() )
  {
    if( !user.isValid() || user.isLocal() ) continue;
    const QString detail = user.workgroups().join( QStringLiteral( " · " ) );
    if( !user.name().contains( query, Qt::CaseInsensitive ) &&
        !detail.contains( query, Qt::CaseInsensitive ) &&
        !user.accountName().contains( query, Qt::CaseInsensitive ) &&
        !user.vCard().fullName( Settings::instance().useUserFirstNameFirstInFullName() ).contains( query, Qt::CaseInsensitive ) ) continue;
    if( contacts.size() < ResultLimit )
      contacts.append( SearchResult{ user.id(), 1, user.name(),
        detail.isEmpty() ? user.accountName() : detail, Bee::avatarForUser( user, QSize( 38, 38 ), true ), QString(), false } );
  }
  for( const Chat& chat : ChatManager::instance().constChatList() )
  {
    if( !chat.isValid() || chat.isDefault() ) continue;
    QStringList matchingMembers;
    if( chat.isGroup() )
    {
      for( VNumber userId : chat.usersId() )
      {
        const User user = UserManager::instance().findUser( userId );
        if( user.isValid() && user.name().contains( query, Qt::CaseInsensitive ) ) matchingMembers << user.name();
      }
      if( (chat.name().contains( query, Qt::CaseInsensitive ) || !matchingMembers.isEmpty()) && groups.size() < ResultLimit )
        groups.append( SearchResult{ chat.id(), 2, chat.name(),
          matchingMembers.isEmpty() ? QStringLiteral( "%1 位成员" ).arg( chat.usersId().size() )
            : QStringLiteral( "包含：%1" ).arg( matchingMembers.join( QStringLiteral( "、" ) ) ),
          ShadowStyle::actionIcon( QStringLiteral( "users-three" ) ), chat.name(), false } );
    }
    const QList<ChatMessage>& history = chat.messages();
    for( int index = history.size() - 1; index >= 0 && messages.size() < ResultLimit; --index )
    {
      const ChatMessage& message = history.at( index );
      if( message.isFromSystem() || message.isHeader() || message.isImagePreview() ) continue;
      QTextDocument plain;
      plain.setHtml( message.message() );
      if( !plain.toPlainText().contains( query, Qt::CaseInsensitive ) ) continue;
      messages.append( SearchResult{ chat.id(), 3, snippet( plain.toPlainText(), query ),
        chat.name() + QStringLiteral( "    " ) + message.timestamp().toString( QStringLiteral( "yyyy/MM/dd HH:mm" ) ),
        ShadowStyle::actionIcon( QStringLiteral( "chats-circle" ) ), chat.name(), false } );
    }
  }
  const QMap<QString, QString>& saved = ChatManager::instance().constHistoryMap();
  for( auto iterator = saved.constBegin(); iterator != saved.constEnd() && messages.size() < ResultLimit; ++iterator )
  {
    if( !iterator.value().contains( query, Qt::CaseInsensitive ) ) continue;
    const QList<HistoryRecord> records = shadowChatHistoryRecords( iterator.value() );
    for( int index = records.size() - 1; index >= 0 && messages.size() < ResultLimit; --index )
    {
      const HistoryRecord& record = records.at( index );
      if( !record.text.contains( query, Qt::CaseInsensitive ) ) continue;
      messages.append( SearchResult{ ID_INVALID, 3, snippet( record.text, query ),
        iterator.key() + QStringLiteral( " · 聊天记录    " ) + record.timestamp.toString( QStringLiteral( "yyyy/MM/dd HH:mm" ) ),
        ShadowStyle::actionIcon( QStringLiteral( "archive" ) ), iterator.key(), true } );
    }
  }
  const QVector<SearchResult> collections[] = { contacts, groups, messages };
  const QStringList labels = QStringList() << QStringLiteral( "联系人" ) << QStringLiteral( "群" ) << QStringLiteral( "消息" );
  for( int section = 0; section < 3; ++section )
  {
    if( mp_tabs->currentIndex() != 0 && mp_tabs->currentIndex() != section + 1 ) continue;
    const QVector<SearchResult>& results = collections[section];
    if( results.isEmpty() ) continue;
    QTreeWidgetItem* header = new QTreeWidgetItem( mp_results, QStringList(
      QStringLiteral( "%1  %2%3" ).arg( labels.at( section ) ).arg( results.size() ).arg( results.size() == ResultLimit ? "+" : "" ) ) );
    header->setData( 0, KindRole, 0 );
    header->setFlags( Qt::ItemIsEnabled );
    if( mp_tabs->currentIndex() == 0 && results.size() > 5 )
    {
      QWidget* headerWidget = new QWidget( mp_results );
      QHBoxLayout* headerLayout = new QHBoxLayout( headerWidget );
      headerLayout->setContentsMargins( 20, 0, 16, 0 );
      headerLayout->addWidget( new QLabel( header->text( 0 ), headerWidget ) );
      headerLayout->addStretch();
      QPushButton* more = new QPushButton( QStringLiteral( "更多 ›" ), headerWidget );
      more->setProperty( "searchMore", true );
      connect( more, &QPushButton::clicked, this, [this, section]() { mp_tabs->setCurrentIndex( section + 1 ); } );
      headerLayout->addWidget( more );
      mp_results->setItemWidget( header, 0, headerWidget );
    }
    const int count = mp_tabs->currentIndex() == 0 ? qMin( 5, results.size() ) : results.size();
    for( int index = 0; index < count; ++index )
    {
      const SearchResult& result = results.at( index );
      QTreeWidgetItem* row = new QTreeWidgetItem( mp_results, QStringList( result.title ) );
      row->setIcon( 0, result.avatar );
      row->setData( 0, Qt::UserRole, QVariant::fromValue<qulonglong>( result.id ) );
      row->setData( 0, KindRole, result.kind );
      row->setData( 0, DetailRole, result.detail );
      row->setData( 0, ChatNameRole, result.chatName );
      row->setData( 0, ArchivedRole, result.archived );
      row->setData( 0, QueryRole, query );
      row->setToolTip( 0, result.title + "\n" + result.detail );
      row->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    }
  }
  const bool empty = mp_results->topLevelItemCount() == 0;
  mp_results->setVisible( !empty );
  mp_empty->setText( QStringLiteral( "没有找到“%1”，试试其他关键词" ).arg( query ) );
  mp_empty->setVisible( empty );
  mp_scope->setText( QStringLiteral( "本机已加载内容 · 每类最多 100 条" ) );
  moveSelection( 1 );
}

void ShadowSearchPanel::moveSelection( int direction )
{
  const int current = mp_results->indexOfTopLevelItem( mp_results->currentItem() );
  int next = current < 0 ? (direction > 0 ? 0 : mp_results->topLevelItemCount() - 1) : current + direction;
  for( ; next >= 0 && next < mp_results->topLevelItemCount(); next += direction )
  {
    QTreeWidgetItem* item = mp_results->topLevelItem( next );
    if( item->data( 0, KindRole ).toInt() > 0 )
    {
      mp_results->setCurrentItem( item );
      mp_results->scrollToItem( item );
      return;
    }
  }
}

void ShadowSearchPanel::activateItem( QTreeWidgetItem* item )
{
  if( !item || item->data( 0, KindRole ).toInt() == 0 ) return;
  const int kind = item->data( 0, KindRole ).toInt();
  const VNumber id = item->data( 0, Qt::UserRole ).toULongLong();
  const QString chatName = item->data( 0, ChatNameRole ).toString();
  const QString query = item->data( 0, QueryRole ).toString();
  const bool archived = item->data( 0, ArchivedRole ).toBool();
  dismiss();
  if( kind == 1 ) emit contactActivated( id );
  else if( kind == 2 ) emit groupActivated( id );
  else emit messageActivated( id, chatName, query, archived );
}

bool ShadowSearchPanel::eventFilter( QObject* watched, QEvent* event )
{
  if( watched == parentWidget() && event->type() == QEvent::Resize && isVisible() ) positionPanel();
  if( !isVisible() ) return QWidget::eventFilter( watched, event );
  if( watched == window() && event->type() == QEvent::WindowDeactivate )
  {
    dismiss();
    return false;
  }
  QWidget* target = qobject_cast<QWidget*>( watched );
  const bool inside = target && (target == this || isAncestorOf( target ));
  if( event->type() == QEvent::MouseButtonPress && target && !inside &&
      target->window() == window() )
  {
    dismiss();
    return false;
  }
  if( inside && event->type() == QEvent::KeyPress )
  {
    QKeyEvent* key = static_cast<QKeyEvent*>( event );
    if( key->key() == Qt::Key_Escape ) { dismiss(); return true; }
    if( key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab )
    {
      const int step = key->key() == Qt::Key_Backtab || key->modifiers().testFlag( Qt::ShiftModifier ) ? -1 : 1;
      mp_tabs->setCurrentIndex( (mp_tabs->currentIndex() + step + mp_tabs->count()) % mp_tabs->count() );
      return true;
    }
    if( watched == mp_search || watched == mp_results || watched == mp_results->viewport() )
    {
      if( key->key() == Qt::Key_Down || key->key() == Qt::Key_Up )
      {
        if( m_debounce.isActive() ) { m_debounce.stop(); refresh(); }
        moveSelection( key->key() == Qt::Key_Down ? 1 : -1 );
        return true;
      }
      if( key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter )
      {
        if( m_debounce.isActive() ) { m_debounce.stop(); refresh(); }
        activateItem( mp_results->currentItem() );
        return true;
      }
    }
  }
  return QWidget::eventFilter( watched, event );
}
