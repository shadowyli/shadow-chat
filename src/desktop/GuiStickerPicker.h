#ifndef GUI_STICKER_PICKER_H
#define GUI_STICKER_PICKER_H
#include <QDialog>
class QListWidget;
class QLabel;
class QPushButton;

class GuiStickerPicker : public QDialog
{
  Q_OBJECT
public:
  explicit GuiStickerPicker(const QString& accountFolder,QWidget* parent=Q_NULLPTR);
  bool importArchive(const QString& path);
  void refresh();
signals:
  void stickerSelected(const QString& originalPath);
private:
  void chooseArchive();
  void removeSelected();
  QString m_accountFolder;
  QListWidget* m_grid;
  QLabel* m_status;
  QPushButton* m_remove;
};
#endif
