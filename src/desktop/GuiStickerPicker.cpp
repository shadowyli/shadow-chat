#include "GuiStickerPicker.h"
#include "ShadowStickerStore.h"
#include "ShadowDialogStyle.h"
#include <QtWidgets>

namespace {
class StickerGrid : public QListWidget {
public:
  explicit StickerGrid(QWidget* parent):QListWidget(parent),lastButton(Qt::NoButton){}
  Qt::MouseButton lastButton;
protected:
  void mousePressEvent(QMouseEvent* event) override {lastButton=event->button();QListWidget::mousePressEvent(event);}
};
}

GuiStickerPicker::GuiStickerPicker(const QString& accountFolder,QWidget* parent)
  :QDialog(parent),m_accountFolder(accountFolder)
{
  setObjectName("ShadowStickerPicker");setWindowTitle(QStringLiteral("我的表情"));resize(530,480);setMinimumSize(390,330);
  ShadowDialogStyle::apply(this);
  QVBoxLayout* layout=new QVBoxLayout(this);layout->setContentsMargins(20,18,20,18);layout->setSpacing(12);
  QHBoxLayout* header=new QHBoxLayout;
  QLabel* title=new QLabel(QStringLiteral("我的表情"),this);title->setProperty("role","title");header->addWidget(title);header->addStretch();
  QPushButton* importButton=new QPushButton(QStringLiteral("导入表情包 ZIP"),this);importButton->setObjectName("StickerImportZip");importButton->setProperty("role","primary");header->addWidget(importButton);layout->addLayout(header);
  QLabel* hint=new QLabel(QStringLiteral("点击表情直接发送 · 也可以右键收藏聊天中的图片"),this);hint->setProperty("role","hint");layout->addWidget(hint);
  m_grid=new StickerGrid(this);m_grid->setObjectName("StickerGrid");m_grid->setViewMode(QListView::IconMode);m_grid->setIconSize(QSize(72,72));m_grid->setGridSize(QSize(94,100));m_grid->setResizeMode(QListView::Adjust);m_grid->setMovement(QListView::Static);m_grid->setSpacing(4);m_grid->setWordWrap(false);m_grid->setSelectionMode(QAbstractItemView::SingleSelection);m_grid->setContextMenuPolicy(Qt::CustomContextMenu);m_grid->setUniformItemSizes(true);layout->addWidget(m_grid,1);
  QHBoxLayout* footer=new QHBoxLayout;m_status=new QLabel(this);m_status->setObjectName("StickerStatus");m_status->setProperty("role","hint");footer->addWidget(m_status,1);
  m_remove=new QPushButton(QStringLiteral("移除表情"),this);m_remove->setObjectName("StickerRemove");m_remove->setEnabled(false);footer->addWidget(m_remove);
  QPushButton* close=new QPushButton(QStringLiteral("关闭"),this);footer->addWidget(close);layout->addLayout(footer);
  connect(close,&QPushButton::clicked,this,&QDialog::reject);
  connect(importButton,&QPushButton::clicked,this,&GuiStickerPicker::chooseArchive);
  connect(m_remove,&QPushButton::clicked,this,&GuiStickerPicker::removeSelected);
  connect(m_grid,&QListWidget::itemSelectionChanged,this,[this](){m_remove->setEnabled(m_grid->currentItem()!=Q_NULLPTR);});
  connect(m_grid,&QListWidget::itemClicked,this,[this](QListWidgetItem* item){if(static_cast<StickerGrid*>(m_grid)->lastButton!=Qt::LeftButton)return;const QString path=item->data(Qt::UserRole+1).toString();if(QFileInfo::exists(path)){emit stickerSelected(path);accept();}else{refresh();m_status->setText(QStringLiteral("原图已不存在，请重新导入。"));}});
  connect(m_grid,&QListWidget::customContextMenuRequested,this,[this](const QPoint& point){QListWidgetItem* item=m_grid->itemAt(point);if(!item)return;m_grid->setCurrentItem(item);QMenu menu(this);QAction* remove=menu.addAction(QStringLiteral("移除表情"));if(menu.exec(m_grid->viewport()->mapToGlobal(point))==remove)removeSelected();});
  refresh();
}

void GuiStickerPicker::refresh()
{
  m_grid->clear();ShadowStickerStore store(m_accountFolder);const auto stickers=store.list();
  foreach(const auto& sticker,stickers){QImageReader reader(sticker.path);reader.setAutoTransform(true);reader.setScaledSize(reader.size().scaled(QSize(72,72),Qt::KeepAspectRatio));const QImage image=reader.read();QListWidgetItem* item=new QListWidgetItem(QIcon(QPixmap::fromImage(image)),sticker.name,m_grid);item->setData(Qt::UserRole,sticker.id);item->setData(Qt::UserRole+1,sticker.path);item->setToolTip(sticker.name+QStringLiteral("\n点击发送原图"));item->setTextAlignment(Qt::AlignCenter);}
  m_status->setText(stickers.isEmpty()?QStringLiteral("还没有收藏表情，导入 ZIP 或收藏聊天图片开始使用。") : QStringLiteral("共 %1 个表情").arg(stickers.size()));m_status->setWordWrap(true);m_remove->setEnabled(false);
}

void GuiStickerPicker::chooseArchive()
{
  QFileDialog dialog(this,QStringLiteral("导入表情包 ZIP"),QString(),QStringLiteral("表情包 (*.zip)"));dialog.setObjectName("StickerZipFileDialog");dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setFileMode(QFileDialog::ExistingFile);
  if(dialog.exec()==QDialog::Accepted&&!dialog.selectedFiles().isEmpty())importArchive(dialog.selectedFiles().first());
}

bool GuiStickerPicker::importArchive(const QString& path)
{
  QString error;int added=0,duplicates=0;ShadowStickerStore store(m_accountFolder);QApplication::setOverrideCursor(Qt::WaitCursor);
  const bool ok=store.importZip(path,&error,&added,&duplicates);QApplication::restoreOverrideCursor();
  if(!ok){m_status->setText(error);return false;}
  refresh();m_status->setText(QStringLiteral("已添加 %1 个表情，跳过 %2 个重复项").arg(added).arg(duplicates));return true;
}

void GuiStickerPicker::removeSelected()
{
  QListWidgetItem* item=m_grid->currentItem();if(!item)return;QString error;ShadowStickerStore store(m_accountFolder);
  if(!store.remove(item->data(Qt::UserRole).toString(),&error)){m_status->setText(error);return;}
  refresh();
}
