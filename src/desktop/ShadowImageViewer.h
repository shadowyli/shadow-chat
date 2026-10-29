#ifndef SHADOWCHAT_IMAGEVIEWER_H
#define SHADOWCHAT_IMAGEVIEWER_H
#include "Config.h"
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QDesktopServices>
#include <QImageReader>

class ShadowImageCanvas : public QGraphicsView
{
public:
  explicit ShadowImageCanvas(QWidget* parent):QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    QPixmap checks(24,24);checks.fill(QColor("#d7d9df"));QPainter painter(&checks);
    painter.fillRect(0,0,12,12,QColor("#f3f4f7"));painter.fillRect(12,12,12,12,QColor("#f3f4f7"));painter.end();
    setBackgroundBrush(QBrush(checks));
  }
  void showImage(const QPixmap& image) { scene()->clear();scene()->addPixmap(image);scene()->setSceneRect(image.rect());resetTransform();fitInView(scene()->sceneRect(),Qt::KeepAspectRatio); }
protected:
  void wheelEvent(QWheelEvent* event) override {
    const double factor=event->angleDelta().y()>0?1.2:1.0/1.2;
    const double next=transform().m11()*factor;
    if(next>=0.01 && next<=32) scale(factor,factor);
    event->accept();
  }
};

class ShadowImageViewer : public QDialog
{
public:
  explicit ShadowImageViewer(const QStringList& paths,const QString& selected,QWidget* parent=Q_NULLPTR):QDialog(parent),m_paths(paths),m_index(qMax(0,paths.indexOf(selected))) {
    setWindowTitle(QString::fromUtf8("图片查看 - 影聊"));resize(1000,740);
    QVBoxLayout* layout=new QVBoxLayout(this);m_info=new QLabel(this);m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(m_info);
    m_canvas=new ShadowImageCanvas(this);layout->addWidget(m_canvas,1);
    QHBoxLayout* actions=new QHBoxLayout;layout->addLayout(actions);
    addButton(actions,QString::fromUtf8("上一张"),[this](){navigate(-1);});
    addButton(actions,QString::fromUtf8("下一张"),[this](){navigate(1);});
    addButton(actions,QString::fromUtf8("适应窗口"),[this](){m_canvas->fitInView(m_canvas->scene()->sceneRect(),Qt::KeepAspectRatio);});
    addButton(actions,QString::fromUtf8("原始大小"),[this](){m_canvas->resetTransform();});
    addButton(actions,QString::fromUtf8("保存原图"),[this](){save(false);});
    addButton(actions,QString::fromUtf8("整组保存"),[this](){save(true);});
    addButton(actions,QString::fromUtf8("打开所在文件夹"),[this](){if(!m_paths.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_paths[m_index]).absolutePath()));});
    QShortcut* previous=new QShortcut(QKeySequence(Qt::Key_Left),this);connect(previous,&QShortcut::activated,this,[this](){navigate(-1);});
    QShortcut* next=new QShortcut(QKeySequence(Qt::Key_Right),this);connect(next,&QShortcut::activated,this,[this](){navigate(1);});
    showCurrent();
  }
  static bool supports(const QString& path) {
    return QStringList({"jpg","jpeg","png","gif","bmp","webp"}).contains(QFileInfo(path).suffix().toLower());
  }
protected:
  void showEvent(QShowEvent* event) override { QDialog::showEvent(event);m_canvas->fitInView(m_canvas->scene()->sceneRect(),Qt::KeepAspectRatio); }
private:
  template<class Callback> void addButton(QHBoxLayout* layout,const QString& name,Callback action) {
    QPushButton* button=new QPushButton(name,this);layout->addWidget(button);connect(button,&QPushButton::clicked,this,action);
  }
  void navigate(int direction) { if(m_paths.isEmpty())return;m_index=(m_index+direction+m_paths.size())%m_paths.size();showCurrent(); }
  void showCurrent() {
    if(m_paths.isEmpty())return;
    const QFileInfo file(m_paths[m_index]);QImageReader reader(file.absoluteFilePath());reader.setAutoTransform(true);const QImage image=reader.read();
    m_canvas->scene()->clear();
    if(image.isNull()) { m_info->setText(QString::fromUtf8("%1 · 文件已移动、删除或无法读取").arg(file.fileName()));return; }
    m_canvas->showImage(QPixmap::fromImage(image));
    m_info->setText(QString::fromUtf8("%1 / %2 · %3 · %4 × %5 · %6").arg(m_index+1).arg(m_paths.size()).arg(file.fileName()).arg(image.width()).arg(image.height()).arg(QLocale().formattedDataSize(file.size())));
  }
  void save(bool all) {
    if(m_paths.isEmpty())return;
    const QString directory=QFileDialog::getExistingDirectory(this,QString::fromUtf8("选择原图保存位置"));if(directory.isEmpty())return;
    const QStringList selected=all?m_paths:QStringList(m_paths[m_index]);int copied=0,failed=0;
    foreach(const QString& path,selected) {
      const QFileInfo file(path);if(!file.isFile()){++failed;continue;}
      QString destination=QDir(directory).filePath(file.fileName());
      if(QFileInfo(destination).absoluteFilePath()==file.absoluteFilePath()){++copied;continue;}
      int number=1;while(QFileInfo::exists(destination))destination=QDir(directory).filePath(file.completeBaseName()+QString(" (%1)").arg(number++)+(file.suffix().isEmpty()?QString():"."+file.suffix()));
      if(QFile::copy(path,destination))++copied;else ++failed;
    }
    QMessageBox::information(this,windowTitle(),QString::fromUtf8("已保存 %1 张原图，失败 %2 张。原文件质量保持不变。").arg(copied).arg(failed));
  }
  QStringList m_paths;int m_index;QLabel* m_info;ShadowImageCanvas* m_canvas;
};
#endif
