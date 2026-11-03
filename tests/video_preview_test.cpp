#ifdef _WIN32
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#endif
#ifdef SHADOW_VIDEO_WITH_MAIN
#include "BeeApplication.h"
#include "Core.h"
#include "GuiMain.h"
#include "Random.h"
#include "Settings.h"
#endif
#include "ShadowDialogChrome.h"
#include "ShadowStyle.h"
#include "ShadowVideoPreview.h"
#include <QtWidgets>
#include <QtTest>
#include <QThread>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <functional>

#define CHECK(value) do { if( !(value) ) { std::fprintf( stderr, "FAIL line %d: %s\n", __LINE__, #value ); return 1; } } while( false )

#ifdef SHADOW_VIDEO_WITH_MAIN
typedef GuiMain VideoTestHost;
#else
typedef QWidget VideoTestHost;
#endif

class ExternalVideoOpenProbe : public QObject
{
  Q_OBJECT
public:
  int count = 0;
public slots:
  void capture( const QUrl& ) { ++count; }
};

class FrameDeliveryProbe : public QObject
{
public:
  int delivered = 0;
protected:
  bool eventFilter( QObject*, QEvent* event ) override
  {
    if( event->type() == QEvent::MetaCall )
      ++delivered;
    return false;
  }
};

static bool waitFor( const std::function<bool()>& condition, int timeout = 15000 )
{
  QElapsedTimer elapsed;
  elapsed.start();
  while( !condition() && elapsed.elapsed() < timeout )
    QTest::qWait( 20 );
  return condition();
}

#ifdef Q_OS_WIN
static bool writeSyntheticVideo( const QString& fileName, int videoWidth = 320, int videoHeight = 180, int frameCount = 90 )
{
  using Microsoft::WRL::ComPtr;
  const int width = videoWidth;
  const int height = videoHeight;
  const int frameRate = frameCount / 6 > 0 ? frameCount / 6 : 15;
  const qint64 frameDuration = 10000000 / frameRate;
  const auto checked = []( HRESULT result, const char* action ) {
    if( FAILED( result ) )
      std::fprintf( stderr, "Media Foundation fixture error %s: 0x%08lx\n", action, static_cast<unsigned long>( result ) );
    return SUCCEEDED( result );
  };
  ComPtr<IMFAttributes> attributes;
  if( !checked( MFCreateAttributes( &attributes, 1 ), "attributes" ) )
    return false;
  attributes->SetUINT32( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE );
  ComPtr<IMFSinkWriter> writer;
  if( !checked( MFCreateSinkWriterFromURL( reinterpret_cast<LPCWSTR>( fileName.utf16() ),
      Q_NULLPTR, attributes.Get(), &writer ), "MP4 writer" ) )
    return false;
  ComPtr<IMFMediaType> output;
  MFCreateMediaType( &output );
  output->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
  output->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_H264 );
  output->SetUINT32( MF_MT_AVG_BITRATE, qMax( 800000, width * height * frameRate / 4 ) );
  output->SetUINT32( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
  MFSetAttributeSize( output.Get(), MF_MT_FRAME_SIZE, width, height );
  MFSetAttributeRatio( output.Get(), MF_MT_FRAME_RATE, frameRate, 1 );
  MFSetAttributeRatio( output.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
  DWORD streamIndex = 0;
  if( !checked( writer->AddStream( output.Get(), &streamIndex ), "H264 stream" ) )
    return false;
  ComPtr<IMFMediaType> input;
  MFCreateMediaType( &input );
  input->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
  input->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_NV12 );
  input->SetUINT32( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
  MFSetAttributeSize( input.Get(), MF_MT_FRAME_SIZE, width, height );
  MFSetAttributeRatio( input.Get(), MF_MT_FRAME_RATE, frameRate, 1 );
  MFSetAttributeRatio( input.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );
  if( !checked( writer->SetInputMediaType( streamIndex, input.Get(), Q_NULLPTR ), "NV12 input" ) ||
      !checked( writer->BeginWriting(), "begin encoding" ) )
    return false;
  QByteArray pixels( width * height * 3 / 2, '\0' );
  for( int frameIndex = 0; frameIndex < frameCount; ++frameIndex )
  {
    const bool red = frameIndex < frameRate * 2;
    std::fill( pixels.begin(), pixels.begin() + width * height, char( red ? 82 : 41 ) );
    for( int offset = width * height; offset < pixels.size(); offset += 2 )
    {
      pixels[offset] = char( red ? 90 : 240 );
      pixels[offset + 1] = char( red ? 240 : 110 );
    }
    const int marker = frameIndex * 3 % (width - 20);
    for( int vertical = 8; vertical < 24; ++vertical )
      for( int horizontal = marker; horizontal < marker + 20; ++horizontal )
        pixels[vertical * width + horizontal] = char( 220 );
    ComPtr<IMFMediaBuffer> buffer;
    if( !checked( MFCreateMemoryBuffer( DWORD( pixels.size() ), &buffer ), "frame buffer" ) )
      return false;
    BYTE* bytes = Q_NULLPTR;
    if( !checked( buffer->Lock( &bytes, Q_NULLPTR, Q_NULLPTR ), "lock frame" ) )
      return false;
    std::memcpy( bytes, pixels.constData(), size_t( pixels.size() ) );
    buffer->Unlock();
    buffer->SetCurrentLength( DWORD( pixels.size() ) );
    ComPtr<IMFSample> sample;
    MFCreateSample( &sample );
    sample->AddBuffer( buffer.Get() );
    sample->SetSampleTime( frameIndex * frameDuration );
    sample->SetSampleDuration( frameDuration );
    if( !checked( writer->WriteSample( streamIndex, sample.Get() ), "encode frame" ) )
      return false;
  }
  return checked( writer->Finalize(), "finalize MP4" );
}
#endif

static bool isRed( const QImage& image )
{
  if( image.isNull() )
    return false;
  const QColor pixel = image.pixelColor( image.width() / 2, image.height() / 2 );
  return pixel.red() > 160 && pixel.blue() < 100;
}

static bool isBlue( const QImage& image )
{
  if( image.isNull() )
    return false;
  const QColor pixel = image.pixelColor( image.width() / 2, image.height() / 2 );
  return pixel.blue() > 160 && pixel.red() < 100;
}

static bool routeVideo( VideoTestHost& main, const QUrl& url )
{
#ifdef SHADOW_VIDEO_WITH_MAIN
  const VNumber chatId = ID_INVALID;
  return QMetaObject::invokeMethod( &main, "openUrlFromChat", Qt::DirectConnection,
    Q_ARG( QUrl, url ), Q_ARG( VNumber, chatId ) );
#else
  ShadowVideoPreview* preview = main.findChild<ShadowVideoPreview*>();
  if( preview )
    preview->openFile( url );
  else
    preview = new ShadowVideoPreview( url, &main );
  preview->show();
  return true;
#endif
}

static QImage snapshot( ShadowVideoPreview* preview, const QString& path )
{
  QFile::remove( path );
  if( !waitFor( [&]() { return preview->captureFrame( QFileInfo( path ).absoluteFilePath() ); }, 3000 ) )
  {
    std::fprintf( stderr, "Snapshot request rejected at time %lld\n", static_cast<long long>( preview->position() ) );
    return QImage();
  }
  QImage image;
  waitFor( [&]() { image.load( path ); return !image.isNull(); }, 3000 );
  if( image.isNull() )
    std::fprintf( stderr, "Snapshot request accepted but PNG absent: %s\n", qPrintable( QFileInfo( path ).absoluteFilePath() ) );
  return image;
}

int main( int argc, char** argv )
{
  QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
  QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
#ifdef SHADOW_VIDEO_WITH_MAIN
  BeeApplication app( argc, argv );
#else
  QApplication app( argc, argv );
#endif
  app.setQuitOnLastWindowClosed( false );
  app.setFont( ShadowStyle::applicationFont() );
  std::setvbuf( stdout, Q_NULLPTR, _IONBF, 0 );
  const QString scale = qEnvironmentVariable( "QT_SCALE_FACTOR", "1" );
  const QString fixture = QDir::currentPath() + QStringLiteral( "/synthetic-red-blue-%1.mp4" ).arg( scale );
  const QString highResolutionFixture = QDir::currentPath() +
    QStringLiteral( "/synthetic-high-resolution-%1.mp4" ).arg( scale );
#ifdef Q_OS_WIN
  const HRESULT com = CoInitializeEx( Q_NULLPTR, COINIT_APARTMENTTHREADED );
  CHECK( SUCCEEDED( com ) );
  CHECK( SUCCEEDED( MFStartup( MF_VERSION ) ) );
  CHECK( writeSyntheticVideo( fixture ) );
  CHECK( writeSyntheticVideo( highResolutionFixture, 2560, 1440, 90 ) );
  MFShutdown();
  CoUninitialize();
#else
  CHECK( app.arguments().size() > 1 );
  CHECK( QFile::copy( app.arguments().at( 1 ), fixture ) );
#endif
  CHECK( QFileInfo( fixture ).size() > 1000 );
  std::printf( "PASS generated H264 MP4: 320x180, 15 fps, 90 real NV12 frames, six seconds, red then blue (%lld bytes)\n",
    static_cast<long long>( QFileInfo( fixture ).size() ) );
  CHECK( QFileInfo( highResolutionFixture ).size() > 1000 );
  const QUrl videoUrl = QUrl::fromLocalFile( fixture );
  CHECK( ShadowVideoPreview::canPreview( videoUrl ) );
  foreach( const QString& suffix, QStringList() << "MP4" << "webm" << "mkv" << "avi" << "mov" << "m4v" << "wmv" )
    CHECK( ShadowVideoPreview::canPreview( QUrl::fromLocalFile( QDir::currentPath() + "/missing." + suffix ) ) );
  CHECK( !ShadowVideoPreview::canPreview( QUrl( "https://example.test/video.mp4" ) ) );
  CHECK( !ShadowVideoPreview::canPreview( QUrl::fromLocalFile( QDir::currentPath() + "/missing.txt" ) ) );
  const QString contentOnly = QDir::currentPath() + QStringLiteral( "/synthetic-content-%1.data" ).arg( scale );
  CHECK( QFile::copy( fixture, contentOnly ) || QFileInfo( contentOnly ).isFile() );
  CHECK( ShadowVideoPreview::canPreview( QUrl::fromLocalFile( contentOnly ) ) );

#ifdef SHADOW_VIDEO_WITH_MAIN
  QTemporaryDir temporary;
  CHECK( temporary.isValid() );
  QSettings::setDefaultFormat( QSettings::IniFormat );
  QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, temporary.path() );
  Random::init();
  Settings& settings = Settings::instance();
  settings.setDefaultFolders();
  settings.createLocalUser( QStringLiteral( "synthetic-video-test" ) );
  settings.setAccountDataFolder( temporary.path() );
  settings.load();
  settings.setChatAutoSave( false );
  settings.setCloseMinimizeInTray( false );
  Core core;
  VideoTestHost main;
  main.loadStyle();
#else
  VideoTestHost main;
#endif
  main.resize( 900, 650 );
  main.show();
  QTest::qWait( 50 );
  ExternalVideoOpenProbe external;
  QDesktopServices::setUrlHandler( QStringLiteral( "file" ), &external, "capture" );
  CHECK( routeVideo( main, videoUrl ) );
  ShadowVideoPreview* preview = main.findChild<ShadowVideoPreview*>();
  CHECK( preview && preview->isVisible() );
  const bool firstFrameReady = waitFor( [&]() {
    return preview->duration() >= 5900 && preview->isSeekable() && preview->position() > 100;
  } );
  if( !firstFrameReady )
    std::fprintf( stderr, "libVLC playback diagnosis state=%d feedback=%s\n", int( preview->playbackState() ),
      qPrintable( preview->findChild<QLabel*>( "shadowVideoFeedback" )->text() ) );
  CHECK( firstFrameReady );
  QObject* player = preview->findChild<QObject*>( "shadowVideoPlayer" );
  CHECK( player );
  CHECK( isRed( snapshot( preview, QStringLiteral( "decoded-first-%1.png" ).arg( scale ) ) ) );
#ifdef Q_OS_WIN
  for( const wchar_t* moduleName : { L"libvlc.dll", L"libvlccore.dll", L"libavcodec_plugin.dll" } )
  {
    wchar_t loadedPath[32768];
    CHECK( GetModuleFileNameW( GetModuleHandleW( moduleName ), loadedPath, 32768 ) > 0 );
    const QString loaded = QDir::fromNativeSeparators( QString::fromWCharArray( loadedPath ) );
    CHECK( loaded.startsWith( QCoreApplication::applicationDirPath() + QStringLiteral( "/video-runtime/" ), Qt::CaseInsensitive ) );
    std::printf( "PASS loaded bundled module: %s\n", qPrintable( loaded ) );
  }
#endif
  QPushButton* play = preview->findChild<QPushButton*>( "shadowVideoPlay" );
  QSlider* progress = preview->findChild<QSlider*>( "shadowVideoProgress" );
  QLabel* time = preview->findChild<QLabel*>( "shadowVideoTime" );
  CHECK( play && progress && time && progress->isEnabled() );
  QTest::mouseClick( play, Qt::LeftButton );
  CHECK( waitFor( [&]() { return preview->playbackState() == ShadowVideoPreview::Paused; } ) );
  const qint64 pausedAt = preview->position();
  QTest::qWait( 250 );
  CHECK( qAbs( preview->position() - pausedAt ) < 120 );
  progress->setSliderDown( true );
  progress->setValue( 7500 );
  progress->setSliderDown( false );
  CHECK( waitFor( [&]() { return preview->position() > 4200; } ) );
  CHECK( isBlue( snapshot( preview, QStringLiteral( "decoded-seek-%1.png" ).arg( scale ) ) ) );
  CHECK( preview->playbackState() == ShadowVideoPreview::Paused && time->text().contains( QStringLiteral( "00:04" ) ) );
  QTest::mouseClick( play, Qt::LeftButton );
  CHECK( waitFor( [&]() { return preview->playbackState() == ShadowVideoPreview::Playing && preview->position() > 4600; } ) );
  progress->setSliderDown( true );
  progress->setValue( 1600 );
  progress->setSliderDown( false );
  CHECK( waitFor( [&]() { return preview->position() < 1800; } ) );
  CHECK( isRed( snapshot( preview, QStringLiteral( "decoded-seek-back-%1.png" ).arg( scale ) ) ) );
  CHECK( preview->playbackState() == ShadowVideoPreview::Playing );
  std::printf( "PASS actual libVLC decode, pause clock, paused seek red->blue, resume and playing seek blue->red; duration=%lld\n",
    static_cast<long long>( preview->duration() ) );

  CHECK( preview->property( "shadowDialogChrome" ).toBool() );
  QWidget* surface = preview->findChild<QWidget*>( "shadowDialogSurface" );
  QWidget* shadow = preview->findChild<QWidget*>( "shadowDialogShadow" );
  QWidget* canvas = preview->findChild<QWidget*>( "shadowVideoCanvas" );
  QPushButton* close = preview->findChild<QPushButton*>( "shadowVideoClose" );
  CHECK( surface && shadow && shadow->isVisible() && canvas && close );
  CHECK( preview->testAttribute( Qt::WA_TranslucentBackground ) );
  CHECK( preview->screen()->availableGeometry().contains( preview->frameGeometry() ) );
  foreach( QWidget* control, QList<QWidget*>() << play << progress << time << close )
    CHECK( surface->rect().contains( QRect( control->mapTo( surface, QPoint() ), control->size() ) ) );
  CHECK( canvas->height() >= 80 && canvas->width() >= 120 );
  preview->raise();
  preview->activateWindow();
  QTest::qWait( 100 );
  const QImage rendered = canvas->grab().toImage();
  rendered.save( QStringLiteral( "native-video-surface-%1.png" ).arg( scale ) );
  CHECK( isRed( rendered ) );
  CHECK( preview->grab().save( QStringLiteral( "native-video-%1.png" ).arg( scale ) ) );
  std::printf( "PASS real decoded frame painted in native shadow dialog, controls visible inside screen at %s (DPR %.2f), window=%dx%d video=%dx%d\n",
    qPrintable( scale ), preview->devicePixelRatioF(), preview->width(), preview->height(), canvas->width(), canvas->height() );

  FrameDeliveryProbe delivery;
  canvas->installEventFilter( &delivery );
  for( unsigned blockedMilliseconds : { 400U, 800U } )
  {
    QCoreApplication::sendPostedEvents( canvas, QEvent::MetaCall );
    const int deliveredBeforeBlock = delivery.delivered;
    const qint64 positionBeforeBlock = preview->position();
    QThread::msleep( blockedMilliseconds );
    CHECK( preview->position() > positionBeforeBlock );
    QCoreApplication::sendPostedEvents( canvas, QEvent::MetaCall );
    const int deliveredAfterBlock = delivery.delivered - deliveredBeforeBlock;
    CHECK( deliveredAfterBlock == 1 );
    std::printf( "PASS GUI blocked %u ms while playback advances: exactly %d queued frame delivered\n",
      blockedMilliseconds, deliveredAfterBlock );
  }
  canvas->removeEventFilter( &delivery );

  progress->setValue( 9950 );
  CHECK( waitFor( [&]() { return preview->playbackState() == ShadowVideoPreview::Ended; }, 5000 ) );
  QTest::mouseClick( play, Qt::LeftButton );
  CHECK( waitFor( [&]() { return preview->playbackState() == ShadowVideoPreview::Playing && preview->position() > 100 && preview->position() < 1800; } ) );
  CHECK( isRed( snapshot( preview, QStringLiteral( "decoded-replay-%1.png" ).arg( scale ) ) ) );
  QPointer<QObject> released = player;
  QPointer<ShadowVideoPreview> closed = preview;
  QTest::keyClick( preview, Qt::Key_Escape );
  CHECK( released.isNull() );
  QApplication::sendPostedEvents( Q_NULLPTR, QEvent::DeferredDelete );
  CHECK( closed.isNull() );
  std::puts( "PASS end-of-media replay and Escape releases libVLC media/player/instance and dialog" );

  CHECK( routeVideo( main, QUrl::fromLocalFile( highResolutionFixture ) ) );
  preview = main.findChild<ShadowVideoPreview*>();
  CHECK( preview && preview->isVisible() );
  CHECK( waitFor( [&]() {
    QObject* highPlayer = preview->findChild<QObject*>( "shadowVideoPlayer" );
    QWidget* highCanvas = preview->findChild<QWidget*>( "shadowVideoCanvas" );
    return highPlayer && highCanvas && preview->duration() >= 5000 &&
      highCanvas->property( "videoFrameSize" ).toSize().width() > 0;
  } ) );
  QWidget* highCanvas = preview->findChild<QWidget*>( "shadowVideoCanvas" );
  const QSize outputSize = highCanvas->property( "videoFrameSize" ).toSize();
  const int outputWidth = outputSize.width();
  const int outputHeight = outputSize.height();
  CHECK( outputWidth <= 1920 && outputHeight <= 1080 );
  CHECK( qAbs( outputWidth * 9 - outputHeight * 16 ) <= 32 );
  CHECK( waitFor( [&]() { return preview->findChild<QPushButton*>( "shadowVideoPlay" )->isEnabled(); } ) );
  highCanvas->installEventFilter( &delivery );
  for( unsigned blockedMilliseconds : { 400U, 800U } )
  {
    QCoreApplication::sendPostedEvents( highCanvas, QEvent::MetaCall );
    const int beforeBlock = delivery.delivered;
    QThread::msleep( blockedMilliseconds );
    QCoreApplication::sendPostedEvents( highCanvas, QEvent::MetaCall );
    CHECK( delivery.delivered - beforeBlock == 1 );
  }
  highCanvas->removeEventFilter( &delivery );
  CHECK( isRed( highCanvas->grab().toImage() ) );
  std::printf( "PASS high-resolution 2560x1440 decoded to %dx%d: 400/800ms GUI stalls each retain one frame\n",
    outputWidth, outputHeight );
  QPointer<ShadowVideoPreview> highClosed = preview;
  QPointer<QObject> highReleased = preview->findChild<QObject*>( "shadowVideoPlayer" );
  QThread::msleep( 150 );
  preview->close();
  CHECK( highReleased.isNull() );
  QCoreApplication::sendPostedEvents( highCanvas, QEvent::MetaCall );
  CHECK( highCanvas->property( "videoFrameSize" ).toSize().isEmpty() );
  QApplication::sendPostedEvents( Q_NULLPTR, QEvent::DeferredDelete );
  CHECK( highClosed.isNull() );
  std::puts( "PASS closing with a pending high-resolution frame releases playback and rejects stale GUI delivery" );

  const QString missing = QDir::currentPath() + QStringLiteral( "/does-not-exist.mp4" );
  CHECK( routeVideo( main, QUrl::fromLocalFile( missing ) ) );
  preview = main.findChild<ShadowVideoPreview*>();
  CHECK( preview && preview->property( "videoError" ).toBool() );
  CHECK( preview->findChild<QLabel*>( "shadowVideoFeedback" )->text().contains( QStringLiteral( "不存在" ) ) );
  CHECK( !preview->findChild<QObject*>( "shadowVideoPlayer" ) && !preview->findChild<QPushButton*>( "shadowVideoPlay" )->isEnabled() );
  const QString corrupt = QDir::currentPath() + QStringLiteral( "/corrupt-%1.mp4" ).arg( scale );
  QFile invalid( corrupt );
  CHECK( invalid.open( QIODevice::WriteOnly ) );
  CHECK( invalid.write( QByteArray( 4096, 'x' ) ) == 4096 );
  invalid.close();
  CHECK( routeVideo( main, QUrl::fromLocalFile( corrupt ) ) );
  CHECK( main.findChildren<ShadowVideoPreview*>().size() == 1 );
  CHECK( waitFor( [&]() { return preview->property( "videoError" ).toBool(); } ) );
  CHECK( !preview->findChild<QLabel*>( "shadowVideoFeedback" )->text().isEmpty() );
  CHECK( !preview->findChild<QObject*>( "shadowVideoPlayer" ) );
  CHECK( preview->findChild<QPushButton*>( "shadowVideoRetry" )->isVisible() );
  CHECK( preview->screen()->availableGeometry().contains( preview->frameGeometry() ) );
  CHECK( preview->grab().save( QStringLiteral( "native-video-error-%1.png" ).arg( scale ) ) );
  CHECK( QFile::remove( corrupt ) && QFile::copy( fixture, corrupt ) );
  QTest::mouseClick( preview->findChild<QPushButton*>( "shadowVideoRetry" ), Qt::LeftButton );
  CHECK( waitFor( [&]() { return !preview->property( "videoError" ).toBool() && preview->position() > 100; } ) );
  CHECK( isRed( snapshot( preview, QStringLiteral( "decoded-retry-%1.png" ).arg( scale ) ) ) );
  CHECK( !preview->findChild<QPushButton*>( "shadowVideoRetry" )->isVisible() );
  released = preview->findChild<QObject*>( "shadowVideoPlayer" );
  preview->openFile( QUrl::fromLocalFile( missing ) );
  CHECK( released.isNull() );
  QTest::qWait( 150 );
  CHECK( !preview->findChild<QObject*>( "shadowVideoPlayer" ) );
  QTest::mouseClick( preview->findChild<QPushButton*>( "shadowVideoClose" ), Qt::LeftButton );
  QApplication::sendPostedEvents( Q_NULLPTR, QEvent::DeferredDelete );
  CHECK( main.findChildren<ShadowVideoPreview*>().isEmpty() );
  CHECK( external.count == 0 );
  QDesktopServices::unsetUrlHandler( QStringLiteral( "file" ) );
  std::puts( "PASS local-video playback, MIME sniff, missing/corrupt feedback, retry with repaired file, stale player rejection, close cleanup; zero external player calls" );
  return 0;
}

#include "video_preview_test.moc"
