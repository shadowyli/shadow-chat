#include "GuiStickerPicker.h"
#include "ShadowStickerStore.h"
#include "ShadowStyle.h"
#include <QtWidgets>

namespace
{
class StickerGrid;

class StickerDelegate : public QStyledItemDelegate
{
public:
  explicit StickerDelegate(StickerGrid* grid);
  void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override;
private:
  StickerGrid* m_grid;
};

class StickerGrid : public QListWidget
{
public:
  explicit StickerGrid(QWidget* parent):QListWidget(parent),lastButton(Qt::NoButton),m_thumbnails(128)
  {
    setItemDelegate(new StickerDelegate(this));
    m_visibilityTimer.setSingleShot(true);
    m_visibilityTimer.setInterval(16);
    connect(&m_visibilityTimer,&QTimer::timeout,this,[this]() {updateVisibleMovies();});
    connect(verticalScrollBar(),&QScrollBar::valueChanged,this,[this]() {scheduleVisibleMovies();});
    connect(verticalScrollBar(),&QScrollBar::rangeChanged,this,[this]() {scheduleVisibleMovies();});
  }
  Qt::MouseButton lastButton;
  QPixmap thumbnail(const QModelIndex& index)
  {
    const QString path=index.data(Qt::UserRole+1).toString();
    if(QMovie* movie=m_movies.value(path,Q_NULLPTR))return movie->currentPixmap();
    if(QPixmap* cached=m_thumbnails.object(path))return *cached;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size=reader.size();
    if(size.isValid())reader.setScaledSize(size.scaled(iconSize(),Qt::KeepAspectRatio));
    const QPixmap image=QPixmap::fromImage(reader.read());
    m_thumbnails.insert(path,new QPixmap(image));
    return image;
  }
  void resetPreviews()
  {
    m_visibilityTimer.stop();
    qDeleteAll(m_movies);
    m_movies.clear();
  }
  void scheduleVisibleMovies()
  {
    if(isVisible()&&!m_visibilityTimer.isActive())m_visibilityTimer.start();
  }
protected:
  void mousePressEvent(QMouseEvent* event) override
  {
    lastButton=event->button();
    QListWidget::mousePressEvent(event);
  }
  void showEvent(QShowEvent* event) override
  {
    QListWidget::showEvent(event);
    scheduleVisibleMovies();
  }
  void hideEvent(QHideEvent* event) override
  {
    resetPreviews();
    QListWidget::hideEvent(event);
  }
  void resizeEvent(QResizeEvent* event) override
  {
    QListWidget::resizeEvent(event);
    scheduleVisibleMovies();
  }
private:
  void updateVisibleMovies()
  {
    if(!isVisible())return;
    QHash<QString,QPersistentModelIndex> visible;
    const QRect viewportRect=viewport()->rect();
    for(int row=0;row<count();++row)
    {
      QListWidgetItem* entry=item(row);
      const QString path=entry->data(Qt::UserRole+1).toString();
      if(visible.size()<12&&!path.isEmpty()&&QFileInfo(path).suffix().compare("gif",Qt::CaseInsensitive)==0&&
         visualItemRect(entry).intersects(viewportRect))
        visible.insert(path,QPersistentModelIndex(indexFromItem(entry)));
    }
    for(auto movie=m_movies.begin();movie!=m_movies.end();)
    {
      if(!visible.contains(movie.key()))
      {
        delete movie.value();
        movie=m_movies.erase(movie);
      }
      else ++movie;
    }
    for(auto entry=visible.constBegin();entry!=visible.constEnd();++entry)
    {
      if(m_movies.contains(entry.key()))continue;
      QImageReader reader(entry.key());
      const QSize size=reader.size();
      QMovie* movie=new QMovie(entry.key(),QByteArray(),this);
      movie->setObjectName("StickerVisibleMovie");
      movie->setCacheMode(QMovie::CacheNone);
      if(size.isValid())movie->setScaledSize(size.scaled(iconSize(),Qt::KeepAspectRatio));
      if(!movie->isValid()){delete movie;continue;}
      const QPersistentModelIndex index=entry.value();
      connect(movie,&QMovie::frameChanged,this,[this,index](int) {
        if(index.isValid()&&isVisible())viewport()->update(visualRect(index));
      });
      m_movies.insert(entry.key(),movie);
      movie->start();
    }
  }
  QTimer m_visibilityTimer;
  QHash<QString,QMovie*> m_movies;
  QCache<QString,QPixmap> m_thumbnails;
};

StickerDelegate::StickerDelegate(StickerGrid* grid):QStyledItemDelegate(grid),m_grid(grid){}

void StickerDelegate::paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const
{
  QStyleOptionViewItem cell(option);
  initStyleOption(&cell,index);
  if(!index.data(Qt::UserRole+2).toBool())
  {
    cell.icon=QIcon(m_grid->thumbnail(index));
    cell.features|=QStyleOptionViewItem::HasDecoration;
    cell.decorationSize=m_grid->iconSize();
  }
  m_grid->style()->drawControl(QStyle::CE_ItemViewItem,&cell,painter,m_grid);
}

QIcon addStickerIcon()
{
  QPixmap image(58,58);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor("#CBD3DD"),1,Qt::DashLine));
  painter.drawRoundedRect(image.rect().adjusted(1,1,-2,-2),6,6);
  painter.setPen(QPen(QColor("#A7B2C1"),1.8,Qt::SolidLine,Qt::RoundCap));
  painter.drawLine(29,18,29,40);
  painter.drawLine(18,29,40,29);
  return QIcon(image);
}
}

GuiStickerPicker::GuiStickerPicker(const QString& accountFolder,QWidget* parent)
  :QDialog(parent),m_accountFolder(accountFolder),m_recent(false),m_managing(false),m_positionPending(false)
{
  setObjectName("ShadowStickerPicker");
  setProperty("shadowDialogChromeSkip",true);
  setWindowFlags(Qt::Popup|Qt::FramelessWindowHint);
  setModal(false);
  setWindowTitle(QStringLiteral("我的收藏"));
  qApp->installEventFilter(this);
  resize(480,300);
  setMinimumSize(280,180);
  setStyleSheet(QStringLiteral(
    "QDialog#ShadowStickerPicker{background:#FFFFFF;border:1px solid #DCE3EC;border-radius:10px;}"
    "QLabel{background:transparent;color:#7A8798;font-size:11px;}"
    "QListWidget{background:#FFFFFF;border:0;outline:0;}"
    "QListWidget::item{border:1px solid transparent;border-radius:6px;padding:3px;}"
    "QListWidget::item:hover{background:#F1F6FD;}"
    "QListWidget::item:selected{background:#EAF2FF;border-color:#C9DCFA;}"
    "QWidget#StickerFooter{background:#F5F7FA;border-top:1px solid #E7ECF2;}"
    "QToolButton{background:transparent;color:#758296;border:0;border-radius:5px;padding:5px;}"
    "QToolButton:hover,QToolButton:checked{background:#E3ECF9;color:#2878EF;}"
    "QToolButton::menu-indicator{image:none;}"
    "QLineEdit{background:#F8FAFC;border:1px solid #DFE6EE;border-radius:5px;padding:5px 8px;}"
    "QPushButton{background:transparent;border:0;color:#61738B;padding:5px 8px;}"
    "QPushButton:hover{color:#2878EF;}"
    "QMenu{background:white;color:#34445E;border:1px solid #DFE6EE;padding:5px;}"
    "QMenu::item{padding:7px 18px;}QMenu::item:selected{background:#EAF2FF;}" ));
  QVBoxLayout* layout=new QVBoxLayout(this);
  layout->setContentsMargins(8,8,8,1);
  layout->setSpacing(4);
  m_search=new QLineEdit(this);
  m_search->setObjectName("StickerSearch");
  m_search->setAccessibleName(QStringLiteral("搜索表情"));
  m_search->setPlaceholderText(QStringLiteral("搜索收藏的表情"));
  m_search->setClearButtonEnabled(true);
  m_search->hide();
  layout->addWidget(m_search);
  connect(m_search,&QLineEdit::textChanged,this,&GuiStickerPicker::refresh);
  m_grid=new StickerGrid(this);
  m_grid->setObjectName("StickerGrid");
  m_grid->setAccessibleName(QStringLiteral("收藏表情列表"));
  m_grid->setViewMode(QListView::IconMode);
  m_grid->setIconSize(QSize(58,58));
  m_grid->setGridSize(QSize(74,74));
  m_grid->setResizeMode(QListView::Adjust);
  m_grid->setMovement(QListView::Static);
  m_grid->setSpacing(2);
  m_grid->setUniformItemSizes(true);
  m_grid->setWordWrap(false);
  m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
  m_grid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout->addWidget(m_grid,1);
  m_status=new QLabel(this);
  m_status->setObjectName("StickerStatus");
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
  QWidget* footer=new QWidget(this);
  footer->setObjectName("StickerFooter");
  QHBoxLayout* footerLayout=new QHBoxLayout(footer);
  footerLayout->setContentsMargins(5,3,5,3);
  footerLayout->setSpacing(6);
  const auto button=[footer,footerLayout](const QString& icon,const QString& label) {
    QToolButton* control=new QToolButton(footer);
    control->setIcon(ShadowStyle::actionIcon(icon));
    control->setIconSize(QSize(20,20));
    control->setFixedSize(30,30);
    control->setToolTip(label);
    control->setAccessibleName(label);
    footerLayout->addWidget(control);
    return control;
  };
  QToolButton* search=button("magnifying-glass",QStringLiteral("搜索表情"));
  connect(search,&QToolButton::clicked,this,[this]() {
    m_search->setVisible(m_search->isHidden());
    if(m_search->isVisible())m_search->setFocus();else m_search->clear();
  });
  QToolButton* add=button("paperclip",QStringLiteral("添加图片或导入 ZIP"));
  add->setObjectName("StickerAddButton");
  add->setIcon(addStickerIcon());
  add->setPopupMode(QToolButton::InstantPopup);
  m_addMenu=new QMenu(this);
  m_addMenu->setObjectName("StickerAddMenu");
  QAction* images=m_addMenu->addAction(QStringLiteral("添加本地图片"));
  connect(images,&QAction::triggered,this,&GuiStickerPicker::chooseImages);
  add->setMenu(m_addMenu);
  QToolButton* favorites=button("smiley",QStringLiteral("我的收藏"));
  favorites->setCheckable(true);
  favorites->setChecked(true);
  QToolButton* recent=button("clock",QStringLiteral("最近使用"));
  recent->setCheckable(true);
  QButtonGroup* categories=new QButtonGroup(this);
  categories->addButton(favorites);
  categories->addButton(recent);
  connect(favorites,&QToolButton::clicked,this,[this]() {m_recent=false;refresh();});
  connect(recent,&QToolButton::clicked,this,[this]() {m_recent=true;refresh();});
  footerLayout->addStretch();
  m_remove=new QPushButton(QStringLiteral("移除"),footer);
  m_remove->setObjectName("StickerRemove");
  m_remove->setEnabled(false);
  m_remove->hide();
  footerLayout->addWidget(m_remove);
  connect(m_remove,&QPushButton::clicked,this,&GuiStickerPicker::removeSelected);
  QToolButton* settings=button("gear-six",QStringLiteral("表情管理"));
  settings->setPopupMode(QToolButton::InstantPopup);
  QMenu* menu=new QMenu(settings);
  QAction* import=menu->addAction(QStringLiteral("导入表情包 ZIP"));
  import->setObjectName("StickerImportZip");
  connect(import,&QAction::triggered,this,&GuiStickerPicker::chooseArchive);
  m_addMenu->addAction(import);
  QAction* manage=menu->addAction(QStringLiteral("管理收藏"));
  manage->setObjectName("StickerManageFavorites");
  manage->setCheckable(true);
  connect(manage,&QAction::toggled,this,[this](bool enabled) {m_managing=enabled;m_remove->setVisible(enabled);});
  settings->setMenu(menu);
  layout->addWidget(footer);
  connect(m_grid,&QListWidget::itemClicked,this,[this](QListWidgetItem* item) {
    if(static_cast<StickerGrid*>(m_grid)->lastButton==Qt::LeftButton)activateSticker(item);
  });
  connect(m_grid,&QListWidget::itemActivated,this,&GuiStickerPicker::activateSticker);
  connect(m_grid,&QListWidget::itemSelectionChanged,this,[this]() {
    m_remove->setEnabled(m_grid->currentItem()&&!m_grid->currentItem()->data(Qt::UserRole).toString().isEmpty());
  });
  connect(m_grid,&QListWidget::customContextMenuRequested,this,[this](const QPoint& point) {
    QListWidgetItem* item=m_grid->itemAt(point);
    if(!item||item->data(Qt::UserRole).toString().isEmpty())return;
    m_grid->setCurrentItem(item);
    QMenu context(this);
    QAction* remove=context.addAction(QStringLiteral("移除表情"));
    if(context.exec(m_grid->viewport()->mapToGlobal(point))==remove)removeSelected();
  });
  refresh();
}

void GuiStickerPicker::showAbove(QWidget* anchor)
{
  if(!anchor)return;
  if(m_anchor)m_anchor->removeEventFilter(this);
  if(m_owner)m_owner->removeEventFilter(this);
  m_anchor=anchor;
  m_owner=anchor->window();
  refresh();
  show();
  positionAboveAnchor();
  raise();
  m_grid->setFocus();
}

void GuiStickerPicker::positionAboveAnchor()
{
  QWidget* anchor=m_anchor;
  if(!anchor||!isVisible())return;
  const QRect screen=anchor->screen()?anchor->screen()->availableGeometry():QApplication::desktop()->availableGeometry();
  resize(qMin(480,screen.width()-16),qMin(300,screen.height()-16));
  const QPoint origin=anchor->mapToGlobal(QPoint());
  int positionY=origin.y()-height()-6;
  if(positionY<screen.top()+8)positionY=origin.y()+anchor->height()+6;
  move(qBound(screen.left()+8,origin.x(),screen.right()-width()-7),
    qBound(screen.top()+8,positionY,screen.bottom()-height()-7));
}

void GuiStickerPicker::scheduleAnchorPosition()
{
  if(m_positionPending)return;
  m_positionPending=true;
  QTimer::singleShot(0,this,[this]() {
    m_positionPending=false;
    if(isVisible())positionAboveAnchor();
  });
}

bool GuiStickerPicker::eventFilter(QObject* watched,QEvent* event)
{
  if(isVisible()&&event->type()==QEvent::MouseButtonPress)
  {
    QWidget* widget=qobject_cast<QWidget*>(watched);
    const auto* mouse=static_cast<QMouseEvent*>(event);
    if(widget&&widget!=m_anchor&&!isAncestorOf(widget)&&!frameGeometry().contains(mouse->globalPos()))hide();
  }
  if(isVisible()&&event->type()==QEvent::KeyPress&&
     static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)
  {
    QWidget* widget=qobject_cast<QWidget*>(watched);
    if(widget&&(widget==this||isAncestorOf(widget))){hide();return true;}
  }
  if(watched==m_owner||watched==m_anchor)
  {
    if(event->type()==QEvent::Hide)hide();
    else if(event->type()==QEvent::Move||event->type()==QEvent::Resize||
            event->type()==QEvent::LayoutRequest)scheduleAnchorPosition();
  }
  return QDialog::eventFilter(watched,event);
}

void GuiStickerPicker::refresh()
{
  static_cast<StickerGrid*>(m_grid)->resetPreviews();
  m_grid->clear();
  QListWidgetItem* add=new QListWidgetItem(addStickerIcon(),QString(),m_grid);
  add->setToolTip(QStringLiteral("添加图片或导入 ZIP"));
  add->setData(Qt::AccessibleTextRole,QStringLiteral("添加图片或导入 ZIP"));
  add->setData(Qt::UserRole+2,true);
  const auto stickers=ShadowStickerStore(m_accountFolder).list();
  QSettings recent(QDir(m_accountFolder).filePath("sticker-panel.ini"),QSettings::IniFormat);
  const QStringList recentIds=recent.value("recent").toStringList();
  QList<ShadowStickerStore::Sticker> visible;
  if(m_recent)
  {
    for(const QString& id:recentIds)
      for(const auto& sticker:stickers)if(sticker.id==id)visible.append(sticker);
  }
  else visible=stickers;
  int count=0;
  for(const auto& sticker:visible)
  {
    if(!sticker.name.contains(m_search->text().trimmed(),Qt::CaseInsensitive))continue;
    QListWidgetItem* item=new QListWidgetItem(QString(),m_grid);
    item->setData(Qt::UserRole,sticker.id);
    item->setData(Qt::UserRole+1,sticker.path);
    item->setData(Qt::AccessibleTextRole,QStringLiteral("收藏表情"));
    item->setToolTip(QStringLiteral("点击发送表情"));
    ++count;
  }
  m_status->setText(count?QString():m_recent?QStringLiteral("暂无最近使用"):QStringLiteral("暂无收藏"));
  m_status->setVisible(!m_status->text().isEmpty());
  m_remove->setEnabled(false);
  static_cast<StickerGrid*>(m_grid)->scheduleVisibleMovies();
}

void GuiStickerPicker::activateSticker(QListWidgetItem* item)
{
  if(!isVisible()||!item)return;
  if(item->data(Qt::UserRole+2).toBool())
  {
    m_addMenu->popup(m_grid->viewport()->mapToGlobal(m_grid->visualItemRect(item).bottomLeft()));
    return;
  }
  if(m_managing)return;
  const QString path=item->data(Qt::UserRole+1).toString();
  if(!QFileInfo::exists(path)){refresh();m_status->setText(QStringLiteral("原图已不存在，请重新添加。"));m_status->show();return;}
  QSettings preferences(QDir(m_accountFolder).filePath("sticker-panel.ini"),QSettings::IniFormat);
  QStringList recent=preferences.value("recent").toStringList();
  const QString id=item->data(Qt::UserRole).toString();
  recent.removeAll(id);
  recent.prepend(id);
  while(recent.size()>30)recent.removeLast();
  preferences.setValue("recent",recent);
  hide();
  emit stickerSelected(path);
}

void GuiStickerPicker::chooseImages()
{
  hide();
  const QStringList files=QFileDialog::getOpenFileNames(parentWidget(),QStringLiteral("添加到表情"),QString(),QStringLiteral("图片 (*.png *.jpg *.jpeg *.gif *.webp)"));
  ShadowStickerStore store(m_accountFolder);
  QString error;
  for(const QString& file:files)if(!store.addFile(file,&error))break;
  if(m_anchor)showAbove(m_anchor);else{refresh();show();}
  if(!error.isEmpty()){m_status->setText(error);m_status->show();}
}

void GuiStickerPicker::chooseArchive()
{
  hide();
  QSettings preferences(QDir(m_accountFolder).filePath("sticker-panel.ini"),QSettings::IniFormat);
  const QString directory=preferredArchiveDirectory(preferences.value("importDirectory").toString(),
    defaultArchiveDirectory(),QString(),QString());
  QFileDialog dialog(parentWidget(),QStringLiteral("导入表情包 ZIP"),directory,QStringLiteral("ZIP 表情包 (*.zip)"));
  dialog.setObjectName("StickerImportDialog");
  dialog.setOption(QFileDialog::DontUseNativeDialog,QGuiApplication::platformName()!=QStringLiteral("windows"));
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setLabelText(QFileDialog::LookIn,QStringLiteral("查找位置"));
  dialog.setLabelText(QFileDialog::FileName,QStringLiteral("文件名"));
  dialog.setLabelText(QFileDialog::FileType,QStringLiteral("文件类型"));
  dialog.setLabelText(QFileDialog::Accept,QStringLiteral("打开"));
  dialog.setLabelText(QFileDialog::Reject,QStringLiteral("取消"));
  QString file;
  if(execArchiveDialog(dialog)==QDialog::Accepted&&!dialog.selectedFiles().isEmpty())
  {
    file=dialog.selectedFiles().first();
    const QFileInfo selected(file);
    if(selected.isFile()&&selected.isReadable()&&QFileInfo(selected.absolutePath()).isReadable())
      preferences.setValue("importDirectory",selected.absolutePath());
  }
  if(m_anchor)showAbove(m_anchor);else show();
  if(!file.isEmpty())importArchive(file);
}

int GuiStickerPicker::execArchiveDialog(QFileDialog& dialog)
{
  return dialog.exec();
}

QString GuiStickerPicker::defaultArchiveDirectory() const
{
  return preferredArchiveDirectory(QString(),QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),QDir::homePath());
}

QString GuiStickerPicker::preferredArchiveDirectory(const QString& previous,const QString& downloads,
                                                   const QString& documents,const QString& home)
{
  const QStringList candidates=QStringList()<<previous<<downloads<<documents<<home;
  for(const QString& candidate:candidates)
  {
    const QFileInfo directory(candidate);
    if(!candidate.isEmpty()&&directory.isDir()&&directory.isReadable())
      return directory.absoluteFilePath();
  }
  return QString();
}

bool GuiStickerPicker::importArchive(const QString& path)
{
  QString error;
  int added=0,duplicates=0;
  const bool imported=ShadowStickerStore(m_accountFolder).importZip(path,&error,&added,&duplicates);
  if(imported)refresh();
  m_status->setText(imported?QStringLiteral("已添加 %1 个表情，跳过 %2 个重复项").arg(added).arg(duplicates):error);
  m_status->show();
  return imported;
}

void GuiStickerPicker::removeSelected()
{
  QListWidgetItem* item=m_grid->currentItem();
  if(!item)return;
  QString error;
  if(!ShadowStickerStore(m_accountFolder).remove(item->data(Qt::UserRole).toString(),&error))
  {m_status->setText(error);m_status->show();return;}
  refresh();
}
