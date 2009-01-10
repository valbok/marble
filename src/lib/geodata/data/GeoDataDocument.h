/*
    Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2007 Murad Tagirov <tmurad@gmail.com>

    This file is part of the KDE project

    This library is free software you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef GeoDataDocument_h
#define GeoDataDocument_h

#include <QtCore/QHash>
#include <QtCore/QVector>

#include "geodata_export.h"

#include "GeoDataContainer.h"
#include "GeoDocument.h"

namespace Marble
{

class GeoDataStyle;
class GeoDataStyleMap;

class GeoDataDocumentPrivate;

/**
 * @short A container for Features, Styles and in the future Schemas.
 *
 * A GeoDataDocument is a container for features, styles, and
 * schemas. This element is required if your KML file uses schemas or
 * shared styles. It is recommended that all Styles be defined in a
 * Document, each with an id, and then later referenced by a
 * styleUrl for a given Feature or StyleMap.
 */
class GEODATA_EXPORT GeoDataDocument : public GeoDocument,
                                       public GeoDataContainer {
public:
    explicit GeoDataDocument( GeoDataObject *parent = 0 );
    ~GeoDataDocument();

    virtual EnumFeatureId featureId() const { return GeoDataDocumentId; };
    virtual bool isGeoDataDocument() const { return true; }

    /**
     * @brief Add a style to the style storage
     * @param style  the new style
     */
    void addStyle( GeoDataStyle* style );

    /**
     * @brief Add a style to the style storage
     * @param style  the new style
     */
    void removeStyle( GeoDataStyle* style );

    /**
     * @brief Return a style in the style storage
     * @param styleId  the id of the style
     */
    GeoDataStyle* style( const QString& styleId ) const;
    
    /**
    * @brief dump a Vector of all styles
    */
    QList<GeoDataStyle*> styles() const;

    /**
    * @brief Add a stylemap to the stylemap storage
    * @param map  the new stylemap
    */
    void addStyleMap( GeoDataStyleMap* map );
    
    /**
    * @brief remove stylemap from storage
    * @param map the styleMap to be removed
    */
    void removeStyleMap( GeoDataStyleMap* map );
    
    /**
     * @brief Return a style in the style storage
     * @param styleId  the id of the style
     */
    GeoDataStyleMap* styleMap( const QString& styleId ) const;
    
    /**
    * @brief dump a Vector of all styles
    */
    QList<GeoDataStyleMap*> styleMaps() const;
    
    // Serialize the Placemark to @p stream
    virtual void pack( QDataStream& stream ) const;
    // Unserialize the Placemark from @p stream
    virtual void unpack( QDataStream& stream );

private:
    Q_DISABLE_COPY( GeoDataDocument )
    GeoDataDocumentPrivate * const d;
};

}

#endif // GeoDataDocument_h
