#include "ShadowChatTranscript.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextImageFormat>
#include <QTextTable>
#include <QUrl>
#include <QtMath>


namespace
{
  const qreal AvatarWidth = 38.0;
  const qreal TimestampWidth = 44.0;
  const qreal BubblePadding = 10.0;
  const qreal MaximumBubbleWidth = 560.0;
  const qreal BubbleRadius = 10.0;
  const int OriginalImageSizeProperty = ShadowChatTranscript::BubbleColorProperty + 1;
  const int DocumentBackgroundProperty = ShadowChatTranscript::BubbleColorProperty + 2;

  class MeasurementDocument : public QTextDocument
  {
  public:
    explicit MeasurementDocument( QTextDocument* source )
      : QTextDocument(), mp_source( source )
    {
      setUndoRedoEnabled( false );
      setDocumentMargin( 0.0 );
      setDefaultFont( source->defaultFont() );
      setDefaultStyleSheet( source->defaultStyleSheet() );
      setBaseUrl( source->baseUrl() );
      setUseDesignMetrics( source->useDesignMetrics() );
      QTextOption option = source->defaultTextOption();
      option.setWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
      setDefaultTextOption( option );
      documentLayout()->setPaintDevice( source->documentLayout()->paintDevice() );
    }

  protected:
    QVariant loadResource( int type, const QUrl& name ) override
    {
      return mp_source->resource( type, name );
    }

  private:
    QTextDocument* mp_source;
  };

  qreal contentWidth( QTextDocument* document, int first, int last, qreal maximum )
  {
    QTextCursor source( document );
    source.setPosition( first );
    source.setPosition( last, QTextCursor::KeepAnchor );
    MeasurementDocument measurement( document );
    QTextCursor destination( &measurement );
    destination.insertFragment( QTextDocumentFragment( source ) );
    measurement.setTextWidth( qMax( 1.0, maximum ) );
    return qCeil( measurement.idealWidth() );
  }

  QColor bubbleColor( QTextTable* table )
  {
    const QTextTableFormat format = table->format();
    const QColor marked = format.property( ShadowChatTranscript::BubbleColorProperty ).value<QColor>();
    if( marked.isValid() )
      return marked;

    const QColor incoming( 240, 242, 244 );
    const QColor outgoing( 221, 235, 255 );
    const QBrush backgrounds[] = { format.background(), table->cellAt( 0, 0 ).format().background() };
    for( const QBrush& background : backgrounds )
    {
      if( background.style() == Qt::SolidPattern &&
          (background.color() == incoming || background.color() == outgoing) )
        return background.color();
    }
    return QColor();
  }

  QSizeF originalImageSize( QTextDocument* document, const QTextImageFormat& format )
  {
    const QSizeF stored = format.property( OriginalImageSizeProperty ).toSizeF();
    if( stored.width() > 0.0 && stored.height() > 0.0 )
      return stored;

    QSizeF intrinsic;
    const QVariant resource = document->resource( QTextDocument::ImageResource, QUrl( format.name() ) );
    if( resource.type() == QVariant::Image )
    {
      const QImage image = resource.value<QImage>();
      intrinsic = QSizeF( image.size() ) / image.devicePixelRatio();
    }
    else if( resource.type() == QVariant::Pixmap )
    {
      const QPixmap pixmap = resource.value<QPixmap>();
      intrinsic = QSizeF( pixmap.size() ) / pixmap.devicePixelRatio();
    }
    else if( resource.type() == QVariant::ByteArray )
    {
      const QImage image = QImage::fromData( resource.toByteArray() );
      if( !image.isNull() )
        intrinsic = QSizeF( image.size() ) / image.devicePixelRatio();
    }

    qreal width = format.hasProperty( QTextFormat::ImageWidth ) ? format.width() : 0.0;
    qreal height = format.hasProperty( QTextFormat::ImageHeight ) ? format.height() : 0.0;
    if( intrinsic.width() > 0.0 && intrinsic.height() > 0.0 )
    {
      if( width <= 0.0 )
        width = height > 0.0 ? height * intrinsic.width() / intrinsic.height() : intrinsic.width();
      if( height <= 0.0 )
        height = width * intrinsic.height() / intrinsic.width();
    }
    return QSizeF( width, height );
  }

  bool fitImages( QTextTable* table, qreal maximum, qreal scale )
  {
    QTextDocument* document = table->document();
    struct ImageChange
    {
      int position;
      QTextImageFormat format;
    };
    QVector<ImageChange> changes;
    for( QTextBlock block = document->findBlock( table->firstPosition() );
         block.isValid() && block.position() <= table->lastPosition(); block = block.next() )
    {
      for( QTextBlock::iterator iterator = block.begin(); !iterator.atEnd(); ++iterator )
      {
        const QTextFragment fragment = iterator.fragment();
        if( !fragment.isValid() || !fragment.charFormat().isImageFormat() )
          continue;

        QTextImageFormat format = fragment.charFormat().toImageFormat();
        const QSizeF original = originalImageSize( document, format );
        if( original.width() <= 0.0 || original.height() <= 0.0 )
          continue;

        const qreal factor = qMin( scale, maximum / original.width() );
        format.setProperty( OriginalImageSizeProperty, original );
        format.setWidth( qMax( 1.0, original.width() * factor ) );
        format.setHeight( qMax( 1.0, original.height() * factor ) );
        if( format != fragment.charFormat() )
          changes.append( ImageChange{ fragment.position(), format } );
      }
    }

    for( const ImageChange& change : changes )
    {
      QTextCursor cursor( document );
      cursor.setPosition( change.position );
      cursor.movePosition( QTextCursor::NextCharacter, QTextCursor::KeepAnchor );
      cursor.setCharFormat( change.format );
    }
    return !changes.isEmpty();
  }

  void normalizeCell( QTextTableCell cell, qreal padding, bool clear_background )
  {
    const QTextTableCellFormat original = cell.format().toTableCellFormat();
    QTextTableCellFormat format = original;
    format.setPadding( padding );
    format.setBorder( 0.0 );
    format.setBorderStyle( QTextFrameFormat::BorderStyle_None );
    if( clear_background )
      format.clearBackground();
    if( format != original )
      cell.setFormat( format );
  }

  void normalizeBubble( QTextTable* table, const QColor& color, qreal maximum )
  {
    const QTextTableFormat original = table->format();
    QTextTableFormat format = original;
    format.setProperty( ShadowChatTranscript::BubbleColorProperty, color );
    format.clearBackground();
    format.setBorder( 0.0 );
    format.setBorderStyle( QTextFrameFormat::BorderStyle_None );
    format.setBorderCollapse( false );
    format.setCellSpacing( 0.0 );
    format.setCellPadding( BubblePadding );
    format.setPadding( 0.0 );
    format.setMargin( 0.0 );
    format.setPosition( QTextFrameFormat::InFlow );
    format.setAlignment( Qt::AlignLeft );
    format.clearProperty( QTextFormat::FrameHeight );
    if( format != original )
      table->setFormat( format );
    normalizeCell( table->cellAt( 0, 0 ), BubblePadding, true );

    const qreal maximum_content = qMax( 1.0, maximum - 2.0 * BubblePadding );
    fitImages( table, maximum_content, 1.0 );
    const QTextTableCell cell = table->cellAt( 0, 0 );
    qreal measured = contentWidth( table->document(), cell.firstPosition(), cell.lastPosition(), maximum_content );
    qreal image_scale = 1.0;
    for( int attempt = 0; attempt < 4 && measured > maximum_content; ++attempt )
    {
      image_scale *= qMax( 0.01, (maximum_content - 1.0) / measured );
      if( !fitImages( table, maximum_content, image_scale ) )
        break;
      measured = contentWidth( table->document(), cell.firstPosition(), cell.lastPosition(), maximum_content );
    }
    const qreal width = qMin( maximum, qMax( 1.0, measured ) + 2.0 * BubblePadding );
    format = table->format();
    format.setWidth( QTextLength( QTextLength::FixedLength, width ) );
    QVector<QTextLength> columns;
    columns.append( QTextLength( QTextLength::FixedLength, width ) );
    format.setColumnWidthConstraints( columns );
    if( format != table->format() )
      table->setFormat( format );
  }
}


const int ShadowChatTranscript::BubbleColorProperty;


ShadowChatTranscript::ShadowChatTranscript( QWidget* parent )
  : QTextBrowser( parent ), m_normalizeTimer( this ), m_normalizing( false ), m_painting( false )
{
  setLineWrapMode( QTextEdit::WidgetWidth );
  setWordWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
  viewport()->setAutoFillBackground( true );
  m_normalizeTimer.setSingleShot( true );
  m_normalizeTimer.setInterval( 0 );
  connect( &m_normalizeTimer, &QTimer::timeout, this, &ShadowChatTranscript::normalizeBubbles );
  connect( this, &QTextEdit::textChanged, this, &ShadowChatTranscript::scheduleNormalization );
  observeDocument();
}

void ShadowChatTranscript::setHtml( const QString& html )
{
  QTextBrowser::setHtml( html );
  normalizeBubbles();
}

void ShadowChatTranscript::insertHtml( const QString& html )
{
  QTextBrowser::insertHtml( html );
  normalizeBubbles();
}

void ShadowChatTranscript::setDocument( QTextDocument* document )
{
  QTextBrowser::setDocument( document );
  observeDocument();
  normalizeBubbles();
}

void ShadowChatTranscript::observeDocument()
{
  if( mp_observedDocument == document() )
    return;

  disconnect( m_layoutChangedConnection );
  disconnect( m_sizeChangedConnection );
  mp_observedDocument = document();
  m_bubbles.clear();
  m_layoutChangedConnection = connect( document(), &QTextDocument::documentLayoutChanged,
                                      this, &ShadowChatTranscript::observeLayout );
  observeLayout();
}

void ShadowChatTranscript::observeLayout()
{
  disconnect( m_sizeChangedConnection );
  m_sizeChangedConnection = connect( document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                                    this, &ShadowChatTranscript::scheduleNormalization );
  scheduleNormalization();
}

void ShadowChatTranscript::scheduleNormalization()
{
  if( !m_normalizing && !m_normalizeTimer.isActive() )
    m_normalizeTimer.start();
}

void ShadowChatTranscript::normalizeBubbles()
{
  if( m_normalizing )
    return;
  if( m_painting )
  {
    scheduleNormalization();
    return;
  }

  QScopedValueRollback<bool> normalizing( m_normalizing, true );
  m_normalizeTimer.stop();
  observeDocument();
  m_bubbles.clear();
  QTextDocument* transcript = document();
  QTextFrame* document_frame = transcript->rootFrame();
  const int viewport_width = viewport()->width();
  const QTextFrameFormat document_format = document_frame->frameFormat();
  const qreal available = qMax( 1.0, viewport_width - document_format.leftMargin() - document_format.rightMargin() );
  const qreal maximum = qMax( 2.0 * BubblePadding + 1.0,
                             qMin( MaximumBubbleWidth, available * 0.70 - AvatarWidth - TimestampWidth ) );

  const int horizontal_scroll = horizontalScrollBar()->value();
  const bool at_bottom = verticalScrollBar()->maximum() > 0 &&
                         verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 1;
  const QTextCursor reading_position = cursorForPosition( QPoint( 0, 0 ) );
  const int reading_top = cursorRect( reading_position ).top();
  const bool was_modified = transcript->isModified();
  QTextCursor edit( transcript );
  edit.beginEditBlock();

  const QList<QTextFrame*> message_frames = document_frame->childFrames();
  for( QTextFrame* frame : message_frames )
  {
    QTextTable* root = qobject_cast<QTextTable*>( frame );
    if( !root || root->rows() != 1 || root->columns() != 3 )
      continue;

    QTextTable* bubble = Q_NULLPTR;
    QColor color;
    const QList<QTextFrame*> children = root->childFrames();
    for( QTextFrame* child : children )
    {
      QTextTable* candidate = qobject_cast<QTextTable*>( child );
      if( !candidate || candidate->rows() != 1 || candidate->columns() != 1 )
        continue;
      const QTextTableCell containing_cell = root->cellAt( candidate->firstPosition() );
      if( !containing_cell.isValid() || containing_cell.column() != 1 )
        continue;
      color = bubbleColor( candidate );
      if( color.isValid() )
      {
        bubble = candidate;
        break;
      }
    }
    if( !bubble )
      continue;

    normalizeBubble( bubble, color, maximum );
    for( int column = 0; column < root->columns(); ++column )
      normalizeCell( root->cellAt( 0, column ), 0.0, false );

    const QTextTableCell body = root->cellAt( 0, 1 );
    const qreal body_width = qMin( maximum, qMax( bubble->format().width().rawValue(),
      contentWidth( transcript, body.firstPosition(), body.lastPosition(), maximum ) ) );
    QTextTableFormat root_format = root->format();
    const bool outgoing = root_format.alignment().testFlag( Qt::AlignRight );
    QVector<QTextLength> columns;
    columns.append( QTextLength( QTextLength::FixedLength, outgoing ? TimestampWidth : AvatarWidth ) );
    columns.append( QTextLength( QTextLength::FixedLength, body_width ) );
    columns.append( QTextLength( QTextLength::FixedLength, outgoing ? AvatarWidth : TimestampWidth ) );
    root_format.setColumnWidthConstraints( columns );
    root_format.setWidth( QTextLength( QTextLength::FixedLength, AvatarWidth + body_width + TimestampWidth ) );
    root_format.setBorder( 0.0 );
    root_format.setBorderStyle( QTextFrameFormat::BorderStyle_None );
    root_format.setBorderCollapse( false );
    root_format.setCellSpacing( 0.0 );
    root_format.setCellPadding( 0.0 );
    root_format.setPadding( 0.0 );
    root_format.setLeftMargin( 0.0 );
    root_format.setRightMargin( 0.0 );
    root_format.setPosition( QTextFrameFormat::InFlow );
    root_format.clearProperty( QTextFormat::FrameHeight );
    if( root_format != root->format() )
      root->setFormat( root_format );

    Bubble entry;
    entry.table = bubble;
    entry.color = color;
    m_bubbles.append( entry );
  }

  if( !m_bubbles.isEmpty() && document_format.background().style() != Qt::NoBrush )
  {
    QTextFrameFormat format = document_format;
    format.setProperty( DocumentBackgroundProperty, format.background() );
    format.clearBackground();
    document_frame->setFrameFormat( format );
  }

  edit.endEditBlock();
  transcript->documentLayout()->documentSize();
  transcript->setModified( was_modified );
  horizontalScrollBar()->setValue( horizontal_scroll );
  if( at_bottom )
    verticalScrollBar()->setValue( verticalScrollBar()->maximum() );
  else
    verticalScrollBar()->setValue( verticalScrollBar()->value() + cursorRect( reading_position ).top() - reading_top );
  viewport()->update();
  if( viewport()->width() != viewport_width )
    m_normalizeTimer.start();
}

int ShadowChatTranscript::bubbleCount() const
{
  int count = 0;
  for( const Bubble& bubble : m_bubbles )
  {
    if( bubble.table && bubble.table->document() == document() )
      ++count;
  }
  return count;
}

QRectF ShadowChatTranscript::bubbleFrameRect( QTextTable* table ) const
{
  QRectF rectangle = document()->documentLayout()->frameBoundingRect( table );
  const QTextTableCellFormat cell = table->cellAt( 0, 0 ).format().toTableCellFormat();
  rectangle.translate( -cell.leftPadding(), -cell.topPadding() );
  return rectangle;
}

QVector<QRectF> ShadowChatTranscript::bubbleFrameRects() const
{
  QVector<QRectF> rectangles;
  rectangles.reserve( m_bubbles.size() );
  for( const Bubble& bubble : m_bubbles )
  {
    if( bubble.table && bubble.table->document() == document() )
      rectangles.append( bubbleFrameRect( bubble.table ) );
  }
  return rectangles;
}

void ShadowChatTranscript::paintEvent( QPaintEvent* event )
{
  QScopedValueRollback<bool> painting( m_painting, true );
  {
    QPainter painter( viewport() );
    painter.setClipRegion( event->region() );
    const QVariant background = document()->rootFrame()->frameFormat().property( DocumentBackgroundProperty );
    if( background.isValid() )
      painter.fillRect( event->rect(), background.value<QBrush>() );

    const int horizontal_offset = isRightToLeft()
      ? horizontalScrollBar()->maximum() - horizontalScrollBar()->value()
      : horizontalScrollBar()->value();
    painter.translate( -horizontal_offset, -verticalScrollBar()->value() );
    const QRectF exposed = QRectF( event->rect() ).translated( horizontal_offset, verticalScrollBar()->value() );
    painter.setRenderHint( QPainter::Antialiasing, true );
    painter.setPen( Qt::NoPen );
    for( const Bubble& bubble : m_bubbles )
    {
      if( !bubble.table || bubble.table->document() != document() )
        continue;
      const QRectF rectangle = bubbleFrameRect( bubble.table );
      if( !rectangle.intersects( exposed ) )
        continue;
      painter.setBrush( bubble.color );
      painter.drawRoundedRect( rectangle, BubbleRadius, BubbleRadius );
    }
  }
  QTextBrowser::paintEvent( event );
}

void ShadowChatTranscript::resizeEvent( QResizeEvent* event )
{
  QTextBrowser::resizeEvent( event );
  normalizeBubbles();
}

void ShadowChatTranscript::changeEvent( QEvent* event )
{
  QTextBrowser::changeEvent( event );
  if( event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange ||
      event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutDirectionChange )
    scheduleNormalization();
}
