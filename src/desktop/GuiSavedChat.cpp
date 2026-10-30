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
// $Id: GuiSavedChat.cpp 1534 2021-12-17 12:23:23Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "BeeUtils.h"
#include "ChatManager.h"
#include "FileDialog.h"
#include "GuiChatItem.h"
#include "GuiChatMessage.h"
#include "GuiSavedChat.h"
#include "IconManager.h"
#include "Settings.h"
#include "ShortcutManager.h"
#include <QSplitter>
#include <QTabBar>
#include <QListWidget>
#include <QDesktopServices>
#include "ShadowImageViewer.h"
#include "ShadowStyle.h"
#include "ShadowDialogStyle.h"
#include "UserManager.h"
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QAbstractTextDocumentLayout>
#include <QtMath>
#include <algorithm>

namespace
{
enum HistoryRole
{
  SenderRole = Qt::UserRole + 2,
  TimeRole,
  PreviewRole,
  AvatarRole,
  ThumbnailRole,
  ResultKeyRole
};

struct HistoryReadingState
{
  QString selectedKey;
  QString visibleKey;
  int visibleTop = 0;
  int listVertical = 0;
  int listHorizontal = 0;
  int sourceVertical = 0;
  int sourceHorizontal = 0;
  int page = 0;
};

HistoryReadingState readingState( QListWidget* results, QTextBrowser* source, QStackedWidget* pages )
{
  HistoryReadingState state;
  if( results->currentItem() && results->currentItem()->isSelected() )
    state.selectedKey = results->currentItem()->data( ResultKeyRole ).toString();
  state.listVertical = results->verticalScrollBar()->value();
  state.listHorizontal = results->horizontalScrollBar()->value();
  state.sourceVertical = source->verticalScrollBar()->value();
  state.sourceHorizontal = source->horizontalScrollBar()->value();
  state.page = pages ? pages->currentIndex() : 0;
  for( int row = 0; row < results->count(); ++row )
  {
    QListWidgetItem* item = results->item( row );
    const QRect area = results->visualItemRect( item );
    if( !area.intersects( results->viewport()->rect() ) )
      continue;
    state.visibleKey = item->data( ResultKeyRole ).toString();
    state.visibleTop = area.top();
    break;
  }
  return state;
}

void restoreReadingState( const HistoryReadingState& state, QListWidget* results,
  QTextBrowser* source, QStackedWidget* pages, QPushButton* contextButton )
{
  const QSignalBlocker selectionBlocked( results );
  QListWidgetItem* selected = Q_NULLPTR;
  QListWidgetItem* visible = Q_NULLPTR;
  for( int row = 0; row < results->count(); ++row )
  {
    QListWidgetItem* item = results->item( row );
    const QString key = item->data( ResultKeyRole ).toString();
    if( !state.selectedKey.isEmpty() && key == state.selectedKey )
      selected = item;
    if( !state.visibleKey.isEmpty() && key == state.visibleKey )
      visible = item;
  }
  if( selected )
    results->setCurrentItem( selected );
  if( contextButton )
    contextButton->setEnabled( selected != Q_NULLPTR );
  if( pages )
    pages->setCurrentIndex( !state.selectedKey.isEmpty() && !selected ? 0 : state.page );
  results->doItemsLayout();
  results->verticalScrollBar()->setValue( state.listVertical );
  if( visible )
  {
    results->scrollToItem( visible, QAbstractItemView::PositionAtTop );
    results->verticalScrollBar()->setValue( results->verticalScrollBar()->value() +
      results->visualItemRect( visible ).top() - state.visibleTop );
  }
  results->horizontalScrollBar()->setValue( state.listHorizontal );
  source->document()->documentLayout()->documentSize();
  source->verticalScrollBar()->setValue( state.sourceVertical );
  source->horizontalScrollBar()->setValue( state.sourceHorizontal );
}

class HistoryResultDelegate : public QStyledItemDelegate
{
public:
  explicit HistoryResultDelegate( QObject* parent ) : QStyledItemDelegate( parent ) {}

  QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const override
  {
    if( !index.data( SenderRole ).isValid() )
      return QStyledItemDelegate::sizeHint( option, index );
    return QSize( 260, index.data( ThumbnailRole ).value<QPixmap>().isNull() ? 94 : 128 );
  }

  void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
  {
    if( !index.data( SenderRole ).isValid() )
    {
      QStyledItemDelegate::paint( painter, option, index );
      return;
    }
    painter->save();
    painter->setRenderHint( QPainter::Antialiasing );
    const QRect area = option.rect.adjusted( 4, 2, -4, -2 );
    if( option.state & (QStyle::State_Selected | QStyle::State_MouseOver) )
    {
      painter->setPen( Qt::NoPen );
      painter->setBrush( QColor( option.state & QStyle::State_Selected ? "#EAF2FF" : "#F5F8FC" ) );
      painter->drawRoundedRect( area, 8, 8 );
    }
    const QString sender = index.data( SenderRole ).toString();
    const QRect avatar( area.left() + 8, area.top() + 12, 32, 32 );
    const QPixmap portrait = index.data( AvatarRole ).value<QPixmap>();
    if( portrait.isNull() )
    {
      painter->setPen( Qt::NoPen );
      painter->setBrush( QColor( "#E3EDFC" ) );
      painter->drawEllipse( avatar );
      painter->setPen( QColor( "#4177BB" ) );
      painter->drawText( avatar, Qt::AlignCenter, sender.left( 1 ) );
    }
    else
      painter->drawPixmap( avatar, portrait );
    const int contentLeft = avatar.right() + 10;
    const int contentWidth = qMax( 40, area.right() - contentLeft - 10 );
    QFont smallFont = option.font;
    smallFont.setPixelSize( 11 );
    painter->setFont( smallFont );
    const QString time = index.data( TimeRole ).toString();
    const int timeWidth = QFontMetrics( smallFont ).horizontalAdvance( time );
    painter->setPen( QColor( "#8693A5" ) );
    painter->drawText( QRect( area.right() - timeWidth - 10, area.top() + 10, timeWidth, 20 ), Qt::AlignRight | Qt::AlignVCenter, time );
    painter->setPen( QColor( "#61728A" ) );
    painter->drawText( QRect( contentLeft, area.top() + 10, qMax( 20, contentWidth - timeWidth - 8 ), 20 ),
      Qt::AlignVCenter, QFontMetrics( smallFont ).elidedText( sender, Qt::ElideRight, qMax( 20, contentWidth - timeWidth - 8 ) ) );
    QRect detail( contentLeft, area.top() + 36, contentWidth, area.height() - 46 );
    const QPixmap thumbnail = index.data( ThumbnailRole ).value<QPixmap>();
    if( !thumbnail.isNull() )
    {
      const QRect imageRect( contentLeft, detail.top(), 64, 64 );
      painter->save();
      QPainterPath clip;
      clip.addRoundedRect( imageRect, 6, 6 );
      painter->setClipPath( clip );
      const QPixmap cropped = thumbnail.scaled(imageRect.size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
      painter->drawPixmap(imageRect.center()-QPoint(cropped.width()/2,cropped.height()/2),cropped);
      painter->restore();
      detail.setLeft( imageRect.right() + 10 );
    }
    painter->setFont( option.font );
    painter->setPen( QColor( "#243247" ) );
    QTextLayout text( index.data( PreviewRole ).toString(), option.font );
    QTextOption textOption;
    textOption.setWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
    text.setTextOption( textOption );
    text.beginLayout();
    int lineTop = detail.top();
    while( lineTop < detail.bottom() - QFontMetrics( option.font ).height() )
    {
      QTextLine line = text.createLine();
      if( !line.isValid() ) break;
      line.setLineWidth( qMax( 20, detail.width() ) );
      line.setPosition( QPointF( detail.left(), lineTop ) );
      lineTop += qCeil( line.height() );
    }
    text.endLayout();
    painter->setClipRect( detail );
    text.draw( painter, QPointF() );
    painter->restore();
  }
};
}

GuiSavedChat::GuiSavedChat( QWidget* parent, bool embedded )
 : QMainWindow( parent ), mp_leFilter(Q_NULLPTR), mp_results(Q_NULLPTR),
   mp_emptyState(Q_NULLPTR), mp_resultsStack(Q_NULLPTR), mp_contextButton(Q_NULLPTR), m_embedded(embedded)
{
  setObjectName( "GuiSavedChat" );
  if( !m_embedded ) ShadowDialogStyle::apply(this);
  setStyleSheet(styleSheet()+QStringLiteral("QListWidget::item { padding: 10px 12px; border-bottom: 1px solid #E7EBF0; } QTabBar::tab { padding: 11px 18px; border: 0; border-bottom: 2px solid transparent; color: #697B94; background: white; } QTabBar::tab:selected { color: #2463EB; border-bottom-color: #2463EB; } QComboBox { min-height: 24px; } QToolBar { background: white; border: 0; padding: 12px 20px; spacing: 8px; } QToolButton { padding: 7px 10px; border: 0; border-radius: 6px; color: #52647F; } QToolButton:hover { background: #EAF1FF; } QTextBrowser { border: 1px solid #E3E8F0; border-radius: 8px; background: white; padding: 10px; }"));
  Bee::removeContextHelpButton( this );
  setWindowFlags( windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint );
  setWindowIcon( IconManager::instance().icon( "saved-chat.png" ) );
  if( m_embedded )
  {
    setWindowFlags( Qt::Widget );
    setMinimumWidth( 320 );
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setStyleSheet( QStringLiteral(
      "GuiSavedChat { background:white; border-left:1px solid #E5EAF1; }"
      "QWidget#ShadowHistorySurface { background:white; }"
      "QLabel { color:#66768D; background:transparent; }"
      "QLabel#ShadowHistoryTitle { color:#17243A; font-size:17px; font-weight:600; }"
      "QLabel#ShadowHistoryConversation { color:#8A97A8; font-size:11px; }"
      "QTabBar::tab { background:white; color:#52647F; padding:10px 8px; border:0; border-bottom:2px solid transparent; }"
      "QTabBar::tab:selected { color:#2877EF; border-bottom-color:#2877EF; }"
      "QLineEdit { background:white; border:1px solid #DFE5ED; border-radius:6px; padding:7px 10px; min-height:18px; }"
      "QLineEdit:focus { border-color:#609BF3; }"
      "QComboBox { background:white; color:#52647F; border:1px solid #DFE5ED; border-radius:5px; padding:5px 20px 5px 6px; min-width:0; min-height:18px; font-size:11px; }"
      "QComboBox::drop-down { border:0; width:18px; }"
      "QListWidget { background:white; border:0; outline:0; padding:0; }"
      "QListWidget::item { border:0; padding:3px; border-radius:8px; }"
      "QListWidget::item:selected { color:#243247; background:#EAF2FF; }"
      "QTextBrowser { border:0; background:white; padding:4px; }"
      "QToolButton { border:0; border-radius:5px; padding:4px; background:transparent; }"
      "QToolButton:hover { background:#EDF3FC; }"
      "QPushButton { color:#2877EF; background:white; border:0; padding:7px; }"
      "QPushButton:disabled { color:#A3ADBC; }" ) );
  }

  mp_teSavedChat = new QTextBrowser( this );
  mp_teSavedChat->setToolTip( tr( "Right click to open menu" ) );
  QPalette p = mp_teSavedChat->palette();
  p.setColor( QPalette::Highlight, Qt::yellow );
  p.setColor( QPalette::HighlightedText, Qt::black );
  mp_teSavedChat->setPalette( p );
  mp_teSavedChat->setFocusPolicy( Qt::StrongFocus ); // need focus for keyboard events like CTRL+c
  mp_teSavedChat->setReadOnly( true );
  mp_teSavedChat->setUndoRedoEnabled( false );
  mp_teSavedChat->setOpenExternalLinks( false );
  mp_teSavedChat->setOpenLinks( false );
  mp_teSavedChat->setAcceptRichText( false );
  mp_teSavedChat->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( mp_teSavedChat, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( customContextMenu( const QPoint& ) ) );
  connect( mp_teSavedChat, SIGNAL( anchorClicked( const QUrl& ) ), this, SLOT( checkAnchorClicked( const QUrl&  ) ) );

  QWidget* panel = new QWidget(this);
  panel->setObjectName( QStringLiteral("ShadowHistorySurface") );
  QVBoxLayout* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(m_embedded?14:24,16,m_embedded?14:24,m_embedded?10:24);
  layout->setSpacing(12);
  QLabel* heading=new QLabel(QStringLiteral("聊天记录"),panel);heading->setProperty("role","title");
  heading->setObjectName( QStringLiteral("ShadowHistoryTitle") );
  QHBoxLayout* headingLayout = new QHBoxLayout;
  headingLayout->addWidget( heading );
  headingLayout->addStretch();
  QToolButton* actions = new QToolButton(panel);
  actions->setAccessibleName(QStringLiteral("聊天记录操作"));
  actions->setIcon(ShadowStyle::actionIcon(QStringLiteral("list")));
  QMenu* actionsMenu = new QMenu(actions);
  actionsMenu->addAction(QStringLiteral("导出 PDF"),this,SLOT(saveHistoryAs()));
  actionsMenu->addAction(QStringLiteral("打印"),this,SLOT(printChat()));
  actionsMenu->addAction(QStringLiteral("清空记录"),this,SLOT(deleteSavedChat()));
  actions->setMenu(actionsMenu);
  actions->setPopupMode(QToolButton::InstantPopup);
  actions->setVisible(m_embedded);
  headingLayout->addWidget(actions);
  QToolButton* closeButton = new QToolButton(panel);
  closeButton->setObjectName(QStringLiteral("ShadowHistoryClose"));
  closeButton->setIcon(ShadowStyle::actionIcon(QStringLiteral("x")));
  closeButton->setAccessibleName(QStringLiteral("关闭聊天记录"));
  closeButton->setToolTip(QStringLiteral("关闭聊天记录"));
  closeButton->setFixedSize(28,28);
  closeButton->setVisible(m_embedded);
  connect(closeButton,&QToolButton::clicked,this,&QWidget::close);
  headingLayout->addWidget(closeButton);
  layout->addLayout(headingLayout);
  QLabel* conversation = new QLabel(panel);
  conversation->setObjectName(QStringLiteral("ShadowHistoryConversation"));
  conversation->setVisible(m_embedded);
  layout->addWidget(conversation);
  mp_categories = new QTabBar(panel);
  mp_categories->addTab(QString::fromUtf8("消息"));
  mp_categories->addTab(QString::fromUtf8("文件"));
  mp_categories->addTab(QString::fromUtf8("图片和视频"));
  mp_categories->addTab(QString::fromUtf8("链接"));
  mp_categories->setExpanding(false);
  layout->addWidget(mp_categories);
  if( m_embedded )
  {
    mp_leFilter = new QLineEdit(panel);
    mp_leFilter->setObjectName("GuiLineEditFilterHistory");
    mp_leFilter->setPlaceholderText(QStringLiteral("搜索消息、文件名或链接"));
    mp_leFilter->setAccessibleName(QStringLiteral("搜索聊天记录"));
    mp_leFilter->setClearButtonEnabled(true);
    layout->addWidget(mp_leFilter);
    connect(mp_leFilter,&QLineEdit::textChanged,this,&GuiSavedChat::filterHistory);
    connect(mp_leFilter,&QLineEdit::returnPressed,this,&GuiSavedChat::showHistoryContext);
  }
  QHBoxLayout* filters = new QHBoxLayout;
  mp_senderFilter = new QComboBox(panel); mp_senderFilter->setAccessibleName(QString::fromUtf8("发送者"));
  mp_dateFilter = new QComboBox(panel); mp_dateFilter->setAccessibleName(QString::fromUtf8("时间范围"));
  mp_dateFilter->addItems(QStringList() << QString::fromUtf8("全部时间") << QString::fromUtf8("今天") << QString::fromUtf8("近 7 天") << QString::fromUtf8("近 30 天"));
  mp_sortOrder = new QComboBox(panel); mp_sortOrder->addItems(QStringList() << QString::fromUtf8("最新在前") << QString::fromUtf8("最早在前"));
  mp_sortOrder->setAccessibleName(QStringLiteral("排序"));
  filters->setSpacing(6);
  for( QComboBox* filter : QList<QComboBox*>{mp_senderFilter,mp_dateFilter,mp_sortOrder} )
  {
    filter->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    filter->setMinimumContentsLength(3);
    filter->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    filters->addWidget(filter,1);
  }
  layout->addLayout(filters);
  mp_resultCount = new QLabel(panel);mp_resultCount->setProperty("role","hint");layout->addWidget(mp_resultCount);
  QSplitter* split = new QSplitter(Qt::Vertical,panel);
  mp_results = new QListWidget(split); mp_results->setWordWrap(true);mp_results->setMinimumHeight(180);
  mp_results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  mp_results->setToolTip(QString::fromUtf8("选择结果，在下方定位原始聊天；点击原文中的文件或链接打开。"));
  mp_results->setObjectName(QStringLiteral("ShadowHistoryResults"));
  split->addWidget(mp_results);split->addWidget(mp_teSavedChat);split->setHandleWidth(12);
  if( m_embedded )
  {
    mp_resultsStack = new QStackedWidget(panel);
    mp_resultsStack->setObjectName(QStringLiteral("ShadowHistoryPages"));
    QWidget* resultsPage = new QWidget(mp_resultsStack);
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsPage);
    resultsLayout->setContentsMargins(0,0,0,0);
    mp_emptyState = new QLabel(QStringLiteral("暂无聊天记录"),resultsPage);
    mp_emptyState->setObjectName(QStringLiteral("ShadowHistoryEmpty"));
    mp_emptyState->setAlignment(Qt::AlignCenter);
    mp_emptyState->setWordWrap(true);
    resultsLayout->addWidget(mp_emptyState);
    resultsLayout->addWidget(mp_results,1);
    mp_results->setMouseTracking(true);
    mp_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mp_results->setItemDelegate(new HistoryResultDelegate(mp_results));
    mp_results->setToolTip(QStringLiteral("选择记录后可查看上下文；双击文件、图片或链接打开。"));
    mp_contextButton = new QPushButton(QStringLiteral("查看选中消息的上下文"),resultsPage);
    mp_contextButton->setObjectName(QStringLiteral("ShadowHistoryContext"));
    mp_contextButton->setEnabled(false);
    connect(mp_contextButton,&QPushButton::clicked,this,&GuiSavedChat::showHistoryContext);
    resultsLayout->addWidget(mp_contextButton);
    QWidget* sourcePage = new QWidget(mp_resultsStack);
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourcePage);
    sourceLayout->setContentsMargins(0,0,0,0);
    QPushButton* back = new QPushButton(QStringLiteral("返回结果"),sourcePage);
    back->setObjectName(QStringLiteral("ShadowHistoryBack"));
    connect(back,&QPushButton::clicked,this,[this]() {mp_resultsStack->setCurrentIndex(0);});
    sourceLayout->addWidget(back,0,Qt::AlignLeft);
    sourceLayout->addWidget(mp_teSavedChat,1);
    mp_resultsStack->addWidget(resultsPage);
    mp_resultsStack->addWidget(sourcePage);
    layout->addWidget(mp_resultsStack,1);
    delete split;
  }
  else
    layout->addWidget(split);
  setCentralWidget(panel);resize(850,700);
  connect(mp_categories,SIGNAL(currentChanged(int)),this,SLOT(filterHistory()));
  connect(mp_senderFilter,SIGNAL(currentIndexChanged(int)),this,SLOT(filterHistory()));
  connect(mp_dateFilter,SIGNAL(currentIndexChanged(int)),this,SLOT(filterHistory()));
  connect(mp_sortOrder,SIGNAL(currentIndexChanged(int)),this,SLOT(filterHistory()));
  connect(mp_results,SIGNAL(itemSelectionChanged()),this,SLOT(locateHistoryResult()));
  connect(mp_results,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item) {
    if(!item->data(Qt::UserRole+1).toString().isEmpty()){checkAnchorClicked(QUrl(item->data(Qt::UserRole+1).toString()));return;}
    const HistoryRecord& record=m_records[item->data(Qt::UserRole).toInt()];
    const QStringList targets=mp_categories->currentIndex()==1?record.files:mp_categories->currentIndex()==2?record.media:record.links;
    if(!targets.isEmpty())checkAnchorClicked(QUrl(targets.first()));
  });
  mp_results->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(mp_results,&QListWidget::customContextMenuRequested,this,[this](const QPoint& position) {
    QListWidgetItem* item=mp_results->itemAt(position);if(!item)return;
    const HistoryRecord& record=m_records[item->data(Qt::UserRole).toInt()];
    const QStringList local=item->data(Qt::UserRole+1).toString().isEmpty()?record.files+record.media:QStringList()<<item->data(Qt::UserRole+1).toString();
    if(local.isEmpty()&&record.links.isEmpty())return;
    QMenu menu(this);QAction* folder=local.isEmpty()?Q_NULLPTR:menu.addAction(QString::fromUtf8("打开所在文件夹"));
    QAction* copyLink=record.links.isEmpty()?Q_NULLPTR:menu.addAction(QString::fromUtf8("复制链接"));
    QAction* chosen=menu.exec(mp_results->viewport()->mapToGlobal(position));
    if(chosen&&chosen==folder)QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(QUrl(local.first()).toLocalFile()).absolutePath()));
    if(chosen&&chosen==copyLink)QApplication::clipboard()->setText(record.links.join("\n"));
  });

  mp_barHistory = new QToolBar( tr( "Show the toolbar" ), this );
  mp_barHistory->setObjectName( "GuiHistoryToolBar" );
  addToolBar( Qt::TopToolBarArea, mp_barHistory );
  mp_barHistory->setAllowedAreas( Qt::BottomToolBarArea | Qt::TopToolBarArea );
  mp_barHistory->setFloatable( false );
  mp_barHistory->setIconSize( Settings::instance().mainBarIconSize() );
  if( !m_embedded ) setupToolBar( mp_barHistory );
  else mp_barHistory->hide();
  mp_barHistory->toggleViewAction()->setVisible( false );

  mp_menuContext = new QMenu( this );
}

void GuiSavedChat::setupToolBar( QToolBar* bar )
{
  /* filter by keywords */
  QLabel* label = new QLabel( bar );
  label->setObjectName( "GuiLabelFilterTextHistory" );
  label->setAlignment( Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter );
  label->setText( QString::fromUtf8("  搜索 ") );
  bar->addWidget( label );
  mp_leFilter = new QLineEdit( bar );
  mp_leFilter->setObjectName( "GuiLineEditFilterHistory" );
#if QT_VERSION >= 0x040700
  mp_leFilter->setPlaceholderText( QString::fromUtf8("搜索消息、文件名或链接") );
  mp_leFilter->setMinimumWidth(240);
#endif
  bar->addWidget( mp_leFilter );
  connect( mp_leFilter, SIGNAL( returnPressed() ), this, SLOT( findTextInHistory() ) );
  connect( mp_leFilter, SIGNAL( textChanged(QString) ), this, SLOT(filterHistory()) );

  /* search button */
  bar->addAction( ShadowStyle::actionIcon( QStringLiteral("magnifying-glass") ), QString::fromUtf8("查找原文"), this, SLOT( findTextInHistory() ) );

  /* flags */
  mp_cbCaseSensitive = new QCheckBox( bar );
  mp_cbCaseSensitive->setObjectName( "GuiCheckBoxFindCaseSensitiveInLog" );
  mp_cbCaseSensitive->setText( tr( "Case sensitive" ) );
  bar->addWidget( mp_cbCaseSensitive )->setVisible(false);
  mp_cbCaseSensitive->hide();

  mp_cbWholeWordOnly = new QCheckBox( bar );
  mp_cbWholeWordOnly->setObjectName( "GuiCheckBoxFindWholeWordOnlyInLog" );
  mp_cbWholeWordOnly->setText( tr( "Whole word" ) );
  bar->addWidget( mp_cbWholeWordOnly )->setVisible(false);
  mp_cbWholeWordOnly->hide();

  /* save as button */
  bar->addSeparator();
  bar->addAction( QIcon(), QStringLiteral("导出 PDF"), this, SLOT( saveHistoryAs() ) );
  bar->addSeparator();
  bar->addAction( QIcon(), QStringLiteral("打印"), this, SLOT( printChat() ) );
  bar->addSeparator();
  bar->addAction( QIcon(), QStringLiteral("清空记录"), this, SLOT( deleteSavedChat() ) );
}

void GuiSavedChat::setSearchText( const QString& text )
{
  mp_leFilter->setText( text );
  mp_categories->setCurrentIndex( 0 );
  filterHistory();
  mp_teSavedChat->moveCursor( QTextCursor::Start );
  mp_teSavedChat->find( text );
}

void GuiSavedChat::showSavedChat( const QString& chat_name )
{
  const bool changed = m_savedChatName != chat_name;
  const HistoryReadingState previousState = changed ? HistoryReadingState()
    : readingState( mp_results, mp_teSavedChat, mp_resultsStack );
  const QString selectedSender = changed ? QString() : mp_senderFilter->currentData().toString();
  if( changed && m_embedded )
  {
    const QSignalBlocker keywordBlocked(mp_leFilter);
    const QSignalBlocker categoryBlocked(mp_categories);
    const QSignalBlocker dateBlocked(mp_dateFilter);
    const QSignalBlocker sortBlocked(mp_sortOrder);
    mp_leFilter->clear();
    mp_categories->setCurrentIndex(0);
    mp_dateFilter->setCurrentIndex(0);
    mp_sortOrder->setCurrentIndex(0);
  }
  m_savedChatName = chat_name;
  QString window_chat_title = chat_name == Settings::instance().defaultChatName() ? GuiChatItem::defaultChatName().toUpper() : chat_name;
  setWindowTitle( QString::fromUtf8("%1 - 聊天记录").arg(window_chat_title) );
  findChild<QLabel*>("ShadowHistoryConversation")->setText(window_chat_title);

  QString html_text = "";
  if( ChatManager::instance().chatHasSavedText( chat_name ) )
    html_text += ChatManager::instance().chatSavedText( chat_name );

  const Chat liveChat=ChatManager::instance().findChatByName(chat_name);
  if(liveChat.isValid()) html_text += GuiChatMessage::chatToHtml(liveChat,false,true,true,true,false,true);

  m_records = shadowChatHistoryRecords(html_text);
  m_recordKeys.clear();
  QHash<QString,int> occurrences;
  for( const HistoryRecord& record : m_records )
  {
    QByteArray identity;
    QDataStream stream( &identity, QIODevice::WriteOnly );
    stream << record.sender << record.timestamp << record.album << record.html;
    const QString digest = QString::fromLatin1(
      QCryptographicHash::hash( identity, QCryptographicHash::Sha256 ).toHex() );
    const int occurrence = occurrences.value( digest );
    occurrences.insert( digest, occurrence + 1 );
    m_recordKeys.append( digest + QLatin1Char(':') + QString::number( occurrence ) );
  }
  mp_senderFilter->blockSignals(true);mp_senderFilter->clear();mp_senderFilter->addItem(QString::fromUtf8("全部发送者"),QString());
  QStringList senders;
  QString indexed;
  for(int i=0;i<m_records.size();++i) {
    const HistoryRecord& r=m_records[i];
    if(!r.sender.isEmpty() && !senders.contains(r.sender)) senders << r.sender;
    indexed += QString("<a name=\"history-%1\"></a>").arg(i)+r.html+"<br>";
  }
  senders.sort();foreach(const QString& sender,senders) mp_senderFilter->addItem(sender,sender);
  const int senderIndex = mp_senderFilter->findData(selectedSender);
  mp_senderFilter->setCurrentIndex(qMax(0,senderIndex));
  mp_senderFilter->blockSignals(false);
  html_text=indexed;
  bool updates_enabled = mp_teSavedChat->updatesEnabled();
  mp_teSavedChat->setUpdatesEnabled( false );
  mp_teSavedChat->setText( html_text );
  mp_teSavedChat->setUpdatesEnabled( updates_enabled );
  rebuildHistoryResults( changed );
  if( !changed )
    restoreReadingState( previousState, mp_results, mp_teSavedChat, mp_resultsStack, mp_contextButton );
  /* Performance issue: do not set the bar to maximum */
}

void GuiSavedChat::customContextMenu( const QPoint& )
{
  mp_menuContext->clear();
  mp_menuContext->addAction( IconManager::instance().icon( "select-all.png" ), tr( "Select All" ), mp_teSavedChat, SLOT( selectAll() ), QKeySequence::SelectAll );
  mp_menuContext->addSeparator();
  mp_menuContext->addSeparator();
  if( !mp_teSavedChat->textCursor().selectedText().isEmpty() )
  {
    mp_menuContext->addAction( IconManager::instance().icon( "copy.png" ), tr( "Copy to clipboard" ), mp_teSavedChat, SLOT( copy() ), QKeySequence::Copy );
    mp_menuContext->addSeparator();
    mp_menuContext->addAction( IconManager::instance().icon( "network.png" ), tr( "Open selected text as url" ), this, SLOT( openSelectedTextAsUrl() ) );
  }
  mp_menuContext->exec( QCursor::pos() );
}

void GuiSavedChat::printChat()
{
  QPrinter printer( QPrinter::HighResolution );
  printer.setFullPage( true );
  QPrintDialog *dlg = new QPrintDialog( &printer, this );
  dlg->setOptions( QAbstractPrintDialog::PrintSelection | QAbstractPrintDialog::PrintPageRange |
                   QAbstractPrintDialog::PrintShowPageSize |  QAbstractPrintDialog::PrintCollateCopies |
#if QT_VERSION >= 0x040700
                   QAbstractPrintDialog::PrintCurrentPage |
#endif
                   QAbstractPrintDialog::PrintToFile );

  if( dlg->exec() == QDialog::Accepted)
    mp_teSavedChat->print( dlg->printer() );
  dlg->deleteLater();
}

void GuiSavedChat::openSelectedTextAsUrl()
{
  QString selected_text = mp_teSavedChat->textCursor().selectedText();
  if( !selected_text.isEmpty() )
  {
#ifdef BEEBEEP_DEBUG
    qDebug() << "Try to open selected text as url:" << selected_text;
#endif
    QUrl url = QUrl::fromUserInput( selected_text );
    emit openUrl( url );
  }
}

void GuiSavedChat::deleteSavedChat()
{
  emit deleteSavedChatRequest( m_savedChatName );
  close();
}

void GuiSavedChat::checkAnchorClicked( const QUrl& url )
{
  if(url.isLocalFile() && !QFileInfo::exists(url.toLocalFile())) {
    QMessageBox::information(this,Settings::instance().programName(),QString::fromUtf8("本地文件已被移动或删除，无法打开。"));return;
  }
  if(url.isLocalFile() && ShadowImageViewer::supports(url.toLocalFile())) {
    QStringList images;
    // Restrict batch actions to the selected record. Never save unrelated
    // images elsewhere in the conversation under an "entire batch" action.
    QString album;
    foreach(const HistoryRecord& record,m_records)if(record.media.contains(url.toString())){album=record.album;break;}
    foreach(const HistoryRecord& record,m_records) {
      if(!album.isEmpty()&&record.album!=album)continue;
      if(album.isEmpty()&&!record.media.contains(url.toString()))continue;
      foreach(const QString& address,record.media) {
      const QString path=QUrl(address).toLocalFile();if(ShadowImageViewer::supports(path) && !images.contains(path))images<<path;
      }
      if(album.isEmpty())break;
    }
    if(!images.contains(url.toLocalFile()))images<<url.toLocalFile();
    ShadowImageViewer viewer(images,url.toLocalFile(),this);viewer.exec();return;
  }
  emit openUrl( url );
}

void GuiSavedChat::filterHistory()
{
  rebuildHistoryResults( true );
}

void GuiSavedChat::rebuildHistoryResults( bool resetReadingState )
{
  if(!mp_results || !mp_leFilter) return;
  const QSignalBlocker selectionBlocked(mp_results);
  if(resetReadingState && mp_resultsStack) mp_resultsStack->setCurrentIndex(0);
  if(mp_contextButton) mp_contextButton->setEnabled(false);
  mp_results->clear();
  const QString keyword=mp_leFilter->text().trimmed();
  const QString sender=mp_senderFilter->currentData().toString();
  const int category=mp_categories->currentIndex();
  mp_results->setViewMode(category==2?QListView::IconMode:QListView::ListMode);
  mp_results->setResizeMode(QListView::Adjust);
  mp_results->setMovement(QListView::Static);
  mp_results->setGridSize(category==2?QSize(180,175):QSize());
  mp_results->setIconSize(category==2?QSize(128,88):QSize(32,32));
  const int days=mp_dateFilter->currentIndex()==1?0:mp_dateFilter->currentIndex()==2?6:29;
  int unknown=0;
  QSet<QString> shownMedia;
  QList<int> recordOrder;
  for(int index=0;index<m_records.size();++index)recordOrder.append(index);
  std::stable_sort(recordOrder.begin(),recordOrder.end(),[this](int left,int right) {
    const QDateTime leftTime=m_records[left].timestamp;
    const QDateTime rightTime=m_records[right].timestamp;
    if(leftTime.isValid()!=rightTime.isValid())return leftTime.isValid();
    if(leftTime!=rightTime)return mp_sortOrder->currentIndex()==0?leftTime>rightTime:leftTime<rightTime;
    return mp_sortOrder->currentIndex()==0?left>right:left<right;
  });
  for(int pos=0;pos<m_records.size();++pos) {
    const int i=recordOrder.at(pos);
    const HistoryRecord& r=m_records[i];
    if(!sender.isEmpty() && r.sender!=sender) continue;
    if(mp_dateFilter->currentIndex()>0 && (!r.timestamp.isValid() || r.timestamp.date()<QDate::currentDate().addDays(-days))) continue;
    QStringList targets=category==1?r.files:category==2?r.media:category==3?r.links:QStringList();
    if(category>0 && targets.isEmpty()) continue;
    const QString searchable=r.text+" "+(category==0?r.files+r.media+r.links:targets).join(" ");
    if(!keyword.isEmpty() && !searchable.contains(keyword,Qt::CaseInsensitive)) continue;
    if(category==2) {
      QStringList uniqueTargets;
      foreach(const QString& target,targets)if(!shownMedia.contains(target)){uniqueTargets<<target;shownMedia.insert(target);}
      targets=uniqueTargets;if(targets.isEmpty())continue;
    }
    if(!r.timestamp.isValid()) ++unknown;
    const QString heading=(r.sender.isEmpty()?QString::fromUtf8("发送者未记录"):r.sender)+"   "+(r.timestamp.isValid()?r.timestamp.toString("yyyy-MM-dd hh:mm:ss"):QString::fromUtf8("时间未记录"));
    if(category==2) {
      foreach(const QString& target,targets) {
        if(!keyword.isEmpty()&&!r.text.contains(keyword,Qt::CaseInsensitive)&&!target.contains(keyword,Qt::CaseInsensitive))continue;
        const QFileInfo file(QUrl(target).toLocalFile());
        const QString day=r.timestamp.isValid()?r.timestamp.toString("yyyy-MM-dd"):QString::fromUtf8("日期未记录");
        QListWidgetItem* tile=new QListWidgetItem(day+"\n"+file.fileName()+"\n"+(file.exists()?QLocale().formattedDataSize(file.size()):QString::fromUtf8("本地文件已丢失")),mp_results);
        tile->setData(Qt::UserRole,i);tile->setData(Qt::UserRole+1,target);
        tile->setData(ResultKeyRole,m_recordKeys.at(i)+QLatin1Char('\n')+target);
        tile->setToolTip(heading+"\n"+file.absoluteFilePath()+QString::fromUtf8("\n单击定位原文；双击查看。"));
        QImageReader reader(file.absoluteFilePath());const QSize size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(128,88,Qt::KeepAspectRatio));
        const QImage thumbnail=reader.read();tile->setIcon(thumbnail.isNull()?style()->standardIcon(QStyle::SP_MediaPlay):QIcon(QPixmap::fromImage(thumbnail)));
      }
      continue;
    }
    QString detail=category==0?r.text:targets.join("\n");
    if(category==1 || category==2) {
      QStringList descriptions;
      foreach(const QString& address,targets) {
        QFileInfo file(QUrl(address).toLocalFile());
        descriptions << QString("%1 · %2 · %3").arg(file.fileName(),file.suffix().toUpper(),file.exists()?QLocale().formattedDataSize(file.size()):QString::fromUtf8("本地文件已移动或删除"));
      }
      detail=descriptions.join("\n");
    }
    if(category==3 && !r.text.isEmpty())detail=r.text+"\n"+detail;
    if(detail.isEmpty() && !r.media.isEmpty()) detail=QString::fromUtf8("[图片和视频] %1").arg(QFileInfo(QUrl(r.media.first()).toLocalFile()).fileName());
    if(detail.size()>650) detail=detail.left(650)+QString::fromUtf8("…");
    const QString caption=category==2?(r.timestamp.isValid()?r.timestamp.toString("yyyy-MM-dd"):QString::fromUtf8("日期未记录"))+"\n"+detail:heading+"\n"+detail;
    QListWidgetItem* item=new QListWidgetItem(caption,mp_results);
    item->setData(Qt::UserRole,i);item->setToolTip(QString::fromUtf8("点击定位原始消息"));
    item->setData(ResultKeyRole,m_recordKeys.at(i));
    if( m_embedded )
    {
      item->setData(SenderRole,r.sender.isEmpty()?QStringLiteral("发送者未记录"):r.sender);
      const QString time = !r.timestamp.isValid() ? QStringLiteral("时间未记录")
        : r.timestamp.date()==QDate::currentDate() ? r.timestamp.toString("HH:mm")
        : r.timestamp.date()==QDate::currentDate().addDays(-1) ? QStringLiteral("昨天")
        : r.timestamp.toString("MM-dd");
      item->setData(TimeRole,time);
      QString preview = detail;
      if(category==0)
      {
        preview.remove(QRegularExpression("^\\s*\\d{4}-\\d{2}-\\d{2}\\s*"));
        preview.remove(QRegularExpression("^\\s*\\[?\\d{1,2}:\\d{2}(?::\\d{2})?\\]?\\s*"));
        if(!r.sender.isEmpty()&&preview.startsWith(r.sender))
          preview=preview.mid(r.sender.size()).trimmed();
        preview.remove(QRegularExpression("^\\s*\\(?\\d{4}-\\d{2}-\\d{2}[ T]\\d{1,2}:\\d{2}(?::\\d{2})?\\)?\\s*"));
        if(preview.startsWith(":")||preview.startsWith(QStringLiteral("：")))preview=preview.mid(1).trimmed();
        preview.remove(QChar::ObjectReplacementCharacter);
        if(!r.media.isEmpty())preview=QStringLiteral("[图片和视频] ")+preview;
      }
      item->setData(PreviewRole,preview);
      for(const User& user : UserManager::instance().userList().toList())
        if(Bee::userNameToShow(user,true)==r.sender)
        {
          item->setData(AvatarRole,ShadowStyle::avatarPixmap(Bee::avatarForUser(user,QSize(32,32),true),QSize(32,32)));
          break;
        }
      if(category==0&&!r.media.isEmpty())
      {
        QImageReader reader(QUrl(r.media.first()).toLocalFile());
        const QSize sourceSize=reader.size();
        if(sourceSize.isValid())reader.setScaledSize(sourceSize.scaled(128,128,Qt::KeepAspectRatioByExpanding));
        item->setData(ThumbnailRole,QPixmap::fromImage(reader.read()));
      }
    }
    if(category==2 && !targets.isEmpty()) {
      QPixmap thumbnail(QUrl(targets.first()).toLocalFile());
      if(!thumbnail.isNull()) item->setIcon(QIcon(thumbnail.scaled(100,80,Qt::KeepAspectRatio,Qt::SmoothTransformation)));
      else item->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
  }
  mp_resultCount->setText(QString::fromUtf8("%1 条结果%2").arg(mp_results->count()).arg(unknown?QString::fromUtf8(" · 部分旧记录没有完整发送者或时间信息"):QString()));
  mp_resultCount->setWordWrap(true);
  if(mp_emptyState)
  {
    mp_emptyState->setText(m_records.isEmpty()?QStringLiteral("暂无聊天记录"):QStringLiteral("没有找到匹配的记录\n试试其他关键词或筛选条件"));
    mp_emptyState->setVisible(mp_results->count()==0);
    mp_results->setVisible(mp_results->count()>0);
  }
  if( resetReadingState )
  {
    mp_results->doItemsLayout();
    mp_results->verticalScrollBar()->setValue(0);
    mp_results->horizontalScrollBar()->setValue(0);
    mp_teSavedChat->verticalScrollBar()->setValue(0);
    mp_teSavedChat->horizontalScrollBar()->setValue(0);
  }
}

void GuiSavedChat::locateHistoryResult()
{
  if(!mp_results->currentItem()) return;
  if(mp_contextButton) mp_contextButton->setEnabled(true);
  mp_teSavedChat->scrollToAnchor(QString("history-%1").arg(mp_results->currentItem()->data(Qt::UserRole).toInt()));
}

void GuiSavedChat::showHistoryContext()
{
  if(!mp_results->currentItem()&&mp_results->count()>0)mp_results->setCurrentRow(0);
  if(!mp_results->currentItem())return;
  if(mp_resultsStack)mp_resultsStack->setCurrentIndex(1);
  locateHistoryResult();
}

void GuiSavedChat::findTextInHistory()
{
  QString txt = mp_leFilter->text().trimmed();
  if( txt.isEmpty() )
    return;

  QTextDocument::FindFlags find_flags;
  if( mp_cbCaseSensitive->isChecked() )
    find_flags |= QTextDocument::FindCaseSensitively;
  if( mp_cbWholeWordOnly->isChecked() )
    find_flags |= QTextDocument::FindWholeWords;

  if( !mp_teSavedChat->find( txt, find_flags ) )
  {
    mp_teSavedChat->moveCursor( QTextCursor::Start );
    if( !mp_teSavedChat->find( txt ) )
    {
      QMessageBox::information( this, Settings::instance().programName(), tr( "%1 not found" ).arg( QString( "\"%1\"" ).arg( txt ) ) + QString( "." ), tr( "Ok" ) );
      return;
    }
  }
}

void GuiSavedChat::saveHistoryAs()
{
  QString file_name = FileDialog::getSaveFileName( this,
                          tr( "Please select a file where to save the messages showed in this window." ),
                          Settings::instance().dataFolder(), "PDF Chat Files (*.pdf)" );
  if( file_name.isEmpty() )
    return;

  if( !file_name.endsWith( QLatin1String( ".pdf" ), Qt::CaseInsensitive ) )
    file_name.append( QLatin1String( ".pdf" ) );

  QPrinter printer;
  printer.setOutputFormat( QPrinter::PdfFormat );
  printer.setOutputFileName( file_name );
  QTextDocument *doc = mp_teSavedChat->document();
  doc->print( &printer );
  QMessageBox::information( this, Settings::instance().programName(), tr( "%1: save completed." ).arg( file_name ), tr( "Ok" ) );
}
