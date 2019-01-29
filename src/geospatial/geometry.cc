/*
 * Copyright (c) 2015-2018 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "geometry.h"

#include <algorithm>
#include <type_traits>


Constraint::Constraint()
	: arcangle(PI_HALF),
	  distance(0.0),
	  radius(RADIUS_GREAT_CIRCLE),
	  sign(Sign::ZERO)
{
	center.normalize();
}


void
Constraint::set_data(double _radius)
{
	if (_radius < MIN_RADIUS_METERS) {
		arcangle = MIN_RADIUS_RADIANS;
		distance = 1.0;
		radius = MIN_RADIUS_METERS;
		sign = Sign::POS;
	} else if (_radius > MAX_RADIUS_HALFSPACE_EARTH) {
		arcangle = M_PI;
		distance = -1.0;
		radius = MAX_RADIUS_HALFSPACE_EARTH;
		sign = Sign::NEG;
	} else {
		arcangle = _radius / M_PER_RADIUS_EARTH;
		distance = std::cos(arcangle);
		radius = _radius;
		if (distance > DBL_TOLERANCE) {
			sign = Sign::POS;
		} else if (distance < -DBL_TOLERANCE) {
			sign = Sign::NEG;
		} else {
			sign = Sign::ZERO;
		}
	}
}


bool
Constraint::operator==(const Constraint& c) const noexcept
{
	return center == c.center && arcangle == c.arcangle;
}


bool
Constraint::operator!=(const Constraint& c) const noexcept
{
	return !operator==(c);
}


bool
Constraint::operator<(const Constraint& c) const noexcept
{
	if (arcangle == c.arcangle) {
		return center < c.center;
	}
	return arcangle < c.arcangle;
}


bool
Constraint::operator>(const Constraint& c) const noexcept
{
	if (arcangle == c.arcangle) {
		return center > c.center;
	}
	return arcangle > c.arcangle;
}
