#ifndef SHADOWDIRECTORYPAGE_H
#define SHADOWDIRECTORYPAGE_H

#include "Config.h"
#include <QMap>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class ShadowDirectoryPage : public QWidget
{
  Q_OBJECT

public:
  enum Section { Contacts = 0, Groups = 1, Organization = 2 };
  explicit ShadowDirectoryPage( QWidget* parent = Q_NULLPTR );
  void setSection( Section section );
  Section section() const { return m_section; }
  void setFilter( const QString& filter );
  void refresh();
  static QString initialForName( const QString& name );

signals:
  void userActivated( VNumber );
  void groupActivated( VNumber );
  void userDetailsRequested( VNumber );
  void createGroupRequested();
  void sectionChanged( int );

protected:
  void resizeEvent( QResizeEvent* ) override;

private:
  void activateItem( QTreeWidgetItem* );
  Section m_section;
  QString m_filter;
  QByteArray m_snapshot;
  bool m_resetScroll;
  QWidget* mp_sidebar;
  QLabel* mp_title;
  QLabel* mp_count;
  QLabel* mp_empty;
  QTreeWidget* mp_list;
  QComboBox* mp_sort;
  QWidget* mp_alphabet;
  QPushButton* mp_createGroup;
  QPushButton* mp_categories[3];
  QVector<QToolButton*> m_indexButtons;
  QMap<QString, QTreeWidgetItem*> m_headers;
};

#endif
