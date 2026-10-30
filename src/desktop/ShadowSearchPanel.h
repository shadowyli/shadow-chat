#ifndef SHADOWSEARCHPANEL_H
#define SHADOWSEARCHPANEL_H

#include "Config.h"

class ShadowSearchPanel : public QWidget
{
  Q_OBJECT

public:
  explicit ShadowSearchPanel( QWidget* parent );
  void open( const QString& query );
  void setQuery( const QString& query );
  void refresh();
  void dismiss();

signals:
  void queryChanged( const QString& );
  void dismissed();
  void contactActivated( VNumber );
  void groupActivated( VNumber );
  void messageActivated( VNumber, const QString&, const QString&, bool );

protected:
  bool eventFilter( QObject*, QEvent* ) override;

private:
  void positionPanel();
  void moveSelection( int direction );
  void activateItem( QTreeWidgetItem* );
  QLineEdit* mp_search;
  QTabBar* mp_tabs;
  QTreeWidget* mp_results;
  QLabel* mp_empty;
  QLabel* mp_scope;
  QTimer m_debounce;
};

#endif
