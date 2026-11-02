#ifndef GUI_STICKER_PICKER_H
#define GUI_STICKER_PICKER_H
#include <QDialog>
#include <QPointer>
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QLineEdit;
class QFileDialog;
class QMenu;
class StickerImportDialogTest;

class GuiStickerPicker : public QDialog
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
  void positionAboveAnchor();
  void scheduleAnchorPosition();
  void removeSelected();
  void chooseImages();
  void activateSticker( QListWidgetItem* );
  bool eventFilter( QObject*, QEvent* ) override;
  QString m_accountFolder;
  QListWidget* m_grid;
  QLabel* m_status;
  QPushButton* m_remove;
  QLineEdit* m_search;
  QMenu* m_addMenu;
  QPointer<QWidget> m_anchor;
  QPointer<QWidget> m_owner;
  bool m_recent;
  bool m_managing;
  bool m_positionPending;
};
#endif
