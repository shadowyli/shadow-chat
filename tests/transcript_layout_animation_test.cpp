#ifdef SHADOW_TRANSCRIPT_REAL_IMAGES
#include "BeeApplication.h"
#include "ChatManager.h"
#include "Core.h"
#include "GuiChat.h"
#include "Protocol.h"
#include "Random.h"
#include "Settings.h"
#include "ShadowChatTranscript.h"
#include "ShadowImageAlbum.h"
#include "ShadowStyle.h"
#include "UserManager.h"
#include <QtWidgets>
#include <QTest>
#include <QTextTable>
#include <cstdio>

class TranscriptImageTestWindow : public QMainWindow
{
public:
  void show()
  {
    bool selected = false;
    const int index = qEnvironmentVariableIntValue( "TRANSCRIPT_TEST_SCREEN", &selected );
    const auto screens = QGuiApplication::screens();
    if( selected && index >= 0 && index < screens.size() )
    {
      winId();
      windowHandle()->setScreen( screens[index] );
      move( screens[index]->availableGeometry().topLeft() + QPoint( 40, 40 ) );
      std::fprintf( stdout, "TEST screen=%d name=%s dpi=%.2f\n", index,
        qPrintable( screens[index]->name() ), screens[index]->logicalDotsPerInchY() );
    }
    QMainWindow::show();
  }
};

#define QMainWindow TranscriptImageTestWindow
#define main transcriptImageOriginalMain
#include "image_bubbles_test.cpp"
#undef main
#undef QMainWindow

int main( int argc, char** argv )
{
  return transcriptImageOriginalMain( argc, argv );
}

#else
#include "ShadowChatTranscript.h"
#include <QtTest>
#include <QtWidgets>
#include <QAbstractTextDocumentLayout>
#include <QMovie>
#include <QTextTable>

namespace
{
  QByteArray animatedGif()
  {
    QByteArray bytes( "GIF89a" );
    auto word = [&]( int value ) {
      bytes.append( char( value & 255 ) );
      bytes.append( char( value >> 8 ) );
    };
    word( 80 );
    word( 60 );
    bytes.append( QByteArray::fromHex( "800000e84c42357dde" ) );
    bytes.append( QByteArray::fromHex( "21ff0b4e45545343415045322e300301000000" ) );
    for( int color = 0; color < 2; ++color )
    {
      bytes.append( QByteArray::fromHex( "21f90400060000002c" ) );
      word( 0 );
      word( 0 );
      word( 80 );
      word( 60 );
      bytes.append( char( 0 ) );
      bytes.append( char( 2 ) );
      QByteArray compressed;
      unsigned accumulator = 0;
      int bits = 0;
      auto code = [&]( unsigned value ) {
        accumulator |= value << bits;
        bits += 3;
        while( bits >= 8 )
        {
          compressed.append( char( accumulator & 255 ) );
          accumulator >>= 8;
          bits -= 8;
        }
      };
      for( int pixel = 0; pixel < 80 * 60; ++pixel )
      {
        code( 4 );
        code( unsigned( color ) );
      }
      code( 5 );
      if( bits )
        compressed.append( char( accumulator ) );
      for( int offset = 0; offset < compressed.size(); offset += 255 )
      {
        const QByteArray part = compressed.mid( offset, 255 );
        bytes.append( char( part.size() ) );
        bytes.append( part );
      }
      bytes.append( char( 0 ) );
    }
    bytes.append( ';' );
    return bytes;
  }

  QString image( const QString& source, int width = 80, int height = 60 )
  {
    return QStringLiteral( "<a href=\"%1\"><img src=\"%1\" width=\"%2\" height=\"%3\"></a>" )
      .arg( source.toHtmlEscaped() ).arg( width ).arg( height );
  }

  QString message( const QString& body, bool outgoing = false, const QString& sender = QString(),
                   const QString& avatar = QString(), bool transfer = false )
  {
    const QString heading = outgoing ? QString() : QStringLiteral(
      "<font color=\"#88929f\" style=\"font-size:11px\">%1</font><br>" ).arg( sender.toHtmlEscaped() );
    const QString content = QStringLiteral(
      "<td width=\"480\">%1<table width=\"480\" cellspacing=\"0\" cellpadding=\"10\" bgcolor=\"%2\">"
      "<tr><td>%3</td></tr></table></td>" ).arg( heading, outgoing ? "#DDEBFF" : "#F0F2F4", body );
    const QString portrait = QStringLiteral(
      "<td width=\"38\" valign=\"top\" align=\"center\"><a href=\"shadowuser:synthetic\">%1</a></td>" )
      .arg( avatar.isEmpty() ? QString() : image( avatar, 32, 32 ) );
    const QString stamp = QStringLiteral( "<td width=\"%1\" valign=\"bottom\" align=\"center\">%2"
      "<font color=\"#9BA4AE\" style=\"font-size:10px\">10:31</font></td>" )
      .arg( transfer ? 64 : 44 ).arg( transfer ? QStringLiteral(
        "<a href=\"shadowtransfer:fixture\"><img src=\"shadowtransfer:fixture\" width=\"12\" height=\"12\"></a>&nbsp;" )
        : QString() );
    return QStringLiteral( "<table align=\"%1\" width=\"562\" cellspacing=\"0\" cellpadding=\"0\" "
      "style=\"margin-top:8px;margin-bottom:16px\"><tr>%2</tr></table>" )
      .arg( outgoing ? "right" : "left", outgoing ? stamp + content + portrait : portrait + content + stamp );
  }

  QList<QTextTable*> bubbleTables( ShadowChatTranscript& transcript )
  {
    QList<QTextTable*> tables;
    for( QTextFrame* frame : transcript.document()->rootFrame()->childFrames() )
      for( QTextFrame* child : frame->childFrames() )
        if( QTextTable* table = qobject_cast<QTextTable*>( child ) )
          if( table->format().hasProperty( ShadowChatTranscript::BubbleColorProperty ) )
            tables.append( table );
    return tables;
  }

  QRectF cursorArea( ShadowChatTranscript& transcript, QTextCursor cursor )
  {
    const int first = cursor.selectionStart();
    const int last = cursor.selectionEnd();
    cursor.setPosition( first );
    const QRect left = transcript.cursorRect( cursor );
    cursor.setPosition( last );
    const QRect right = transcript.cursorRect( cursor );
    return QRectF( left.left() + transcript.horizontalScrollBar()->value(),
                   left.top() + transcript.verticalScrollBar()->value(),
                   right.left() - left.left(), qMax( left.height(), right.height() ) );
  }

  QTextCursor findImage( QTextDocument* document, const QString& source )
  {
    for( QTextBlock block = document->begin(); block.isValid(); block = block.next() )
      for( auto iterator = block.begin(); !iterator.atEnd(); ++iterator )
      {
        const QTextFragment fragment = iterator.fragment();
        if( fragment.charFormat().isImageFormat() && fragment.charFormat().toImageFormat().name() == source )
        {
          QTextCursor cursor( document );
          cursor.setPosition( fragment.position() );
          cursor.setPosition( fragment.position() + 1, QTextCursor::KeepAnchor );
          return cursor;
        }
      }
    return QTextCursor();
  }

  QRectF imageArea( ShadowChatTranscript& transcript, QTextCursor cursor )
  {
    const QTextImageFormat format = cursor.charFormat().toImageFormat();
    cursor.setPosition( cursor.selectionStart() );
    const QRect rectangle = transcript.cursorRect( cursor );
    return QRectF( rectangle.left() + transcript.horizontalScrollBar()->value(),
      rectangle.bottom() + 1 + transcript.verticalScrollBar()->value() - format.height(),
      format.width(), format.height() );
  }

  void prepare( ShadowChatTranscript& transcript, const QString& html, const QSize& size = QSize( 960, 620 ) )
  {
    QFont font( "Segoe UI" );
    font.setPixelSize( 14 );
    transcript.setFont( font );
    transcript.setFrameShape( QFrame::NoFrame );
    transcript.resize( size );
    transcript.move( QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint( 40, 40 ) );
    transcript.setHtml( html );
    transcript.show();
    QTest::qWait( 100 );
    transcript.verticalScrollBar()->setValue( 0 );
  }

  void screenshot( QWidget& widget, const QString& name )
  {
    const QString directory = qEnvironmentVariable( "TRANSCRIPT_TEST_ARTIFACTS", QDir::currentPath() );
    const QString scale = qEnvironmentVariable( "QT_SCALE_FACTOR", "1" );
    const QString file = directory + "/" + name + "-scale-" + scale + ".png";
    QVERIFY2( widget.grab().save( file ), qPrintable( file ) );
    qInfo().noquote() << "SCREENSHOT" << file;
  }
}

class TranscriptLayoutAnimationTest : public QObject
{
  Q_OBJECT
private slots:
  void contentFitsActualGeometry()
  {
    QTemporaryDir directory;
    const QString photo = directory.filePath( "photo.png" );
    const QString avatar = directory.filePath( "avatar.png" );
    QImage bitmap( 80, 60, QImage::Format_RGB32 );
    bitmap.fill( QColor( "#54a6b4" ) );
    QVERIFY( bitmap.save( photo ) );
    QVERIFY( bitmap.scaled( 32, 32 ).save( avatar ) );
    const QString photoUrl = QUrl::fromLocalFile( photo ).toString();
    const QString avatarUrl = QUrl::fromLocalFile( avatar ).toString();
    ShadowChatTranscript transcript;
    prepare( transcript, message( "OK", true, QString(), avatarUrl ) +
      message( QStringLiteral( "短消息" ), false, QStringLiteral( "名字很长也不应该撑宽气泡的合成成员" ), avatarUrl ) +
      message( image( photoUrl ), false, "Media", avatarUrl ) +
      message( "sent", true, QString(), avatarUrl, true ) );
    QCOMPARE( transcript.bubbleCount(), 4 );
    const auto tables = bubbleTables( transcript );
    const auto areas = transcript.bubbleFrameRects();
    for( int index : { 0, 1, 3 } )
    {
      const QString label = index == 0 ? "OK" : index == 1 ? QStringLiteral( "短消息" ) : "sent";
      const QRectF glyphs = cursorArea( transcript, transcript.document()->find( label ) );
      qInfo() << "GEOMETRY text" << label << "bubble" << areas[index] << "glyphs" << glyphs;
      QVERIFY( glyphs.left() - areas[index].left() >= 8.0 );
      QVERIFY( glyphs.left() - areas[index].left() <= 12.0 );
      QVERIFY( areas[index].right() - glyphs.right() >= 8.0 );
      QVERIFY( areas[index].right() - glyphs.right() <= 13.0 );
      QVERIFY( qAbs( glyphs.top() - areas[index].top() - 10.0 ) <= 2.0 );
      QVERIFY( qAbs( areas[index].bottom() - glyphs.bottom() - 10.0 ) <= 2.0 );
    }
    const QRectF media = imageArea( transcript, findImage( transcript.document(), photoUrl ) );
    qInfo() << "GEOMETRY image" << areas[2] << media;
    QVERIFY( qAbs( areas[2].width() - 100.0 ) <= 2.0 );
    QVERIFY( qAbs( areas[2].height() - 80.0 ) <= 2.0 );
    QVERIFY( qAbs( media.left() - areas[2].left() - 10.0 ) <= 2.0 );
    QVERIFY( qAbs( areas[2].right() - media.right() - 10.0 ) <= 2.0 );
    QVERIFY( qAbs( media.top() - areas[2].top() - 10.0 ) <= 2.0 );
    QVERIFY( qAbs( areas[2].bottom() - media.bottom() - 10.0 ) <= 2.0 );
    for( int index = 0; index < tables.size(); ++index )
    {
      QTextTable* row = qobject_cast<QTextTable*>( tables[index]->parentFrame() );
      const bool outgoing = row->format().alignment().testFlag( Qt::AlignRight );
      const int timeColumn = outgoing ? 0 : 2;
      const int avatarColumn = outgoing ? 2 : 0;
      QCOMPARE( row->format().columnWidthConstraints()[timeColumn].rawValue(), index == 3 ? 64.0 : 44.0 );
      QCOMPARE( row->format().columnWidthConstraints()[avatarColumn].rawValue(), 38.0 );
      QTextCursor stamp = transcript.document()->find( "10:31", row->cellAt( 0, timeColumn ).firstCursorPosition() );
      const QRectF timestamp = cursorArea( transcript, stamp );
      const qreal gap = outgoing ? areas[index].left() - timestamp.right() : timestamp.left() - areas[index].right();
      qInfo() << "GEOMETRY timestamp" << index << timestamp << "gap" << gap;
      QVERIFY( gap >= 0.0 && gap <= (index == 3 ? 24.0 : 13.0) );
      QTextCursor portrait = row->cellAt( 0, avatarColumn ).firstCursorPosition();
      while( portrait.position() < row->cellAt( 0, avatarColumn ).lastPosition() && !portrait.charFormat().isImageFormat() )
        portrait.movePosition( QTextCursor::NextCharacter );
      const QRectF portraitArea = imageArea( transcript, portrait );
      const qreal avatarGap = outgoing ? portraitArea.left() - areas[index].right() : areas[index].left() - portraitArea.right();
      qInfo() << "GEOMETRY avatar" << portraitArea << "gap" << avatarGap;
      QVERIFY( avatarGap >= 0.0 && avatarGap <= 9.0 );
    }
    QCOMPARE( transcript.horizontalScrollBar()->maximum(), 0 );
    screenshot( transcript, "transcript-layout" );
  }

  void nicknameStaysOnOneIndependentLine()
  {
    const QString sender = QStringLiteral( "长昵称<&>完整成员名称" ).repeated( 14 );
    const QString body = QStringLiteral( "完整短正文" );
    const QString html = message( body, false, sender ) + message( body, false, QStringLiteral( "短名" ) );
    ShadowChatTranscript transcript;
    prepare( transcript, html, QSize( 960, 620 ) );
    QTextDocument original;
    original.setHtml( html );
    QCOMPARE( transcript.toPlainText(), original.toPlainText() );
    QVERIFY( transcript.toHtml().contains( sender.toHtmlEscaped() ) );
    const auto tables = bubbleTables( transcript );
    QCOMPARE( tables.size(), 2 );
    const auto areas = transcript.bubbleFrameRects();
    QCOMPARE( areas[0].width(), areas[1].width() );
    auto layout = transcript.document()->documentLayout();
    const QRectF firstRow = layout->frameBoundingRect( tables[0]->parentFrame() );
    const QRectF secondRow = layout->frameBoundingRect( tables[1]->parentFrame() );
    qInfo() << "NICKNAME rows" << firstRow << secondRow << "bubbles" << areas;
    QVERIFY( qAbs( firstRow.height() - secondRow.height() ) <= 1.0 );
    QVERIFY( firstRow.height() < 90.0 );
    const QTextCursor fullName = transcript.document()->find( sender );
    QVERIFY( !fullName.isNull() );
    QVERIFY( !fullName.block().isVisible() );
    QFont nameFont = transcript.font();
    nameFont.setPixelSize( 11 );
    const int lineHeight = QFontMetrics( nameFont ).height();
    const QImage rendered = transcript.viewport()->grab().toImage();
    const qreal scale = rendered.devicePixelRatio();
    const QRect header = QRectF( areas[0].left(), firstRow.top(),
      transcript.viewport()->width() - areas[0].left(), areas[0].top() - firstRow.top() ).toAlignedRect();
    QRect ink;
    for( int vertical = qMax( 0, qFloor( header.top() * scale ) );
         vertical < qMin( rendered.height(), qCeil( header.bottom() * scale ) ); ++vertical )
      for( int horizontal = qMax( 0, qFloor( header.left() * scale ) );
           horizontal < qMin( rendered.width(), qCeil( header.right() * scale ) ); ++horizontal )
      {
        const QColor color = rendered.pixelColor( horizontal, vertical );
        if( qAbs( color.red() - 136 ) <= 10 && qAbs( color.green() - 146 ) <= 10 &&
            qAbs( color.blue() - 159 ) <= 10 )
          ink = ink.united( QRect( horizontal, vertical, 1, 1 ) );
      }
    qInfo() << "NICKNAME rendered-ink" << ink << "scale" << scale << "one-line-height" << lineHeight;
    QVERIFY( !ink.isEmpty() );
    QVERIFY( ink.height() <= qCeil( lineHeight * scale ) );
    QVERIFY( ink.width() > areas[0].width() * scale );
    QVERIFY( ink.right() <= qCeil( (areas[0].left() + 560.0) * scale ) );
    const QPoint hover( qRound( areas[0].right() + 30 ),
      qRound( areas[0].top() - 4.0 - lineHeight / 2.0 ) );
    QVERIFY( hover.x() > areas[0].right() );
    QHelpEvent help( QEvent::ToolTip, hover, transcript.viewport()->mapToGlobal( hover ) );
    QApplication::sendEvent( transcript.viewport(), &help );
    QTRY_VERIFY( QToolTip::isVisible() );
    QTextDocument tooltip;
    tooltip.setHtml( QToolTip::text() );
    QCOMPARE( tooltip.toPlainText(), sender );
    QToolTip::hideText();
    const QTextCursor selected = transcript.document()->find( body );
    transcript.setTextCursor( selected );
    const QString plain = transcript.toPlainText();
    const QString saved = transcript.toHtml();
    for( int width : { 320, 1400, 420, 960 } )
    {
      transcript.resize( width, 620 );
      QTest::qWait( 120 );
      QCOMPARE( transcript.toPlainText(), plain );
      QCOMPARE( transcript.textCursor().selectedText(), body );
      QCOMPARE( transcript.bubbleFrameRects()[0].width(), areas[0].width() );
      QCOMPARE( transcript.horizontalScrollBar()->maximum(), 0 );
    }
    transcript.setTextCursor( QTextCursor( transcript.document() ) );
    transcript.setHtml( saved );
    QTest::qWait( 100 );
    QVERIFY( !transcript.document()->find( sender ).isNull() );
    QCOMPARE( transcript.bubbleFrameRects()[0].width(), areas[0].width() );
    screenshot( transcript, "transcript-nickname" );
  }

  void nestedTablesLinksSelectionAndResize()
  {
    ShadowChatTranscript transcript;
    const QString body = "<table width=\"700\" cellpadding=\"3\"><tr><td width=\"350\">A</td>"
      "<td width=\"350\"><a href=\"https://example.invalid/?a=1&amp;b=2\">click</a></td></tr></table>"
      "<br><b>bold</b> <i>italic</i><br>last line";
    prepare( transcript, message( body ) + message( QString( 400, 'W' ), true ) );
    QTextCursor selected = transcript.document()->find( "bold" );
    transcript.setTextCursor( selected );
    const QString plain = transcript.toPlainText();
    const QString html = transcript.toHtml();
    QVERIFY( transcript.bubbleFrameRects()[0].width() < 180.0 );
    for( int width : { 420, 1400, 620, 1200 } )
    {
      transcript.resize( width, 620 );
      QTest::qWait( 120 );
      qInfo() << "RESIZE width" << width << "viewport" << transcript.viewport()->width()
              << "wrap" << transcript.lineWrapColumnOrWidth()
              << "document" << transcript.document()->documentLayout()->documentSize()
              << "horizontal-maximum" << transcript.horizontalScrollBar()->maximum();
      QCOMPARE( transcript.toPlainText(), plain );
      QCOMPARE( transcript.textCursor().selectedText(), QString( "bold" ) );
      QCOMPARE( transcript.horizontalScrollBar()->maximum(), 0 );
      for( const QRectF& area : transcript.bubbleFrameRects() )
        QVERIFY( area.right() <= transcript.viewport()->width() + 1 );
    }
    QTextCursor link = transcript.document()->find( "click" );
    const QRectF target = cursorArea( transcript, link );
    const QPoint point = target.center().toPoint() - QPoint( 0, transcript.verticalScrollBar()->value() );
    QCOMPARE( transcript.anchorAt( point ), QString( "https://example.invalid/?a=1&b=2" ) );
    QTextDocument roundTrip;
    roundTrip.setHtml( html );
    transcript.setHtml( html );
    QTest::qWait( 120 );
    QCOMPARE( transcript.toPlainText(), roundTrip.toPlainText() );
    QCOMPARE( transcript.bubbleCount(), 2 );
  }

  void documentDeviceDpiKeepsRealImageBounds()
  {
    QTemporaryDir directory;
    const QString path = directory.filePath( "landscape.png" );
    QImage original( 480, 260, QImage::Format_RGB32 );
    original.fill( QColor( "#548AC5" ) );
    QVERIFY( original.save( path ) );
    const QString source = QUrl::fromLocalFile( path ).toString();
    const QString single = "<table cellspacing=\"0\" cellpadding=\"0\"><tr><td align=\"center\">" +
      image( source, 360, 195 ) + "</td></tr></table>";
    QString album = "<table cellspacing=\"6\" cellpadding=\"0\">";
    for( int index = 0; index < 9; ++index )
    {
      if( index % 3 == 0 )
        album += "<tr>";
      album += "<td align=\"center\">" + image( source, 144, 78 ) + "</td>";
      if( index % 3 == 2 )
        album += "</tr>";
    }
    album += "</table>";
    QVector<QImage> devices;
    for( int dpi : { 96, 120, 144, 96 } )
    {
      QImage device( 1, 1, QImage::Format_RGB32 );
      device.setDotsPerMeterX( qRound( dpi / 0.0254 ) );
      device.setDotsPerMeterY( qRound( dpi / 0.0254 ) );
      devices.append( device );
    }
    ShadowChatTranscript transcript;
    transcript.resize( 1100, 620 );
    transcript.setHtml( message( single, true ) + message( album, true ) );
    const QString plain = transcript.toPlainText();
    for( QImage& device : devices )
    {
      transcript.document()->documentLayout()->setPaintDevice( &device );
      transcript.normalizeBubbles();
      const auto rectangles = transcript.bubbleFrameRects();
      QCOMPARE( rectangles.size(), 2 );
      qInfo() << "DOCUMENT-DPI" << device.logicalDpiY() << "rectangles" << rectangles;
      for( const QRectF& rectangle : rectangles )
      {
        QVERIFY( rectangle.width() <= 560.0 );
        QVERIFY( rectangle.right() <= transcript.viewport()->width() + 1.0 );
      }
      QVERIFY( qAbs( rectangles[0].width() - qMin( 560.0, 380.0 * device.logicalDpiY() / 96.0 ) ) <= 1.0 );
      QCOMPARE( transcript.toPlainText(), plain );
    }
    QVERIFY( qAbs( transcript.bubbleFrameRects()[0].width() - 380.0 ) <= 1.0 );
  }

  void appendPreservesExistingBubblesWithoutLargeFormatChanges()
  {
    ShadowChatTranscript transcript;
    QString html;
    for( int index = 0; index < 24; ++index )
      html += message( QString( "stable-selection-%1 " ).arg( index ) + QString( 50, 'a' ),
        index % 2, QStringLiteral( "合成成员" ) );
    prepare( transcript, html );
    const QString plain = transcript.toPlainText();
    const auto originalBubbles = bubbleTables( transcript );
    const QTextTableFormat firstFormat = originalBubbles.first()->format();
    QTextCursor selected = transcript.document()->find( "stable-selection-0" );
    transcript.setTextCursor( selected );
    int largeRemovals = 0;
    connect( transcript.document(), &QTextDocument::contentsChange, &transcript,
      [&]( int, int removed, int ) { if( removed > 64 ) ++largeRemovals; } );
    QTextCursor appended( transcript.document() );
    appended.movePosition( QTextCursor::End );
    appended.insertHtml( message( "new", true ) );
    transcript.normalizeBubbles();
    QCOMPARE( transcript.bubbleCount(), 25 );
    QCOMPARE( largeRemovals, 0 );
    QCOMPARE( originalBubbles.first()->format(), firstFormat );
    QCOMPARE( transcript.textCursor().selectedText(), QString( "stable-selection-0" ) );
    QVERIFY( transcript.toPlainText().startsWith( plain.left( plain.size() - 1 ) ) );
  }

  void animationChangesPixelsAndPauses()
  {
    QTemporaryDir directory;
    const QString path = directory.filePath( "original.gif" );
    QFile file( path );
    QVERIFY( file.open( QIODevice::WriteOnly ) );
    const QByteArray original = animatedGif();
    QCOMPARE( file.write( original ), qint64( original.size() ) );
    file.close();
    const QString source = QUrl::fromLocalFile( path ).toString();
    ShadowChatTranscript transcript;
    prepare( transcript, message( image( source ) ) + QString( 80, '\n' ).replace( "\n", "<p>filler</p>" ) );
    QTRY_COMPARE( transcript.findChildren<QMovie*>().size(), 1 );
    QMovie* movie = transcript.findChildren<QMovie*>().first();
    QCOMPARE( movie->state(), QMovie::Running );
    QCOMPARE( movie->cacheMode(), QMovie::CacheAll );
    QCOMPARE( movie->frameCount(), 2 );
    QSignalSpy frames( movie, &QMovie::frameChanged );
    QSignalSpy edits( transcript.document(), &QTextDocument::contentsChanged );
    const auto geometry = transcript.bubbleFrameRects();
    const QString html = transcript.toHtml();
    const QString text = transcript.toPlainText();
    const QImage first = transcript.document()->resource( QTextDocument::ImageResource, QUrl( source ) ).value<QImage>();
    QVERIFY( !first.isNull() );
    const QPoint pixel = imageArea( transcript, findImage( transcript.document(), source ) ).center().toPoint();
    auto renderedColor = [&]() {
      const QImage rendered = transcript.viewport()->grab().toImage();
      return rendered.pixelColor( qRound( pixel.x() * rendered.devicePixelRatio() ),
                                  qRound( pixel.y() * rendered.devicePixelRatio() ) );
    };
    const QColor firstPixel = renderedColor();
    QTRY_VERIFY( frames.size() >= 3 );
    QTRY_VERIFY( transcript.document()->resource( QTextDocument::ImageResource, QUrl( source ) )
      .value<QImage>().pixelColor( 0, 0 ) != first.pixelColor( 0, 0 ) );
    QTRY_VERIFY( renderedColor() != firstPixel );
    QCOMPARE( edits.size(), 0 );
    QCOMPARE( transcript.bubbleFrameRects(), geometry );
    QCOMPARE( transcript.toHtml(), html );
    QCOMPARE( transcript.toPlainText(), text );
    transcript.verticalScrollBar()->setValue( transcript.verticalScrollBar()->maximum() );
    QTRY_COMPARE( movie->state(), QMovie::Paused );
    const int pausedFrames = frames.size();
    QTest::qWait( 220 );
    QCOMPARE( frames.size(), pausedFrames );
    transcript.verticalScrollBar()->setValue( 0 );
    QTRY_COMPARE( movie->state(), QMovie::Running );
    QTRY_VERIFY( frames.size() > pausedFrames );
    transcript.hide();
    QCOMPARE( movie->state(), QMovie::Paused );
    const int hiddenFrames = frames.size();
    QTest::qWait( 220 );
    QCOMPARE( frames.size(), hiddenFrames );
    transcript.show();
    QTRY_COMPARE( movie->state(), QMovie::Running );
    QTRY_VERIFY( frames.size() > hiddenFrames );
    transcript.setHtml( QString() );
    QCOMPARE( transcript.findChildren<QMovie*>().size(), 0 );
    transcript.setHtml( html );
    QTRY_COMPARE( transcript.findChildren<QMovie*>().size(), 1 );
    QMovie* reopened = transcript.findChildren<QMovie*>().first();
    QSignalSpy reopenedFrames( reopened, &QMovie::frameChanged );
    QTRY_VERIFY( reopenedFrames.size() >= 2 );
    QCOMPARE( transcript.toPlainText(), text );
    QCOMPARE( transcript.bubbleFrameRects(), geometry );
    QVERIFY( file.open( QIODevice::ReadOnly ) );
    QCOMPARE( file.readAll(), original );
    screenshot( transcript, "transcript-animated" );
  }

  void movieConcurrencyAndFiltering()
  {
    QTemporaryDir directory;
    QString body = "<table cellspacing=\"0\" cellpadding=\"1\">";
    for( int index = 0; index < 16; ++index )
    {
      const QString path = directory.filePath( QString( "media-%1.gif" ).arg( index ) );
      QFile file( path );
      QVERIFY( file.open( QIODevice::WriteOnly ) );
      file.write( animatedGif() );
      const QString source = QUrl::fromLocalFile( path ).toString();
      if( index % 4 == 0 )
        body += "<tr>";
      body += "<td>" + image( source, 20, 15 ) + "</td>";
      if( index % 4 == 3 )
        body += "</tr>";
    }
    body += "</table>";
    const QString avatar = QUrl::fromLocalFile( directory.filePath( "media-0.gif" ) ).toString();
    const QString fake = directory.filePath( "fake.gif" );
    QImage png( 10, 10, QImage::Format_RGB32 );
    png.fill( Qt::green );
    QVERIFY( png.save( fake, "PNG" ) );
    ShadowChatTranscript transcript;
    prepare( transcript, message( body, false, "Synthetic album", avatar ) +
      message( image( QUrl::fromLocalFile( fake ).toString() ) +
               image( "https://example.invalid/remote.gif" ) ) );
    QTRY_COMPARE( transcript.findChildren<QMovie*>().size(), 12 );
    const QImage avatarFrame = transcript.document()->resource( QTextDocument::ImageResource, QUrl( avatar ) ).value<QImage>();
    QVERIFY( !avatarFrame.isNull() );
    for( QMovie* movie : transcript.findChildren<QMovie*>() )
    {
      QCOMPARE( movie->state(), QMovie::Running );
      QVERIFY( movie->fileName() != fake );
      QCOMPARE( movie->scaledSize(), QSize( 80, 60 ) );
    }
    QTest::qWait( 250 );
    QCOMPARE( transcript.document()->resource( QTextDocument::ImageResource, QUrl( avatar ) ).value<QImage>(), avatarFrame );
    transcript.hide();
    for( QMovie* movie : transcript.findChildren<QMovie*>() )
      QCOMPARE( movie->state(), QMovie::Paused );
    screenshot( transcript, "transcript-album" );
  }

  void continuousResizeHeartbeatAndScroll()
  {
    ShadowChatTranscript transcript;
    QString html;
    for( int index = 0; index < 360; ++index )
      html += message( QString( "message-%1 " ).arg( index ) + QString( 80 + index % 100, 'W' ), index % 2,
        index % 2 ? QString() : QStringLiteral( "压测成员" ) + QString::number( index ) );
    prepare( transcript, html );
    transcript.verticalScrollBar()->setValue( transcript.verticalScrollBar()->maximum() / 2 );
    QTextCursor anchor = transcript.cursorForPosition( QPoint( 60, 40 ) );
    const int anchorTop = transcript.cursorRect( anchor ).top();
    const QString plain = transcript.toPlainText();
    QTimer heartbeat;
    heartbeat.setTimerType( Qt::PreciseTimer );
    heartbeat.setInterval( 5 );
    QElapsedTimer elapsed;
    elapsed.start();
    qint64 previous = 0;
    qint64 maximumGap = 0;
    int beats = 0;
    connect( &heartbeat, &QTimer::timeout, &transcript, [&]() {
      const qint64 now = elapsed.elapsed();
      maximumGap = qMax( maximumGap, now - previous );
      previous = now;
      ++beats;
    } );
    QTimer drag;
    drag.setInterval( 8 );
    int steps = 0;
    connect( &drag, &QTimer::timeout, &transcript, [&]() {
      transcript.resize( 460 + (steps * 37) % 1000, 620 );
      if( ++steps >= 100 )
        drag.stop();
    } );
    heartbeat.start();
    drag.start();
    QTRY_VERIFY_WITH_TIMEOUT( !drag.isActive(), 5000 );
    QTest::qWait( 180 );
    heartbeat.stop();
    qInfo() << "PERFORMANCE bubbles" << transcript.bubbleCount() << "resizes" << steps
            << "heartbeats" << beats << "maximum-gap-ms" << maximumGap << "elapsed-ms" << elapsed.elapsed();
    QCOMPARE( steps, 100 );
    QVERIFY( beats >= 70 );
    QVERIFY( elapsed.elapsed() < 3500 );
    QVERIFY2( maximumGap < 250, qPrintable( QString( "heartbeat blocked for %1 ms" ).arg( maximumGap ) ) );
    QCOMPARE( transcript.toPlainText(), plain );
    QCOMPARE( transcript.horizontalScrollBar()->maximum(), 0 );
    transcript.resize( 700, 620 );
    QTest::qWait( 150 );
    QTextCursor reading = transcript.cursorForPosition( QPoint( 60, 40 ) );
    reading.setPosition( reading.block().position() );
    const int readingTop = transcript.cursorRect( reading ).top();
    transcript.resize( 740, 620 );
    QTest::qWait( 150 );
    qInfo() << "SCROLL reading" << reading.position() << "old-top" << readingTop
            << "new-top" << transcript.cursorRect( reading ).top()
            << "scroll" << transcript.verticalScrollBar()->value();
    QVERIFY( qAbs( transcript.cursorRect( reading ).top() - readingTop ) <= 4 );
    QVERIFY( anchor.document() == transcript.document() );
    QVERIFY( anchorTop < transcript.height() );
    transcript.verticalScrollBar()->setValue( transcript.verticalScrollBar()->maximum() );
    transcript.resize( 600, 620 );
    QTest::qWait( 150 );
    QCOMPARE( transcript.verticalScrollBar()->value(), transcript.verticalScrollBar()->maximum() );
    screenshot( transcript, "transcript-resize" );
  }

  void historyPreservesStickerAndMetadata()
  {
    const QByteArray metadata = QJsonDocument( QJsonObject{
      { "sender", "Synthetic" }, { "outgoing", false }, { "type", 8 },
      { "time", "2026-11-02T10:31:00" } } ).toJson( QJsonDocument::Compact ).toBase64();
    const QString html = QString( "<!--shadow-record:%1--><!--shadow-sticker-->"
      "<b>Synthetic</b>: <a href=\"file:///synthetic.gif\"><img src=\"file:///synthetic.gif\" width=\"80\" height=\"60\"></a>" )
      .arg( QString::fromLatin1( metadata ) );
    const auto entries = ShadowChatTranscript::historyEntries( html );
    QCOMPARE( entries.size(), 1 );
    QVERIFY( entries.first().html.contains( "<!--shadow-sticker-->" ) );
    QVERIFY( entries.first().html.contains( "synthetic.gif" ) );
    QCOMPARE( entries.first().sender, QString( "Synthetic" ) );
    QVERIFY( entries.first().hasDirection && !entries.first().outgoing );
    QCOMPARE( entries.first().timestamp, QDateTime::fromString( "2026-11-02T10:31:00", Qt::ISODate ) );
    const auto bubbled = ShadowChatTranscript::historyEntries(
      QString( "<!--shadow-record:%1--><!--shadow-sticker-->" ).arg( QString::fromLatin1( metadata ) ) +
      message( "retained" ) );
    QCOMPARE( bubbled.size(), 1 );
    QVERIFY( bubbled.first().alreadyBubbled );
    QVERIFY( bubbled.first().html.contains( "<!--shadow-sticker-->" ) );
    const auto failed = ShadowChatTranscript::historyEntries(
      QString( "<!--shadow-record:%1--><!--shadow-transfer-error--><b>Synthetic</b>: failure fixture" )
        .arg( QString::fromLatin1( metadata ) ) );
    QCOMPARE( failed.size(), 1 );
    QVERIFY( failed.first().html.contains( "<!--shadow-transfer-error-->" ) );
    QVERIFY( failed.first().html.contains( "failure fixture" ) );
    const QString context = "<!--shadow-context:" + QString( 64, 'a' ) + ":3-->";
    const auto contextual = ShadowChatTranscript::historyEntries(
      QString( "<!--shadow-record:%1-->%2<b>Synthetic</b>: contextual fixture" )
        .arg( QString::fromLatin1( metadata ), context ) );
    QCOMPARE( contextual.size(), 1 );
    QVERIFY( contextual.first().html.contains( context ) );
    QVERIFY( contextual.first().html.contains( "contextual fixture" ) );
    const auto contextualBubble = ShadowChatTranscript::historyEntries(
      QString( "<!--shadow-record:%1--><html><head>%2</head><body>%3</body></html>" )
        .arg( QString::fromLatin1( metadata ), context, message( "contextual bubble" ) ) );
    QCOMPARE( contextualBubble.size(), 1 );
    QVERIFY( contextualBubble.first().alreadyBubbled );
    QVERIFY( contextualBubble.first().html.contains( context ) );
  }
};

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
  QApplication application( argc, argv );
  TranscriptLayoutAnimationTest tests;
  return QTest::qExec( &tests, argc, argv );
}

#include "transcript_layout_animation_test.moc"
#endif
