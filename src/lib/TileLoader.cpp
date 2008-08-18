/**
 * This file is part of the Marble Desktop Globe.
 *
 * Copyright 2005-2007 Torsten Rahn <tackat@kde.org>"
 * Copyright 2007      Inge Wallin  <ingwa@kde.org>"
 * Copyright 2008      Jens-Michael Hoffmann <jensmh@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#include "TileLoader.h"

#include "global.h"
#include "GeoSceneTexture.h"
#include "HttpDownloadManager.h"
#include "TextureTile.h"
#include "MarbleDirs.h"
#include "TileId.h"
#include "MarbleModel.h"
#include "TileLoaderHelper.h"

#include <QtCore/QCache>
#include <QtCore/QDebug>
#include <QtCore/QHash>

using namespace Marble;

#ifdef Q_CC_MSVC
# ifndef KDEWIN_MATH_H
   long qreal log(int i) { return log((long qreal)i); }
# endif
#endif

class TileLoader::Private
{
    public:
        Private()
            : m_downloadManager( 0 ),
              m_textureLayer( 0 )
        {
            m_tileCache.setMaxCost( 20000 * 1024 ); // Cache size measured in bytes
        }

        ~Private()
        {
        }

        HttpDownloadManager *m_downloadManager;
        GeoSceneTexture     *m_textureLayer;
        QHash <TileId, TextureTile*>  m_tileHash;
        int           m_tileWidth;
        int           m_tileHeight;
        QCache <TileId, TextureTile>  m_tileCache;
};



TileLoader::TileLoader( HttpDownloadManager *downloadManager, MarbleModel* parent)
    : d( new Private() ),
      m_parent(parent)
{
    setDownloadManager( downloadManager );
}

TileLoader::~TileLoader()
{
    flush();
    d->m_tileCache.clear();
    if ( d->m_downloadManager != 0 )
        d->m_downloadManager->disconnect( this );

    delete d;
}

void TileLoader::setDownloadManager( HttpDownloadManager *downloadManager )
{
    if ( d->m_downloadManager != 0 ) {
        d->m_downloadManager->disconnect( this );
        d->m_downloadManager = 0;
    }

    d->m_downloadManager = downloadManager;
    if ( d->m_downloadManager != 0 ) {
        connect( d->m_downloadManager, SIGNAL( downloadComplete( QString, QString ) ),
                 this,              SLOT( reloadTile( QString, QString ) ) );
    }
}

void TileLoader::setTextureLayer( GeoSceneTexture *textureLayer )
{
    // Initialize map theme.
    flush();
    d->m_tileCache.clear();

    d->m_textureLayer = textureLayer;

    TileId id;
    TextureTile tile( id );
    tile.loadRawTile( d->m_textureLayer, 0, 0, 0 );

    // We assume that all tiles have the same size. TODO: check to be safe
    d->m_tileWidth  = tile.rawtile().width();
    d->m_tileHeight = tile.rawtile().height();
}

void TileLoader::resetTilehash()
{
    QHash<TileId, TextureTile*>::const_iterator it = d->m_tileHash.constBegin();
    while ( it != d->m_tileHash.constEnd() ) {
        it.value()->setUsed( false );
        ++it;
    }
}

void TileLoader::cleanupTilehash()
{
    // Make sure that tiles which haven't been used during the last
    // rendering of the map at all get removed from the tile hash.

    QHashIterator<TileId, TextureTile*> it( d->m_tileHash );
    while ( it.hasNext() ) {
        it.next();
        if ( it.value()->used() == false ) {

            bool inCache = d->m_tileCache.insert( it.key(), it.value(), it.value()->numBytes() );
            d->m_tileHash.remove( it.key() );
            if ( inCache == false )
                delete it.value();
        }
    }
}

void TileLoader::flush()
{
    // Remove all tiles from m_tileHash
    QHashIterator<TileId, TextureTile*> it( d->m_tileHash );
    while ( it.hasNext() ) {
        it.next();

        bool inCache = d->m_tileCache.insert( it.key(), it.value(), it.value()->numBytes() );
        d->m_tileHash.remove( it.key() );
        if ( inCache == false )
            delete it.value();
    }

    d->m_tileHash.clear();
}

int TileLoader::tileWidth() const
{
    return d->m_tileWidth;
}

int TileLoader::tileHeight() const
{
    return d->m_tileHeight;
}

int TileLoader::globalWidth( int level ) const
{
    return d->m_tileWidth * TileLoaderHelper::levelToColumn( d->m_textureLayer->levelZeroColumns(),
                                                             level );
}

int TileLoader::globalHeight( int level ) const
{
    return d->m_tileHeight * TileLoaderHelper::levelToRow( d->m_textureLayer->levelZeroRows(),
                                                           level );
}

TextureTile* TileLoader::loadTile( int tilx, int tily, int tileLevel )
{
    TileId tileId( tileLevel, tilx, tily );

    // check if the tile is in the hash
    TextureTile * tile = d->m_tileHash.value( tileId, 0 );
    if ( tile ) {
        tile->setUsed( true );
        return tile;
    }
    // here ends the performance critical section of this method

    // the tile was not in the hash or has been removed because of expiration
    // so check if it is in the cache
    tile = d->m_tileCache.take( tileId );
    if ( tile ) {
        // the tile was in the cache, but is it up to date?
        const QDateTime now = QDateTime::currentDateTime();
        if ( tile->created().secsTo( now ) < d->m_textureLayer->expire()) {
            d->m_tileHash[tileId] = tile;
            tile->setUsed( true );
            return tile;
        } else {
            delete tile;
            tile = 0;
        }
    }

    // tile (valid) has not been found in hash or cache, so load it from disk
    // and place it in the hash from where it will get transfered to the cache

    // qDebug() << "load Tile from Disk: " << tileId.toString();
    tile = new TextureTile( tileId );
    d->m_tileHash[tileId] = tile;

    if ( d->m_downloadManager != 0 ) {
        connect( tile, SIGNAL( downloadTile( QUrl, QString, QString ) ),
                 d->m_downloadManager, SLOT( addJob( QUrl, QString, QString ) ) );
    }
    connect( tile, SIGNAL( tileUpdateDone() ),
             this, SIGNAL( tileUpdateAvailable() ) );

    tile->loadRawTile( d->m_textureLayer, tileLevel, tilx, tily, &( d->m_tileCache ) );
    tile->loadTile( false );

    // TODO should emit signal rather than directly calling paintTile
    // emit paintTile( tile, tilx, tily, tileLevel, d->m_theme, false );
    m_parent->paintTile( tile, tilx, tily, tileLevel, d->m_textureLayer, false );

    return tile;
}

GeoSceneTexture* TileLoader::textureLayer() const
{
    return d->m_textureLayer;
}

quint64 TileLoader::volatileCacheLimit() const
{
    return d->m_tileCache.maxCost() / 1024;
}

int TileLoader::maxCompleteTileLevel( GeoSceneTexture *textureLayer )
{
    bool noerr = true;

    int tilelevel = -1;
    int trylevel  = 0;

    const int levelZeroColumns = textureLayer->levelZeroColumns();
    const int levelZeroRows = textureLayer->levelZeroRows();

    // if ( m_bitmaplayer.type.toLower() == "bitmap" ){
    while ( noerr == true ) {
        const int maxRow = TileLoaderHelper::levelToRow( levelZeroRows, trylevel );

        for ( int row = 0; noerr && row < maxRow; ++row ) {
            const int maxColumn = TileLoaderHelper::levelToColumn( levelZeroColumns, trylevel );

            for ( int column = 0; noerr && column < maxColumn; ++column ) {
                QString tilepath = MarbleDirs::path(
                    TileLoaderHelper::relativeTileFileName( textureLayer, trylevel, column, row ));
                // qDebug() << tilepath;
                noerr = QFile::exists( tilepath );
            }
        }

        if ( noerr == true )
            tilelevel = trylevel;

        ++trylevel;
    }

    if ( tilelevel == -1 ) {
        qDebug("No Tiles Found!");
    }

    //qDebug() << "Detected maximum complete tile level: " << tilelevel;

    return tilelevel;
}


int TileLoader::maxPartialTileLevel( GeoSceneTexture *textureLayer )
{
    QString tilepath = MarbleDirs::path( TileLoaderHelper::themeStr( textureLayer ) );
//    qDebug() << "TileLoader::maxPartialTileLevel tilepath" << tilepath;
    QStringList leveldirs = ( QDir( tilepath ) ).entryList( QDir::AllDirs | QDir::NoSymLinks | QDir::NoDotAndDotDot );

    int maxtilelevel = -1;
    QString str;
    bool ok = true;

    QStringList::const_iterator constIterator;
    for ( constIterator = leveldirs.constBegin();
          constIterator != leveldirs.constEnd();
         ++constIterator)
    {
        int value = (*constIterator).toInt( &ok, 10 );
        // qDebug() << "Value: " << value  << "Ok: " << ok;
        if ( ok && value > maxtilelevel )
            maxtilelevel = value;
    }

//    qDebug() << "Detected maximum tile level that contains data: "
//             << maxtilelevel;

    return maxtilelevel;
}


bool TileLoader::baseTilesAvailable( GeoSceneTexture *textureLayer )
{
    bool noerr = true; 
    const int  levelZeroColumns = textureLayer->levelZeroColumns();
    const int  levelZeroRows    = textureLayer->levelZeroRows();

    // Check whether the tiles from the lowest texture level are available
    //
    // FIXME: marble could theoretically start without local tiles, too.
    //        They can be downloaded.
    for ( int column = 0; noerr && column < levelZeroColumns; ++column ) {
        for ( int row = 0; noerr && row < levelZeroRows; ++row ) {

            const QString tilepath = MarbleDirs::path( TileLoaderHelper::relativeTileFileName(
                textureLayer, 0, column, row ));

            noerr = QFile::exists( tilepath );
        }
    }

    //qDebug() << "Mandatory most basic tile level is fully available: " 
    //	     << noerr;

    return noerr;
}

void TileLoader::setVolatileCacheLimit( quint64 kiloBytes )
{
    qDebug() << QString("Setting tile cache to %1 kilobytes.").arg( kiloBytes );
    d->m_tileCache.setMaxCost( kiloBytes * 1024 );
}

void TileLoader::reloadTile( const QString &idStr )
{
//    qDebug() << "TileLoader::reloadTile:" << idStr;
 
    const TileId id = TileId::fromString( idStr );
    if ( d->m_tileHash.contains( id ) ) {
        int  level = id.zoomLevel();
        int  y     = id.y();
        int  x     = id.x();

        // TODO should emit signal rather than directly calling paintTile
//         emit paintTile( d->m_tileHash[id], x, y, level, d->m_theme, true );
        (d->m_tileHash[id])->loadRawTile( d->m_textureLayer, level, x, y, &( d->m_tileCache ) ); 
        m_parent->paintTile( d->m_tileHash[id], x, y, level, d->m_textureLayer, true );
//         (d->m_tileHash[id]) -> reloadTile( x, y, level, d->m_theme );
    } else {
      // Remove "false" tile from cache so it doesn't get loaded anymore
      d->m_tileCache.remove( id );
      qDebug() << "No such ID:" << idStr;
    }
}

void TileLoader::reloadTile( const QString &relativeUrlString, const QString &_id )
{
    Q_UNUSED( relativeUrlString );
    // qDebug() << "Reloading Tile" << relativeUrlString << "id:" << _id;

    reloadTile( _id );
}

void TileLoader::reloadTile( const QString& serverUrlString, const QString &relativeUrlString,
                             const QString &_id )
{
    Q_UNUSED( serverUrlString );
    Q_UNUSED( relativeUrlString );
    // qDebug() << "Reloading Tile" << serverUrlString << relativeUrlString << "id:" << _id;

    reloadTile( _id );
}

void TileLoader::update()
{
    flush(); // trigger a reload of all tiles that are currently in use
    d->m_tileCache.clear(); // clear the tile cache in physical memory
    emit tileUpdateAvailable();
}

#include "TileLoader.moc"
