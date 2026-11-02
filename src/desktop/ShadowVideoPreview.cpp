#include "ShadowVideoPreview.h"
#include "ShadowDialogChrome.h"
#include "ShadowDialogStyle.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QLibrary>
#include <QMimeDatabase>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdint>
#include <cstring>

struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;

class ShadowVideoCanvas : public QWidget
{
public:
  explicit ShadowVideoCanvas( QWidget* parent ) : QWidget( parent ) {}
  void setFrame( const QImage& image )
  {
    if( m_image.size() != image.size() )
      setProperty( "videoFrameSize", image.size() );
    m_image = image;
    update();
  }

protected:
  void paintEvent( QPaintEvent* ) override
  {
    QPainter painter( this );
    painter.fillRect( rect(), QColor( "#101723" ) );
    if( m_image.isNull() )
      return;
    const QSize fitted = m_image.size().scaled( size(), Qt::KeepAspectRatio );
    painter.setRenderHint( QPainter::SmoothPixmapTransform );
    painter.drawImage( QRect( QPoint( (width() - fitted.width()) / 2, (height() - fitted.height()) / 2 ), fitted ), m_image );
  }

private:
  QImage m_image;
};

namespace
{
class VlcApi
{
public:
  typedef libvlc_instance_t* (*New)( int, const char* const* );
  typedef void (*Release)( libvlc_instance_t* );
  typedef const char* (*Version)();
  typedef libvlc_media_t* (*MediaNewPath)( libvlc_instance_t*, const char* );
  typedef void (*MediaRelease)( libvlc_media_t* );
  typedef void (*MediaOption)( libvlc_media_t*, const char* );
  typedef libvlc_media_player_t* (*PlayerNew)( libvlc_instance_t* );
  typedef void (*PlayerRelease)( libvlc_media_player_t* );
  typedef void (*SetMedia)( libvlc_media_player_t*, libvlc_media_t* );
  typedef void (*SetHwnd)( libvlc_media_player_t*, void* );
  typedef void (*SetInput)( libvlc_media_player_t*, unsigned );
  typedef int (*Play)( libvlc_media_player_t* );
  typedef void (*Pause)( libvlc_media_player_t*, int );
  typedef void (*Stop)( libvlc_media_player_t* );
  typedef int (*GetState)( libvlc_media_player_t* );
  typedef int64_t (*GetTime)( libvlc_media_player_t* );
  typedef void (*SetTime)( libvlc_media_player_t*, int64_t );
  typedef int (*Seekable)( libvlc_media_player_t* );
  typedef unsigned (*VideoOutput)( libvlc_media_player_t* );
  typedef int (*Snapshot)( libvlc_media_player_t*, unsigned, const char*, unsigned, unsigned );
  typedef void* (*LockCallback)( void*, void** );
  typedef void (*UnlockCallback)( void*, void*, void* const* );
  typedef void (*DisplayCallback)( void*, void* );
  typedef unsigned (*FormatCallback)( void**, char*, unsigned*, unsigned*, unsigned*, unsigned* );
  typedef void (*CleanupCallback)( void* );
  typedef void (*VideoCallbacks)( libvlc_media_player_t*, LockCallback, UnlockCallback, DisplayCallback, void* );
  typedef void (*FormatCallbacks)( libvlc_media_player_t*, FormatCallback, CleanupCallback );
  New create = Q_NULLPTR;
  Release release = Q_NULLPTR;
  Version version = Q_NULLPTR;
  MediaNewPath mediaNewPath = Q_NULLPTR;
  MediaRelease mediaRelease = Q_NULLPTR;
  MediaOption mediaOption = Q_NULLPTR;
  PlayerNew playerNew = Q_NULLPTR;
  PlayerRelease playerRelease = Q_NULLPTR;
  SetMedia setMedia = Q_NULLPTR;
  SetHwnd setHwnd = Q_NULLPTR;
  SetInput setKeyInput = Q_NULLPTR;
  SetInput setMouseInput = Q_NULLPTR;
  Play play = Q_NULLPTR;
  Pause pause = Q_NULLPTR;
  Stop stop = Q_NULLPTR;
  GetState state = Q_NULLPTR;
  GetTime time = Q_NULLPTR;
  GetTime length = Q_NULLPTR;
  SetTime setTime = Q_NULLPTR;
  Seekable seekable = Q_NULLPTR;
  VideoOutput videoOutput = Q_NULLPTR;
  Snapshot snapshot = Q_NULLPTR;
  VideoCallbacks videoCallbacks = Q_NULLPTR;
  FormatCallbacks formatCallbacks = Q_NULLPTR;

  bool load()
  {
    if( m_ready )
      return true;
#ifndef Q_OS_WIN
    return false;
#else
    const QString runtime = QCoreApplication::applicationDirPath() + QStringLiteral( "/video-runtime" );
    if( !QFileInfo::exists( runtime + QStringLiteral( "/plugins" ) ) )
      return false;
    m_core.setFileName( runtime + QStringLiteral( "/libvlccore.dll" ) );
    m_library.setFileName( runtime + QStringLiteral( "/libvlc.dll" ) );
    m_core.setLoadHints( QLibrary::PreventUnloadHint );
    m_library.setLoadHints( QLibrary::PreventUnloadHint );
    if( !m_core.load() || !m_library.load() )
      return false;
    create = resolve<New>( "libvlc_new" );
    release = resolve<Release>( "libvlc_release" );
    version = resolve<Version>( "libvlc_get_version" );
    mediaNewPath = resolve<MediaNewPath>( "libvlc_media_new_path" );
    mediaRelease = resolve<MediaRelease>( "libvlc_media_release" );
    mediaOption = resolve<MediaOption>( "libvlc_media_add_option" );
    playerNew = resolve<PlayerNew>( "libvlc_media_player_new" );
    playerRelease = resolve<PlayerRelease>( "libvlc_media_player_release" );
    setMedia = resolve<SetMedia>( "libvlc_media_player_set_media" );
    setHwnd = resolve<SetHwnd>( "libvlc_media_player_set_hwnd" );
    setKeyInput = resolve<SetInput>( "libvlc_video_set_key_input" );
    setMouseInput = resolve<SetInput>( "libvlc_video_set_mouse_input" );
    play = resolve<Play>( "libvlc_media_player_play" );
    pause = resolve<Pause>( "libvlc_media_player_set_pause" );
    stop = resolve<Stop>( "libvlc_media_player_stop" );
    state = resolve<GetState>( "libvlc_media_player_get_state" );
    time = resolve<GetTime>( "libvlc_media_player_get_time" );
    length = resolve<GetTime>( "libvlc_media_player_get_length" );
    setTime = resolve<SetTime>( "libvlc_media_player_set_time" );
    seekable = resolve<Seekable>( "libvlc_media_player_is_seekable" );
    videoOutput = resolve<VideoOutput>( "libvlc_media_player_has_vout" );
    snapshot = resolve<Snapshot>( "libvlc_video_take_snapshot" );
    videoCallbacks = resolve<VideoCallbacks>( "libvlc_video_set_callbacks" );
    formatCallbacks = resolve<FormatCallbacks>( "libvlc_video_set_format_callbacks" );
    m_ready = create && release && version && mediaNewPath && mediaRelease && mediaOption && playerNew && playerRelease &&
      setMedia && setHwnd && setKeyInput && setMouseInput && play && pause && stop && state &&
      time && length && setTime && seekable && videoOutput && snapshot && videoCallbacks && formatCallbacks;
    if( m_ready && QString::fromUtf8( version() ).section( QLatin1Char( ' ' ), 0, 0 ) != QStringLiteral( "3.0.24" ) )
      m_ready = false;
    if( m_ready )
      qputenv( "VLC_PLUGIN_PATH", QDir::toNativeSeparators( runtime + QStringLiteral( "/plugins" ) ).toUtf8() );
    return m_ready;
#endif
  }

  static VlcApi& instance()
  {
    static VlcApi api;
    return api;
  }

private:
  template<typename Function> Function resolve( const char* name )
  {
    return reinterpret_cast<Function>( m_library.resolve( name ) );
  }
  QLibrary m_core;
  QLibrary m_library;
  bool m_ready = false;
};
}

class ShadowVlcPlayback : public QObject
{
public:
  explicit ShadowVlcPlayback( ShadowVideoCanvas* host, QObject* parent )
    : QObject( parent ), api( VlcApi::instance() ), canvas( host )
  {
    setObjectName( QStringLiteral( "shadowVideoPlayer" ) );
    if( !api.load() )
      return;
    const char* arguments[] = { "--ignore-config", "--no-video-title-show", "--no-media-library",
      "--no-osd", "--no-interact", "--no-plugins-cache", "--avcodec-hw=none", "--quiet" };
    instance = api.create( int( sizeof( arguments ) / sizeof( arguments[0] ) ), arguments );
    if( !instance )
      return;
    player = api.playerNew( instance );
    if( player )
    {
      api.videoCallbacks( player, lockFrame, unlockFrame, displayFrame, this );
      api.formatCallbacks( player, setupFormat, Q_NULLPTR );
      api.setKeyInput( player, 0 );
      api.setMouseInput( player, 0 );
    }
  }

  ~ShadowVlcPlayback() override
  {
    if( player )
    {
      api.stop( player );
      api.playerRelease( player );
    }
    if( instance )
      api.release( instance );
  }

  bool open( const QString& path )
  {
    if( !player )
      return false;
    libvlc_media_t* media = api.mediaNewPath( instance, QDir::toNativeSeparators( path ).toUtf8().constData() );
    if( !media )
      return false;
    api.mediaOption( media, ":avcodec-hw=none" );
    api.setMedia( player, media );
    api.mediaRelease( media );
    return api.play( player ) == 0;
  }

  int state() const { return player ? api.state( player ) : 7; }
  qint64 position() const { return player ? qMax<int64_t>( 0, api.time( player ) ) : 0; }
  qint64 duration() const
  {
    const qint64 value = player ? api.length( player ) : 0;
    if( value > 0 )
      lastDuration = value;
    return lastDuration;
  }
  bool isSeekable() const { return player && api.seekable( player ); }
  bool hasVideo() const { return player && api.videoOutput( player ) > 0; }
  void pause() { if( player ) api.pause( player, 1 ); }
  void play()
  {
    if( player )
    {
      if( state() == 6 )
        api.stop( player );
      api.play( player );
    }
  }
  void setPosition( qint64 value ) { if( player ) api.setTime( player, value ); }
  bool snapshot( const QString& path ) const
  {
    return player && hasVideo() && api.snapshot( player, 0, QDir::toNativeSeparators( path ).toUtf8().constData(), 0, 0 ) == 0;
  }
  static unsigned setupFormat( void** opaque, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines )
  {
    ShadowVlcPlayback* playback = static_cast<ShadowVlcPlayback*>( *opaque );
    if( !*width || !*height || *width > 16384 || *height > 16384 )
      return 0;
    QMutexLocker locker( &playback->frameMutex );
    const double scale = qMin( 1.0, qMin( 1920.0 / double( *width ), 1080.0 / double( *height ) ) );
    *width = qMax( 1U, unsigned( qRound( *width * scale ) ) );
    *height = qMax( 1U, unsigned( qRound( *height * scale ) ) );
    std::memcpy( chroma, "RV32", 4 );
    playback->frameWidth = int( *width );
    playback->frameHeight = int( *height );
    playback->framePitch = int( (*width * 4 + 31) & ~31U );
    pitches[0] = unsigned( playback->framePitch );
    lines[0] = *height;
    playback->pixels.resize( playback->framePitch * playback->frameHeight + 31 );
    playback->frameData = reinterpret_cast<uchar*>(
      (reinterpret_cast<quintptr>( playback->pixels.data() ) + 31) & ~quintptr( 31 ) );
    return 1;
  }
  static void* lockFrame( void* opaque, void** planes )
  {
    ShadowVlcPlayback* playback = static_cast<ShadowVlcPlayback*>( opaque );
    playback->frameMutex.lock();
    planes[0] = playback->frameData;
    return Q_NULLPTR;
  }
  static void unlockFrame( void* opaque, void*, void* const* )
  {
    static_cast<ShadowVlcPlayback*>( opaque )->frameMutex.unlock();
  }
  static void displayFrame( void* opaque, void* )
  {
    ShadowVlcPlayback* playback = static_cast<ShadowVlcPlayback*>( opaque );
    {
      QMutexLocker locker( &playback->frameMutex );
      if( playback->frameQueued || !playback->frameData || !playback->frameWidth || !playback->frameHeight )
        return;
      playback->pendingFrame = QImage( playback->frameData, playback->frameWidth,
        playback->frameHeight, playback->framePitch, QImage::Format_RGB32 ).copy();
      playback->frameQueued = true;
    }
    const QPointer<ShadowVideoCanvas> target = playback->canvas;
    const QPointer<ShadowVlcPlayback> active = playback;
    if( !target || !QMetaObject::invokeMethod( target, [target, active]() {
      if( !active )
        return;
      const QImage image = active->takePendingFrame();
      if( target && !image.isNull() )
        target->setFrame( image );
      if( active )
        active->releasePendingFrame();
    }, Qt::QueuedConnection ) )
      playback->releasePendingFrame();
  }

  QImage takePendingFrame()
  {
    QMutexLocker locker( &frameMutex );
    const QImage image = pendingFrame;
    pendingFrame = QImage();
    return image;
  }

  void releasePendingFrame()
  {
    QMutexLocker locker( &frameMutex );
    pendingFrame = QImage();
    frameQueued = false;
  }
  VlcApi& api;
  libvlc_instance_t* instance = Q_NULLPTR;
  libvlc_media_player_t* player = Q_NULLPTR;
  QPointer<ShadowVideoCanvas> canvas;
  QMutex frameMutex;
  QByteArray pixels;
  QImage pendingFrame;
  uchar* frameData = Q_NULLPTR;
  int frameWidth = 0;
  int frameHeight = 0;
  int framePitch = 0;
  bool frameQueued = false;
  mutable qint64 lastDuration = 0;
};

ShadowVideoPreview::ShadowVideoPreview( const QUrl& url, QWidget* parent ) : QDialog( parent )
{
  setObjectName( QStringLiteral( "ShadowVideoPreview" ) );
  setWindowTitle( QStringLiteral( "视频预览" ) );
  setAttribute( Qt::WA_DeleteOnClose );
  setMinimumSize( 320, 260 );
  resize( 760, 480 );
  ShadowDialogStyle::apply( this );
  QVBoxLayout* body = new QVBoxLayout( this );
  body->setContentsMargins( 16, 12, 16, 16 );
  body->setSpacing( 10 );
  m_fileName = new QLabel( this );
  m_fileName->setObjectName( QStringLiteral( "shadowVideoFileName" ) );
  m_fileName->setTextFormat( Qt::PlainText );
  m_fileName->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
  body->addWidget( m_fileName );
  m_canvas = new ShadowVideoCanvas( this );
  m_canvas->setObjectName( QStringLiteral( "shadowVideoCanvas" ) );
  m_canvas->setMinimumSize( 120, 80 );
  m_canvas->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
  m_canvas->setAccessibleName( QStringLiteral( "视频画面" ) );
  m_canvas->setStyleSheet( QStringLiteral( "background:#101723;" ) );
  body->addWidget( m_canvas, 1 );
  m_feedback = new QLabel( this );
  m_feedback->setObjectName( QStringLiteral( "shadowVideoFeedback" ) );
  m_feedback->setTextFormat( Qt::PlainText );
  m_feedback->setWordWrap( true );
  m_feedback->setMinimumHeight( 32 );
  m_feedback->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
  body->addWidget( m_feedback );
  m_progress = new QSlider( Qt::Horizontal, this );
  m_progress->setObjectName( QStringLiteral( "shadowVideoProgress" ) );
  m_progress->setRange( 0, 10000 );
  m_progress->setSingleStep( 100 );
  m_progress->setPageStep( 1000 );
  m_progress->setAccessibleName( QStringLiteral( "视频播放进度" ) );
  m_progress->setAccessibleDescription( QStringLiteral( "拖动或使用方向键调整播放位置" ) );
  body->addWidget( m_progress );
  QHBoxLayout* controls = new QHBoxLayout;
  controls->setSpacing( 10 );
  m_play = new QPushButton( QStringLiteral( "播放" ), this );
  m_play->setObjectName( QStringLiteral( "shadowVideoPlay" ) );
  m_play->setProperty( "role", "primary" );
  m_play->setMinimumHeight( 36 );
  controls->addWidget( m_play );
  m_time = new QLabel( QStringLiteral( "00:00 / 00:00" ), this );
  m_time->setObjectName( QStringLiteral( "shadowVideoTime" ) );
  m_time->setAccessibleName( QStringLiteral( "当前时间与视频总时长" ) );
  controls->addWidget( m_time );
  controls->addStretch();
  m_retry = new QPushButton( QStringLiteral( "重试" ), this );
  m_retry->setObjectName( QStringLiteral( "shadowVideoRetry" ) );
  m_retry->setMinimumHeight( 36 );
  controls->addWidget( m_retry );
  QPushButton* closeButton = new QPushButton( QStringLiteral( "关闭" ), this );
  closeButton->setObjectName( QStringLiteral( "shadowVideoClose" ) );
  closeButton->setMinimumHeight( 36 );
  controls->addWidget( closeButton );
  body->addLayout( controls );
  m_loadTimeout = new QTimer( this );
  m_loadTimeout->setSingleShot( true );
  m_loadTimeout->setInterval( 15000 );
  connect( m_loadTimeout, &QTimer::timeout, this, [this]() {
    fail( QStringLiteral( "视频加载超时，未能解码画面。请重试或确认视频文件完整。" ) );
  } );
  m_refresh = new QTimer( this );
  m_refresh->setInterval( 100 );
  connect( m_refresh, &QTimer::timeout, this, [this]() {
    if( !m_player || m_failed )
      return;
    if( m_player->state() == 7 || (m_player->state() == 6 && !m_player->duration()) )
      fail( QStringLiteral( "无法解码或播放此视频，文件可能已损坏或格式不受支持。" ) );
    else
    {
      if( m_player->hasVideo() )
        m_loadTimeout->stop();
      updateControls();
      updatePosition( m_player->position() );
    }
  } );
  connect( closeButton, &QPushButton::clicked, this, &QDialog::reject );
  connect( m_retry, &QPushButton::clicked, this, [this]() { openFile( m_url ); } );
  connect( m_play, &QPushButton::clicked, this, [this]() {
    if( !m_player || m_failed )
      return;
    if( playbackState() == Playing )
      m_player->pause();
    else
    {
      if( playbackState() == Ended )
        m_player->setPosition( 0 );
      m_player->play();
    }
  } );
  connect( m_progress, &QSlider::sliderPressed, this, [this]() {
    m_resumeAfterSeek = playbackState() == Playing;
    if( m_resumeAfterSeek )
      m_player->pause();
  } );
  connect( m_progress, &QSlider::valueChanged, this, [this]( int value ) {
    m_time->setText( timeText( duration() * value / 10000 ) + QStringLiteral( " / " ) + timeText( duration() ) );
    if( !m_progress->isSliderDown() )
      seekToSlider();
  } );
  connect( m_progress, &QSlider::sliderReleased, this, [this]() {
    seekToSlider();
    if( m_player && m_resumeAfterSeek )
      m_player->play();
    m_resumeAfterSeek = false;
  } );
  ShadowDialogChrome::install();
  ensurePolished();
  openFile( url );
}

ShadowVideoPreview::~ShadowVideoPreview() { releasePlayer(); }

ShadowVideoPreview::PlaybackState ShadowVideoPreview::playbackState() const
{
  if( m_failed ) return Failed;
  if( !m_player ) return Stopped;
  switch( m_player->state() )
  {
    case 3: return Playing;
    case 4: return Paused;
    case 5: return Stopped;
    case 6: return Ended;
    case 7: return Failed;
    default: return Loading;
  }
}

qint64 ShadowVideoPreview::duration() const { return m_player ? m_player->duration() : 0; }
qint64 ShadowVideoPreview::position() const { return m_player ? m_player->position() : 0; }
bool ShadowVideoPreview::isSeekable() const { return m_player && m_player->isSeekable(); }
bool ShadowVideoPreview::captureFrame( const QString& path ) const { return m_player && m_player->snapshot( path ); }

bool ShadowVideoPreview::canPreview( const QUrl& url )
{
  if( !url.isValid() || !url.isLocalFile() )
    return false;
  const QFileInfo file( url.toLocalFile() );
  static const QStringList extensions = QStringList() << "mp4" << "m4v" << "webm" << "mkv" <<
    "avi" << "mov" << "wmv" << "mpg" << "mpeg" << "m2v" << "m2ts" << "mts" <<
    "ts" << "3gp" << "3g2" << "ogv" << "flv";
  if( extensions.contains( file.suffix().toLower() ) )
    return true;
  return file.isFile() && QMimeDatabase().mimeTypeForFile( file, QMimeDatabase::MatchContent ).name().startsWith( QStringLiteral( "video/" ) );
}

void ShadowVideoPreview::createPlayer() { m_player = new ShadowVlcPlayback( static_cast<ShadowVideoCanvas*>( m_canvas ), this ); }

void ShadowVideoPreview::openFile( const QUrl& url )
{
  releasePlayer();
  m_url = url;
  m_failed = false;
  m_resumeAfterSeek = false;
  setProperty( "videoError", false );
  m_fileName->setText( QFileInfo( url.toLocalFile() ).fileName() );
  m_fileName->setToolTip( m_fileName->text() );
  m_retry->hide();
  m_progress->setValue( 0 );
  m_progress->setEnabled( false );
  m_play->setEnabled( false );
  m_time->setText( QStringLiteral( "00:00 / 00:00" ) );
  m_feedback->setText( QStringLiteral( "正在加载视频…" ) );
  if( !canPreview( url ) )
  {
    fail( QStringLiteral( "此链接不是可预览的本地视频文件。" ) );
    return;
  }
  const QFileInfo file( url.toLocalFile() );
  if( !file.exists() || !file.isFile() )
  {
    fail( QStringLiteral( "视频文件不存在或已被移动，请重新下载或确认文件位置。" ) );
    return;
  }
  QFile readable( file.absoluteFilePath() );
  if( !readable.open( QIODevice::ReadOnly ) )
  {
    fail( QStringLiteral( "无法读取视频文件，请检查文件访问权限。" ) );
    return;
  }
  if( !readable.size() )
  {
    fail( QStringLiteral( "视频文件为空，无法播放。" ) );
    return;
  }
  readable.close();
  createPlayer();
  if( !m_player->player )
  {
    fail( QStringLiteral( "视频播放组件不可用，请使用包含视频运行时的完整安装包。" ) );
    return;
  }
  if( !m_player->open( file.absoluteFilePath() ) )
  {
    fail( QStringLiteral( "无法打开此视频文件，请确认文件完整后重试。" ) );
    return;
  }
  m_loadTimeout->start();
  m_refresh->start();
}

void ShadowVideoPreview::releasePlayer()
{
  m_loadTimeout->stop();
  m_refresh->stop();
  delete m_player;
  m_player = Q_NULLPTR;
  static_cast<ShadowVideoCanvas*>( m_canvas )->setFrame( QImage() );
}

void ShadowVideoPreview::fail( const QString& message )
{
  m_failed = true;
  setProperty( "videoError", true );
  releasePlayer();
  m_feedback->setText( message );
  m_play->setEnabled( false );
  m_progress->setEnabled( false );
  m_retry->show();
}

QString ShadowVideoPreview::timeText( qint64 milliseconds ) const
{
  const qint64 seconds = qMax<qint64>( 0, milliseconds / 1000 );
  const QString minutes = QStringLiteral( "%1:%2" ).arg( (seconds / 60) % 60, 2, 10, QLatin1Char( '0' ) )
    .arg( seconds % 60, 2, 10, QLatin1Char( '0' ) );
  return seconds < 3600 ? minutes : QString::number( seconds / 3600 ) + QLatin1Char( ':' ) + minutes;
}

void ShadowVideoPreview::updateControls()
{
  if( !m_player || m_failed )
    return;
  m_play->setText( playbackState() == Playing ? QStringLiteral( "暂停" ) : QStringLiteral( "播放" ) );
  m_play->setEnabled( duration() > 0 );
  m_progress->setEnabled( duration() > 0 && isSeekable() );
  if( playbackState() == Ended || m_player->hasVideo() )
    m_feedback->setText( playbackState() == Ended ? QStringLiteral( "播放完毕" ) :
      playbackState() == Playing ? QStringLiteral( "正在播放" ) : QStringLiteral( "已暂停" ) );
}

void ShadowVideoPreview::updatePosition( qint64 value )
{
  if( m_progress->isSliderDown() )
    return;
  const QSignalBlocker blocked( m_progress );
  m_progress->setValue( duration() > 0 ? int( qBound<qint64>( 0, value * 10000 / duration(), 10000 ) ) : 0 );
  m_time->setText( timeText( value ) + QStringLiteral( " / " ) + timeText( duration() ) );
}

void ShadowVideoPreview::seekToSlider()
{
  if( m_player && !m_failed && isSeekable() )
    m_player->setPosition( duration() * m_progress->value() / 10000 );
}

void ShadowVideoPreview::fitToScreen()
{
  const QPoint anchor = parentWidget() ? parentWidget()->frameGeometry().center() : frameGeometry().center();
  const QRect available = qApp->desktop()->availableGeometry( anchor ).adjusted( 8, 8, -8, -8 );
  const QSize fitted = size().boundedTo( available.size() );
  const QPoint topLeft = anchor - QPoint( fitted.width() / 2, fitted.height() / 2 );
  setGeometry( qBound( available.left(), topLeft.x(), available.right() - fitted.width() + 1 ),
    qBound( available.top(), topLeft.y(), available.bottom() - fitted.height() + 1 ), fitted.width(), fitted.height() );
}

void ShadowVideoPreview::showEvent( QShowEvent* event )
{
  QDialog::showEvent( event );
  fitToScreen();
  QTimer::singleShot( 0, this, [this]() { fitToScreen(); } );
}

void ShadowVideoPreview::done( int result ) { releasePlayer(); QDialog::done( result ); }
void ShadowVideoPreview::closeEvent( QCloseEvent* event ) { releasePlayer(); QDialog::closeEvent( event ); }
