#include <QCoreApplication>
#include <QApplication>
#include <QTemporaryDir>
#include <QDebug>
#include "FileTransferPeer.h"
#include "Protocol.h"
#include "ChatMessageData.h"
#include "GuiFileTransfer.h"
#include "Settings.h"
#include "User.h"
#include <QMenu>
#include <QTimer>
#include <cstdio>

class TransferProbe : public FileTransferPeer
{
public:
  using FileTransferPeer::sendFileHeader;
  using FileTransferPeer::setTransferCompleted;
  using FileTransferPeer::checkDownloadData;
  using FileTransferPeer::temporaryFilePath;
  void prepareDownload( const QString& temporary, const QString& target, qint64 size )
  {
    setTransferType( FileInfo::Download );
    m_fileInfo.setPath( target );
    m_fileInfo.setName( "asset.psd" );
    m_fileInfo.setId( 1 );
    m_fileInfo.setSize( size );
    m_file.setFileName( temporary );
    m_state = Transferring;
  }
  TransferState state() const { return m_state; }
};

class PlainFrameSocket : public ConnectionSocket
{
public:
  explicit PlainFrameSocket( QObject* parent = Q_NULLPTR ) : ConnectionSocket( parent )
  { setUserId( 99 ); useEncryption( false ); }
  using ConnectionSocket::serializeData;
};
class FrameServer : public QTcpServer
{
public:
  QStringList frames;
protected:
  void incomingConnection( qintptr descriptor ) override
  {
    PlainFrameSocket* socket = new PlainFrameSocket( this );
    socket->setSocketDescriptor( descriptor );
    QObject::connect( socket, &ConnectionSocket::dataReceived, this, [this]( const QByteArray& data ) { frames << QString::fromUtf8( data ); } );
  }
};

static void write( const QString& path, const QByteArray& data )
{
  QFile file( path );
  if( !file.open( QIODevice::WriteOnly ) || file.write( data ) != data.size() )
    qFatal( "Unable to prepare fixture" );
}

int main( int argc, char** argv )
{
  QApplication app( argc, argv );
  QTemporaryDir directory;
  if( !directory.isValid() ) return 1;
  int failures = 0;
  auto check = [&]( bool ok, const char* description ) {
    qInfo() << (ok ? "PASS" : "FAIL") << description;
    std::printf( "%s %s\n", ok ? "PASS" : "FAIL", description );
    if( !ok ) ++failures;
  };
  ChatMessageData groupData;
  groupData.setGroupId( "project" ); groupData.setGroupName( "Art" );
  groupData.setGroupOwnerHash( "owner-hash" );
  Message groupMessage( Message::Group, 123, "" );
  groupMessage.setData( Protocol::instance().chatMessageDataToString( groupData ) + QChar( QChar::LineSeparator ) + "future-field" );
  const ChatMessageData decoded = Protocol::instance().dataFromChatMessage( groupMessage );
  check( decoded.groupOwnerHash() == "owner-hash" && decoded.groupId() == "project", "extended group metadata retains owner and ignores future fields" );
  FrameServer server;
  check( server.listen( QHostAddress::LocalHost, 0 ), "coalesced frame fixture listens" );
  QTcpSocket writer; writer.connectToHost( QHostAddress::LocalHost, server.serverPort() );
  check( writer.waitForConnected( 1000 ), "coalesced frame fixture connects" );
  PlainFrameSocket serializer;
  writer.write( serializer.serializeData( "first" ) + serializer.serializeData( "second" ) + serializer.serializeData( "third" ) ); writer.flush();
  QElapsedTimer frameTime; frameTime.start();
  while( server.frames.size() < 3 && frameTime.elapsed() < 1000 ) QCoreApplication::processEvents( QEventLoop::AllEvents, 10 );
  check( server.frames == (QStringList() << "first" << "second" << "third"), "coalesced frames drain immediately without heartbeat" );
  FileInfo sameMetadata( 9, FileInfo::Download ); sameMetadata.setName( "same.png" ); sameMetadata.setFileHash( "equal-source-metadata" );
  TransferProbe receiveA, receiveB;
  receiveA.setId( 100 ); receiveB.setId( 101 );
  receiveA.setTransferType( FileInfo::Download ); receiveB.setTransferType( FileInfo::Download );
  receiveA.setFileInfo( FileInfo::Download, sameMetadata ); receiveB.setFileInfo( FileInfo::Download, sameMetadata );
  check( receiveA.temporaryFilePath() != receiveB.temporaryFilePath(), "concurrent equal-metadata receives own separate scratch files" );

  // A replaced file must fail before a header is sent, even if still readable.
  const QString source = directory.filePath( "source.psd" );
  write( source, "original" );
  FileInfo offered( 1, FileInfo::Upload );
  offered.setName( "source.psd" );
  offered.setPath( source );
  offered.setSize( 8 );
  offered.setLastModified( QFileInfo( source ).lastModified() );
  write( source, "replacement" );
  TransferProbe changed;
  changed.setFileInfo( FileInfo::Upload, offered );
  changed.sendFileHeader();
  check( changed.state() == FileTransferPeer::Error, "changed source rejects transfer" );
  QFile::remove( source );
  TransferProbe missing;
  missing.setFileInfo( FileInfo::Upload, offered );
  missing.sendFileHeader();
  check( missing.state() == FileTransferPeer::Error, "missing source rejects transfer" );

  // A valid cache file cannot be promoted beneath a path occupied by a file.
  const QString blocker = directory.filePath( "blocker" );
  write( blocker, "keep" );
  const QString partial = directory.filePath( "transfer.part" );
  write( partial, "asset" );
  TransferProbe renameFailure;
  renameFailure.prepareDownload( partial, blocker + "/asset.psd", 5 );
  renameFailure.setTransferCompleted();
  check( renameFailure.state() == FileTransferPeer::Error, "save failure cannot report completed" );

  TransferProbe writeFailure;
  writeFailure.prepareDownload( blocker + "/unwritable.part", directory.filePath( "out.psd" ), 5 );
  writeFailure.checkDownloadData( "asset" );
  check( writeFailure.state() == FileTransferPeer::Error, "disk write failure cannot report completed" );
  check( !QFile::exists( directory.filePath( "out.psd" ) ), "failed receive leaves no success file" );
  const QString existing = directory.filePath( "asset.psd" );
  const QString collisionPartial = directory.filePath( "collision.part" );
  write( existing, "keep" ); write( collisionPartial, "asset" );
  TransferProbe collision;
  collision.prepareDownload( collisionPartial, existing, 5 );
  collision.setTransferCompleted();
  QFile oldFile( existing ); oldFile.open( QIODevice::ReadOnly );
  check( oldFile.readAll() == "keep", "name collision preserves old file" );
  QFile newFile( directory.filePath( "asset (1).psd" ) ); newFile.open( QIODevice::ReadOnly );
  check( newFile.readAll() == "asset", "collision saves numbered version even if final ack fails" );
  // A failed receive is manually restarted with a fresh peer. Stale scratch
  // bytes must be truncated, and the user's existing material must survive.
  Settings::instance().setResumeFileTransfer( false );
  const QString retryPartial = directory.filePath( "retry.part" );
  write( retryPartial, "old-incomplete-content-that-must-not-survive" );
  TransferProbe retry;
  retry.prepareDownload( retryPartial, existing, 5 );
  retry.checkDownloadData( "asset" );
  QFile retryFile( directory.filePath( "asset (2).psd" ) ); retryFile.open( QIODevice::ReadOnly );
  check( retryFile.readAll() == "asset", "manual retry truncates stale scratch and saves a new numbered file" );
  oldFile.seek( 0 );
  check( oldFile.readAll() == "keep", "manual retry cannot overwrite existing material" );
  GuiFileTransfer transferList;
  User peer; peer.setId( 99 ); peer.setName( "Teammate" );
  FileInfo failedInfo( 99, FileInfo::Download ); failedInfo.setName( "asset.psd" ); failedInfo.setPath( existing );
  transferList.setMessage( 77, peer, failedInfo, "disk full", FileTransferPeer::Error );
  transferList.setCurrentItem( transferList.topLevelItem( 0 ) );
  transferList.topLevelItem( 0 )->setSelected( true );
  bool retryVisible = false, retryRequested = false;
  QObject::connect( &transferList, static_cast<void (GuiFileTransfer::*)(VNumber, const FileInfo&)>(&GuiFileTransfer::resumeTransfer),
    [&]( VNumber id, const FileInfo& info ) { retryRequested = id == 99 && info.name() == "asset.psd"; } );
  QTimer::singleShot( 50, [&]() {
    QMenu* menu = qobject_cast<QMenu*>( QApplication::activePopupWidget() );
    if( !menu ) return;
    foreach( QAction* action, menu->actions() )
      if( action->text() == QStringLiteral( "从头重试" ) ) { retryVisible = true; action->trigger(); break; }
    menu->close();
  } );
  QTimer::singleShot( 1500, [&]() { if( QApplication::activePopupWidget() ) QApplication::activePopupWidget()->close(); } );
  QMetaObject::invokeMethod( &transferList, "openMenu", Qt::DirectConnection, Q_ARG( QPoint, QPoint( 1, 1 ) ) );
  check( retryVisible && retryRequested, "failed transfer exposes working from-start retry with resume disabled" );
  return failures ? 1 : 0;
}
