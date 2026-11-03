#ifndef GUI_STICKER_PICKER_H
#define GUI_STICKER_PICKER_H
#include "ShadowPopup.h"
#include <QPointer>
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QLineEdit;
class QFileDialog;
class QMenu;
class StickerImportDialogTest;

class GuiStickerPicker : public ShadowPopup
{
  Q_OBJECT
public:
  explicit GuiStickerPicker(const QString& accountFolder,QWidget* parent=Q_NULLPTR);
  bool importArchive(const QString& path);
  void refresh();
  void showAbove( QWidget* anchor );
  QString accountFolder() const { return m_accountFolder; }
signals:
  void stickerSelected(const QString& originalPath);
protected:
  virtual int execArchiveDialog( QFileDialog& dialog );
  virtual QString defaultArchiveDirectory() const;
private:
  friend class StickerImportDialogTest;
  static QString preferredArchiveDirectory( const QString& previous, const QString& downloads,
                                           const QString& documents, const QString& home );
  void chooseArchive();
  void removeSelected();
  void chooseImages();
  void activateSticker( QListWidgetItem* );
  QString m_accountFolder;
  QListWidget* m_grid;
  QLabel* m_status;
  QPushButton* m_remove;
  QLineEdit* m_search;
  QMenu* m_addMenu;
  QPointer<QWidget> m_anchor;
  bool m_recent;
  bool m_managing;
};
#endif
