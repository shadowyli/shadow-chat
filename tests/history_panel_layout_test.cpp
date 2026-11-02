#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiSavedChat.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowSearchPanel.h"
#include "ShadowStyle.h"
#include "ShadowVideoPreview.h"
#include "UserManager.h"
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static void settle()
{
  for( int iteration = 0; iteration < 6; ++iteration )
    QApplication::processEvents();
}

static bool contained( QWidget* widget, QWidget* ancestor )
{
  return ancestor->rect().contains( QRect( widget->mapTo( ancestor, QPoint() ), widget->size() ) );
}

static QString record( const QString& sender, const QDateTime& time, const QString& text )
{
  QJsonObject metadata;
  metadata.insert( "sender", sender );
  metadata.insert( "time", time.toString( Qt::ISODate ) );
  return QStringLiteral( "<!--shadow-record:%1--><p>%2</p><br>" ).arg(
    QString::fromLatin1( QJsonDocument( metadata ).toJson( QJsonDocument::Compact ).toBase64() ), text );
}

static bool compactTabs( QTabBar* tabs )
{
  for( int index = 0; index < tabs->count(); ++index )
  {
    const QRect area = tabs->tabRect( index );
    const int textWidth = tabs->fontMetrics().horizontalAdvance( tabs->tabText( index ) );
    std::printf( "TAB %s rect %d,%d %dx%d text %d\n", qPrintable( tabs->tabText( index ) ),
      area.x(), area.y(), area.width(), area.height(), textWidth );
    if( !tabs->rect().contains( area ) || area.width() < textWidth ||
        area.width() > textWidth + 28 ) return false;
    if( index > 0 && area.left() != tabs->tabRect( index - 1 ).right() + 1 ) return false;
  }
  return true;
}

static bool fittedUnderline( QTabBar* tabs )
{
  const QPixmap capture = tabs->grab();
  const QImage image = capture.toImage();
  const qreal ratio = capture.devicePixelRatioF();
  int left = image.width();
  int right = -1;
  for( int vertical = qMax( 0, image.height() - qCeil( 5 * ratio ) ); vertical < image.height(); ++vertical )
    for( int horizontal = 0; horizontal < image.width(); ++horizontal )
    {
      const QColor color = image.pixelColor( horizontal, vertical );
      if( color.blue() > 180 && color.red() < 80 && color.green() > 70 && color.green() < 170 )
      {
        left = qMin( left, horizontal );
        right = qMax( right, horizontal );
      }
    }
  if( right < left ) return false;
  const QRect current = tabs->tabRect( tabs->currentIndex() );
  const int textWidth = tabs->fontMetrics().horizontalAdvance( tabs->tabText( tabs->currentIndex() ) );
  const qreal underlineWidth = (right - left + 1) / ratio;
  const qreal center = (left + right + 1) / (2 * ratio);
  std::printf( "UNDERLINE %s width %.1f center %.1f text %d\n",
    qPrintable( tabs->tabText( tabs->currentIndex() ) ), underlineWidth, center, textWidth );
  return qAbs( underlineWidth - textWidth ) <= 3 && qAbs( center - (current.x() + current.width() / 2.0) ) <= 2;
}

static void key( QWidget* target, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier )
{
  QKeyEvent event( QEvent::KeyPress, code, modifiers );
  QApplication::sendEvent( target, &event );
  settle();
}

static QList<QListWidgetItem*> mediaItems( QListWidget* results )
{
  QList<QListWidgetItem*> items;
  for( int row = 0; row < results->count(); ++row )
    if( !results->item(row)->data(Qt::UserRole+1).toString().isEmpty() ) items.append(results->item(row));
  return items;
}

static bool twoColumns( QListWidget* results )
{
  results->doItemsLayout();
  QMap<int,QList<QRect>> rows;
  int headers=0;
  int previousHeaderBottom=-1;
  for( int index=0; index<results->count(); ++index )
  {
    QListWidgetItem* item=results->item(index);
    const QRect area=results->visualItemRect(item);
    std::printf("MEDIA rect %d,%d %dx%d header %d\n",area.x(),area.y(),area.width(),area.height(),
      item->data(Qt::UserRole).toInt()<0);
    if(item->data(Qt::UserRole).toInt()<0)
    {
      if(area.width()<results->viewport()->width()-2 || (item->flags()&Qt::ItemIsSelectable))return false;
      previousHeaderBottom=area.bottom();
      ++headers;
    }
    else
    {
      if(!item->text().isEmpty() || area.width()>results->viewport()->width()/2+1 ||
        area.top()<=previousHeaderBottom || area.left()<0 || area.right()>=results->viewport()->width())return false;
      rows[area.top()].append(area);
    }
  }
  int paired=0;
  for(const QList<QRect>& row:rows)
  {
    if(row.size()>2)return false;
    if(row.size()==2)
    {
      if(row.first().intersects(row.last()))return false;
      ++paired;
    }
  }
  return headers==3 && paired>=2 && results->horizontalScrollBar()->maximum()==0;
}

static void clickItem( QListWidget* results, QListWidgetItem* item )
{
  results->scrollToItem(item,QAbstractItemView::PositionAtCenter);
  settle();
  const QPoint point=results->visualItemRect(item).center();
  QMouseEvent press(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
  QApplication::sendEvent(results->viewport(),&press);
  QMouseEvent release(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
  QApplication::sendEvent(results->viewport(),&release);
  settle();
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  BeeApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  Random::init();
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( "history-layout-fixture" );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  Core core;
  User peer( 97321 );
  peer.setName( QStringLiteral( "测试同事" ) );
  peer.setHash( "history-layout-synthetic-peer" );
  UserManager::instance().setUser( peer );
  const QDateTime base = QDateTime::currentDateTime().addSecs( -6000 );
  QString archive;
  for( int index = 0; index < 80; ++index )
    archive += record( index % 2 ? "Alice" : "Bob", base.addSecs( index * 60 ),
      QStringLiteral( "layout message %1 · 阅读位置与上下文保持" ).arg( index, 3, 10, QLatin1Char( '0' ) ) );
  CHECK(argc>1);
  const QString videoSource=QString::fromLocal8Bit(argv[1]);
  CHECK(QFileInfo(videoSource).size()>1000);
  CHECK(QFile::copy(videoSource,temporary.filePath("sample.mp4")));
  const QString videoUrl=QUrl::fromLocalFile(temporary.filePath("sample.mp4")).toString();
  QStringList images;
  for(int index=0;index<5;++index)
  {
    const QString filename=temporary.filePath(QString("sample-%1.png").arg(index));
    QPixmap sample(320,220);
    sample.fill(QColor(QStringList{"#D8E8FF","#E2E8E1","#F3E4D5","#DEDFF4","#D7EBEB"}.at(index)));
    QPainter painter(&sample);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#7190B2"));
    painter.drawRoundedRect(QRect(35+index*8,45,180,125),14,14);
    painter.setBrush(QColor("#F5FAFF"));
    painter.drawEllipse(QPoint(205,80),26,26);
    painter.end();
    CHECK(sample.save(filename));
    images.append(QUrl::fromLocalFile(filename).toString());
  }
  const QString imageUrl = images.first();
  const QString fileUrl = QUrl::fromLocalFile( temporary.filePath( "sample.txt" ) ).toString();
  const QDateTime today(QDate::currentDate(),QTime(12,0));
  for(int index=0;index<5;++index)
    archive+=record(index%2?"Bob":"Alice",index<2?today.addSecs(index*60):index<4?today.addDays(-1).addSecs(index*60):QDateTime(),
      QStringLiteral("<a href=\"%1\">测试图片</a>").arg(images.at(index)));
  archive+=record("Alice",today.addSecs(120),QStringLiteral("<a href=\"%1\">测试视频</a>").arg(videoUrl));
  archive += record( "Bob", base, QStringLiteral( "<a href=\"%1\">测试文件</a>" ).arg( fileUrl ) );
  archive += record( "Alice", base, QStringLiteral( "<a href=\"https://example.test/design\">测试链接</a>" ) );
  const QString duplicate=record("Alice",base,QStringLiteral("duplicate anchored message"));
  archive+=duplicate+duplicate;
  QMap<QString, QString> histories;
  histories.insert( "luoliv", archive );
  ChatManager::instance().addSavedChats( histories );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR", QDir::currentPath() );
  QDir().mkpath( proof );
  QWidget host;
  host.setStyleSheet( "QWidget { background:white; }" );
  QHBoxLayout* hostLayout = new QHBoxLayout( &host );
  hostLayout->setContentsMargins( 0, 0, 0, 0 );
  hostLayout->setSpacing( 0 );
  QLabel* chat = new QLabel( QStringLiteral( "聊天区\n\n测试同事\n历史面板保持独立边界" ), &host );
  chat->setAlignment( Qt::AlignTop );
  chat->setStyleSheet( "padding:20px;color:#52647F;" );
  chat->setFixedWidth( 260 );
  hostLayout->addWidget( chat );
  GuiSavedChat* history = new GuiSavedChat( &host, true );
  hostLayout->addWidget( history );
  history->showSavedChat( "luoliv" );
  host.resize( 580, 560 );
  host.show();
  settle();
  QTabBar* tabs = history->findChild<QTabBar*>( "ShadowHistoryTabs" );
  QListWidget* results = history->findChild<QListWidget*>( "ShadowHistoryResults" );
  QStackedWidget* pages = history->findChild<QStackedWidget*>( "ShadowHistoryPages" );
  QTextBrowser* source = history->findChild<QTextBrowser*>();
  QPushButton* context = history->findChild<QPushButton*>( "ShadowHistoryContext" );
  QLineEdit* keyword = history->findChild<QLineEdit*>( "GuiLineEditFilterHistory" );
  QWidget* edge = history->findChild<QWidget*>( "ShadowHistoryEdge" );
  CHECK( tabs && results && pages && source && context && keyword && edge );
  CHECK(pages->count()==1 && !source->isVisible());
  int contextRequests=0;
  QString requestedChat,requestedKey;
  QObject::connect(history,&GuiSavedChat::contextRequested,history,
    [&](const QString& chatName,const QString& recordKey) {
      ++contextRequests;requestedChat=chatName;requestedKey=recordKey;
    });
  CHECK( !history->findChild<QLabel*>( "ShadowHistoryConversation" ) );
  CHECK( history->windowTitle().contains( "luoliv" ) );
  CHECK( edge->width() == 7 && edge->height() >= history->height() - 2 );
  const QPixmap edgePixmap = edge->grab();
  const QImage edgeImage = edgePixmap.toImage();
  CHECK( edgeImage.pixelColor( 0, edgeImage.height() / 2 ).red() >
    edgeImage.pixelColor( edgeImage.width() - 1, edgeImage.height() / 2 ).red() );
  CHECK( results->count() == 90 );
  CHECK(QMetaObject::invokeMethod(history,"showHistoryContext",Qt::DirectConnection));
  CHECK(contextRequests==0 && !results->currentItem());
  host.resize( 580, 1 );
  settle();
  CHECK( contained( keyword, history ) && contained( context, history ) );
  CHECK( host.grab().save( proof + "/history-minimum-height.png" ) );
  for( int width : {320, 360, 400} )
  {
    host.resize( width + chat->width(), 560 );
    settle();
    CHECK( history->width() == width );
    CHECK( compactTabs( tabs ) );
    CHECK( contained( keyword, history ) && contained( context, history ) );
    for( QComboBox* filter : history->findChildren<QComboBox*>() )
    {
      QStyleOptionComboBox option;
      option.initFrom( filter );
      option.editable = false;
      const QRect text = filter->style()->subControlRect( QStyle::CC_ComboBox, &option,
        QStyle::SC_ComboBoxEditField, filter );
      CHECK( contained( filter, history ) );
      std::printf( "FILTER %s text %d available %d widget %d\n", qPrintable( filter->currentText() ),
        filter->fontMetrics().horizontalAdvance( filter->currentText() ), text.width(), filter->width() );
      CHECK( text.width() >= filter->fontMetrics().horizontalAdvance( filter->currentText() ) );
    }
    for( int category = 0; category < tabs->count(); ++category )
    {
      tabs->setCurrentIndex( category );
      settle();
      CHECK( fittedUnderline( tabs ) );
      CHECK( category == 0 ? results->count() == 90 : category==2?mediaItems(results).size()==6:results->count()==1 );
      if( category == 2 )
      {
        CHECK(twoColumns(results));
        CHECK(mediaItems(results).first()->data(Qt::UserRole+1).toString()==videoUrl);
      }
      CHECK( host.grab().save( proof + QStringLiteral( "/history-%1-tab-%2.png" ).arg( width ).arg( category ) ) );
    }
  }
  tabs->setCurrentIndex( 0 );
  keyword->setText( "layout message" );
  QComboBox* sender = nullptr;
  QComboBox* order = nullptr;
  for( QComboBox* filter : history->findChildren<QComboBox*>() )
  {
    if( filter->accessibleName() == QStringLiteral( "发送者" ) ) sender = filter;
    if( filter->accessibleName() == QStringLiteral( "排序" ) ) order = filter;
  }
  CHECK( sender && order );
  sender->setCurrentIndex( sender->findData( "Alice" ) );
  order->setCurrentIndex( 1 );
  CHECK( results->count() == 40 );
  results->setCurrentRow( 18 );
  results->scrollToItem( results->currentItem(), QAbstractItemView::PositionAtTop );
  settle();
  const QString selected = results->currentItem()->text();
  context->click();
  settle();
  CHECK( pages->currentIndex() == 0 && contextRequests==1 && requestedChat=="luoliv" );
  const QList<HistoryRecord> originalRecords=shadowChatHistoryRecords(histories["luoliv"]);
  const QStringList originalKeys=shadowHistoryRecordKeys(originalRecords);
  CHECK(requestedKey==originalKeys.at(results->currentItem()->data(Qt::UserRole).toInt()));
  CHECK(!source->isVisible() && !source->isAncestorOf(context));
  const QString selectedKey=requestedKey;
  const int listPosition=results->verticalScrollBar()->value();
  histories["luoliv"] += record( "Bob", base.addSecs( 8000 ), "new unrelated record" );
  ChatManager::instance().addSavedChats( histories );
  history->showSavedChat( "luoliv" );
  settle();
  CHECK( pages->currentIndex() == 0 && results->currentItem()->text() == selected );
  CHECK(results->verticalScrollBar()->value()==listPosition);
  CHECK( sender->currentData().toString() == "Alice" && order->currentIndex() == 1 );
  CHECK( keyword->text() == "layout message" );
  CHECK( ChatManager::instance().chatSavedText( "luoliv" ) == histories["luoliv"] );
  context->click();
  CHECK(contextRequests==2 && requestedKey==selectedKey);
  keyword->clear();
  sender->setCurrentIndex(0);
  tabs->setCurrentIndex(2);
  order->setCurrentIndex(0);
  settle();
  QListWidgetItem* video=nullptr;
  QListWidgetItem* image=nullptr;
  for(QListWidgetItem* tile:mediaItems(results))
  {
    if(tile->data(Qt::UserRole+1).toString()==videoUrl)video=tile;
    if(tile->data(Qt::UserRole+1).toString()==imageUrl)image=tile;
  }
  CHECK(video && image);
  QUrl opened;
  QObject::connect(history,&GuiSavedChat::openUrl,history,[&](const QUrl& url){opened=url;});
  clickItem(results,video);
  CHECK(opened==QUrl(videoUrl) && ShadowVideoPreview::canPreview(opened));
  CHECK(results->currentItem()==video && contextRequests==2);
  opened=QUrl();
  key(results,Qt::Key_Return);
  CHECK(opened==QUrl(videoUrl) && contextRequests==2);
  const QString videoKey=shadowHistoryRecordKeys(shadowChatHistoryRecords(histories["luoliv"])).at(video->data(Qt::UserRole).toInt());
  context->click();
  CHECK(contextRequests==3 && requestedKey==videoKey && pages->currentIndex()==0);
  order->setCurrentIndex(1);
  settle();
  CHECK(results->currentItem() && results->currentItem()->data(Qt::UserRole+1).toString()==videoUrl);
  CHECK(twoColumns(results));
  context->click();
  CHECK(contextRequests==4 && requestedKey==videoKey);
  histories["luoliv"].prepend(record("Earlier",today.addDays(-4),"earlier archived message"));
  ChatManager::instance().addSavedChats(histories);
  history->showSavedChat("luoliv");
  settle();
  CHECK(results->currentItem() && results->currentItem()->data(Qt::UserRole+1).toString()==videoUrl);
  context->click();
  CHECK(contextRequests==5 && requestedKey==videoKey);
  bool imagePreview=false;
  QTimer::singleShot(80,history,[&]() {
    QDialog* preview=qobject_cast<QDialog*>(QApplication::activeModalWidget());
    imagePreview=preview && preview->findChild<QGraphicsView*>();
    if(preview)preview->reject();
  });
  image=nullptr;
  for(QListWidgetItem* tile:mediaItems(results))
    if(tile->data(Qt::UserRole+1).toString()==imageUrl)image=tile;
  CHECK(image);
  clickItem(results,image);
  CHECK(imagePreview && contextRequests==5);
  context->click();
  CHECK(contextRequests==6);
  const QString imageKey=requestedKey;
  for(int width:{320,360,400})
  {
    host.resize(width+chat->width(),560);
    settle();
    CHECK(twoColumns(results));
    CHECK(results->currentItem() && results->currentItem()->data(Qt::UserRole+1).toString()==imageUrl);
    context->click();
    CHECK(requestedKey==imageKey);
  }
  sender->setCurrentIndex(sender->findData("Alice"));
  CHECK(mediaItems(results).size()==4 && results->currentItem());
  QComboBox* dates=nullptr;
  for(QComboBox* filter:history->findChildren<QComboBox*>())
    if(filter->accessibleName()==QStringLiteral("时间范围"))dates=filter;
  CHECK(dates);
  dates->setCurrentIndex(1);
  CHECK(mediaItems(results).size()==2 && results->currentItem());
  keyword->setText("sample.mp4");
  CHECK(mediaItems(results).size()==1 && !results->currentItem());
  CHECK(mediaItems(results).first()->text().isEmpty());
  results->setCurrentItem(mediaItems(results).first());
  context->click();
  CHECK(requestedKey==videoKey);
  keyword->clear();
  sender->setCurrentIndex(0);
  dates->setCurrentIndex(0);
  CHECK(twoColumns(results));
  image=nullptr;
  for(QListWidgetItem* tile:mediaItems(results))
    if(tile->data(Qt::UserRole+1).toString()==imageUrl)image=tile;
  CHECK(image);
  results->setCurrentItem(image);
  context->click();
  CHECK(requestedKey==imageKey);
  const int beforeMissing=contextRequests;
  const QString imageRecord=record("Alice",today,QStringLiteral("<a href=\"%1\">测试图片</a>").arg(imageUrl));
  histories["luoliv"].remove(imageRecord);
  ChatManager::instance().addSavedChats(histories);
  context->click();
  CHECK(contextRequests==beforeMissing && !context->isEnabled());
  CHECK(ChatManager::instance().chatSavedText("luoliv")==histories["luoliv"]);
  history->showSavedChat("luoliv");
  CHECK(!results->currentItem() && !context->isEnabled());
  tabs->setCurrentIndex(0);
  keyword->setText("duplicate anchored message");
  CHECK(results->count()==2);
  results->setCurrentRow(0);
  context->click();
  const QString duplicateFirst=requestedKey;
  results->setCurrentRow(1);
  context->click();
  CHECK(requestedKey!=duplicateFirst);
  CHECK(requestedKey.section(':',0,0)==duplicateFirst.section(':',0,0));
  hostLayout->removeWidget(history);
  chat->setFixedWidth(640);
  host.resize(640,620);
  history->setGeometry(320,0,320,620);
  history->raise();
  const QRect chatBefore=chat->geometry();
  settle();
  tabs->setCurrentIndex(2);
  keyword->clear();
  CHECK(chat->geometry()==chatBefore && history->width()==320);
  CHECK(host.grab().save(proof+"/history-overlay-host.png"));
  host.hide();
  QWidget searchHost;
  searchHost.resize( 760, 620 );
  ShadowSearchPanel search( &searchHost );
  searchHost.show();
  search.open( QStringLiteral( "测试同事" ) );
  search.refresh();
  settle();
  QTabBar* searchTabs = search.findChild<QTabBar*>( "ShadowSearchTabs" );
  QLineEdit* searchInput = search.findChild<QLineEdit*>( "ShadowSearchInput" );
  QTreeWidget* searchResults = search.findChild<QTreeWidget*>( "ShadowGlobalSearchResults" );
  CHECK( searchTabs && searchInput && searchResults && compactTabs( searchTabs ) );
  for( int width : {360, 460, 640} )
  {
    searchHost.resize( width, 520 );
    settle();
    CHECK( contained( &search, &searchHost ) && contained( searchInput, &search ) );
    CHECK( compactTabs( searchTabs ) );
    for( QLabel* hint : search.findChildren<QLabel*>() )
      if( hint->isVisible() ) CHECK( contained( hint, &search ) );
    CHECK( searchHost.grab().save( proof + QStringLiteral( "/search-width-%1.png" ).arg( width ) ) );
  }
  searchHost.resize( 760, 620 );
  settle();
  for( int category = 0; category < searchTabs->count(); ++category )
  {
    searchTabs->setCurrentIndex( category );
    settle();
    CHECK( fittedUnderline( searchTabs ) );
    CHECK( searchHost.grab().save( proof + QStringLiteral( "/search-tab-%1.png" ).arg( category ) ) );
  }
  searchTabs->setCurrentIndex( 0 );
  key( searchInput, Qt::Key_Tab );
  CHECK( searchTabs->currentIndex() == 1 && searchInput->text() == QStringLiteral( "测试同事" ) );
  key( searchInput, Qt::Key_Backtab, Qt::ShiftModifier );
  CHECK( searchTabs->currentIndex() == 0 );
  key( searchInput, Qt::Key_Tab );
  CHECK( searchResults->currentItem() );
  VNumber activated = ID_INVALID;
  QObject::connect( &search, &ShadowSearchPanel::contactActivated, &searchHost,
    [&activated]( VNumber userId ) { activated = userId; } );
  key( searchInput, Qt::Key_Return );
  CHECK( activated == peer.id() && !search.isVisible() );
  search.open( QStringLiteral( "测试同事" ) );
  settle();
  key( searchInput, Qt::Key_Escape );
  CHECK( !search.isVisible() );
  std::printf( "PASS %s DPR %.2f: history 320/360/400 two-column dated media, no tile captions, image click/native video route, outward context only, shared exact keys and duplicate occurrence, sort/refresh/resize selection, missing original rejects, immutable manager archive, overlay host bounds, original tabs/filter/search regressions\n",
    qPrintable( QApplication::platformName() ), host.devicePixelRatioF() );
  return 0;
}
