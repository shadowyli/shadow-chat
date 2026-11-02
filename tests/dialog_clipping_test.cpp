#include "ShadowDialogChrome.h"
#include "ShadowDialogStyle.h"
#include "GuiWizard.h"
#include <QtWidgets>
#include <cstdio>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while(false)

static void settle()
{
  for( int iteration = 0; iteration < 6; ++iteration )
    QApplication::processEvents();
}

static QRect mappedRect( QWidget* widget, QWidget* ancestor )
{
  return QRect( widget->mapTo( ancestor, QPoint() ), widget->size() );
}

static bool contained( QWidget* widget, QWidget* ancestor )
{
  return ancestor->rect().contains( mappedRect( widget, ancestor ) );
}

class ResizeEvents final : public QObject
{
public:
  int changes = 0;

protected:
  bool eventFilter( QObject*, QEvent* event ) override
  {
    if( event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest )
      ++changes;
    return false;
  }
};

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  QApplication application( argc, argv );
  application.setFont( ShadowStyle::applicationFont() );
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  ShadowDialogChrome::install( &application );
  QDialog dialog;
  dialog.setWindowTitle( QStringLiteral( "@ 同事 · 选择要提醒的联系人" ) );
  ShadowDialogStyle::apply( &dialog );
  QVBoxLayout* original = new QVBoxLayout( &dialog );
  original->setContentsMargins( 20, 16, 20, 16 );
  QLineEdit* search = new QLineEdit( &dialog );
  search->setPlaceholderText( QStringLiteral( "搜索姓名或部门" ) );
  original->addWidget( search );
  QTreeWidget* people = new QTreeWidget( &dialog );
  people->setHeaderLabels( QStringList() << QStringLiteral( "联系人" ) << QStringLiteral( "部门" ) );
  people->header()->setSectionResizeMode( QHeaderView::Stretch );
  for( int index = 0; index < 6; ++index )
    new QTreeWidgetItem( people, QStringList() << QStringLiteral( "测试同事 %1" ).arg( index + 1 ) << QStringLiteral( "产品设计组" ) );
  original->addWidget( people, 1 );
  QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
  buttons->button( QDialogButtonBox::Ok )->setText( QStringLiteral( "确定" ) );
  buttons->button( QDialogButtonBox::Cancel )->setText( QStringLiteral( "取消" ) );
  original->addWidget( buttons );
  dialog.resize( 480, 400 );
  dialog.show();
  settle();
  QWidget* surface = dialog.findChild<QWidget*>( "shadowDialogSurface" );
  QWidget* body = dialog.findChild<QWidget*>( "shadowDialogBody" );
  QToolButton* close = dialog.findChild<QToolButton*>( "shadowDialogClose" );
  CHECK( surface && body && close );
  std::printf( "DPR %.2f dialog %dx%d surface %d,%d %dx%d body %d,%d %dx%d layout %d,%d %dx%d\n",
    dialog.devicePixelRatioF(), dialog.width(), dialog.height(),
    surface->x(), surface->y(), surface->width(), surface->height(),
    body->x(), body->y(), body->width(), body->height(),
    original->geometry().x(), original->geometry().y(), original->geometry().width(), original->geometry().height() );
  const QString proof = qEnvironmentVariable( "SHADOWCHAT_UI_PROOF_DIR", QDir::currentPath() );
  QDir().mkpath( proof );
  CHECK( dialog.grab().save( proof + "/dialog-normal.png" ) );
  CHECK( body->layout() == original );
  CHECK( contained( surface, &dialog ) && contained( body, surface ) );
  CHECK( contained( search, body ) && contained( people, body ) && contained( buttons, body ) );
  CHECK( contained( close, surface ) );
  CHECK( surface->mask().contains( people->mapTo( surface, people->rect().bottomRight() ) ) );
  GuiWizard wizard;
  wizard.show();
  settle();
  QWidget* wizardSurface = wizard.findChild<QWidget*>( "shadowDialogSurface" );
  QWidget* wizardBody = wizard.findChild<QWidget*>( "shadowDialogBody" );
  CHECK( wizardSurface && wizardBody && wizardBody->layout() );
  CHECK( contained( wizardBody, wizardSurface ) );
  for( QPushButton* button : wizard.findChildren<QPushButton*>() )
    if( button->isVisible() ) CHECK( contained( button, wizardSurface ) );
  CHECK( wizard.findChildren<QWidget*>( "shadowDialogSurface" ).size() == 1 );
  CHECK( wizard.grab().save( proof + "/dialog-constructor-polish.png" ) );
  wizard.hide();
  const QSize normal = dialog.size();
  dialog.resize( 1, 1 );
  settle();
  std::printf( "Minimum %dx%d hint %dx%d surface %dx%d body %dx%d buttons %d,%d %dx%d\n",
    dialog.width(), dialog.height(), dialog.minimumSizeHint().width(), dialog.minimumSizeHint().height(),
    surface->width(), surface->height(), body->width(), body->height(),
    buttons->x(), buttons->y(), buttons->width(), buttons->height() );
  CHECK( dialog.grab().save( proof + "/dialog-minimum.png" ) );
  CHECK( contained( body, surface ) && contained( buttons, body ) && contained( search, body ) );
  CHECK( contained( buttons->button( QDialogButtonBox::Ok ), surface ) );
  CHECK( contained( buttons->button( QDialogButtonBox::Cancel ), surface ) );
  dialog.resize( normal + QSize( 120, 80 ) );
  settle();
  CHECK( contained( body, surface ) && contained( people, body ) && contained( close, surface ) );
  dialog.hide();
  dialog.show();
  settle();
  CHECK( dialog.findChildren<QWidget*>( "shadowDialogSurface" ).size() == 1 );
  CHECK( contained( body, surface ) && contained( buttons, surface ) );
  ResizeEvents resizeEvents;
  dialog.installEventFilter( &resizeEvents );
  surface->installEventFilter( &resizeEvents );
  body->installEventFilter( &resizeEvents );
  QElapsedTimer resizeTimer;
  resizeTimer.start();
  for( int iteration = 0; iteration < 400; ++iteration )
  {
    dialog.resize( 360 + iteration % 220, 290 + iteration % 350 );
    settle();
    CHECK( contained( body, surface ) && contained( buttons, body ) );
  }
  const int settledEvents = resizeEvents.changes;
  settle();
  settle();
  CHECK( resizeEvents.changes == settledEvents );
  CHECK( resizeEvents.changes < 400 * 12 );
  std::printf( "RESIZE 400 operations, %d resize/layout events, %lld ms, settled queue stable\n",
    resizeEvents.changes, resizeTimer.elapsed() );
  QInputDialog mention;
  mention.setWindowTitle( QStringLiteral( "@ 同事" ) );
  mention.setLabelText( QStringLiteral( "选择要提醒的同事" ) );
  mention.setComboBoxItems( QStringList() << QStringLiteral( "测试同事 · 设计组" ) << QStringLiteral( "测试同事 · 产品组" ) );
  mention.setComboBoxEditable( false );
  mention.show();
  settle();
  QWidget* mentionSurface = mention.findChild<QWidget*>( "shadowDialogSurface" );
  QWidget* mentionBody = mention.findChild<QWidget*>( "shadowDialogBody" );
  CHECK( mentionSurface && mentionBody );
  CHECK( mention.grab().save( proof + "/dialog-mention.png" ) );
  for( QWidget* control : mentionBody->findChildren<QWidget*>( QString(), Qt::FindDirectChildrenOnly ) )
    if( control->isVisible() ) CHECK( contained( control, mentionBody ) && contained( control, mentionSurface ) );
  mention.resize( 1, 1 );
  settle();
  CHECK( mention.grab().save( proof + "/dialog-mention-minimum.png" ) );
  for( QPushButton* button : mention.findChildren<QPushButton*>() )
    if( button->isVisible() ) CHECK( contained( button, mentionSurface ) );
  QDialog skipped;
  skipped.setProperty( "shadowDialogChromeSkip", true );
  QVBoxLayout* skippedLayout = new QVBoxLayout( &skipped );
  skippedLayout->addWidget( new QLineEdit( &skipped ) );
  skipped.ensurePolished();
  delete skipped.layout();
  QVBoxLayout* rebuiltLayout = new QVBoxLayout( &skipped );
  QPushButton* skippedAction = new QPushButton( QStringLiteral( "构造期重排" ), &skipped );
  rebuiltLayout->addWidget( skippedAction );
  skipped.show();
  settle();
  CHECK( !skipped.findChild<QWidget*>( "shadowDialogSurface" ) && contained( skippedAction, &skipped ) );
  QDialog fixed;
  fixed.setFixedSize( 320, 180 );
  QVBoxLayout* fixedLayout = new QVBoxLayout( &fixed );
  fixedLayout->addWidget( new QLabel( QStringLiteral( "固定尺寸弹窗保留内容" ), &fixed ) );
  fixedLayout->addWidget( new QPushButton( QStringLiteral( "最右侧操作" ), &fixed ) );
  fixed.show();
  settle();
  CHECK( fixed.minimumSize() == fixed.maximumSize() );
  QWidget* fixedBody = fixed.findChild<QWidget*>( "shadowDialogBody" );
  QWidget* fixedSurface = fixed.findChild<QWidget*>( "shadowDialogSurface" );
  CHECK( fixedBody && fixedSurface && contained( fixedBody, fixedSurface ) );
  for( QPushButton* button : fixed.findChildren<QPushButton*>() )
    if( button->isVisible() ) CHECK( contained( button, fixedSurface ) );
  std::printf( "PASS %s DPR %.2f: dialog bounds, rightmost content/actions, minimum resize, repeated show, constructor polish surface, skipped constructor rebuild, fixed dimensions\n",
    qPrintable( QApplication::platformName() ), dialog.devicePixelRatioF() );
  return 0;
}
