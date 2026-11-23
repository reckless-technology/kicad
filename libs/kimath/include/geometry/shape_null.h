/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 CERN
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __SHAPE_NULL_H
#define __SHAPE_NULL_H

#include <geometry/shape.h>
#include <math/box2.h>
#include <math/vector2d.h>

#include <algorithm>

class SHAPE_NULL : public SHAPE
{
public:
    SHAPE_NULL() :
        SHAPE( SH_NULL )
    {}

    SHAPE_NULL( const SHAPE_NULL& aOther [[maybe_unused]] ) :
        SHAPE( SH_NULL )
    {};

    ~SHAPE_NULL()
    {}

    SHAPE* Clone() const override
    {
        return new SHAPE_NULL( *this );
    }

    SHAPE_NULL& operator=( const SHAPE_NULL& ) = default;

    const BOX2I BBox( int aClearance [[maybe_unused]] = 0 ) const override
    {
        return BOX2I();
    }

    bool Collide( const SEG& aSeg [[maybe_unused]], int aClearance [[maybe_unused]] = 0, int* aActual [[maybe_unused]] = nullptr,
                  VECTOR2I* aLocation [[maybe_unused]] = nullptr ) const override
    {
        return false;
    }

    void Move( const VECTOR2I& aVector [[maybe_unused]] ) override
    {
    }

    void Rotate( const EDA_ANGLE& aAngle [[maybe_unused]], const VECTOR2I& aCenter [[maybe_unused]] = { 0, 0 } ) override
    {
    }

    bool IsSolid() const override
    {
        return false;
    }

    void TransformToPolygon( SHAPE_POLY_SET& aBuffer [[maybe_unused]], int aError [[maybe_unused]],
                             ERROR_LOC aErrorLoc [[maybe_unused]] ) const override {}
};

#endif
