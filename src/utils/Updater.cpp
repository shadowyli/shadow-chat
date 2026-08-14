//////////////////////////////////////////////////////////////////////
//
// BeeBEEP Copyright (C) 2010-2021 Marco Mastroddi
//
// BeeBEEP is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// BeeBEEP is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with BeeBEEP. If not, see <http://www.gnu.org/licenses/>.
//
// Author: Marco Mastroddi <marco.mastroddi(AT)gmail.com>
//
// $Id: Updater.cpp 1455 2020-12-23 10:17:53Z mastroddi $
//
//////////////////////////////////////////////////////////////////////

#include "HttpDownloader.h"
#include "Settings.h"
#include "Updater.h"


Updater::Updater( QObject *parent )
 : QObject( parent ), m_versionAvailable( "" ), m_downloadUrl( "" ), m_news( "" )
{
  setObjectName( "Updater" );
}

void Updater::checkForNewVersion()
{
  m_versionAvailable = "";
  m_downloadUrl = "";
  m_news = "";
  QUrl url( Settings::instance().lastVersionUrl() );
#ifdef SHADOWCHAT_SAFE_BUILD
  if( url.scheme() != QLatin1String( "https" ) || url.host() != QLatin1String( "shadowyli.github.io" ) ||
      !url.path().startsWith( QLatin1String( "/shadow-chat/" ) ) )
  {
    qWarning() << "ShadowChat blocked an invalid update manifest URL";
    emit jobCompleted();
    return;
  }
#endif

  HttpDownloader* http_downloader = new HttpDownloader( this );
  connect( http_downloader, SIGNAL( downloadCompleted( const QString& ) ),this, SLOT( onDownloadCompleted( const QString& ) ), Qt::QueuedConnection );
  connect( http_downloader, SIGNAL( jobFinished() ), this, SIGNAL( jobCompleted() ) );

  http_downloader->setOverwriteExistingFiles( true );
  http_downloader->addUrl( url );

  QMetaObject::invokeMethod( http_downloader, "startDownload", Qt::QueuedConnection );
}

void Updater::onDownloadCompleted( const QString& file_path )
{
  HttpDownloader* http_downloader = qobject_cast<HttpDownloader*>( sender() );
  if( !http_downloader )
  {
    qWarning() << "Updater received a signal from invalid HttpDownloader instance";
    return;
  }

#ifdef BEEBEEP_DEBUG
  qDebug() << qPrintable( file_path ) << "download completed";
#endif

  QSettings sets( file_path, QSettings::IniFormat );
  if( !sets.allKeys().isEmpty() )
  {
    sets.beginGroup( Settings::instance().operatingSystem( false ) );
    m_versionAvailable = sets.value( "CurrentVersion", "" ).toString();
    m_downloadUrl = sets.value( "DownloadUrl", "" ).toString();
    sets.endGroup();
    sets.beginGroup( "Info" );
    m_news = sets.value( "News", "" ).toString();
    sets.endGroup();
  }

  if( m_versionAvailable.isEmpty() )
    qWarning() << file_path << "is not valid to check new version";

  if( !m_downloadUrl.isEmpty() )
  {
    QUrl url = QUrl::fromUserInput( m_downloadUrl );
#ifdef SHADOWCHAT_SAFE_BUILD
    const bool product_page = url.scheme() == QLatin1String( "https" ) &&
                              url.host() == QLatin1String( "shadowyli.github.io" ) &&
                              url.path().startsWith( QLatin1String( "/shadow-chat/" ) );
    const bool github_release = url.scheme() == QLatin1String( "https" ) &&
                                url.host() == QLatin1String( "github.com" ) &&
                                url.path().startsWith( QLatin1String( "/shadowyli/shadow-chat/releases/" ) );
    if( !product_page && !github_release )
    {
      qWarning() << "ShadowChat blocked an untrusted update download URL";
      m_downloadUrl.clear();
    }
    else
#endif
      m_downloadUrl = url.toString();
  }

#ifdef SHADOWCHAT_SAFE_BUILD
  // The static manifest is intentionally data-only. Do not render remotely
  // supplied HTML/news in chats in the hardened build.
  m_news.clear();
#endif

  QFile file_downloaded( file_path );
  if( !file_downloaded.remove() )
  {
    qWarning() << "Unable to remove file" << file_path << "now so it is added to temporaty files";
    Settings::instance().addTemporaryFilePath( file_path );
  }

  http_downloader->cleanUp();
}
