//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2015      Dennis Nienhüser <nienhueser@kde.org>
//

#include <OsmRelation.h>
#include <MarbleDebug.h>
#include <GeoDataLineStyle.h>
#include <GeoDataPlacemark.h>
#include <GeoDataPoint.h>
#include <GeoDataPolyStyle.h>
#include <GeoDataStyle.h>
#include <GeoDataDocument.h>
#include <osm/OsmObjectManager.h>
#include <MarbleDirs.h>
#include <StyleBuilder.h>

namespace Marble {

QSet<StyleBuilder::OsmTag> OsmWay::s_areaTags;

void OsmWay::create(GeoDataDocument *document, const OsmNodes &nodes, QSet<qint64> &usedNodes) const
{
    const double height = extractBuildingHeight(m_osmData);

    OsmPlacemarkData osmData = m_osmData;
    GeoDataGeometry *geometry = 0;

    if (isArea()) {
        GeoDataLinearRing linearRing;

        foreach(qint64 nodeId, m_references) {
            auto const nodeIter = nodes.constFind(nodeId);
            if (nodeIter == nodes.constEnd()) {
                return;
            }

            OsmNode const & node = nodeIter.value();
            osmData.addNodeReference(node.coordinates(), node.osmData());
            linearRing.append(node.coordinates());
            usedNodes << nodeId;
        }

        linearRing = GeoDataLinearRing(linearRing.optimized());
        if (!linearRing.isEmpty() && height != 0) {
            linearRing.first().setAltitude(height);
        }

        geometry = new GeoDataLinearRing(linearRing);
    } else {
        GeoDataLineString lineString;

        foreach(qint64 nodeId, m_references) {
            auto const nodeIter = nodes.constFind(nodeId);
            if (nodeIter == nodes.constEnd()) {
                return;
            }

            OsmNode const & node = nodeIter.value();
            osmData.addNodeReference(node.coordinates(), node.osmData());
            lineString.append(node.coordinates());
            usedNodes << nodeId;
        }

        lineString = lineString.optimized();
        if (!lineString.isEmpty() && height != 0) {
            lineString.first().setAltitude(height);
        }

        geometry = new GeoDataLineString(lineString);
    }

    Q_ASSERT(geometry != nullptr);
    if (height != 0) {
        geometry->setAltitudeMode(RelativeToGround);
    }

    OsmObjectManager::registerId(m_osmData.id());

    const auto visualCategory = StyleBuilder::determineVisualCategory(m_osmData);

    GeoDataPlacemark *placemark = new GeoDataPlacemark;
    placemark->setGeometry(geometry);
    placemark->setVisualCategory(visualCategory);
    placemark->setName(m_osmData.tagValue(QStringLiteral("name")));
    if (placemark->name().isEmpty()) {
        placemark->setName(m_osmData.tagValue(QStringLiteral("ref")));
    }
    if (placemark->name().isEmpty()) {
        placemark->setName(m_osmData.tagValue(QStringLiteral("addr:housename")));
    }
    if (placemark->name().isEmpty()) {
        placemark->setName(m_osmData.tagValue(QStringLiteral("addr:housenumber")));
    }
    placemark->setOsmData(osmData);
    placemark->setVisible(visualCategory != GeoDataPlacemark::None);

    document->append(placemark);

    QVector<NamedEntry> namedEntries = extractNamedEntries(osmData);
    if (!namedEntries.isEmpty()) {
        foreach (const auto &namedEntry, namedEntries) {
            GeoDataPlacemark *entry = new GeoDataPlacemark();
            entry->setCoordinate(namedEntry.coordinates);
            entry->setName(namedEntry.label);
            entry->setOsmData(namedEntry.osmData);
            entry->setVisualCategory(visualCategory);
            entry->setVisible(visualCategory != GeoDataPlacemark::None);

            document->append(entry);
        }
    }
}

const QVector<qint64> &OsmWay::references() const
{
    return m_references;
}

OsmPlacemarkData &OsmWay::osmData()
{
    return m_osmData;
}

const OsmPlacemarkData &OsmWay::osmData() const
{
    return m_osmData;
}

void OsmWay::addReference(qint64 id)
{
    m_references << id;
}

bool OsmWay::isArea() const
{
    // @TODO A single OSM way can be both closed and non-closed, e.g. landuse=grass with barrier=fence.
    // We need to create two separate ways in cases like that to support this.
    // See also https://wiki.openstreetmap.org/wiki/Key:area

    bool const isLinearFeature =
            m_osmData.containsTag(QStringLiteral("area"), QStringLiteral("no")) ||
            m_osmData.containsTagKey(QStringLiteral("highway")) ||
            m_osmData.containsTagKey(QStringLiteral("barrier"));
    if (isLinearFeature) {
        return false;
    }

    bool const isAreaFeature = m_osmData.containsTagKey(QStringLiteral("landuse"));
    if (isAreaFeature) {
        return true;
    }

    for (auto iter = m_osmData.tagsBegin(), end=m_osmData.tagsEnd(); iter != end; ++iter) {
        const auto tag = StyleBuilder::OsmTag(iter.key(), iter.value());
        if (isAreaTag(tag)) {
            return true;
        }
    }

    bool const isImplicitlyClosed = m_references.size() > 1 && m_references.front() == m_references.last();
    return isImplicitlyClosed;
}

bool OsmWay::isAreaTag(const StyleBuilder::OsmTag &keyValue)
{
    if (s_areaTags.isEmpty()) {
        // All these tags can be found updated at
        // http://wiki.openstreetmap.org/wiki/Map_Features#Landuse

        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("water")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("wood")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("beach")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("wetland")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("glacier")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("scrub")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("natural"), QStringLiteral("cliff")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("area"), QStringLiteral("yes")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("waterway"), QStringLiteral("riverbank")));

        foreach (const auto tag, StyleBuilder::buildingTags()) {
            s_areaTags.insert(tag);
        }
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("man_made"), QStringLiteral("bridge")));

        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("graveyard")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("parking")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("parking_space")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("bicycle_parking")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("college")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("hospital")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("kindergarten")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("school")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("amenity"), QStringLiteral("university")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("common")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("garden")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("golf_course")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("marina")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("playground")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("pitch")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("park")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("sports_centre")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("stadium")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("swimming_pool")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("leisure"), QStringLiteral("track")));

        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("military"), QStringLiteral("danger_area")));

        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("marble_land"), QStringLiteral("landmass")));
        s_areaTags.insert(StyleBuilder::OsmTag(QStringLiteral("settlement"), QStringLiteral("yes")));
    }

    return s_areaTags.contains(keyValue);
}

double OsmWay::extractBuildingHeight(const OsmPlacemarkData &osmData)
{
    double height = 8.0;

    QHash<QString, QString>::const_iterator tagIter;
    if ((tagIter = osmData.findTag(QStringLiteral("height"))) != osmData.tagsEnd()) {
        /** @todo Also parse non-SI units, see https://wiki.openstreetmap.org/wiki/Key:height#Height_of_buildings */
        QString const heightValue = QString(tagIter.value()).remove(QStringLiteral(" meters")).remove(QStringLiteral(" m"));
        bool extracted = false;
        double extractedHeight = heightValue.toDouble(&extracted);
        if (extracted) {
            height = extractedHeight;
        }
    } else if ((tagIter = osmData.findTag(QStringLiteral("building:levels"))) != osmData.tagsEnd()) {
        int const levels = tagIter.value().toInt();
        int const skipLevels = osmData.tagValue(QStringLiteral("building:min_level")).toInt();
        /** @todo Is 35 as an upper bound for the number of levels sane? */
        height = 3.0 * qBound(1, 1+levels-skipLevels, 35);
    }

    return qBound(1.0, height, 1000.0);
}

QVector<OsmWay::NamedEntry> OsmWay::extractNamedEntries(const OsmPlacemarkData &osmData)
{
    QVector<NamedEntry> entries;

    const auto end = osmData.nodeReferencesEnd();
    for (auto iter = osmData.nodeReferencesBegin(); iter != end; ++iter) {
        const auto tagIter = iter.value().findTag(QStringLiteral("addr:housenumber"));
        if (tagIter != iter.value().tagsEnd()) {
            NamedEntry entry;
            entry.coordinates = iter.key();
            entry.label = tagIter.value();
            entry.osmData = *iter;
            entries.push_back(entry);
        }
    }

    return entries;
}

}
