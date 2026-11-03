#include "ChatManager.h"
#include "BeeApplication.h"
#include "Core.h"
#include "GuiSavedChat.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "UserManager.h"
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static QString recordHtml( const QString& sender, const QDateTime& time, const QString& body )
{
  QJsonObject metadata;
  metadata.insert( "sender", sender );
  metadata.insert( "time", time.toString( Qt::ISODate ) );
  return QStringLiteral( "<!--shadow-record:%1--><p>%2</p><br>" ).arg(
    QString::fromLatin1( QJsonDocument( metadata ).toJson( QJsonDocument::Compact ).toBase64() ), body );
}

static QListWidgetItem* findResult( QListWidget* results, const QString& text )
{
  for( int row = 0; row < results->count(); ++row )
    if( results->item( row )->text().contains( text ) )
      return results->item( row );
  return Q_NULLPTR;
}

static QListWidgetItem* firstVisible( QListWidget* results )
{
  for( int row = 0; row < results->count(); ++row )
    if( results->visualItemRect( results->item( row ) ).intersects( results->viewport()->rect() ) )
      return results->item( row );
  return Q_NULLPTR;
}

static void settle( QApplication& application )
{
  for( int iteration = 0; iteration < 4; ++iteration )
    application.processEvents();
}

int main( int argc, char** argv )
{
  if( qEnvironmentVariableIsEmpty( "QT_QPA_PLATFORM" ) )
    qputenv( "QT_QPA_PLATFORM", "offscreen" );
  BeeApplication application( argc, argv );
  Random::init();
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "history-refresh-test" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  Core core;

  User peer( 97001 );
  peer.setName( "Synthetic history sender" );
  peer.setHash( "history-refresh-synthetic-peer" );
  UserManager::instance().setUser( peer );
  Group group;
  group.setId( 99001 );
  group.setName( "Synthetic history refresh" );
  group.setChatType( Group::GroupChat );
  group.addUser( ID_LOCAL_USER );
  group.addUser( peer.id() );
  Chat live;
  live.setGroup( group );
  live.addMessage( ChatMessage( peer.id(), Protocol::instance().chatMessage(live,QStringLiteral("live incoming 0")), ChatMessage::Chat, true ) );
  ChatManager::instance().setChat( live );

  const QDateTime base = QDateTime::currentDateTime().addSecs( -18000 );
  QString archive;
  for( int index = 0; index < 80; ++index )
    archive += recordHtml( index % 2 ? "Alice" : "Bob", base.addSecs( index * 60 ),
      QStringLiteral( "history message %1" ).arg( index, 3, 10, QLatin1Char('0') ) );
  const QString duplicate = recordHtml( "Alice", base.addSecs( 5000 ), "exact duplicate" );
  archive += duplicate + duplicate;
  QMap<QString,QString> histories;
  histories.insert( live.name(), archive );
  histories.insert( "Other synthetic history", recordHtml( "Carol", base, "other conversation" ) );
  ChatManager::instance().addSavedChats( histories );

  GuiSavedChat history( Q_NULLPTR, true );
  QList<QPair<QString,QString>> contextRequests;
  QObject::connect( &history, &GuiSavedChat::contextRequested,
    [&]( const QString& chatName, const QString& recordKey ) {
      contextRequests.append( qMakePair( chatName, recordKey ) );
    } );
  history.resize( 390, 620 );
  history.showSavedChat( live.name() );
  history.show();
  settle( application );
  QListWidget* results = history.findChild<QListWidget*>( "ShadowHistoryResults" );
  QStackedWidget* pages = history.findChild<QStackedWidget*>( "ShadowHistoryPages" );
  QTextBrowser* source = history.findChild<QTextBrowser*>();
  QLineEdit* keyword = history.findChild<QLineEdit*>( "GuiLineEditFilterHistory" );
  QTabBar* categories = history.findChild<QTabBar*>();
  QPushButton* context = history.findChild<QPushButton*>( "ShadowHistoryContext" );
  QComboBox* sender = Q_NULLPTR;
  QComboBox* dates = Q_NULLPTR;
  QComboBox* order = Q_NULLPTR;
  for( QComboBox* combo : history.findChildren<QComboBox*>() )
  {
    if( combo->accessibleName() == QStringLiteral("发送者") ) sender = combo;
    if( combo->accessibleName() == QStringLiteral("时间范围") ) dates = combo;
    if( combo->accessibleName() == QStringLiteral("排序") ) order = combo;
  }
  CHECK( results && pages && source && keyword && categories && context && sender && dates && order );
  CHECK( pages->count() == 1 && pages->currentWidget()->isAncestorOf( results ) );
  CHECK( results->count() == 83 );

  QListWidgetItem* selected = findResult( results, "history message 030" );
  CHECK( selected );
  const QString selectedKey = shadowHistoryRecordKeys( shadowChatHistoryRecords( archive ) ).at( 30 );
  CHECK( selected->data( Qt::UserRole + 7 ).toString() == selectedKey );
  results->setCurrentItem( selected );
  results->scrollToItem( selected, QAbstractItemView::PositionAtTop );
  results->verticalScrollBar()->setValue( results->verticalScrollBar()->value() + 17 );
  settle( application );
  const int oldRow = results->currentRow();
  QListWidgetItem* visible = firstVisible( results );
  CHECK( visible );
  const QString visibleText = visible->text();
  const int visibleTop = results->visualItemRect( visible ).top();
  live.addMessage( ChatMessage( peer.id(), Protocol::instance().chatMessage(live,QStringLiteral("live incoming 1")), ChatMessage::Chat, true ) );
  ChatManager::instance().setChat( live );
  history.showSavedChat( live.name() );
  settle( application );
  CHECK( results->currentItem() && results->currentItem()->text().contains( "history message 030" ) );
  CHECK( results->currentRow() == oldRow + 1 );
  CHECK( firstVisible( results ) && firstVisible( results )->text() == visibleText );
  CHECK( qAbs( results->visualItemRect( firstVisible( results ) ).top() - visibleTop ) <= 1 );
  CHECK( pages->currentIndex() == 0 && context->isEnabled() );

  context->click();
  settle( application );
  CHECK( contextRequests.size() == 1 );
  CHECK( contextRequests.last() == qMakePair( live.name(), selectedKey ) );
  CHECK( pages->count() == 1 && pages->currentIndex() == 0 && !source->isVisible() );
  CHECK( results->currentItem()->data( Qt::UserRole + 7 ).toString() == selectedKey );
  CHECK( firstVisible( results )->text() == visibleText );
  CHECK( qAbs( results->visualItemRect( firstVisible( results ) ).top() - visibleTop ) <= 1 );
  const int sourceScroll = source->verticalScrollBar()->value();
  const int hiddenListScroll = results->verticalScrollBar()->value();
  live.addMessage( ChatMessage( peer.id(), Protocol::instance().chatMessage(live,QStringLiteral("live incoming 2")), ChatMessage::Chat, true ) );
  ChatManager::instance().setChat( live );
  history.showSavedChat( live.name() );
  settle( application );
  CHECK( pages->count() == 1 && pages->currentIndex() == 0 && context->isEnabled() );
  CHECK( results->currentItem() && results->currentItem()->text().contains( "history message 030" ) );
  CHECK( results->currentItem()->data( Qt::UserRole + 7 ).toString() == selectedKey );
  CHECK( firstVisible( results )->text() == visibleText );
  CHECK( qAbs( results->visualItemRect( firstVisible( results ) ).top() - visibleTop ) <= 1 );
  CHECK( source->verticalScrollBar()->value() == sourceScroll );
  CHECK( results->verticalScrollBar()->value() > hiddenListScroll );
  CHECK( ChatManager::instance().chatSavedText( live.name() ) == archive );
  context->click();
  CHECK( contextRequests.size() == 2 && contextRequests.last() == qMakePair( live.name(), selectedKey ) );
  const int refreshedListScroll = results->verticalScrollBar()->value();
  history.showSavedChat( live.name() );
  settle( application );
  CHECK( pages->count() == 1 && pages->currentIndex() == 0 && source->verticalScrollBar()->value() == sourceScroll );
  CHECK( results->verticalScrollBar()->value() == refreshedListScroll );
  CHECK( contextRequests.size() == 2 );
  CHECK( results->currentItem()->data( Qt::UserRole + 7 ).toString() == selectedKey );

  keyword->setText( "history message" );
  settle( application );
  CHECK( pages->count() == 1 && pages->currentIndex() == 0 && context->isEnabled() );
  CHECK( results->currentItem() && results->currentItem()->data(Qt::UserRole+7).toString() == selectedKey );
  CHECK( source->verticalScrollBar()->value() == sourceScroll );
  CHECK( contextRequests.size() == 2 );
  keyword->setText( "history message 079" );
  settle( application );
  CHECK( pages->currentIndex() == 0 && !results->currentItem() && !context->isEnabled() );
  CHECK( results->count() == 1 && results->item(0)->text().contains("history message 079") );
  CHECK( results->verticalScrollBar()->value() == 0 && source->verticalScrollBar()->value() == 0 );
  keyword->clear();
  selected = findResult( results, "history message 030" );
  CHECK( selected );
  results->setCurrentItem( selected );
  const int oldRecordIndex = selected->data( Qt::UserRole ).toInt();
  histories[live.name()].prepend( recordHtml( "Earlier sender", base.addSecs(-60), "earlier archived record" ) );
  ChatManager::instance().addSavedChats( histories );
  history.showSavedChat( live.name() );
  CHECK( results->currentItem() && results->currentItem()->text().contains( "history message 030" ) );
  CHECK( results->currentItem()->data( Qt::UserRole ).toInt() == oldRecordIndex + 1 );
  CHECK( results->currentItem()->data( Qt::UserRole + 7 ).toString() == selectedKey );
  CHECK( ChatManager::instance().chatSavedText( live.name() ) == histories[live.name()] );

  for( int row = 0; row < results->count(); ++row )
    if( results->item(row)->data(Qt::UserRole).toInt() == 82 )
      results->setCurrentRow( row );
  CHECK( results->currentItem() && results->currentItem()->text().contains( "exact duplicate" ) );
  const QString duplicateKey = results->currentItem()->data( Qt::UserRole + 7 ).toString();
  histories[live.name()] += duplicate;
  ChatManager::instance().addSavedChats( histories );
  history.showSavedChat( live.name() );
  CHECK( results->currentItem() && results->currentItem()->data(Qt::UserRole).toInt() == 82 );
  CHECK( results->currentItem()->data( Qt::UserRole + 7 ).toString() == duplicateKey );
  context->click();
  CHECK( contextRequests.last() == qMakePair( live.name(), duplicateKey ) );
  CHECK( ChatManager::instance().chatSavedText( live.name() ) == histories[live.name()] );

  for( QComboBox* filter : QList<QComboBox*>{sender, dates, order} )
  {
    results->setCurrentItem( findResult(results,"history message 031") );
    context->click();
    const QString matchingFilterKey = results->currentItem()->data(Qt::UserRole+7).toString();
    const int matchingRequestCount = contextRequests.size();
    filter->setCurrentIndex(1);
    settle( application );
    CHECK( pages->count() == 1 && pages->currentIndex() == 0 && context->isEnabled() );
    CHECK( results->currentItem() && results->currentItem()->data(Qt::UserRole+7).toString() == matchingFilterKey );
    CHECK( contextRequests.size() == matchingRequestCount );
    context->click();
    CHECK( contextRequests.size() == matchingRequestCount + 1 );
    CHECK( contextRequests.last() == qMakePair(live.name(),matchingFilterKey) );
    filter->setCurrentIndex(0);
  }
  sender->setCurrentIndex(sender->findData("Bob"));
  settle(application);
  CHECK( pages->count() == 1 && pages->currentIndex() == 0 && !results->currentItem() && !context->isEnabled() );
  CHECK( results->verticalScrollBar()->value() == 0 && source->verticalScrollBar()->value() == 0 );
  sender->setCurrentIndex(0);

  sender->setCurrentIndex(sender->findData("Alice"));
  order->setCurrentIndex(1);
  keyword->setText("history message");
  results->setCurrentItem(findResult(results,"history message 031"));
  context->click();
  settle(application);
  const int filteredSourceScroll = source->verticalScrollBar()->value();
  const int filteredListScroll = results->verticalScrollBar()->value();
  const QString filteredKey = results->currentItem()->data( Qt::UserRole + 7 ).toString();
  CHECK( contextRequests.last() == qMakePair( live.name(), filteredKey ) );
  CHECK( pages->count() == 1 && pages->currentIndex() == 0 );
  live.addMessage( ChatMessage( peer.id(), Protocol::instance().chatMessage(live,QStringLiteral("unrelated new sender")), ChatMessage::Chat, true ) );
  ChatManager::instance().setChat(live);
  history.showSavedChat(live.name());
  settle(application);
  CHECK( sender->currentData().toString() == "Alice" && order->currentIndex() == 1 );
  CHECK( keyword->text() == "history message" && pages->count() == 1 && pages->currentIndex() == 0 );
  CHECK( results->currentItem() && results->currentItem()->text().contains("history message 031") );
  CHECK( results->currentItem()->data( Qt::UserRole + 7 ).toString() == filteredKey );
  CHECK( results->verticalScrollBar()->value() == filteredListScroll );
  CHECK( source->verticalScrollBar()->value() == filteredSourceScroll );
  CHECK( ChatManager::instance().chatSavedText( live.name() ) == histories[live.name()] );
  keyword->clear();
  sender->setCurrentIndex(0);
  order->setCurrentIndex(0);

  const QString firstImage = QUrl::fromLocalFile( temporary.filePath("first.png") ).toString();
  const QString secondImage = QUrl::fromLocalFile( temporary.filePath("second.png") ).toString();
  histories[live.name()] += recordHtml( "Alice", base.addSecs(6000),
    QStringLiteral("<a href=\"%1\">first</a> <a href=\"%2\">second</a>").arg(firstImage,secondImage) );
  ChatManager::instance().addSavedChats( histories );
  history.showSavedChat( live.name() );
  categories->setCurrentIndex(2);
  CHECK( results->count() == 3 );
  int mediaItems = 0;
  int dateHeaders = 0;
  for( int row = 0; row < results->count(); ++row )
  {
    if( results->item(row)->data(Qt::UserRole+8).toBool() )
    {
      ++dateHeaders;
      CHECK( !results->item(row)->flags().testFlag(Qt::ItemIsSelectable) );
    }
    else if( results->item(row)->data(Qt::UserRole+9).toBool() )
      ++mediaItems;
  }
  CHECK( mediaItems == 2 && dateHeaders == 1 );
  for( int row = 0; row < results->count(); ++row )
    if( results->item(row)->data(Qt::UserRole+1).toString() == secondImage )
      results->setCurrentRow(row);
  CHECK( results->currentItem() );
  const QString mediaKey = results->currentItem()->data(Qt::UserRole+7).toString().section( QLatin1Char('\n'), 0, 0 );
  live.addMessage( ChatMessage( peer.id(), Protocol::instance().chatMessage(live,QStringLiteral("live incoming 3")), ChatMessage::Chat, true ) );
  ChatManager::instance().setChat(live);
  history.showSavedChat(live.name());
  CHECK( results->currentItem() && results->currentItem()->data(Qt::UserRole+1).toString() == secondImage );
  CHECK( results->currentItem()->data(Qt::UserRole+7).toString().section( QLatin1Char('\n'), 0, 0 ) == mediaKey );
  context->click();
  CHECK( contextRequests.last() == qMakePair( live.name(), mediaKey ) );
  categories->setCurrentIndex(0);
  CHECK( pages->currentIndex() == 0 && !results->currentItem() );

  results->setCurrentItem(findResult(results,"exact duplicate"));
  context->click();
  histories[live.name()].remove(duplicate);
  ChatManager::instance().addSavedChats(histories);
  history.showSavedChat(live.name());
  CHECK( pages->currentIndex() == 0 && !results->currentItem() && !context->isEnabled() );
  const int requestsBeforeMissingSelection = contextRequests.size();
  context->click();
  CHECK( contextRequests.size() == requestsBeforeMissingSelection );
  CHECK( ChatManager::instance().chatSavedText( live.name() ) == histories[live.name()] );

  results->setCurrentItem( findResult(results,"history message 030") );
  context->click();
  history.showSavedChat("Other synthetic history");
  settle(application);
  CHECK( pages->currentIndex() == 0 && !results->currentItem() && !context->isEnabled() );
  CHECK( results->count() == 1 && results->item(0)->text().contains("other conversation") );
  CHECK( results->verticalScrollBar()->value() == 0 && source->verticalScrollBar()->value() == 0 );
  CHECK( pages->count() == 1 && !source->isVisible() );
  CHECK( ChatManager::instance().chatSavedText( live.name() ) == histories[live.name()] );
  std::puts("PASS history refresh: stable context signal keys, one results page, preserved reading position, unchanged archive, reordered results, repeated refresh, shifted indices, duplicates, active filters, date-grouped media, missing selection and conversation reset");
  return 0;
}
