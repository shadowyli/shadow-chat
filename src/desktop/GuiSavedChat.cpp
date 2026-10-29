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


GuiSavedChat::GuiSavedChat( QWidget* parent )
 : QMainWindow( parent ), mp_leFilter(Q_NULLPTR), mp_results(Q_NULLPTR)
{
  setObjectName( "GuiSavedChat" );
  ShadowDialogStyle::apply(this);
  setStyleSheet(styleSheet()+QStringLiteral("QListWidget::item { padding: 10px 12px; border-bottom: 1px solid #E7EBF0; } QTabBar::tab { padding: 11px 18px; border: 0; border-bottom: 2px solid transparent; color: #697B94; background: white; } QTabBar::tab:selected { color: #2463EB; border-bottom-color: #2463EB; } QComboBox { min-height: 24px; } QToolBar { background: white; border: 0; padding: 12px 20px; spacing: 8px; } QToolButton { padding: 7px 10px; border: 0; border-radius: 6px; color: #52647F; } QToolButton:hover { background: #EAF1FF; } QTextBrowser { border: 1px solid #E3E8F0; border-radius: 8px; background: white; padding: 10px; }"));
  Bee::removeContextHelpButton( this );
  setWindowFlags( windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint );
  setWindowIcon( IconManager::instance().icon( "saved-chat.png" ) );

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
  QVBoxLayout* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(24,16,24,24);
  layout->setSpacing(12);
  QLabel* heading=new QLabel(QStringLiteral("聊天记录"),panel);heading->setProperty("role","title");layout->addWidget(heading);
  mp_categories = new QTabBar(panel);
  mp_categories->addTab(QString::fromUtf8("消息"));
  mp_categories->addTab(QString::fromUtf8("文件"));
  mp_categories->addTab(QString::fromUtf8("图片和视频"));
  mp_categories->addTab(QString::fromUtf8("链接"));
  mp_categories->setExpanding(false);
  layout->addWidget(mp_categories);
  QHBoxLayout* filters = new QHBoxLayout;
  mp_senderFilter = new QComboBox(panel); mp_senderFilter->setAccessibleName(QString::fromUtf8("发送者"));
  mp_dateFilter = new QComboBox(panel); mp_dateFilter->setAccessibleName(QString::fromUtf8("时间范围"));
  mp_dateFilter->addItems(QStringList() << QString::fromUtf8("全部时间") << QString::fromUtf8("今天") << QString::fromUtf8("近 7 天") << QString::fromUtf8("近 30 天"));
  mp_sortOrder = new QComboBox(panel); mp_sortOrder->addItems(QStringList() << QString::fromUtf8("最新在前") << QString::fromUtf8("最早在前"));
  filters->addWidget(mp_senderFilter);filters->addWidget(mp_dateFilter);filters->addWidget(mp_sortOrder);filters->addStretch();
  layout->addLayout(filters);
  mp_resultCount = new QLabel(panel);mp_resultCount->setProperty("role","hint");layout->addWidget(mp_resultCount);
  QSplitter* split = new QSplitter(Qt::Vertical,panel);
  mp_results = new QListWidget(split); mp_results->setWordWrap(true);mp_results->setMinimumHeight(180);
  mp_results->setToolTip(QString::fromUtf8("选择结果，在下方定位原始聊天；点击原文中的文件或链接打开。"));
  split->addWidget(mp_results);split->addWidget(mp_teSavedChat);split->setHandleWidth(12);layout->addWidget(split);
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
  setupToolBar( mp_barHistory );
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

void GuiSavedChat::showSavedChat( const QString& chat_name )
{
  m_savedChatName = chat_name;
  QString window_chat_title = chat_name == Settings::instance().defaultChatName() ? GuiChatItem::defaultChatName().toUpper() : chat_name;
  setWindowTitle( QString::fromUtf8("%1 - 聊天记录").arg(window_chat_title) );

  QString html_text = "";
  if( ChatManager::instance().chatHasSavedText( chat_name ) )
    html_text += ChatManager::instance().chatSavedText( chat_name );

  const Chat liveChat=ChatManager::instance().findChatByName(chat_name);
  if(liveChat.isValid()) html_text += GuiChatMessage::chatToHtml(liveChat,false,true,true,true,false,true);

  m_records = shadowChatHistoryRecords(html_text);
  mp_senderFilter->blockSignals(true);mp_senderFilter->clear();mp_senderFilter->addItem(QString::fromUtf8("全部发送者"),QString());
  QStringList senders;
  QString indexed;
  for(int i=0;i<m_records.size();++i) {
    const HistoryRecord& r=m_records[i];
    if(!r.sender.isEmpty() && !senders.contains(r.sender)) senders << r.sender;
    indexed += QString("<a name=\"history-%1\"></a>").arg(i)+r.html+"<br>";
  }
  senders.sort();foreach(const QString& sender,senders) mp_senderFilter->addItem(sender,sender);
  mp_senderFilter->blockSignals(false);
  html_text=indexed;
  bool updates_enabled = mp_teSavedChat->updatesEnabled();
  mp_teSavedChat->setUpdatesEnabled( false );
  mp_teSavedChat->setText( html_text );
  mp_teSavedChat->setUpdatesEnabled( updates_enabled );
  filterHistory();
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
  if(!mp_results || !mp_leFilter) return;
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
  for(int pos=0;pos<m_records.size();++pos) {
    const int i=mp_sortOrder->currentIndex()==0?m_records.size()-1-pos:pos;
    const HistoryRecord& r=m_records[i];
    if(!sender.isEmpty() && r.sender!=sender) continue;
    if(mp_dateFilter->currentIndex()>0 && (!r.timestamp.isValid() || r.timestamp.date()<QDate::currentDate().addDays(-days))) continue;
    QStringList targets=category==1?r.files:category==2?r.media:category==3?r.links:QStringList();
    if(category>0 && targets.isEmpty()) continue;
    const QString searchable=r.text+" "+targets.join(" ");
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
    if(category==2 && !targets.isEmpty()) {
      QPixmap thumbnail(QUrl(targets.first()).toLocalFile());
      if(!thumbnail.isNull()) item->setIcon(QIcon(thumbnail.scaled(100,80,Qt::KeepAspectRatio,Qt::SmoothTransformation)));
      else item->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
  }
  mp_resultCount->setText(QString::fromUtf8("%1 条结果%2").arg(mp_results->count()).arg(unknown?QString::fromUtf8(" · 部分旧记录没有完整发送者或时间信息"):QString()));
}

void GuiSavedChat::locateHistoryResult()
{
  if(!mp_results->currentItem()) return;
  mp_teSavedChat->scrollToAnchor(QString("history-%1").arg(mp_results->currentItem()->data(Qt::UserRole).toInt()));
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
