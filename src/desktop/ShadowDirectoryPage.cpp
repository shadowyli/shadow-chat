#include "ShadowDirectoryPage.h"
#include "BeeUtils.h"
#include "ChatManager.h"
#include "Settings.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QCollator>
#include <QDataStream>
#include <QStyledItemDelegate>
#include <QTextCodec>
#include <algorithm>

namespace
{
  const int KindRole = Qt::UserRole + 1;
  const int KeyRole = Qt::UserRole + 2;
  const int DetailRole = Qt::UserRole + 3;

  struct DirectoryEntry
  {
    VNumber id;
    int kind;
    QString name;
    QString detail;
    QString key;
    QIcon avatar;
  };

  QIcon categoryIcon( const QString& name, const QColor& color )
  {
    QPixmap image( 40, 40 );
    image.fill( Qt::transparent );
    QPainter painter( &image );
    painter.setRenderHint( QPainter::Antialiasing );
    painter.setPen( Qt::NoPen );
    painter.setBrush( color );
    painter.drawEllipse( image.rect().adjusted( 1, 1, -1, -1 ) );
    painter.drawPixmap( 8, 8, ShadowStyle::tintedPixmap(
      QStringLiteral( ":/images/shadowchat/icons/%1.svg" ).arg( name ), QSize( 24, 24 ), Qt::white ) );
    return QIcon( image );
  }

  class DirectoryDelegate : public QStyledItemDelegate
  {
  public:
    explicit DirectoryDelegate( QObject* parent ) : QStyledItemDelegate( parent ) {}

    QSize sizeHint( const QStyleOptionViewItem&, const QModelIndex& index ) const override
    {
      return QSize( 300, index.data( KindRole ).toInt() == 0 ? 34 : 62 );
    }

    void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
    {
      painter->save();
      painter->setClipRect( option.rect );
      painter->setRenderHint( QPainter::Antialiasing );
      if( index.data( KindRole ).toInt() == 0 )
      {
        QFont font = ShadowStyle::applicationFont();
        font.setPixelSize( 12 );
        painter->setFont( font );
        const QString key = index.data().toString();
        const int width = qMin( option.rect.width() - 40, qMax( 34, QFontMetrics( font ).horizontalAdvance( key ) + 22 ) );
        const QRect badge( option.rect.left() + 20, option.rect.top() + 7, width, 22 );
        painter->setPen( Qt::NoPen );
        painter->setBrush( QColor( "#F0F2F4" ) );
        painter->drawRoundedRect( badge, 11, 11 );
        painter->setPen( QColor( "#7B8592" ) );
        painter->drawText( badge.adjusted( 8, 0, -8, 0 ), Qt::AlignCenter,
          QFontMetrics( font ).elidedText( key, Qt::ElideRight, width - 16 ) );
        painter->restore();
        return;
      }
      const QRect row = option.rect.adjusted( 8, 2, -8, -2 );
      if( option.state.testFlag( QStyle::State_Selected ) || option.state.testFlag( QStyle::State_MouseOver ) )
      {
        painter->setPen( Qt::NoPen );
        painter->setBrush( option.state.testFlag( QStyle::State_Selected ) ? QColor( "#E8F1FF" ) : QColor( "#F5F8FC" ) );
        painter->drawRoundedRect( row, 7, 7 );
      }
      const QRect avatar( option.rect.left() + 20, option.rect.center().y() - 18, 36, 36 );
      const QIcon icon = index.data( Qt::DecorationRole ).value<QIcon>();
      painter->drawPixmap( avatar, ShadowStyle::avatarPixmap( icon, avatar.size() ) );
      const int textLeft = avatar.right() + 13;
      const int available = qMax( 0, option.rect.right() - textLeft - 20 );
      const QString name = index.data().toString();
      const QString detail = index.data( DetailRole ).toString();
      QFont font = ShadowStyle::applicationFont();
      font.setPixelSize( 14 );
      painter->setFont( font );
      const QFontMetrics metrics( font );
      const int nameWidth = qMin( metrics.horizontalAdvance( name ) + 2,
                                 detail.isEmpty() ? available : qMax( 80, available / 2 ) );
      painter->setPen( QColor( "#243044" ) );
      painter->drawText( QRect( textLeft, row.top(), nameWidth, row.height() ), Qt::AlignLeft | Qt::AlignVCenter,
        metrics.elidedText( name, Qt::ElideRight, nameWidth ) );
      if( !detail.isEmpty() && available > nameWidth + 20 )
      {
        font.setPixelSize( 12 );
        painter->setFont( font );
        painter->setPen( QColor( "#8993A0" ) );
        const int detailWidth = available - nameWidth - 16;
        painter->drawText( QRect( textLeft + nameWidth + 16, row.top(), detailWidth, row.height() ),
          Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics( font ).elidedText( detail, Qt::ElideRight, detailWidth ) );
      }
      if( option.state.testFlag( QStyle::State_HasFocus ) )
      {
        painter->setBrush( Qt::NoBrush );
        painter->setPen( QPen( QColor( "#A4C1EF" ), 1 ) );
        painter->drawRoundedRect( row.adjusted( 1, 1, -1, -1 ), 7, 7 );
      }
      painter->restore();
    }
  };
}

ShadowDirectoryPage::ShadowDirectoryPage( QWidget* parent )
  : QWidget( parent ), m_section( Contacts ), m_resetScroll( true )
{
  setObjectName( QStringLiteral( "ShadowDirectoryPage" ) );
  setStyleSheet( QStringLiteral(
    "QWidget#ShadowDirectoryPage{background:#FFFFFF;color:#243044;}"
    "QWidget#ShadowDirectorySidebar{background:#F2F7FA;border-right:1px solid #E6EDF2;}"
    "QLabel#ShadowDirectoryHeading{font-size:20px;font-weight:600;color:#172033;}"
    "QPushButton[category=true]{text-align:left;background:transparent;border:0;border-radius:9px;padding:13px 12px;color:#243044;font-size:16px;}"
    "QPushButton[category=true]:hover{background:#E7EFF7;}"
    "QPushButton[category=true]:checked{background:#DCEAFE;color:#1D4F9B;}"
    "QPushButton[category=true]:focus{border:1px solid #9DBEEB;}"
    "QWidget#ShadowDirectoryHeader{background:#FFFFFF;border-bottom:1px solid #EDF0F3;}"
    "QLabel#ShadowDirectoryTitle{font-size:17px;color:#172033;}"
    "QLabel#ShadowDirectoryCount{font-size:12px;color:#929CAA;}"
    "QComboBox#ShadowDirectorySort{background:#FFFFFF;border:1px solid #E1E6EB;border-radius:5px;padding:5px 10px;min-width:116px;color:#526174;}"
    "QTreeWidget#ShadowDirectoryList{background:#FFFFFF;border:0;outline:0;padding:6px 0;}"
    "QLabel#ShadowDirectoryEmpty{color:#84909D;font-size:14px;padding:24px;}"
    "QToolButton[index=true]{background:transparent;border:0;border-radius:9px;color:#8994A1;font-size:10px;padding:0;}"
    "QToolButton[index=true]:hover,QToolButton[index=true]:focus{background:#E5EFFF;color:#2563EB;}"
    "QToolButton[index=true]:disabled{color:#D1D7DF;}"
    "QLabel#ShadowDirectoryView{color:#697684;border:1px solid #E1E6EB;border-radius:5px;padding:6px 12px;font-size:12px;}"
    "QPushButton#ShadowDirectoryCreateGroup{background:#FFFFFF;border:1px solid #DFE6EE;border-radius:6px;padding:6px 12px;color:#2457B8;}" ) );
  QHBoxLayout* root = new QHBoxLayout( this );
  root->setContentsMargins( 0, 0, 0, 0 );
  root->setSpacing( 0 );
  mp_sidebar = new QWidget( this );
  mp_sidebar->setObjectName( QStringLiteral( "ShadowDirectorySidebar" ) );
  mp_sidebar->setFixedWidth( 280 );
  QVBoxLayout* categories = new QVBoxLayout( mp_sidebar );
  categories->setContentsMargins( 14, 22, 14, 18 );
  categories->setSpacing( 12 );
  QLabel* heading = new QLabel( QStringLiteral( "通讯录" ), mp_sidebar );
  heading->setObjectName( QStringLiteral( "ShadowDirectoryHeading" ) );
  categories->addWidget( heading );
  categories->addSpacing( 12 );
  const QStringList labels = QStringList() << QStringLiteral( "联系人" ) << QStringLiteral( "群组" ) << QStringLiteral( "组织架构" );
  const QStringList names = QStringList() << QStringLiteral( "ShadowDirectoryContactsButton" )
    << QStringLiteral( "ShadowDirectoryGroupsButton" ) << QStringLiteral( "ShadowDirectoryOrganizationButton" );
  for( int section = 0; section < 3; ++section )
  {
    QPushButton* button = new QPushButton( labels.at( section ), mp_sidebar );
    button->setObjectName( names.at( section ) );
    button->setProperty( "category", true );
    button->setCheckable( true );
    button->setMinimumHeight( 62 );
    button->setIconSize( QSize( 40, 40 ) );
    button->setIcon( categoryIcon( section == Contacts ? QStringLiteral( "user-circle" ) : QStringLiteral( "users-three" ),
      section == Organization ? QColor( "#F26378" ) : QColor( "#527CFF" ) ) );
    button->setAccessibleName( labels.at( section ) );
    mp_categories[section] = button;
    connect( button, &QPushButton::clicked, this, [this, section]() { setSection( static_cast<Section>( section ) ); } );
  }
  categories->addWidget( mp_categories[Groups] );
  categories->addWidget( mp_categories[Organization] );
  categories->addWidget( mp_categories[Contacts] );
  categories->addStretch();
  mp_categories[Contacts]->setChecked( true );
  root->addWidget( mp_sidebar );
  QWidget* content = new QWidget( this );
  QVBoxLayout* body = new QVBoxLayout( content );
  body->setContentsMargins( 0, 0, 0, 0 );
  body->setSpacing( 0 );
  QWidget* header = new QWidget( content );
  header->setObjectName( QStringLiteral( "ShadowDirectoryHeader" ) );
  header->setFixedHeight( 72 );
  QHBoxLayout* toolbar = new QHBoxLayout( header );
  toolbar->setContentsMargins( 20, 12, 22, 12 );
  toolbar->setSpacing( 12 );
  mp_title = new QLabel( QStringLiteral( "联系人" ), header );
  mp_title->setObjectName( QStringLiteral( "ShadowDirectoryTitle" ) );
  mp_count = new QLabel( header );
  mp_count->setObjectName( QStringLiteral( "ShadowDirectoryCount" ) );
  toolbar->addWidget( mp_title );
  toolbar->addWidget( mp_count );
  toolbar->addStretch();
  mp_createGroup = new QPushButton( QStringLiteral( "新建群聊" ), header );
  mp_createGroup->setObjectName( QStringLiteral( "ShadowDirectoryCreateGroup" ) );
  connect( mp_createGroup, &QPushButton::clicked, this, &ShadowDirectoryPage::createGroupRequested );
  toolbar->addWidget( mp_createGroup );
  mp_createGroup->hide();
  mp_sort = new QComboBox( header );
  mp_sort->setObjectName( QStringLiteral( "ShadowDirectorySort" ) );
  mp_sort->setAccessibleName( QStringLiteral( "姓名排序" ) );
  mp_sort->addItem( QStringLiteral( "昵称 · A—Z" ) );
  mp_sort->addItem( QStringLiteral( "昵称 · Z—A" ) );
  connect( mp_sort, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]() {
    m_resetScroll = true;
    refresh();
  } );
  toolbar->addWidget( mp_sort );
  QLabel* view = new QLabel( QStringLiteral( "列表视图" ), header );
  view->setObjectName( QStringLiteral( "ShadowDirectoryView" ) );
  toolbar->addWidget( view );
  body->addWidget( header );
  QWidget* listing = new QWidget( content );
  QHBoxLayout* listLayout = new QHBoxLayout( listing );
  listLayout->setContentsMargins( 0, 6, 8, 12 );
  listLayout->setSpacing( 0 );
  QVBoxLayout* entries = new QVBoxLayout;
  entries->setContentsMargins( 0, 0, 0, 0 );
  mp_list = new QTreeWidget( listing );
  mp_list->setObjectName( QStringLiteral( "ShadowDirectoryList" ) );
  mp_list->setAccessibleName( QStringLiteral( "通讯录名单，按 Enter 打开会话" ) );
  mp_list->setColumnCount( 1 );
  mp_list->setHeaderHidden( true );
  mp_list->setRootIsDecorated( false );
  mp_list->setIndentation( 0 );
  mp_list->setFrameShape( QFrame::NoFrame );
  mp_list->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  mp_list->setSelectionMode( QAbstractItemView::SingleSelection );
  mp_list->setMouseTracking( true );
  mp_list->setItemDelegate( new DirectoryDelegate( mp_list ) );
  mp_list->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( mp_list, &QTreeWidget::itemActivated, this, [this]( QTreeWidgetItem* item, int ) { activateItem( item ); } );
  connect( mp_list, &QTreeWidget::customContextMenuRequested, this, [this]( const QPoint& point ) {
    QTreeWidgetItem* item = mp_list->itemAt( point );
    if( !item || item->data( 0, KindRole ).toInt() == 0 ) return;
    const VNumber id = item->data( 0, Qt::UserRole ).toULongLong();
    const int kind = item->data( 0, KindRole ).toInt();
    QMenu menu( this );
    menu.addAction( QStringLiteral( "打开会话" ), this, [this, id, kind]() {
      if( kind == 1 ) emit userActivated( id );
      else emit groupActivated( id );
    } );
    if( kind == 1 )
      menu.addAction( QStringLiteral( "查看资料" ), this, [this, id]() { emit userDetailsRequested( id ); } );
    menu.exec( mp_list->viewport()->mapToGlobal( point ) );
  } );
  mp_empty = new QLabel( listing );
  mp_empty->setObjectName( QStringLiteral( "ShadowDirectoryEmpty" ) );
  mp_empty->setAlignment( Qt::AlignCenter );
  mp_empty->setWordWrap( true );
  entries->addWidget( mp_list, 1 );
  entries->addWidget( mp_empty, 1 );
  mp_empty->hide();
  listLayout->addLayout( entries, 1 );
  mp_alphabet = new QWidget( listing );
  mp_alphabet->setObjectName( QStringLiteral( "ShadowDirectoryAlphabet" ) );
  mp_alphabet->setFixedWidth( 26 );
  QVBoxLayout* alphabet = new QVBoxLayout( mp_alphabet );
  alphabet->setContentsMargins( 0, 6, 0, 6 );
  alphabet->setSpacing( 0 );
  alphabet->addStretch();
  const QString keys = QStringLiteral( "ABCDEFGHIJKLMNOPQRSTUVWXYZ#" );
  for( QChar key : keys )
  {
    QToolButton* button = new QToolButton( mp_alphabet );
    button->setText( QString( key ) );
    button->setProperty( "index", true );
    button->setFixedSize( 24, 17 );
    button->setAccessibleName( QStringLiteral( "跳转到 %1" ).arg( key ) );
    button->setEnabled( false );
    connect( button, &QToolButton::clicked, this, [this, key]() {
      QTreeWidgetItem* header = m_headers.value( QString( key ), Q_NULLPTR );
      if( header ) mp_list->scrollToItem( header, QAbstractItemView::PositionAtTop );
    } );
    m_indexButtons.append( button );
    alphabet->addWidget( button );
  }
  alphabet->addStretch();
  listLayout->addWidget( mp_alphabet );
  body->addWidget( listing, 1 );
  root->addWidget( content, 1 );
  refresh();
}

QString ShadowDirectoryPage::initialForName( const QString& name )
{
  const QString normalized = name.trimmed().normalized( QString::NormalizationForm_D );
  if( normalized.isEmpty() ) return QStringLiteral( "#" );
  const QChar first = normalized.at( 0 ).toUpper();
  if( first >= QLatin1Char( 'A' ) && first <= QLatin1Char( 'Z' ) ) return QString( first );
  QTextCodec* codec = QTextCodec::codecForName( "GB2312" );
  if( !codec || !codec->canEncode( first ) ) return QStringLiteral( "#" );
  const QByteArray encoded = codec->fromUnicode( QString( first ) );
  if( encoded.size() != 2 ) return QStringLiteral( "#" );
  const int code = static_cast<unsigned char>( encoded.at( 0 ) ) * 256 +
                  static_cast<unsigned char>( encoded.at( 1 ) ) - 65536;
  if( code < -20319 || code > -10247 ) return QStringLiteral( "#" );
  const int starts[] = { -20319, -20284, -19776, -19219, -18711, -18527, -18240, -17923,
    -17418, -16475, -16213, -15641, -15166, -14923, -14915, -14631, -14150, -14091,
    -13319, -12839, -12557, -11848, -11056 };
  const QString initials = QStringLiteral( "ABCDEFGHJKLMNOPQRSTWXYZ" );
  for( int index = initials.size() - 1; index >= 0; --index )
    if( code >= starts[index] ) return initials.mid( index, 1 );
  return QStringLiteral( "#" );
}

void ShadowDirectoryPage::setSection( Section section )
{
  if( section < Contacts || section > Organization ) return;
  if( m_section == section )
  {
    mp_categories[section]->setChecked( true );
    return;
  }
  m_section = section;
  for( int index = 0; index < 3; ++index ) mp_categories[index]->setChecked( index == section );
  mp_title->setText( section == Contacts ? QStringLiteral( "联系人" )
    : section == Groups ? QStringLiteral( "群组" ) : QStringLiteral( "组织架构" ) );
  mp_createGroup->setVisible( section == Groups );
  mp_alphabet->setVisible( section != Organization );
  m_resetScroll = true;
  refresh();
  emit sectionChanged( static_cast<int>( section ) );
}

void ShadowDirectoryPage::setFilter( const QString& filter )
{
  const QString query = filter.trimmed();
  if( m_filter == query ) return;
  m_filter = query;
  m_resetScroll = true;
  refresh();
}

void ShadowDirectoryPage::refresh()
{
  QVector<DirectoryEntry> entries;
  QByteArray snapshot;
  QDataStream state( &snapshot, QIODevice::WriteOnly );
  state << static_cast<int>( m_section ) << m_filter << mp_sort->currentIndex();
  const auto matches = [this]( const QString& name, const QString& detail, const QString& account ) {
    return m_filter.isEmpty() || name.contains( m_filter, Qt::CaseInsensitive ) ||
      detail.contains( m_filter, Qt::CaseInsensitive ) || account.contains( m_filter, Qt::CaseInsensitive );
  };
  if( m_section == Groups )
  {
    for( const Chat& chat : ChatManager::instance().constChatList() )
    {
      if( !chat.isValid() || !chat.isGroup() || chat.isDefault() ) continue;
      const QString detail = QStringLiteral( "%1 位成员" ).arg( chat.usersId().size() );
      if( !matches( chat.name(), detail, QString() ) ) continue;
      entries.append( DirectoryEntry{ chat.id(), 2, chat.name(), detail, initialForName( chat.name() ),
        categoryIcon( QStringLiteral( "users-three" ), QColor( "#6B8DFF" ) ) } );
      state << static_cast<quint64>( chat.id() ) << chat.name() << detail;
    }
  }
  else
  {
    for( const User& user : UserManager::instance().userList().toList() )
    {
      if( !user.isValid() || user.isLocal() ) continue;
      const QString name = user.name().trimmed().isEmpty() ? Bee::userNameToShow( user, false ) : user.name().trimmed();
      QStringList workgroups = user.workgroups();
      workgroups.removeAll( QString() );
      workgroups.removeDuplicates();
      const QString detail = workgroups.join( QStringLiteral( " · " ) );
      if( !matches( name, detail, user.accountName() + "\n" +
          user.vCard().fullName( Settings::instance().useUserFirstNameFirstInFullName() ) ) ) continue;
      const QIcon avatar = Bee::avatarForUser( user, QSize( 36, 36 ), true );
      if( m_section == Organization )
      {
        if( workgroups.isEmpty() ) workgroups << QStringLiteral( "未分组" );
        for( const QString& group : workgroups )
          entries.append( DirectoryEntry{ user.id(), 1, name, detail, group, avatar } );
      }
      else
        entries.append( DirectoryEntry{ user.id(), 1, name, detail, initialForName( name ), avatar } );
      state << static_cast<quint64>( user.id() ) << name << detail << user.accountName()
            << user.status() << user.color() << user.hash() << user.vCard().photoHash();
    }
  }
  if( snapshot == m_snapshot && !m_resetScroll ) return;
  m_snapshot = snapshot;
  QCollator collator( QLocale( QLocale::Chinese, QLocale::China ) );
  collator.setNumericMode( true );
  collator.setCaseSensitivity( Qt::CaseInsensitive );
  const bool descending = mp_sort->currentIndex() == 1;
  std::sort( entries.begin(), entries.end(), [&collator, descending]( const DirectoryEntry& left, const DirectoryEntry& right ) {
    if( left.key == "#" && right.key != "#" ) return false;
    if( left.key != "#" && right.key == "#" ) return true;
    int comparison = collator.compare( left.key, right.key );
    if( comparison == 0 ) comparison = collator.compare( left.name, right.name );
    if( comparison == 0 ) return left.id < right.id;
    return descending ? comparison > 0 : comparison < 0;
  } );
  const int scroll = m_resetScroll ? 0 : mp_list->verticalScrollBar()->value();
  const QTreeWidgetItem* selected = mp_list->currentItem();
  const VNumber selectedId = selected ? selected->data( 0, Qt::UserRole ).toULongLong() : ID_INVALID;
  const QString selectedKey = selected ? selected->data( 0, KeyRole ).toString() : QString();
  mp_list->setUpdatesEnabled( false );
  mp_list->clear();
  m_headers.clear();
  QString previousKey;
  for( const DirectoryEntry& entry : entries )
  {
    if( entry.key != previousKey )
    {
      QTreeWidgetItem* header = new QTreeWidgetItem( mp_list, QStringList( entry.key ) );
      header->setData( 0, KindRole, 0 );
      header->setData( 0, KeyRole, entry.key );
      header->setFlags( Qt::ItemIsEnabled );
      m_headers.insert( entry.key, header );
      previousKey = entry.key;
    }
    QTreeWidgetItem* row = new QTreeWidgetItem( mp_list, QStringList( entry.name ) );
    row->setData( 0, Qt::UserRole, QVariant::fromValue<qulonglong>( entry.id ) );
    row->setData( 0, KindRole, entry.kind );
    row->setData( 0, KeyRole, entry.key );
    row->setData( 0, DetailRole, entry.detail );
    row->setIcon( 0, entry.avatar );
    row->setToolTip( 0, entry.name + (entry.detail.isEmpty() ? QString() : "\n" + entry.detail) +
      QStringLiteral( "\n双击或按 Enter 打开会话，右键查看资料" ) );
    row->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    if( entry.id == selectedId && entry.key == selectedKey ) mp_list->setCurrentItem( row );
  }
  mp_list->doItemsLayout();
  mp_list->verticalScrollBar()->setValue( scroll );
  mp_list->setUpdatesEnabled( true );
  m_resetScroll = false;
  for( QToolButton* button : m_indexButtons ) button->setEnabled( m_headers.contains( button->text() ) );
  QSet<VNumber> identities;
  for( const DirectoryEntry& entry : entries ) identities.insert( entry.id );
  mp_count->setText( QStringLiteral( "%1 %2" ).arg( identities.size() ).arg( m_section == Groups ? QStringLiteral( "个群组" ) : QStringLiteral( "位联系人" ) ) );
  mp_empty->setText( !m_filter.isEmpty() ? QStringLiteral( "没有找到匹配结果，试试其他姓名或组织名称。" )
    : m_section == Groups ? QStringLiteral( "还没有群组，可点击“新建群聊”邀请同事。" )
    : QStringLiteral( "还没有联系人，连接局域网后会在这里显示发现的同事。" ) );
  mp_empty->setVisible( entries.isEmpty() );
  mp_list->setVisible( !entries.isEmpty() );
}

void ShadowDirectoryPage::activateItem( QTreeWidgetItem* item )
{
  if( !item ) return;
  const VNumber id = item->data( 0, Qt::UserRole ).toULongLong();
  if( item->data( 0, KindRole ).toInt() == 1 ) emit userActivated( id );
  else if( item->data( 0, KindRole ).toInt() == 2 ) emit groupActivated( id );
}

void ShadowDirectoryPage::resizeEvent( QResizeEvent* event )
{
  QWidget::resizeEvent( event );
  mp_sidebar->setFixedWidth( qBound( 210, width() * 24 / 100, 336 ) );
  const int buttonHeight = qBound( 10, (height() - 120) / 27, 17 );
  for( QToolButton* button : m_indexButtons ) button->setFixedHeight( buttonHeight );
}
