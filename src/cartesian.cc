/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include "cartesian.h"


// Constructor receives latitude, longitude and their units on specific CRS
// which are converted to cartesian coordinates, and then they are converted to CRS WGS84.
Cartesian::Cartesian(double lat, double lon, double height, LatLongUnits units, int _SRID)
{
	std::map<int, int>::const_iterator it;
	if ((it = SRIDS_DATUMS.find(_SRID)) == SRIDS_DATUMS.end()) {
		throw MSG_Error(std::string("SRID = " + std::to_string(_SRID) + " is not supported").c_str());
	}

	datum = it->second;
	SRID = _SRID;
	toCartesian(lat, lon, height, units);
	if (SRID != WGS84) transform2WGS84();
}


// Constructor receives latitude, longitude and their units on WGS84 CRS.
// which are converted to cartesian coordinates.
Cartesian::Cartesian(double lat, double lon, double height, LatLongUnits units)
{
	SRID = WGS84;
	datum = SRIDS_DATUMS.find(SRID)->second;
	toCartesian(lat, lon, height, units);
}


// Constructor receives a cartesian coordinate.
// This constructor receive a cartesian coordinate,
// which was obtained from WGS84 CRS.
Cartesian::Cartesian(double _x, double _y, double _z)
{
	SRID = WGS84;
	datum = SRIDS_DATUMS.find(SRID)->second;
	x = _x;
	y = _y;
	z = _z;
}


// Constructor default.
Cartesian::Cartesian() {
	SRID = WGS84;
	datum = SRIDS_DATUMS.find(SRID)->second;
	x = 0;
	y = 0;
	z = 1;
}


// Return if the SRID is supported for cartesian.
bool
Cartesian::is_SRID_supported(int _SRID)
{
	std::map<int, int>::const_iterator it;
	if ((it = SRIDS_DATUMS.find(_SRID)) == SRIDS_DATUMS.end()) {
		return false;
	}
	return true;
}


// Applies 7 - Helmert transformation to this point using the datum parameters.
void
Cartesian::transform2WGS84()
{
	datum_t t = datums[datum];
	double s_1 = t.s + 1;

	// Apply transform.
	double x2 = t.tx + s_1 * (x - t.rz * y + t.ry * z);
	double y2 = t.ty + s_1 * (t.rz * x + y - t.rx * z);
	double z2 = t.tz + s_1 * (- t.ry * x + t.rx * y + z);

	x = x2;
	y = y2;
	z = z2;

	SRID = WGS84;
	datum = SRIDS_DATUMS.find(SRID)->second;
}


// Converts this coordinate from (geodetic) latitude / longitude coordinates to
// (geocentric) cartesian (x, y, z) coordinates on the CRS specified by SRID.
//
// Function receives latitude, longitude, ellipsoid height and their units.
void
Cartesian::toCartesian(double lat, double lon, double height, LatLongUnits units)
{
	// If lat and lon are in degrees convert to radians.
	if (units == DEGREES) {
		lat = lat * RAD_PER_DEG;
		lon = lon * RAD_PER_DEG;
	}

	if (lat < -PI_HALF || lat > PI_HALF) {
		throw MSG_Error("Latitude out-of-range");
	}

	ellipsoid_t ellipsoid = ellipsoids[datums[datum].ellipsoid];

	double a = ellipsoid.major_axis;
	double e2 = ellipsoid.e2;

	double cos_lat = cos(lat);
	double sin_lat = sin(lat);
	double v = a / sqrt(1 - e2 * sin_lat * sin_lat);

	x = (v + height) * cos_lat * cos(lon);
	y = (v + height) * cos_lat * sin(lon);
	z = ((1 - e2) * v + height) * sin_lat;
}


// The function atan2 is the arctangent function with two arguments.
// The purpose of using two arguments instead of one is to gather information on the signs
// of the inputs in order to return the appropriate quadrant of the computed angle,
// which is not possible for the single-argument arctangent function.
double
Cartesian::atan2(double y, double x)
{
	if (x > 0) return atan(y / x);
	if (y >= 0 && x < 0) return atan((y / x) + M_PI);
	if (y < 0  && x < 0) return atan((y / x) - M_PI);
	if (y > 0 && x == 0) return M_PI / 2.0;
	if (y < 0 && x == 0) return - M_PI / 2.0;
	if (y == 0 && x == 0) throw MSG_Error("Undefined atan2(0.0, 0.0)");
	return 0.0;
}


// Convert decimal format of lat and lon to Degrees Minutes Seconds
// Return a string with "lat  lon  height".
std::string
Cartesian::Decimal2Degrees()
{
	double lat, lon, height;
	toGeodetic(lat, lon, height);
	int dlat = (int)lat;
	int dlon = (int)lon;
	int mlat = (int)((lat - dlat) * 60.0);
	int mlon = (int)((lon - dlon) * 60.0);
	double slat = (lat - dlat - mlat / 60.0) * 3600.0;
	double slon = (lon - dlon - mlon / 60.0) * 3600.0;
	std::string direction;
	if (lat >= 0) {
		direction = "''N";
	} else {
		direction = "''S";
		dlat = - 1 * dlat;
		mlat = - 1 * mlat;
		slat = - 1 * slat;
	}
	std::string res =  std::to_string(dlat) + "°" + std::to_string(mlat) + "'" + std::to_string(slat) + direction;
	if (lon >= 0) {
		direction = "''E";
	} else {
		direction = "''W";
		dlon = - 1 * dlon;
		mlon = - 1 * mlon;
		slon = - 1 * slon;
	}
	res += "  " + std::to_string(dlon) + "°" + std::to_string(mlon) + "'" + std::to_string(slon) + direction + "  " + std::to_string(height);
	return res;
}


// Converts (geocentric) cartesian (x, y, z) to (ellipsoidal geodetic) latitude / longitude coordinates.
// Modified lat and lon in degrees, height in meters.
void
Cartesian::toGeodetic(double &lat, double &lon, double &height)
{
	lon = atan2(y, x);
	double p = sqrt(x * x + y * y);

	ellipsoid_t ellipsoid = ellipsoids[datums[datum].ellipsoid];

	double a = ellipsoid.major_axis;
	double e2 = ellipsoid.e2;

	lat = atan2(z, p * (1 - e2));
	double sin_lat = sin(lat);
	double v = a / sqrt(1 - e2 * sin_lat * sin_lat);
	double diff = 1;
	while (diff <= -DBL_TOLERANCE || diff >= DBL_TOLERANCE) {
		double lat2 = atan2(z + e2 * v * sin_lat, p);
		diff = lat - lat2;
		lat = lat2;
		sin_lat = sin(lat);
		v = a / sqrt(1 - e2 * sin_lat * sin_lat);
	}

	height = (p / cos(lat)) - v;
	lat = lat * DEG_PER_RAD;
	lon = lon * DEG_PER_RAD;
}


double
Cartesian::operator *(const Cartesian &p) const
{
	return (x * p.x + y * p.y + z * p.z);
}


Cartesian
Cartesian::operator ^(const Cartesian &p) const
{
	return Cartesian(y * p.z - p.y * z, z * p.x - p.z * x, x * p.y - p.x * y);
}


Cartesian
Cartesian::operator +(const Cartesian &p) const
{
	return Cartesian(x + p.x, y + p.y, z + p.z);
}


Cartesian
Cartesian::operator -(const Cartesian &p) const
{
	return Cartesian(x - p.x, y - p.y, z - p.z);
}


bool
Cartesian::operator ==(const Cartesian &p) const
{
	return (x == p.x && y == p.y && z == p.z && SRID == p.getSRID());
}


bool
Cartesian::operator !=(const Cartesian &p) const
{
	return (x != p.x || y != p.y || z != p.z || SRID != p.getSRID());
}


Cartesian&
Cartesian::operator =(const Cartesian &p)
{
	x = p.x;
	y = p.y;
	z = p.z;
	SRID = p.getSRID();
	datum = p.getDatum();
	return *this;
}


void
Cartesian::normalize()
{
	double sum = sqrt(x * x + y * y + z * z);
	x /= sum;
	y /= sum;
	z /= sum;
}


Cartesian
Cartesian::get_inverse()
{
	return Cartesian(-x, -y, -z);
}


double
Cartesian::norm()
{
	return sqrt(x * x + y * y + z * z);
}


double
Cartesian::distance(Cartesian &p)
{
	Cartesian c(x - p.x, y - p.y, z - p.z);
	return c.norm();
}


std::string
Cartesian::as_string() const
{
	return "CRS = WGS84\n(" + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + ")";
}


char*
Cartesian::c_str() const
{
	std::string tmp = "SRID = " + std::to_string(SRID) + "\n(" + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + ")";
	char *str = new char[tmp.size() + 1];
	strcpy(str, tmp.c_str());
	return str;
}


int
Cartesian::getSRID() const
{
	return SRID;
}


int
Cartesian::getDatum() const
{
	return datum;
}