#include "ShadowChatTranscript.h"
#include "ChatMessage.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QMovie>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextImageFormat>
#include <QTextList>
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
  const int MaximumMovies = 12;

  qreal documentScale( QTextDocument* document )
  {
    const QPaintDevice* device = document->documentLayout()->paintDevice();
    if( !device )
      return 1.0;
    const QScreen* screen = QGuiApplication::primaryScreen();
    const qreal reference = QCoreApplication::testAttribute( Qt::AA_Use96Dpi ) || !screen
      ? 96.0 : qRound( screen->logicalDotsPerInchY() );
    return device->logicalDpiY() / qMax( 1.0, reference );
  }

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

  void copyMeasuredContents( QTextFrame::iterator begin, QTextFrame::iterator end, QTextCursor& destination )
  {
    bool first = true;
    for( auto iterator = begin; iterator != end; ++iterator )
    {
      if( QTextTable* table = qobject_cast<QTextTable*>( iterator.currentFrame() ) )
      {
        QTextTableFormat format = table->format();
        format.clearProperty( QTextFormat::FrameWidth );
        format.clearProperty( QTextFormat::FrameHeight );
        format.clearColumnWidthConstraints();
        format.setPosition( QTextFrameFormat::InFlow );
        QTextTable* copy = destination.insertTable( table->rows(), table->columns(), format );
        for( int row = 0; row < table->rows(); ++row )
          for( int column = 0; column < table->columns(); ++column )
          {
            const QTextTableCell cell = table->cellAt( row, column );
            if( cell.row() != row || cell.column() != column )
              continue;
            if( cell.rowSpan() > 1 || cell.columnSpan() > 1 )
              copy->mergeCells( row, column, cell.rowSpan(), cell.columnSpan() );
            QTextTableCell target = copy->cellAt( row, column );
            target.setFormat( cell.format() );
            QTextCursor content = target.firstCursorPosition();
            copyMeasuredContents( cell.begin(), cell.end(), content );
          }
        destination.setPosition( copy->lastPosition() + 1 );
        first = true;
      }
      else if( QTextFrame* frame = iterator.currentFrame() )
        copyMeasuredContents( frame->begin(), frame->end(), destination );
      else if( iterator.currentBlock().isValid() )
      {
        const QTextBlock block = iterator.currentBlock();
        if( !first )
          destination.insertBlock();
        first = false;
        destination.setBlockFormat( block.blockFormat() );
        if( block.textList() )
          destination.createList( block.textList()->format() );
        for( auto part = block.begin(); !part.atEnd(); ++part )
        {
          const QTextFragment fragment = part.fragment();
          if( fragment.isValid() )
            destination.insertText( fragment.text(), fragment.charFormat() );
        }
      }
    }
  }

  qreal contentWidth( QTextDocument* document, const QTextTableCell& cell, qreal maximum )
  {
    MeasurementDocument measurement( document );
    QTextCursor destination( &measurement );
    copyMeasuredContents( cell.begin(), cell.end(), destination );
    const qreal scale = documentScale( document );
    measurement.setTextWidth( qMax( 1.0, maximum * scale ) );
    return qCeil( measurement.idealWidth() / scale );
  }

  QColor bubbleColor( QTextTable* table )
  {
    const QTextTableFormat format = table->format();
    const QColor marked = format.property( ShadowChatTranscript::BubbleColorProperty ).value<QColor>();
    if( marked.isValid() )
      return marked;

    const QColor incoming( 240, 242, 244 );
    const QColor outgoing( 221, 235, 255 );
    const QBrush backgrounds[] = { format.background(), table->cellAt( 0, 0 ).format().background(),
                                  format.borderBrush() };
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
    const QString context = QRegularExpression(
      "<!--shadow-context:[0-9a-fA-F]{64}:[0-9]+-->" ).match( html ).captured();
    const auto preserveMarkers = [&]() {
      for( const QString& flag : { QStringLiteral( "<!--shadow-attachment-->" ),
                                  QStringLiteral( "<!--shadow-sticker-->" ),
                                  QStringLiteral( "<!--shadow-transfer-error-->" ),
                                  QStringLiteral( "<!--shadow-transfer-log-->" ) } )
        if( html.contains( flag ) && !entry.html.contains( flag ) )
          entry.html.prepend( flag );
      if( !context.isEmpty() && !entry.html.contains( context ) )
        entry.html.prepend( context );
    };
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
      preserveMarkers();
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
    preserveMarkers();
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

        qreal image_maximum = maximum;
        QTextCursor location( document );
        location.setPosition( fragment.position() );
        for( QTextTable* nested = location.currentTable(); nested && nested != table;
             nested = qobject_cast<QTextTable*>( nested->parentFrame() ) )
        {
          const QTextTableCell cell = nested->cellAt( fragment.position() );
          const QTextTableFormat format = nested->format();
          image_maximum = qMax( 1.0, (image_maximum - format.cellSpacing() * (nested->columns() + 1)) *
            cell.columnSpan() / nested->columns() - 2.0 * format.cellPadding() );
        }
        const qreal factor = qMin( scale, image_maximum / original.width() );
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
    format.clearProperty( QTextFormat::FrameWidth );
    if( clear_background )
      format.clearBackground();
    if( format != original )
      cell.setFormat( format );
  }

  qreal prepareBubble( QTextTable* table, const QColor& color )
  {
    const QTextTableFormat original = table->format();
    QTextTableFormat format = original;
    format.setProperty( ShadowChatTranscript::BubbleColorProperty, color );
    format.setBorderBrush( color );
    format.clearBackground();
    format.setBorder( 0.0 );
    format.setBorderStyle( QTextFrameFormat::BorderStyle_None );
    format.setBorderCollapse( false );
    format.setCellSpacing( 0.0 );
    format.setCellPadding( BubblePadding );
    format.setPadding( 0.0 );
    format.setLeftMargin( 0.0 );
    format.setRightMargin( 0.0 );
    format.setBottomMargin( 0.0 );
    format.setPosition( QTextFrameFormat::InFlow );
    format.setAlignment( Qt::AlignLeft );
    format.clearProperty( QTextFormat::FrameHeight );
    if( format != original )
      table->setFormat( format );
    normalizeCell( table->cellAt( 0, 0 ), BubblePadding, true );
    QList<QTextFrame*> nested = table->childFrames();
    for( int index = 0; index < nested.size(); ++index )
    {
      QTextFrame* frame = nested.at( index );
      nested.append( frame->childFrames() );
      QTextTable* child = qobject_cast<QTextTable*>( frame );
      if( !child )
        continue;
      QTextTableFormat child_format = child->format();
      child_format.clearProperty( QTextFormat::FrameWidth );
      child_format.clearColumnWidthConstraints();
      child_format.setPosition( QTextFrameFormat::InFlow );
      if( child_format != child->format() )
        child->setFormat( child_format );
    }

    const qreal maximum_width = MaximumBubbleWidth / documentScale( table->document() );
    const qreal maximum_content = qMax( 1.0, maximum_width - 2.0 * BubblePadding );
    fitImages( table, maximum_content, 1.0 );
    const QTextTableCell cell = table->cellAt( 0, 0 );
    qreal measured = contentWidth( table->document(), cell, maximum_content );
    qreal image_scale = 1.0;
    for( int attempt = 0; attempt < 4 && measured > maximum_content; ++attempt )
    {
      image_scale *= qMax( 0.01, (maximum_content - 1.0) / measured );
      if( !fitImages( table, maximum_content, image_scale ) )
        break;
      measured = contentWidth( table->document(), cell, maximum_content );
    }
    return qMin( maximum_width, qMax( 1.0, measured ) + 2.0 * BubblePadding );
  }

  QString localGifPath( const QString& source, const QUrl& base )
  {
    QUrl url( source );
    QString local;
    if( QFileInfo( source ).isAbsolute() && url.scheme().size() <= 1 )
      local = source;
    else
    {
      if( url.isRelative() )
        url = base.resolved( url );
      if( !url.isLocalFile() || (!url.host().isEmpty() && url.host() != "localhost") )
        return QString();
      local = url.toLocalFile();
    }
    if( local.startsWith( "//" ) || local.startsWith( "\\\\" ) ||
        QFileInfo( local ).suffix().compare( "gif", Qt::CaseInsensitive ) != 0 )
      return QString();
    return local;
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
  : QTextBrowser( parent ), m_normalizeTimer( this ), m_transferAnimationTimer(this),
    m_mediaVisibilityTimer( this ), m_transferAngle(0), m_normalizing( false ), m_painting( false ),
    m_contentsDirty( true ), m_resizePending( false ), m_resizeAtBottom( false ), m_lastMaximum( -1.0 )
{
  m_transferAnimationTimer.setObjectName("ShadowTransferAnimationTimer");
  m_transferAnimationTimer.setInterval(80);
  connect(&m_transferAnimationTimer,&QTimer::timeout,this,[this]() {
    m_transferAngle=(m_transferAngle+24)%360;
    viewport()->update();
  });
  setLineWrapMode( QTextEdit::FixedPixelWidth );
  setLineWrapColumnOrWidth( viewport()->width() );
  setWordWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
  viewport()->setAutoFillBackground( true );
  m_normalizeTimer.setSingleShot( true );
  m_normalizeTimer.setInterval( 0 );
  connect( &m_normalizeTimer, &QTimer::timeout, this, &ShadowChatTranscript::normalizeBubbles );
  m_mediaVisibilityTimer.setSingleShot( true );
  m_mediaVisibilityTimer.setInterval( 16 );
  connect( &m_mediaVisibilityTimer, &QTimer::timeout, this, &ShadowChatTranscript::updateMediaVisibility );
  connect( verticalScrollBar(), &QScrollBar::valueChanged, this, &ShadowChatTranscript::scheduleMediaVisibility );
  connect( horizontalScrollBar(), &QScrollBar::valueChanged, this, &ShadowChatTranscript::scheduleMediaVisibility );
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
  if( mp_observedDocument )
    disconnect( mp_observedDocument, Q_NULLPTR, this, Q_NULLPTR );
  mp_observedDocument = document();
  m_bubbles.clear();
  m_contentsDirty = true;
  m_lastMaximum = -1.0;
  connect( document(), &QTextDocument::contentsChange, this, [this]( int position, int removed, int added ) {
    if( m_normalizing )
      return;
    m_contentsDirty = true;
    const int end = position + qMax( removed, added );
    for( Bubble& bubble : m_bubbles )
      if( bubble.row && position <= bubble.row->lastPosition() && end >= bubble.row->firstPosition() )
        bubble.dirty = true;
    scheduleNormalization();
  } );
  m_layoutChangedConnection = connect( document(), &QTextDocument::documentLayoutChanged,
                                      this, &ShadowChatTranscript::observeLayout );
  observeLayout();
}

void ShadowChatTranscript::observeLayout()
{
  disconnect( m_sizeChangedConnection );
  document()->documentLayout()->setPaintDevice( viewport() );
  m_sizeChangedConnection = connect( document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                                    this, &ShadowChatTranscript::scheduleNormalization );
  scheduleNormalization();
}

void ShadowChatTranscript::scheduleNormalization()
{
  if( !m_normalizing && !m_normalizeTimer.isActive() )
    m_normalizeTimer.start( 0 );
}

void ShadowChatTranscript::invalidateContents()
{
  if( m_normalizing )
    return;
  m_contentsDirty = true;
  for( Bubble& bubble : m_bubbles )
    bubble.dirty = true;
  scheduleNormalization();
}

void ShadowChatTranscript::rebuildBubbles()
{
  QHash<QTextTable*,Bubble> previous;
  for( const Bubble& bubble : m_bubbles )
    if( bubble.table )
      previous.insert( bubble.table, bubble );
  m_bubbles.clear();
  const qreal scale = documentScale( document() );
  QList<QTextFrame*> frames = document()->rootFrame()->childFrames();
  for( int index = 0; index < frames.size(); ++index )
    frames.append( frames.at( index )->childFrames() );
  for( QTextFrame* frame : frames )
  {
    QTextTable* row = qobject_cast<QTextTable*>( frame );
    if( !row || row->rows() != 1 || row->columns() != 3 )
      continue;
    for( QTextFrame* child : row->childFrames() )
    {
      QTextTable* table = qobject_cast<QTextTable*>( child );
      if( !table || table->rows() != 1 || table->columns() != 1 )
        continue;
      const QTextTableCell cell = row->cellAt( table->firstPosition() );
      const QColor color = bubbleColor( table );
      if( !cell.isValid() || cell.column() != 1 || !color.isValid() )
        continue;
      const auto cached = previous.constFind( table );
      if( cached != previous.constEnd() && !cached->dirty && cached->row == row &&
          qFuzzyCompare( cached->deviceScale, scale ) )
      {
        m_bubbles.append( cached.value() );
        break;
      }
      Bubble bubble;
      bubble.row = row;
      bubble.table = table;
      bubble.color = color;
      bubble.preferredWidth = prepareBubble( table, color );
      bubble.deviceScale = scale;
      bubble.dirty = false;
      const bool outgoing = row->format().alignment().testFlag( Qt::AlignRight );
      if( !outgoing )
      {
        const QTextTableCell content = row->cellAt( 0, 1 );
        QVector<QTextBlock> headings;
        for( auto iterator = content.begin(); iterator != content.end(); ++iterator )
        {
          if( iterator.currentFrame() == table )
            break;
          const QTextBlock block = iterator.currentBlock();
          if( !block.isValid() )
            continue;
          QString text = block.text();
          text.replace( QChar::LineSeparator, QLatin1Char( ' ' ) );
          text = text.trimmed();
          if( !text.isEmpty() )
          {
            if( !bubble.sender.isEmpty() )
              bubble.sender.append( QLatin1Char( ' ' ) );
            bubble.sender.append( text );
            if( headings.isEmpty() )
            {
              bubble.senderFont = block.charFormat().font().resolve( document()->defaultFont() );
              bubble.senderColor = QColor( "#88929F" );
              for( auto part = block.begin(); !part.atEnd(); ++part )
                if( !part.fragment().text().trimmed().isEmpty() )
                {
                  const QTextCharFormat format = part.fragment().charFormat();
                  bubble.senderFont = format.font().resolve( document()->defaultFont() );
                  if( format.foreground().style() != Qt::NoBrush )
                    bubble.senderColor = format.foreground().color();
                  break;
                }
            }
          }
          headings.append( block );
        }
        if( !bubble.sender.isEmpty() )
        {
          bubble.senderFont.setPixelSize( 11 );
          bubble.senderHeight = qCeil( QFontMetricsF( bubble.senderFont ).height() ) + 4.0;
          for( QTextBlock block : headings )
            if( block.isVisible() )
            {
              block.setVisible( false );
              document()->markContentsDirty( block.position(), block.length() );
            }
        }
      }
      const QTextTableCell timestamp = row->cellAt( 0, outgoing ? 0 : 2 );
      for( auto iterator = timestamp.begin(); iterator != timestamp.end(); ++iterator )
        for( auto part = iterator.currentBlock().begin(); !part.atEnd(); ++part )
          if( part.fragment().charFormat().anchorHref().startsWith( "shadowtransfer:" ) ||
              (part.fragment().charFormat().isImageFormat() &&
               part.fragment().charFormat().toImageFormat().name().startsWith( "shadowtransfer:" )) )
            bubble.timestampWidth = 64.0;
      m_bubbles.append( bubble );
      break;
    }
  }
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
  QTextDocument* transcript = document();
  QTextFrame* document_frame = transcript->rootFrame();
  const int viewport_width = viewport()->width();
  const QTextFrameFormat document_format = document_frame->frameFormat();
  const qreal scale = documentScale( transcript );
  const qreal available = qMax( 1.0, viewport_width -
    (document_format.leftMargin() + document_format.rightMargin()) * scale );
  const qreal maximum = qMax( 2.0 * BubblePadding + 1.0,
    qMin( MaximumBubbleWidth, available * 0.70 - (AvatarWidth + TimestampWidth) * scale ) / scale );
  const bool metrics_changed = std::any_of( m_bubbles.constBegin(), m_bubbles.constEnd(),
    [scale]( const Bubble& bubble ) { return !qFuzzyCompare( bubble.deviceScale, scale ); } );
  if( !m_contentsDirty && !metrics_changed && qFuzzyCompare( maximum, m_lastMaximum ) &&
      lineWrapColumnOrWidth() == viewport_width )
  {
    m_resizePending = false;
    scheduleMediaVisibility();
    return;
  }
  const bool rebuild = m_contentsDirty || metrics_changed;
  m_contentsDirty = false;
  m_lastMaximum = maximum;

  const int horizontal_scroll = horizontalScrollBar()->value();
  const bool at_bottom = m_resizePending ? m_resizeAtBottom :
    (verticalScrollBar()->maximum() > 0 &&
     verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 1);
  QTextCursor reading_position = cursorForPosition( QPoint( 0, 0 ) );
  for( const Bubble& bubble : m_bubbles )
    if( bubble.table && bubble.table->document() == transcript &&
        bubbleFrameRect( bubble.table ).bottom() >= verticalScrollBar()->value() )
    {
      reading_position.setPosition( bubble.table->firstPosition() );
      break;
    }
  const int reading_top = cursorRect( reading_position ).top();
  const bool was_modified = transcript->isModified();
  QTextCursor edit( transcript );
  edit.beginEditBlock();
  if( metrics_changed )
    transcript->markContentsDirty( 0, transcript->characterCount() );
  if( lineWrapColumnOrWidth() != viewport_width )
    setLineWrapColumnOrWidth( viewport_width );

  if( rebuild )
    rebuildBubbles();
  for( const Bubble& entry : m_bubbles )
  {
    QTextTable* root = entry.row;
    QTextTable* bubble = entry.table;
    if( !root || !bubble )
      continue;
    for( int column = 0; column < root->columns(); ++column )
      normalizeCell( root->cellAt( 0, column ), 0.0, false );

    const qreal body_width = qMin( maximum, entry.preferredWidth );
    const qreal content_width = qMax( 1.0, body_width - 2.0 * BubblePadding );
    fitImages( bubble, content_width, 1.0 );
    QTextTableFormat bubble_format = bubble->format();
    bubble_format.setWidth( QTextLength( QTextLength::FixedLength, body_width ) );
    bubble_format.setTopMargin( entry.senderHeight / scale );
    bubble_format.setColumnWidthConstraints( { QTextLength( QTextLength::FixedLength, body_width ) } );
    if( bubble_format != bubble->format() )
      bubble->setFormat( bubble_format );
    QTextTableFormat root_format = root->format();
    const bool outgoing = root_format.alignment().testFlag( Qt::AlignRight );
    QVector<QTextLength> columns;
    const qreal timestampWidth = entry.timestampWidth;
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
  if( rebuild )
    rebuildMedia();
  scheduleMediaVisibility();
  if( viewport()->width() != viewport_width )
    m_normalizeTimer.start( 0 );
  else
    m_resizePending = false;
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
  const qreal scale = documentScale( document() );
  rectangle.translate( -cell.leftPadding() * scale, -cell.topPadding() * scale );
  rectangle.setHeight( qMax( 0.0, rectangle.height() - qRound( table->format().topMargin() * scale ) ) );
  return rectangle;
}

QRectF ShadowChatTranscript::senderLabelRect( const Bubble& bubble ) const
{
  if( !bubble.table || bubble.table->document() != document() || bubble.sender.isEmpty() )
    return QRectF();
  const QRectF body = bubbleFrameRect( bubble.table );
  const qreal right = lineWrapColumnOrWidth() -
    document()->rootFrame()->frameFormat().rightMargin() * documentScale( document() );
  return QRectF( body.left(), body.top() - bubble.senderHeight,
                 qMax( 0.0, qMin( MaximumBubbleWidth, right - body.left() ) ),
                 bubble.senderHeight - 4.0 );
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

QTextTable* ShadowChatTranscript::messageFrameAt( const QPoint& viewportPosition ) const
{
  const int horizontal = isRightToLeft()
    ? horizontalScrollBar()->maximum() - horizontalScrollBar()->value()
    : horizontalScrollBar()->value();
  const QPointF position = viewportPosition + QPoint( horizontal, verticalScrollBar()->value() );
  for( const Bubble& bubble : m_bubbles )
  {
    if( !bubble.row || !bubble.table )
      continue;
    const QRectF row = document()->documentLayout()->frameBoundingRect( bubble.row );
    if( senderLabelRect( bubble ).contains( position ) || row.contains( position ) )
      return bubble.row;
  }
  return Q_NULLPTR;
}

void ShadowChatTranscript::rebuildMedia()
{
  QHash<QString,AnimatedMedia> previous = m_media;
  m_media.clear();
  QSet<QString> static_sources;
  for( QTextBlock block = document()->begin(); block.isValid(); block = block.next() )
    for( auto iterator = block.begin(); !iterator.atEnd(); ++iterator )
    {
      const QTextFragment fragment = iterator.fragment();
      if( !fragment.charFormat().isImageFormat() )
        continue;
      const QTextImageFormat image = fragment.charFormat().toImageFormat();
      QTextCursor location( document() );
      location.setPosition( fragment.position() );
      bool inside = false;
      for( QTextFrame* frame = location.currentFrame(); frame; frame = frame->parentFrame() )
        inside = inside || frame->frameFormat().hasProperty( BubbleColorProperty );
      if( !inside || image.anchorHref().startsWith( "shadowuser:" ) || image.name().contains( "chat-avatar-" ) )
        static_sources.insert( image.name() );
    }
  for( const Bubble& bubble : m_bubbles )
  {
    if( !bubble.table )
      continue;
    for( QTextBlock block = document()->findBlock( bubble.table->firstPosition() );
         block.isValid() && block.position() <= bubble.table->lastPosition(); block = block.next() )
      for( auto iterator = block.begin(); !iterator.atEnd(); ++iterator )
      {
        const QTextFragment fragment = iterator.fragment();
        if( !fragment.isValid() || !fragment.charFormat().isImageFormat() )
          continue;
        QTextImageFormat image = fragment.charFormat().toImageFormat();
        if( image.anchorHref().startsWith( "shadowuser:" ) || image.name().contains( "chat-avatar-" ) )
          continue;
        const QString local = localGifPath( image.name(), document()->baseUrl() );
        if( local.isEmpty() )
          continue;
        QTextCursor cursor( document() );
        cursor.setPosition( fragment.position() );
        cursor.setPosition( fragment.position() + 1, QTextCursor::KeepAnchor );
        if( static_sources.contains( image.name() ) )
        {
          const QVariant initial = document()->resource( QTextDocument::ImageResource, QUrl( image.name() ) );
          QUrl animation_source = QUrl::fromLocalFile( local );
          animation_source.setFragment( "shadow-transcript-animation" );
          image.setName( animation_source.toString() );
          document()->addResource( QTextDocument::ImageResource, animation_source, initial );
          cursor.setCharFormat( image );
        }
        if( !m_media.contains( image.name() ) )
        {
          AnimatedMedia media = previous.take( image.name() );
          media.path = local;
          media.occurrences.clear();
          m_media.insert( image.name(), media );
        }
        m_media[image.name()].occurrences.append( cursor );
      }
  }
  for( const AnimatedMedia& media : previous )
    delete media.movie.data();
}

QRectF ShadowChatTranscript::mediaRect( const QTextCursor& cursor ) const
{
  const QTextImageFormat format = cursor.charFormat().toImageFormat();
  QTextCursor position = cursor;
  position.setPosition( cursor.selectionStart() );
  const QRect rectangle = cursorRect( position );
  const qreal scale = documentScale( document() );
  const qreal width = qRound( qRound( format.width() ) * scale );
  const qreal height = qRound( qRound( format.height() ) * scale );
  return QRectF( rectangle.left(), rectangle.bottom() + 1.0 - height, width, height );
}

void ShadowChatTranscript::scheduleMediaVisibility()
{
  if( isVisible() && !m_media.isEmpty() && !m_mediaVisibilityTimer.isActive() )
    m_mediaVisibilityTimer.start();
}

void ShadowChatTranscript::updateMediaVisibility()
{
  QStringList visible;
  const QRectF exposed( viewport()->rect() );
  if( isVisible() )
    for( auto iterator = m_media.constBegin(); iterator != m_media.constEnd(); ++iterator )
      for( const QTextCursor& cursor : iterator->occurrences )
        if( cursor.document() == document() && mediaRect( cursor ).intersects( exposed ) )
        {
          visible.append( iterator.key() );
          break;
        }
  std::sort( visible.begin(), visible.end(), [this]( const QString& left, const QString& right ) {
    return m_media.value( left ).occurrences.first().selectionStart() <
           m_media.value( right ).occurrences.first().selectionStart();
  } );
  visible = visible.mid( 0, MaximumMovies );
  int movies = 0;
  for( auto iterator = m_media.begin(); iterator != m_media.end(); ++iterator )
    if( iterator->movie )
    {
      ++movies;
      if( !visible.contains( iterator.key() ) && iterator->movie->state() == QMovie::Running )
        iterator->movie->setPaused( true );
    }
  for( const QString& source : visible )
  {
    AnimatedMedia& media = m_media[source];
    if( media.rejected )
      continue;
    if( !media.movie )
    {
      QImageReader reader( media.path );
      if( reader.format().toLower() != "gif" || !reader.supportsAnimation() ||
          reader.size().isEmpty() )
      {
        media.rejected = true;
        continue;
      }
      if( movies >= MaximumMovies )
        for( auto iterator = m_media.begin(); iterator != m_media.end(); ++iterator )
          if( iterator->movie && !visible.contains( iterator.key() ) )
          {
            delete iterator->movie.data();
            --movies;
            break;
          }
      if( movies >= MaximumMovies )
        continue;
      QMovie* movie = new QMovie( media.path, "gif", this );
      movie->setObjectName( QStringLiteral( "ShadowTranscriptMovie" ) );
      movie->setCacheMode( QMovie::CacheAll );
      movie->setScaledSize( reader.size().scaled( reader.size().boundedTo( QSize( 120, 120 ) ),
                                                Qt::KeepAspectRatio ) );
      media.movie = movie;
      ++movies;
      connect( movie, &QMovie::frameChanged, this, [this, source]( int ) { updateMediaFrame( source ); } );
    }
    if( media.movie->state() == QMovie::Paused )
      media.movie->setPaused( false );
    else if( media.movie->state() == QMovie::NotRunning )
      media.movie->start();
  }
}

void ShadowChatTranscript::updateMediaFrame( const QString& source )
{
  const auto found = m_media.constFind( source );
  if( found == m_media.constEnd() || !found->movie || !isVisible() )
    return;
  const QImage frame = found->movie->currentImage();
  if( frame.isNull() )
    return;
  document()->addResource( QTextDocument::ImageResource, QUrl( source ), frame );
  for( const QTextCursor& cursor : found->occurrences )
    if( cursor.document() == document() )
      viewport()->update( mediaRect( cursor ).toAlignedRect().adjusted( -1, -1, 1, 1 ) );
}

void ShadowChatTranscript::paintEvent( QPaintEvent* event )
{
  QAbstractTextDocumentLayout* layout = document()->documentLayout();
  if( layout->paintDevice() != viewport() )
    layout->setPaintDevice( viewport() );
  const qreal scale = documentScale( document() );
  if( std::any_of( m_bubbles.constBegin(), m_bubbles.constEnd(),
        [scale]( const Bubble& bubble ) { return !qFuzzyCompare( bubble.deviceScale, scale ); } ) )
    normalizeBubbles();
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
  {
    QPainter painter( viewport() );
    painter.setClipRegion( event->region() );
    const int horizontal_offset = isRightToLeft()
      ? horizontalScrollBar()->maximum() - horizontalScrollBar()->value()
      : horizontalScrollBar()->value();
    painter.translate( -horizontal_offset, -verticalScrollBar()->value() );
    const QRectF exposed = QRectF( event->rect() ).translated( horizontal_offset, verticalScrollBar()->value() );
    for( const Bubble& bubble : m_bubbles )
    {
      const QRectF rectangle = senderLabelRect( bubble );
      if( rectangle.isEmpty() || !rectangle.intersects( exposed ) )
        continue;
      painter.setFont( bubble.senderFont );
      painter.setPen( bubble.senderColor );
      const QString label = QFontMetricsF( bubble.senderFont ).elidedText(
        bubble.sender, Qt::ElideRight, rectangle.width() );
      painter.drawText( rectangle, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, label );
    }
  }
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
  scheduleMediaVisibility();
}

void ShadowChatTranscript::hideEvent( QHideEvent* event )
{
  m_transferAnimationTimer.stop();
  m_mediaVisibilityTimer.stop();
  for( const AnimatedMedia& media : m_media )
    if( media.movie && media.movie->state() == QMovie::Running )
      media.movie->setPaused( true );
  QTextBrowser::hideEvent(event);
}

bool ShadowChatTranscript::viewportEvent( QEvent* event )
{
  if(event->type()==QEvent::ToolTip)
  {
    const QHelpEvent* help=static_cast<QHelpEvent*>(event);
    const int horizontal_offset = isRightToLeft()
      ? horizontalScrollBar()->maximum() - horizontalScrollBar()->value()
      : horizontalScrollBar()->value();
    const QPointF position = help->pos() + QPoint( horizontal_offset, verticalScrollBar()->value() );
    for( const Bubble& bubble : m_bubbles )
      if( senderLabelRect( bubble ).contains( position ) )
      {
        QToolTip::showText( help->globalPos(),
          QStringLiteral( "<qt>%1</qt>" ).arg( bubble.sender.toHtmlEscaped() ), viewport() );
        return true;
      }
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
  if( !m_resizePending && !m_normalizing )
  {
    m_resizePending = true;
    m_resizeAtBottom = verticalScrollBar()->maximum() > 0 &&
      verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 1;
  }
  QTextBrowser::resizeEvent( event );
  m_normalizeTimer.start( 48 );
  scheduleMediaVisibility();
}

void ShadowChatTranscript::changeEvent( QEvent* event )
{
  QTextBrowser::changeEvent( event );
  if( event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange ||
      event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutDirectionChange )
    invalidateContents();
}
