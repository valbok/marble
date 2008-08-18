//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2007      Inge Wallin  <ingwa@kde.org>"
// Copyright 2008      Jens-Michael Hoffmann <jensmh@gmx.de>
//


#include "ViewportParams.h"

#include <QtCore/QDebug>
#include <QtCore/QRect>

#include "SphericalProjection.h"
#include "EquirectProjection.h"
#include "MercatorProjection.h"

#include "AbstractProjectionHelper.h"

namespace Marble
{

class ViewportParamsPrivate
{
public:
     ViewportParamsPrivate();

    // These two go together.  m_currentProjection points to one of
    // the static Projection classes at the bottom.
    Projection           m_projection;
    AbstractProjection  *m_currentProjection;

    // Parameters that determine the painting
    Quaternion           m_planetAxis;   // Position, coded in a quaternion
    mutable matrix       m_planetAxisMatrix;
    int                  m_radius;       // Zoom level (pixels / globe radius)

    QSize                m_size;         // width, height

    // FIXME: The usage of the class BoundingBox as a whole is DEPRECATED
    //        Please use the class GeoDataLatLon(Alt)Box instead! 
    BoundingBox m_boundingBox;  // What the view currently can see

    static SphericalProjection  s_sphericalProjection;
    static EquirectProjection   s_equirectProjection;
    static MercatorProjection   s_mercatorProjection;
};

ViewportParamsPrivate::ViewportParamsPrivate()
    : m_projection( Spherical ),                    // Default projection
      m_currentProjection( &s_sphericalProjection ),
      m_planetAxis(),
      m_planetAxisMatrix(),
      m_radius( 2000 ),
      m_size( 100, 100 ),
      m_boundingBox()
{
} 


// FIXME: avoid usage of static objects
SphericalProjection  ViewportParamsPrivate::s_sphericalProjection;
EquirectProjection   ViewportParamsPrivate::s_equirectProjection;
MercatorProjection   ViewportParamsPrivate::s_mercatorProjection;


ViewportParams::ViewportParams()
    : d( new ViewportParamsPrivate )
{
    // Default view
    setPlanetAxis( Quaternion( 1.0, 0.0, 0.0, 0.0 ) );
}

ViewportParams::~ViewportParams()
{
    delete d;
}


// ================================================================
//                    Getters and setters


Projection ViewportParams::projection() const
{
    return d->m_projection;
}

AbstractProjection *ViewportParams::currentProjection() const
{
    return d->m_currentProjection;
}

void ViewportParams::setProjection(Projection newProjection)
{
    d->m_projection = newProjection;

    // Set the pointer to the current projection class.
    switch ( newProjection ) {
    case Spherical:
        d->m_currentProjection = &d->s_sphericalProjection;
        break;
    case Equirectangular:
        d->m_currentProjection = &d->s_equirectProjection;
        break;
    case Mercator:
        d->m_currentProjection = &d->s_mercatorProjection;
        break;
    }

    // Adjust the active Region
    currentProjection()->helper()->createActiveRegion( this );
    currentProjection()->helper()->createProjectedRegion( this );

    // We now need to reset the planetAxis to make sure 
    // that it's a valid axis orientation!
    // So this line is important (although it might look odd) ! :
    setPlanetAxis( planetAxis() );
}

int ViewportParams::polarity() const
{
    // For mercator this just gives the extreme latitudes
    // instead of the actual poles but it works fine as well:
    GeoDataCoordinates northPole( 0.0, +currentProjection()->maxLat() );
    GeoDataCoordinates southPole( 0.0, -currentProjection()->maxLat() );

    bool globeHidesN, globeHidesS;
    int x;
    int yN, yS;

    currentProjection()->screenCoordinates( northPole, this,
                                          x, yN, globeHidesN );
    currentProjection()->screenCoordinates( southPole, this,
                                          x, yS, globeHidesS );

    int polarity = 0;

    // case of the flat map:
    if ( !globeHidesN && !globeHidesS ) {
        if ( yN < yS ) {
            polarity = +1;
        }
        if ( yS < yN ) {
            polarity = -1;
        }
    }
    else {
        if ( !globeHidesN && yN < height() / 2 ) {
            polarity = +1;
        }
        if ( !globeHidesN && yN > height() / 2 ) {
            polarity = -1;
        }
        if ( !globeHidesS && yS > height() / 2 ) {
            polarity = +1;
        }
        if ( !globeHidesS && yS < height() / 2 ) {
            polarity = -1;
        }
    }

    return polarity;
}

int ViewportParams::radius() const
{
    return d->m_radius;
}

void ViewportParams::setRadius(int newRadius)
{
    d->m_radius = newRadius;

    // Adjust the active Region
    currentProjection()->helper()->createActiveRegion( this );
    currentProjection()->helper()->createProjectedRegion( this );
}

bool ViewportParams::globeCoversViewport() const
{
    // This first test is a quick one that will catch all really big
    // radii and prevent overflow in the real test.
    if ( d->m_radius > d->m_size.width() + d->m_size.height() )
        return true;

    // This is the real test.  The 4 is because we are really
    // comparing to width/2 and height/2.
    if ( 4 * d->m_radius * d->m_radius >= d->m_size.width() * d->m_size.width()
                                          + d->m_size.height() * d->m_size.height() )
        return true;

    return false;
}

Quaternion ViewportParams::planetAxis() const
{
    return d->m_planetAxis;
}

bool ViewportParams::setPlanetAxis(const Quaternion &newAxis)
{
    bool valid = true;

    qreal maxLat = currentProjection()->maxLat();

    // Make sure that the planetAxis doesn't get invalid
    // The planetAxis is invalid if lat exceeds
    // the maximum latitude or longitude as specified by the 
    // projection
    // This is should not be done for projections where the
    // maximum latitude is traversable (e.g. for a sphere).

    if ( !currentProjection()->traversableMaxLat() && fabs( newAxis.pitch() ) > maxLat ) {

        qreal centerLon, centerLat;
        centerCoordinates( centerLon, centerLat );

        // Normalize latitude and longitude
        GeoDataPoint::normalizeLat( centerLat );

        // Checking whether the latitude is valid:
        if ( fabs( centerLat ) > maxLat )
        {
            valid = false;
            centerLat = maxLat * centerLat / fabs( centerLat );
        }
        
        d->m_planetAxis.createFromEuler( -centerLat, centerLon, newAxis.roll() );
    }
    else {
        d->m_planetAxis = newAxis;
    }

    // creating matching planetAxis matrix
    planetAxis().inverse().toMatrix( d->m_planetAxisMatrix );

    // Adjust the projected and active Region
    currentProjection()->helper()->createActiveRegion( this );
    currentProjection()->helper()->createProjectedRegion( this );

    return valid;
}

matrix * ViewportParams::planetAxisMatrix() const
{
    return &d->m_planetAxisMatrix;
}

int ViewportParams::width()  const
{
    return d->m_size.width();
}

int ViewportParams::height() const
{
    return d->m_size.height();
}

QSize ViewportParams::size() const
{
    return d->m_size;
}


void ViewportParams::setWidth(int newWidth)
{
    d->m_size.setWidth( newWidth );

    // Adjust the active Region
    currentProjection()->helper()->createActiveRegion( this );
    currentProjection()->helper()->createProjectedRegion( this );
}

void ViewportParams::setHeight(int newHeight)
{
    d->m_size.setHeight( newHeight );

    // Adjust the active Region
    currentProjection()->helper()->createActiveRegion( this );
    currentProjection()->helper()->createProjectedRegion( this );
}

void ViewportParams::setSize(QSize newSize)
{
    d->m_size = newSize;

    // Adjust the active Region
    currentProjection()->helper()->createActiveRegion( this );
    currentProjection()->helper()->createProjectedRegion( this );
}

BoundingBox ViewportParams::boundingBox() const
{
    return d->m_boundingBox;
}

void ViewportParams::setBoundingBox( const BoundingBox & boundingBox )
{
    d->m_boundingBox = boundingBox;
}

// ================================================================
//                        Other functions


void ViewportParams::centerCoordinates( qreal &centerLon, qreal &centerLat ) const
{
    // Calculate translation of center point
    centerLat = - d->m_planetAxis.pitch();
    if ( centerLat > M_PI )
        centerLat -= 2 * M_PI;

    centerLon = + d->m_planetAxis.yaw();
    if ( centerLon > M_PI )
        centerLon -= 2 * M_PI;
}

GeoDataLatLonAltBox ViewportParams::viewLatLonAltBox() const
{
    return d->m_currentProjection->latLonAltBox( QRect( QPoint( 0, 0 ), 
                        d->m_size ),
                        this );
}


bool  ViewportParams::mapCoversViewport() const
{
    return d->m_currentProjection->mapCoversViewport( this );
}

}
