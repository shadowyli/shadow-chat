//////////////////////////////////////////////////////////////////////
//
// 影聊 multi-image send confirmation.
//
//////////////////////////////////////////////////////////////////////

#include "BeeUtils.h"
#include "GuiImageBatchPreview.h"
#include "ShadowStyle.h"


GuiImageBatchPreview::GuiImageBatchPreview( const QStringList& file_paths, QWidget* parent )
  : QDialog( parent )
{
  setObjectName( "ShadowImageBatchDialog" );
  setWindowTitle( QStringLiteral( "发送多张图片 - 影聊" ) );
  setWindowModality( Qt::WindowModal );
  const int max_previews = qMin( 8, file_paths.size() );
  const int preview_rows = qMax( 1, (max_previews + 3) / 4 );
  const bool has_remainder = file_paths.size() > max_previews;
  const int preview_height = 8 + preview_rows * 128 + (preview_rows - 1) * 10 + (has_remainder ? 44 : 0);
  const int dialog_height = qMin( 590, 264 + preview_height );
  setMinimumSize( 600, dialog_height );
  resize( 680, dialog_height );
  setStyleSheet( ShadowStyle::applicationStyleSheet() );

  QVBoxLayout* root_layout = new QVBoxLayout( this );
  root_layout->setContentsMargins( 24, 22, 24, 20 );
  root_layout->setSpacing( 12 );

  QLabel* title = new QLabel( QStringLiteral( "发送 %1 张图片" ).arg( file_paths.size() ), this );
  title->setObjectName( "ShadowBatchTitle" );
  QLabel* hint = new QLabel( QStringLiteral( "一次确认即可发送；对方会在聊天中看到图片预览，并可逐张保存原图。" ), this );
  hint->setObjectName( "ShadowBatchHint" );
  hint->setWordWrap( true );
  root_layout->addWidget( title );
  root_layout->addWidget( hint );

  QScrollArea* scroll_area = new QScrollArea( this );
  scroll_area->setObjectName( "ShadowBatchScroll" );
  scroll_area->setWidgetResizable( true );
  scroll_area->setFrameShape( QFrame::NoFrame );
  scroll_area->setFixedHeight( preview_height );
  QWidget* preview_root = new QWidget( scroll_area );
  preview_root->setObjectName( "ShadowBatchPreviewRoot" );
  QGridLayout* preview_layout = new QGridLayout( preview_root );
  preview_layout->setContentsMargins( 4, 4, 4, 4 );
  preview_layout->setHorizontalSpacing( 10 );
  preview_layout->setVerticalSpacing( 10 );

  qint64 total_size = 0;
  for( int i = 0; i < file_paths.size(); ++i )
  {
    QFileInfo file_info( file_paths.at( i ) );
    total_size += file_info.size();
    if( i >= max_previews )
      continue;

    QFrame* card = new QFrame( preview_root );
    card->setObjectName( "ShadowThumbnailCard" );
    card->setMinimumSize( 132, 118 );
    QVBoxLayout* card_layout = new QVBoxLayout( card );
    card_layout->setContentsMargins( 8, 8, 8, 7 );
    card_layout->setSpacing( 6 );
    QLabel* image_label = new QLabel( card );
    image_label->setObjectName( "ShadowThumbnailImage" );
    image_label->setAlignment( Qt::AlignCenter );
    image_label->setMinimumSize( 112, 82 );

    QImageReader reader( file_paths.at( i ) );
    reader.setAutoTransform( true );
    QImage image = reader.read();
    if( !image.isNull() )
      image_label->setPixmap( QPixmap::fromImage( image ).scaled( 112, 82, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );

    QLabel* name_label = new QLabel( card );
    name_label->setObjectName( "ShadowThumbnailName" );
    name_label->setText( QFontMetrics( name_label->font() ).elidedText( file_info.fileName(), Qt::ElideMiddle, 112 ) );
    name_label->setToolTip( file_info.fileName() );
    name_label->setAlignment( Qt::AlignCenter );
    card_layout->addWidget( image_label );
    card_layout->addWidget( name_label );
    preview_layout->addWidget( card, i / 4, i % 4 );
  }

  if( file_paths.size() > max_previews )
  {
    QLabel* remainder = new QLabel( QStringLiteral( "还有 %1 张图片将一并发送" ).arg( file_paths.size() - max_previews ), preview_root );
    remainder->setObjectName( "ShadowBatchRemainder" );
    remainder->setAlignment( Qt::AlignCenter );
    preview_layout->addWidget( remainder, (max_previews + 3) / 4, 0, 1, 4 );
  }
  scroll_area->setWidget( preview_root );
  root_layout->addWidget( scroll_area );

  QLabel* summary = new QLabel( QStringLiteral( "共 %1 张 · %2" )
                                .arg( file_paths.size() )
                                .arg( QLocale().formattedDataSize( total_size ) ), this );
  summary->setObjectName( "ShadowBatchSummary" );
  root_layout->addWidget( summary );

  mp_caption = new QLineEdit( this );
  mp_caption->setObjectName( "ShadowBatchCaption" );
  mp_caption->setPlaceholderText( QStringLiteral( "添加一句说明（可选）" ) );
  mp_caption->setAccessibleName( QStringLiteral( "图片说明" ) );
  root_layout->addWidget( mp_caption );

  QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this );
  buttons->setObjectName( "ShadowBatchButtons" );
  buttons->button( QDialogButtonBox::Cancel )->setText( QStringLiteral( "取消" ) );
  buttons->button( QDialogButtonBox::Ok )->setText( QStringLiteral( "发送 %1 张" ).arg( file_paths.size() ) );
  buttons->button( QDialogButtonBox::Ok )->setObjectName( "ShadowPrimaryButton" );
  connect( buttons, SIGNAL( accepted() ), this, SLOT( accept() ) );
  connect( buttons, SIGNAL( rejected() ), this, SLOT( reject() ) );
  root_layout->addWidget( buttons );
}

bool GuiImageBatchPreview::isImageBatch( const QStringList& file_paths )
{
  if( file_paths.size() < 2 )
    return false;
  foreach( const QString& file_path, file_paths )
  {
    QFileInfo file_info( file_path );
    const QString suffix=file_info.suffix().toLower();
    const bool previewable=QStringList({"jpg","jpeg","png","gif","bmp","webp"}).contains(suffix);
    if( !file_info.isFile() || !previewable )
      return false;
  }
  return true;
}

bool GuiImageBatchPreview::confirm( const QStringList& file_paths, QWidget* parent, QString* caption )
{
  if( !isImageBatch( file_paths ) )
    return true;
  GuiImageBatchPreview dialog( file_paths, parent );
  if( dialog.exec() != QDialog::Accepted )
    return false;
  if( caption )
    *caption = dialog.caption();
  return true;
}

QString GuiImageBatchPreview::caption() const
{
  return mp_caption ? mp_caption->text().trimmed() : QString();
}
