#include "ShadowChatTranscript.h"
#include "ChatMessage.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextImageFormat>
#include <QTextTable>
#include <QUrl>
#include <QtMath>
#include <QHelpEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QToolTip>
#include <algorithm>


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

  bool containsBubble( QTextFrame* frame )
  {
    QTextTable* table = qobject_cast<QTextTable*>( frame );
    if( table && table->rows() == 1 && table->columns() == 3 )
      for( QTextFrame* child : table->childFrames() )
      {
        QTextTable* body = qobject_cast<QTextTable*>( child );
        if( body && body->rows() == 1 && body->columns() == 1 && bubbleColor( body ).isValid() )
          return true;
      }
    for( QTextFrame* child : frame->childFrames() )
      if( containsBubble( child ) )
        return true;
    return false;
  }

  QString historyInputHtml( const QString& html )
  {
    QString normalized = html;
    const QRegularExpression attributes( "(<[^>]*?\\s(?:href|src)\\s*=\\s*)'([^']*)'",
                                        QRegularExpression::CaseInsensitiveOption );
    QList<QRegularExpressionMatch> matches;
    auto iterator = attributes.globalMatch( html );
    while( iterator.hasNext() )
      matches.append( iterator.next() );
    for( int index = matches.size() - 1; index >= 0; --index )
    {
      const QRegularExpressionMatch match = matches.at( index );
      QString value = match.captured( 2 );
      value.replace( '"', "&quot;" );
      normalized.replace( match.capturedStart(), match.capturedLength(),
                          match.captured( 1 ) + "\"" + value + "\"" );
    }
    return normalized;
  }

  QString historyBodyHtml( const QString& html )
  {
    const QRegularExpressionMatch body = QRegularExpression(
      "<body\\b[^>]*>([\\s\\S]*)</body\\s*>", QRegularExpression::CaseInsensitiveOption ).match( html );
    QString visible = body.hasMatch() ? body.captured( 1 ) : html;
    const QRegularExpression anchors( "<a\\b[^>]*>([\\s\\S]*?)</a\\s*>",
                                      QRegularExpression::CaseInsensitiveOption );
    QList<QRegularExpressionMatch> matches;
    auto iterator = anchors.globalMatch( visible );
    while( iterator.hasNext() )
      matches.append( iterator.next() );
    for( int index = matches.size() - 1; index >= 0; --index )
    {
      const QRegularExpressionMatch anchor = matches.at( index );
      QString label = anchor.captured( 1 );
      const QRegularExpression text( "(^|>)([^<]+)" );
      QList<QRegularExpressionMatch> labels;
      auto parts = text.globalMatch( label );
      while( parts.hasNext() )
        labels.append( parts.next() );
      for( int part = labels.size() - 1; part >= 0; --part )
      {
        const QRegularExpressionMatch match = labels.at( part );
        QString value = match.captured( 2 );
        value.replace( QRegularExpression( "\\bbeeimgtmp-" ), QStringLiteral( "截图-" ) );
        label.replace( match.capturedStart( 2 ), match.capturedLength( 2 ), value );
      }
      visible.replace( anchor.capturedStart( 1 ), anchor.capturedLength( 1 ), label );
    }
    return visible;
  }

  struct LegacyHeading
  {
    QString sender;
    QDateTime timestamp;
    int length = 0;
  };

  LegacyHeading legacyHeading( QTextDocument* document, int position, const QString& text,
                               const QString& knownSender = QString() )
  {
    LegacyHeading heading;
    const QRegularExpression stamp( QStringLiteral(
      "^\\s*\\((\\d{4}-\\d{2}-\\d{2}(?:[ T]\\d{1,2}:\\d{2}(?::\\d{2})?(?: [aApP][mM])?)?|"
      "\\d{1,2}:\\d{2}:\\d{2}(?: [aApP][mM])?)\\)\\s*" ) );
    auto readStamp = [&]() {
      const QRegularExpressionMatch match = stamp.match( text.mid( heading.length ) );
      if( !match.hasMatch() )
        return false;
      const QString value = match.captured( 1 );
      const QStringList formats = { "yyyy-MM-dd H:mm:ss", "yyyy-MM-dd H:mm", "yyyy-MM-dd",
                                    "yyyy-MM-dd h:mm:ss AP", "yyyy-MM-dd h:mm AP" };
      for( const QString& format : formats )
      {
        heading.timestamp = QLocale( QLocale::English ).toDateTime( value, format );
        if( heading.timestamp.isValid() )
          break;
      }
      heading.length += match.capturedLength();
      return true;
    };
    const bool stamped = readStamp();
    const QRegularExpressionMatch sender = QRegularExpression(
      QStringLiteral( "^\\s*([^\\n\\r:：]{1,160})[:：][ \\t]*" ) ).match( text.mid( heading.length ) );
    if( sender.hasMatch() )
    {
      const QString name = sender.captured( 1 ).trimmed();
      QTextCursor cursor( document );
      cursor.setPosition( qMin( position + heading.length, document->characterCount() - 1 ) );
      cursor.movePosition( QTextCursor::NextCharacter, QTextCursor::KeepAnchor );
      if( (!knownSender.isEmpty() && name == knownSender) ||
          (knownSender.isEmpty() && (stamped || cursor.charFormat().fontWeight() >= QFont::Bold) &&
           name != "http" && name != "https" && name != "file") )
      {
        heading.sender = name;
        heading.length += sender.capturedLength();
        if( !stamped )
          readStamp();
      }
    }
    return heading;
  }

  ShadowChatTranscript::HistoryEntry historyEntry( const QString& html, const QJsonObject& metadata )
  {
    ShadowChatTranscript::HistoryEntry entry;
    entry.sender = metadata.value( "sender" ).toString();
    entry.album = metadata.value( "album" ).toString();
    entry.type = metadata.value( "type" ).toInt( -1 );
    entry.timestamp = QDateTime::fromString( metadata.value( "time" ).toString(), Qt::ISODate );
    entry.hasDirection = metadata.value( "outgoing" ).isBool();
    entry.outgoing = metadata.value( "outgoing" ).toBool();
    const QRegularExpressionMatch sender = QRegularExpression(
      "<!--shadow-sender:([A-Za-z0-9+/=]+)-->" ).match( html );
    if( entry.sender.isEmpty() && sender.hasMatch() )
      entry.sender = QString::fromUtf8( QByteArray::fromBase64( sender.captured( 1 ).toLatin1() ) );
    if( entry.album.isEmpty() )
      entry.album = QRegularExpression( "<!--shadow-album:([{}A-Za-z0-9-]+)-->" ).match( html ).captured( 1 );
    QTextDocument document;
    document.setDefaultStyleSheet( ".bee-quote { background-color:#EAF0F6; color:#647A94; }" );
    document.setHtml( historyInputHtml( html ) );
    entry.alreadyBubbled = containsBubble( document.rootFrame() );
    if( entry.alreadyBubbled )
    {
      entry.html = historyBodyHtml( historyInputHtml( html ) );
      return entry;
    }
    const LegacyHeading heading = legacyHeading( &document, 0, document.toPlainText(), entry.sender );
    if( entry.sender.isEmpty() )
      entry.sender = heading.sender;
    if( entry.type < 0 && entry.sender.isEmpty() && heading.length > 0 )
      entry.type = ChatMessage::System;
    if( !entry.timestamp.isValid() )
      entry.timestamp = heading.timestamp;
    if( heading.length > 0 )
    {
      QTextCursor cursor( &document );
      cursor.setPosition( 0 );
      cursor.setPosition( heading.length, QTextCursor::KeepAnchor );
      cursor.removeSelectedText();
      entry.html = historyBodyHtml( document.toHtml() );
    }
    else
      entry.html = historyBodyHtml( historyInputHtml( html ) );
    for( const QString& flag : { QStringLiteral( "<!--shadow-attachment-->" ),
                                QStringLiteral( "<!--shadow-transfer-log-->" ) } )
      if( html.contains( flag ) && !entry.html.contains( flag ) )
        entry.html.prepend( flag );
    return entry;
  }

  void appendLegacyEntries( QList<ShadowChatTranscript::HistoryEntry>& entries, const QString& html )
  {
    QTextDocument document;
    document.setDefaultStyleSheet( ".bee-quote { background-color:#EAF0F6; color:#647A94; }" );
    document.setHtml( historyInputHtml( html ) );
    QSet<int> boundaries;
    boundaries.insert( 0 );
    boundaries.insert( document.characterCount() - 1 );
    for( QTextFrame* frame : document.rootFrame()->childFrames() )
      if( containsBubble( frame ) )
      {
        boundaries.insert( frame->firstPosition() - 1 );
        boundaries.insert( frame->lastPosition() + 1 );
      }
    for( QTextBlock block = document.begin(); block.isValid(); block = block.next() )
    {
      QTextCursor cursor( block );
      if( cursor.currentFrame() != document.rootFrame() )
        continue;
      const QString text = block.text();
      int offset = 0;
      while( offset < text.size() )
      {
        int end = text.indexOf( QChar::LineSeparator, offset );
        if( end < 0 )
          end = text.size();
        cursor.setPosition( block.position() + offset );
        cursor.movePosition( QTextCursor::NextCharacter, QTextCursor::KeepAnchor );
        const QTextCharFormat format = cursor.charFormat();
        const LegacyHeading heading = legacyHeading( &document, block.position() + offset,
                                                       text.mid( offset, end - offset ) );
        if( heading.length > 0 && !format.isAnchor() && format.background().style() == Qt::NoBrush )
          boundaries.insert( block.position() + offset );
        offset = end + 1;
      }
    }
    QList<int> positions = boundaries.values();
    std::sort( positions.begin(), positions.end() );
    for( int index = 0; index + 1 < positions.size(); ++index )
    {
      QTextCursor cursor( &document );
      cursor.setPosition( positions.at( index ) );
      cursor.setPosition( positions.at( index + 1 ), QTextCursor::KeepAnchor );
      const QTextDocumentFragment fragment( cursor );
      if( fragment.toPlainText().trimmed().isEmpty() &&
          !fragment.toPlainText().contains( QChar::ObjectReplacementCharacter ) )
        continue;
      entries.append( historyEntry( fragment.toHtml(), QJsonObject() ) );
    }
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

QList<ShadowChatTranscript::HistoryEntry> ShadowChatTranscript::historyEntries( const QString& html )
{
  QList<HistoryEntry> entries;
  const QRegularExpression tokens( QStringLiteral(
    "<!--[\\s\\S]*?-->|<\\s*(/?)\\s*table\\b[^>]*>" ), QRegularExpression::CaseInsensitiveOption );
  const QRegularExpression marker( "^<!--shadow-record:([A-Za-z0-9+/=]+)-->$" );
  int depth = 0;
  int start = 0;
  bool marked = false;
  QJsonObject metadata;
  auto iterator = tokens.globalMatch( html );
  while( iterator.hasNext() )
  {
    const QRegularExpressionMatch token = iterator.next();
    const QRegularExpressionMatch record = marker.match( token.captured() );
    if( record.hasMatch() && depth == 0 )
    {
      const QJsonDocument decoded = QJsonDocument::fromJson(
        QByteArray::fromBase64( record.captured( 1 ).toLatin1() ) );
      if( !decoded.isObject() )
        continue;
      const QString preceding = html.mid( start, token.capturedStart() - start );
      if( marked )
        entries.append( historyEntry( preceding, metadata ) );
      else
        appendLegacyEntries( entries, preceding );
      metadata = decoded.object();
      marked = true;
      start = token.capturedEnd();
    }
    else if( !token.captured().startsWith( "<!--" ) )
      depth = qMax( 0, depth + (token.captured( 1 ).isEmpty() ? 1 : -1) );
  }
  if( marked )
    entries.append( historyEntry( html.mid( start ), metadata ) );
  else
    appendLegacyEntries( entries, html );
  return entries;
}


ShadowChatTranscript::ShadowChatTranscript( QWidget* parent )
  : QTextBrowser( parent ), m_normalizeTimer( this ), m_transferAnimationTimer(this), m_transferAngle(0), m_normalizing( false ), m_painting( false )
{
  m_transferAnimationTimer.setObjectName("ShadowTransferAnimationTimer");
  m_transferAnimationTimer.setInterval(80);
  connect(&m_transferAnimationTimer,&QTimer::timeout,this,[this]() {
    m_transferAngle=(m_transferAngle+24)%360;
    viewport()->update();
  });
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

  QList<QTextFrame*> message_frames = document_frame->childFrames();
  for( int index = 0; index < message_frames.size(); ++index )
    message_frames.append( message_frames.at( index )->childFrames() );
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
    qreal timestampWidth=TimestampWidth;
    const QTextTableCell timeCell=root->cellAt(0,outgoing?0:2);
    QTextCursor timestampContent(transcript);
    timestampContent.setPosition(timeCell.firstPosition());
    timestampContent.setPosition(timeCell.lastPosition(),QTextCursor::KeepAnchor);
    if(timestampContent.selection().toHtml().contains("shadowtransfer:"))timestampWidth=64.0;
    columns.append( QTextLength( QTextLength::FixedLength, outgoing ? timestampWidth : AvatarWidth ) );
    columns.append( QTextLength( QTextLength::FixedLength, body_width ) );
    columns.append( QTextLength( QTextLength::FixedLength, outgoing ? AvatarWidth : timestampWidth ) );
    root_format.setColumnWidthConstraints( columns );
    root_format.setWidth( QTextLength( QTextLength::FixedLength, AvatarWidth + body_width + timestampWidth ) );
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
  if(m_transferIndicators.isEmpty())return;
  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);
  for(QTextBlock block=document()->begin();block.isValid();block=block.next())
    for(QTextBlock::iterator iterator=block.begin();!iterator.atEnd();++iterator)
    {
      const QTextFragment fragment=iterator.fragment();
      if(!fragment.charFormat().isImageFormat())continue;
      const QUrl source(fragment.charFormat().toImageFormat().name());
      if(source.scheme()!="shadowtransfer")continue;
      const auto state=m_transferIndicators.value(source.path());
      if(state.first==NoIndicator)continue;
      QTextCursor cursor(document());
      cursor.setPosition(fragment.position());
      const QRect cursorArea=cursorRect(cursor);
      const QRectF icon(cursorArea.left(),cursorArea.top()+qMax(0,(cursorArea.height()-12)/2),12,12);
      if(!icon.intersects(event->rect()))continue;
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(QColor(state.first==FailedIndicator?"#D65C58":"#85A5D5"),1.4,Qt::SolidLine,Qt::RoundCap));
      if(state.first==SendingIndicator)
        painter.drawArc(icon.adjusted(1,1,-1,-1),m_transferAngle*16,260*16);
      else
      {
        painter.drawEllipse(icon.adjusted(1,1,-1,-1));
        const QPointF center=icon.center();
        if(state.first==QueuedIndicator)
        {
          painter.drawLine(center,center+QPointF(0,-3));
          painter.drawLine(center,center+QPointF(2,1));
        }
        else
        {
          painter.drawLine(center+QPointF(0,-3),center+QPointF(0,0));
          painter.drawPoint(center+QPointF(0,3));
        }
      }
    }
}

QVariant ShadowChatTranscript::loadResource( int type, const QUrl& name )
{
  if(type==QTextDocument::ImageResource&&name.scheme()=="shadowtransfer")
  {
    QImage placeholder(12,12,QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(Qt::transparent);
    return placeholder;
  }
  return QTextBrowser::loadResource(type,name);
}

int ShadowChatTranscript::transferIndicatorState( const QString& sourceId ) const
{
  return m_transferIndicators.value(sourceId).first;
}

void ShadowChatTranscript::setTransferIndicators( const QHash<QString,QPair<int,QString>>& states )
{
  const bool changed=m_transferIndicators!=states;
  m_transferIndicators=states;
  bool sending=false;
  for(const auto& state:states)sending=sending||state.first==SendingIndicator;
  if(sending&&isVisible())
  {
    if(!m_transferAnimationTimer.isActive())m_transferAnimationTimer.start();
  }
  else m_transferAnimationTimer.stop();
  if(changed)viewport()->update();
}

void ShadowChatTranscript::showEvent( QShowEvent* event )
{
  QTextBrowser::showEvent(event);
  setTransferIndicators(m_transferIndicators);
}

void ShadowChatTranscript::hideEvent( QHideEvent* event )
{
  m_transferAnimationTimer.stop();
  QTextBrowser::hideEvent(event);
}

bool ShadowChatTranscript::viewportEvent( QEvent* event )
{
  if(event->type()==QEvent::ToolTip)
  {
    const QHelpEvent* help=static_cast<QHelpEvent*>(event);
    const QString target=anchorAt(help->pos());
    const QUrl source(target);
    if(source.scheme()=="shadowtransfer")
    {
      const auto state=m_transferIndicators.value(source.path());
      if(!state.second.isEmpty())QToolTip::showText(help->globalPos(),state.second,this);
      return true;
    }
  }
  return QTextBrowser::viewportEvent(event);
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
