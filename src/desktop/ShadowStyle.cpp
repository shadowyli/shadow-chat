//////////////////////////////////////////////////////////////////////
//
// 影聊 desktop design tokens and icon helpers.
//
//////////////////////////////////////////////////////////////////////

#include <QPainter>
#include <QPainterPath>
#include <QFontDatabase>
#include <QDir>
#include "ShadowStyle.h"


namespace
{
  const QColor kTextSecondary( "#5F6B7A" );
  const QColor kBrand( "#2563EB" );
  const QColor kNavIcon( "#AEB8C6" );
  const QColor kNavIconActive( "#FFFFFF" );

  QString iconResourcePath( const QString& name )
  {
    return QStringLiteral( ":/images/shadowchat/icons/%1.svg" ).arg( name );
  }

  QIcon iconWithStates( const QString& path, const QColor& normal, const QColor& active )
  {
    const QSize raster_size( 64, 64 );
    const QPixmap normal_pixmap = ShadowStyle::tintedPixmap( path, raster_size, normal );
    const QPixmap active_pixmap = ShadowStyle::tintedPixmap( path, raster_size, active );
    const QPixmap disabled_pixmap = ShadowStyle::tintedPixmap( path, raster_size, QColor( "#9CA6B4" ) );

    QIcon icon;
    icon.addPixmap( normal_pixmap, QIcon::Normal, QIcon::Off );
    icon.addPixmap( active_pixmap, QIcon::Active, QIcon::Off );
    icon.addPixmap( active_pixmap, QIcon::Normal, QIcon::On );
    icon.addPixmap( active_pixmap, QIcon::Selected, QIcon::On );
    icon.addPixmap( disabled_pixmap, QIcon::Disabled, QIcon::Off );
    return icon;
  }
}


QFont ShadowStyle::applicationFont()
{
  static const int chineseFont = QFontDatabase::addApplicationFont(QDir::fromNativeSeparators(qEnvironmentVariable("WINDIR",QStringLiteral("C:/Windows")))+QStringLiteral("/Fonts/msyh.ttc"));
  Q_UNUSED(chineseFont);
  QFont font( QStringLiteral( "Microsoft YaHei UI" ), 9 );
  font.setStyleStrategy( QFont::PreferAntialias );
  return font;
}

QPixmap ShadowStyle::tintedPixmap( const QString& resource_path, const QSize& size, const QColor& color )
{
  QPixmap source = QIcon( resource_path ).pixmap( size );
  if( source.isNull() )
    return source;

  QPixmap tinted( source.size() );
  tinted.fill( Qt::transparent );
  QPainter painter( &tinted );
  painter.drawPixmap( 0, 0, source );
  painter.setCompositionMode( QPainter::CompositionMode_SourceIn );
  painter.fillRect( tinted.rect(), color );
  painter.end();
  return tinted;
}

QIcon ShadowStyle::navigationIcon( const QString& name )
{
  return iconWithStates( iconResourcePath( name ), kNavIcon, kNavIconActive );
}

QIcon ShadowStyle::actionIcon( const QString& name )
{
  return iconWithStates( iconResourcePath( name ), kTextSecondary, kBrand );
}

QPixmap ShadowStyle::avatarPixmap( const QIcon& icon, const QSize& size )
{
  QPixmap avatar( size );
  avatar.fill( Qt::transparent );
  QPainter painter( &avatar );
  painter.setRenderHint( QPainter::Antialiasing );
  QPainterPath circle;
  circle.addEllipse( QRectF( QPointF(), QSizeF( size ) ) );
  painter.setClipPath( circle );
  painter.drawPixmap( QRect( QPoint(), size ), icon.pixmap( size ) );
  return avatar;
}

QString ShadowStyle::applicationStyleSheet()
{
  // Semantic token map: brand #2563EB, canvas #F5F7FB, surface #FFFFFF,
  // primary text #172033, secondary text #5F6B7A, stroke #E3E8F0.
  return QStringLiteral(
    "QWidget { font-family: 'Microsoft YaHei UI', 'Microsoft YaHei', 'Segoe UI'; font-size: 13px; }"
    "QWidget#ShadowRoot { background: #FFFFFF; color: #172033; font-family: 'Microsoft YaHei UI', 'Segoe UI'; }"
    "QWidget#ShadowTitleBar { background: #E8F0F4; border-bottom: 1px solid #DDE7EC; }"
    "QWidget#ShadowShellContent { background: #F5F7FB; }"
    "QToolButton#ShadowWindowButton, QToolButton#ShadowCloseButton { background: transparent; border: 0; border-radius: 0; min-width: 42px; max-width: 42px; min-height: 36px; max-height: 36px; padding: 0; }"
    "QToolButton#ShadowWindowButton:hover { background: #EEF2F7; }"
    "QToolButton#ShadowWindowButton:focus, QToolButton#ShadowCloseButton:focus { border: 1px solid #2563EB; }"
    "QToolButton#ShadowCloseButton:hover { background: #D83B3E; color: #FFFFFF; }"
    "QToolButton#ShadowTitleProfile { background: transparent; border: 0; padding: 4px; min-width: 32px; min-height: 32px; border-radius: 16px; }"
    "QToolButton#ShadowTitleProfile:hover { background: #DCE7EF; }"
    "QWidget#ShadowNavRail { background: #172033; border: 0; }"
    "QLabel#ShadowBrandName { color: #FFFFFF; font-size: 14px; font-weight: 600; padding-bottom: 4px; }"
    "QToolButton#ShadowNavButton { color: #AEB8C6; background: transparent; border: 1px solid transparent; border-radius: 10px; padding: 8px 4px; min-height: 48px; }"
    "QToolButton#ShadowNavButton:hover { background: #222D43; color: #FFFFFF; }"
    "QToolButton#ShadowNavButton:focus { border: 1px solid #8CB4FF; }"
    "QToolButton#ShadowNavButton:checked { background: #2563EB; color: #FFFFFF; }"
    "QToolButton#ShadowNavButton::menu-indicator { image: none; }"
    "QWidget#ShadowListPanel { background: #F2F7FA; border-right: 1px solid #E3E8F0; }"
    "QLabel#ShadowEyebrow { color: #2563EB; font-size: 11px; font-weight: 600; }"
    "QLabel#ShadowSectionTitle { color: #172033; font-size: 21px; font-weight: 600; }"
    "QLabel#ShadowSectionHint { color: #6B7686; font-size: 12px; padding-bottom: 4px; }"
    "QLineEdit#ShadowGlobalSearch { background: #FFFFFF; border: 1px solid #DFE7ED; border-radius: 6px; padding: 3px 10px; color: #172033; selection-background-color: #CFE0FF; }"
    "QLineEdit#ShadowGlobalSearch:hover { background: #EEF2F7; }"
    "QLineEdit#ShadowGlobalSearch:focus { background: #FFFFFF; border: 1px solid #9BBCEB; }"
    "QWidget#ShadowDirectoryTabs { background: transparent; }"
    "QToolButton#ShadowSegmentButton { background: #F2F5F9; color: #5F6B7A; border: 1px solid transparent; border-radius: 8px; padding: 7px 18px; min-height: 18px; }"
    "QToolButton#ShadowSegmentButton:hover { background: #EAF1FF; color: #1D4ED8; }"
    "QToolButton#ShadowSegmentButton:focus { border-color: #2563EB; }"
    "QToolButton#ShadowSegmentButton:checked { background: #E8F0FF; color: #1D4ED8; font-weight: 600; }"
    "QFrame#ShadowNetworkStrip { background: #F8FAFD; border: 1px solid #E6EBF2; border-radius: 9px; }"
    "QFrame#ShadowNetworkStrip[state='connecting'] { background: #FFFBEB; border-color: #F5D98A; }"
    "QFrame#ShadowNetworkStrip[state='offline'] { background: #FFF6F5; border-color: #F2C5C0; }"
    "QLabel#ShadowNetworkDot { background: #16A66A; border-radius: 4px; min-width: 8px; max-width: 8px; min-height: 8px; max-height: 8px; }"
    "QLabel#ShadowNetworkDot[state='connecting'] { background: #D99B22; }"
    "QLabel#ShadowNetworkDot[state='offline'] { background: #D75A4A; }"
    "QLabel#ShadowNetworkText { color: #5F6B7A; font-size: 11px; }"
    "QPushButton#ShadowInlineAction { color: #2457B8; background: transparent; border: 0; padding: 3px 5px; font-size: 11px; font-weight: 600; }"
    "QPushButton#ShadowInlineAction:hover { color: #1D4ED8; text-decoration: underline; }"
    "QWidget#ShadowEmptyState { background: #FFFFFF; }"
    "QLabel#ShadowEmptyTitle { color: #243044; font-size: 15px; font-weight: 600; }"
    "QLabel#ShadowEmptyHint { color: #6B7686; font-size: 12px; }"
    "QWidget#ShadowConversationPanel, QWidget#ShadowChatPage, QWidget#ShadowChatBody { background: #FFFFFF; }"
    "QWidget#ShadowChatHeader { background: #FFFFFF; border-bottom: 1px solid #E3E8F0; }"
    "QLabel#ShadowChatTitle { color: #172033; font-size: 17px; font-weight: 600; }"
    "QLabel#ShadowChatSubtitle { color: #6B7686; font-size: 12px; }"
    "QToolButton#ShadowBackButton { background: transparent; border: 1px solid transparent; border-radius: 8px; padding: 6px; }"
    "QToolButton#ShadowBackButton:hover { background: #EEF3FA; }"
    "QToolButton#ShadowBackButton:focus { border-color: #2563EB; }"
    "QWidget#ShadowChatPlaceholder { background: #F5F7FB; }"
    "QFrame#ShadowWelcomeCard { background: #FFFFFF; border: 1px solid #E3E8F0; border-radius: 16px; }"
    "QLabel#ShadowPlaceholderTitle { color: #172033; font-size: 18px; font-weight: 600; }"
    "QLabel#ShadowPlaceholderHint { color: #6B7686; font-size: 13px; padding-top: 2px; }"
    "QLabel#ShadowPrivacyPill { color: #2457B8; background: #EAF1FF; border: 1px solid #D4E2FF; border-radius: 9px; padding: 7px 11px; font-size: 12px; }"
    "QPushButton#ShadowPrimaryButton { color: #FFFFFF; background: #2563EB; border: 1px solid #2563EB; border-radius: 9px; padding: 8px 18px; min-height: 20px; font-weight: 600; }"
    "QPushButton#ShadowPrimaryButton:hover { background: #1D4ED8; border-color: #1D4ED8; }"
    "QPushButton#ShadowPrimaryButton:focus { border: 2px solid #8CB4FF; padding: 7px 17px; }"
    "QDialog#ShadowImageBatchDialog { background: #FFFFFF; color: #172033; }"
    "QDialog#ShadowOnboardingDialog { background: #F5F7FB; color: #172033; }"
    "QLabel#ShadowOnboardingTitle { color: #172033; font-size: 24px; font-weight: 600; }"
    "QLabel#ShadowOnboardingSubtitle, QLabel#ShadowOnboardingAccount { color: #6B7686; font-size: 12px; }"
    "QFrame#ShadowOnboardingCard { background: #FFFFFF; border: 1px solid #E3E8F0; border-radius: 12px; }"
    "QFrame#ShadowOnboardingTrust { background: #EAF1FF; border: 1px solid #D4E2FF; border-radius: 12px; }"
    "QLabel#ShadowOnboardingStep { color: #243044; font-size: 14px; font-weight: 600; }"
    "QLabel#ShadowOnboardingBody { color: #40516A; font-size: 12px; }"
    "QLabel#ShadowOnboardingAvatar { background: #EEF2F7; border: 1px solid #DCE3EC; border-radius: 36px; }"
    "QLineEdit#ShadowOnboardingName { background: #F7F9FC; border: 1px solid #DCE3EC; border-radius: 8px; padding: 8px 10px; color: #172033; }"
    "QLineEdit#ShadowOnboardingName:focus { background: #FFFFFF; border: 2px solid #2563EB; padding: 7px 9px; }"
    "QLabel#ShadowBatchTitle { color: #172033; font-size: 19px; font-weight: 600; }"
    "QLabel#ShadowBatchHint, QLabel#ShadowBatchSummary { color: #6B7686; font-size: 12px; }"
    "QScrollArea#ShadowBatchScroll, QWidget#ShadowBatchPreviewRoot { background: #F5F7FB; border: 0; }"
    "QFrame#ShadowThumbnailCard { background: #FFFFFF; border: 1px solid #E3E8F0; border-radius: 10px; }"
    "QLabel#ShadowThumbnailImage { background: #EEF2F7; border-radius: 7px; }"
    "QLabel#ShadowThumbnailName { color: #5F6B7A; font-size: 11px; }"
    "QLabel#ShadowBatchRemainder { color: #2457B8; background: #EAF1FF; border-radius: 8px; padding: 8px; }"
    "QLineEdit#ShadowBatchCaption { background: #F7F9FC; border: 1px solid #DCE3EC; border-radius: 9px; padding: 9px 11px; color: #172033; }"
    "QLineEdit#ShadowBatchCaption:focus { background: #FFFFFF; border: 2px solid #2563EB; padding: 8px 10px; }"
    "QTabWidget#GuiTabMain::pane { border: 0; background: #FFFFFF; }"
    "QWidget#GuiChatList, QWidget#GuiUserList, QWidget#GuiGroupList, QWidget#GuiHome, QWidget#GuiSavedChatList { background: #FFFFFF; border: 0; }"
    "QTreeWidget#ShadowSearchResults, QTreeWidget#GuiCustomList { background: #FFFFFF; border: 0; outline: 0; alternate-background-color: #FAFBFD; show-decoration-selected: 1; }"
    "QTreeWidget#ShadowSearchResults::item, QTreeWidget#GuiCustomList::item { min-height: 44px; padding: 5px 8px; border: 0; border-radius: 8px; color: #243044; }"
    "QTreeWidget#ShadowSearchResults::item:hover, QTreeWidget#GuiCustomList::item:hover { background: #F0F4FA; }"
    "QTreeWidget#ShadowSearchResults::item:selected, QTreeWidget#GuiCustomList::item:selected { background: #E7EFFF; color: #1D4ED8; }"
    "QTreeWidget#ShadowSearchResults::branch { background: transparent; }"
    "QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
    "QScrollBar::handle:vertical { background: #C9D1DC; min-height: 28px; border-radius: 4px; }"
    "QScrollBar::handle:vertical:hover { background: #AEB8C6; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    "QMainWindow#GuiFloatingChat { background: #FFFFFF; border: 0; }"
    "QFrame#ShadowTranscriptFrame { background: #FFFFFF; border: 0; }"
    "QFrame#ShadowComposer { background: #FFFFFF; border: 1px solid #E2E6EA; border-radius: 12px; margin: 8px 16px 16px 16px; }"
    "QTextEdit#GuiMessageEdit { background: transparent; color: #243044; border: 0; padding: 4px; selection-background-color: #CFE0FF; }"
    "QTextEdit#GuiMessageEdit:focus { border: 0; }"
    "QFrame#ShadowComposer[dropActive='true'] { background: #F3F7FF; border: 2px solid #2563EB; }"
    "QLabel#ShadowComposerHint { color: #8A94A3; font-size: 11px; }"
    "QToolButton#ShadowComposerAction { color: #5F6B7A; background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 5px; }"
    "QToolButton#ShadowComposerAction:hover { background: #EEF3FA; color: #1D4ED8; }"
    "QToolButton#ShadowComposerAction:focus { border-color: #2563EB; }"
    "QPushButton#ShadowSendButton { color: #FFFFFF; background: #2563EB; border: 1px solid #2563EB; border-radius: 8px; padding: 6px 16px; font-weight: 600; }"
    "QPushButton#ShadowSendButton:hover { background: #1D4ED8; border-color: #1D4ED8; }"
    "QPushButton#ShadowSendButton:disabled { color: #AEB8C6; background: #E7EBF0; border-color: #E7EBF0; }"
    "QPushButton#ShadowSendButton:focus { border: 2px solid #8CB4FF; padding: 5px 15px; }"
    "QToolBar#GuiFloatingChatToolBar, QToolBar#GuiFloatingChatMemberToolBar { background: #FFFFFF; border: 0; border-top: 1px solid #E3E8F0; spacing: 5px; padding: 5px 10px; }"
    "QToolBar QToolButton { border: 1px solid transparent; border-radius: 7px; padding: 5px; }"
    "QToolBar QToolButton:hover { background: #EEF3FA; }"
    "QToolBar QToolButton:focus { border-color: #2563EB; }"
    "QTextEdit { border: 0; background: #FFFFFF; color: #172033; selection-background-color: #CFE0FF; }"
    "QPushButton { border: 1px solid #DCE3EC; border-radius: 8px; padding: 7px 13px; background: #FFFFFF; color: #243044; }"
    "QPushButton:hover { background: #EEF3FA; }"
    "QPushButton:focus { border-color: #2563EB; }"
    "QPushButton#ShadowSendButton { color: #FFFFFF; background: #2563EB; border: 1px solid #2563EB; border-radius: 8px; padding: 6px 16px; font-weight: 600; }"
    "QPushButton#ShadowSendButton:hover { background: #1D4ED8; border-color: #1D4ED8; }"
    "QPushButton#ShadowSendButton:disabled { color: #8A94A3; background: #E7EBF0; border-color: #E7EBF0; }"
    "QPushButton#ShadowSendButton:focus { border: 2px solid #8CB4FF; padding: 5px 15px; }"
    "QMenu { background: #FFFFFF; border: 1px solid #DCE3EC; border-radius: 9px; padding: 6px; color: #243044; }"
    "QMenu::item { padding: 8px 30px 8px 12px; border-radius: 6px; }"
    "QMenu::item:selected { background: #E8F0FF; color: #1D4ED8; }"
    "QSplitter#ShadowContentSplitter::handle { background: #E3E8F0; }"
  );
}
