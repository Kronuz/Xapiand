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

#include <cmath>


constexpr int DATUM_WGS84 = 0; // Datum for WGS84.


/*
 * More ellipsoids available in:
 * http://earth-info.nga.mil/GandG/coordsys/datums/ellips.txt
 * http://icvficheros.icv.gva.es/ICV/geova/erva/Utilidades/jornada_ETRS89/1_ANTECEDENTES_IGN.pdf
 * http://www.geocachingtoolbox.com/?page=datumEllipsoidDetails
 */
const ellipsoid_t ellipsoids[12] = {
	// Used by GPS and default in this aplication.
	{ "World Geodetic System 1984 (WE)",	 6378137,     6356752.314245179, 0.00669437999014131699613723 },
	{ "Geodetic Reference System 1980 (RF)", 6378137,     6356752.314140356, 0.00669438002290078762535911 },
	{ "Airy 1830 (AA)",						 6377563.396, 6356256.909237285, 0.00667053999998536347457648 },
	{ "Modified Airy (AM)",					 6377340.189, 6356034.447938534, 0.00667053999998536347457648 },
	// Hayford 1909.
	{ "International 1924 (IN)",			 6378388,     6356911.946127946, 0.00672267002233332199662165 },
	{ "Bessel 1841 (BR)",					 6377397.155, 6356078.962818188, 0.00667437223180214468008836 },
	{ "Helmert 1906 (HE)",					 6378200,     6356818.169627891, 0.00669342162296594322796213 },
	{ "Australian National (AN)",			 6378160,     6356774.719195305, 0.00669454185458763715976614 },
	// The most used in Mexico. http://www.inegi.org.mx/inegi/SPC/doc/internet/Sistema_de_Coordenadas.pdf
	{ "Clarke 1866 (CC)",  					 6378206.4,   6356583.799998980, 0.00676865799760964394479365 },
	// Also called GRS 1967 Modified
	{ "South American 1969 (SA)", 			 6378160,     6356774.719195305, 0.00669454185458763715976614 },
	{ "Krassovsky 1940 (KA)", 				 6378245,     6356863.018773047, 0.00669342162296594322796213 },
	{ "Worl Geodetic System 1972 (WD)",		 6378135,     6356750.520016093, 0.00669431777826672197122802 }
};


/*
 * Datums: with associated ellipsoid and Helmert transform parameters to convert given CRS
 * to WGS84 CRS.
 *
 * More are available from:
 * http://earth-info.nga.mil/GandG/coordsys/datums/NATO_DT.pdfs
 * http://georepository.com/search/by-name/?query=&include_world=on
 */
const datum_t datums[17] = {
	// World Geodetic System 1984 (WGS84)
	// EPSG_SRID: 4326
	// Code NATO: WGE
	{ "World Geodetic System 1984 (WGS84)", WE, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
	// World Geodetic System 1972
	// EPSG_SRID: 4322
	// Code NATO: WGC-7
	{ "Worl Geodetic System 1972 (WGS72)", WD, 0.0, 0.0, 4.5, 0.0, 0.0, (0.554 / 3600.0) * RAD_PER_DEG, 0.219 / 1E6},
	// North American Datum 1983 USA - Hawaii - main islands
	// EPSG_SRID: 4269
	// Code NATO: NAR(H)
	{ "North American Datum 1983 US - Hawaii (NAD83)", RF, 1, 1, -1, 0.0, 0.0, 0.0, 0.0 },
	// NORTH AMERICAN 1927 USA - CONUS - onshore
	// EPSG_SRID: 4267
	// Code NATO: NAS(C)
	{ "North American 1927 US-CONUS (NAD27)", CC, -8, 160, 176, 0.0, 0.0, 0.0, 0.0 },
	// Ordnance Survey Great Britain 1936 - UK - Great Britain; Isle of Man
	// EPSG_SRID: 4277
	// Code NATO: OGB-7
	{ "Ordnance Survey Great Britain 1936 (OSGB36)", AA, 446.448, -125.157, 542.06, (0.150 / 3600.0) * RAD_PER_DEG, (0.247 / 3600.0) * RAD_PER_DEG, (0.8421 / 3600.0) * RAD_PER_DEG, -20.4894 / 1E6 },
	// IRELAND 1975, Europe - Ireland (Republic and Ulster) - onshore
	// EPSG_SRID: 4300
	{ "Ireland 1975 (TM75)", AM, 482.5, -130.6, 564.6, (-1.042 / 3600.0) * RAD_PER_DEG, (-0.214 / 3600.0) * RAD_PER_DEG, (-0.631 / 3600.0) * RAD_PER_DEG, 8.150 / 1E6 },
	// IRELAND 1965, Europe - Ireland (Republic and Ulster) - onshore
	// EPSG_SRID: 4299
	// Code NATO: IRL-7
	{ "Ireland 1965 (TM65)", AM, 482.530, -130.596, 564.557, (-1.042 / 3600.0) * RAD_PER_DEG, (-0.214 / 3600.0) * RAD_PER_DEG, (-0.631 / 3600.0) * RAD_PER_DEG, 8.150 / 1E6 },
	// European Datum 1979 (ED79), Europe - west
	// EPSG_SRID: 4668
	// http://georepository.com/transformation_15752/ED79-to-WGS-84-1.html
	{ "European Datum 1979 (ED79)", IN, -86, -98, -119, 0.0, 0.0, 0.0, 0.0 },
	// European Datum 1950, Europe - west (DMA ED50 mean)
	// EPSG_SRID: 4230
	// Code NATO: EUR(M)
	// http://georepository.com/transformation_1133/ED50-to-WGS-84-1.html
	{ "European Datum 1950 (ED50)", IN, -87, -98, -121, 0.0, 0.0, 0.0, 0.0 },
	// Tokyo Japan, Asia - Japan and South Korea
	// EPSG_SRID: 4301
	// Code NATO: TOY(A)
	{ "Tokyo Japan (TOYA)", BR, -148, 507, 685, 0.0, 0.0, 0.0, 0.0 },
	// DHDN (RAUENBERG), Germany - West Germany all states
	// EPSG_SRID: 4314
	// Code NATO: RAU-7
	{ "Deutsches Hauptdreiecksnetz (DHDN)", BR, 582, 105, 414, (1.04 / 3600.0) * RAD_PER_DEG, (0.35 / 3600.0) * RAD_PER_DEG, (-3.08 / 3600.0) * RAD_PER_DEG, 8.3 /1E6 },
	// OLD EGYPTIAN 1907 - Egypt.
	// EPSG_SRID: 4229
	// Code NATO: OEG
	{ "Egypt 1907 (OEG)", HE, -130, 110, -13, 0.0, 0.0, 0.0, 0.0 },
	// AUSTRALIAN GEODETIC 1984, Australia - all states
	// EPSG_SRID: 4203
	// Code NATO: AUG-7
	{ "Australian Geodetic 1984 (AGD84)", AN, -116, -50.47, 141.69, (0.23 / 3600.0) * RAD_PER_DEG, (0.39 / 3600.0) * RAD_PER_DEG, (0.344 / 3600.0) * RAD_PER_DEG, 0.0983 / 1E6 },
	// SOUTH AMERICAN 1969 - South America - SAD69 by country
	// EPSG_SRID: 4618
	// Code NATO: SAN(M)
	{ "South American 1969 (SAD69)", SA, -57, 1, -41, 0.0, 0.0, 0.0, 0.0 },
	// PULKOVO 1942	- Germany - East Germany all states
	// EPSG_SRID: 4178
	// Code NATO: PUK-7
	{ "Pulkovo 1942 (PUL42)", KA, 21.58719, -97.541, -60.925, (1.01378 / 3600.0) * RAD_PER_DEG, (0.58117 / 3600.0) * RAD_PER_DEG, (0.2348 / 3600.0) * RAD_PER_DEG, -4.6121 / 1E6 },
	// HERMANNSKOGEL, Former Yugoslavia.
	// EPSG_SRID: 3906
	// Code NATO: HER-7
	{ "MGI 1901 (MGI1901)", BR, 515.149, 186.233, 511.959, (5.49721 / 3600.0) * RAD_PER_DEG, (3.51742 / 3600.0) * RAD_PER_DEG, (-12.948 / 3600.0) * RAD_PER_DEG, 0.782 / 1E6 },
	// GGRS87, Greece
	// EPSG_SRID: 4121
	// Code NATO: GRX
	{ "GGRS87", RF, -199.87, 74.79, 246.62, 0.0, 0.0, 0.0, 0.0 }
};


const std::map<int, int> SRIDS_DATUMS({
	std::pair<int, int>(WGS84,    0),
	std::pair<int, int>(WGS72,    1),
	std::pair<int, int>(NAD83,    2),
	std::pair<int, int>(NAD27,    3),
	std::pair<int, int>(OSGB36,   4),
	std::pair<int, int>(TM75,     5),
	std::pair<int, int>(TM65,     6),
	std::pair<int, int>(ED79,     7),
	std::pair<int, int>(ED50,     8),
	std::pair<int, int>(TOYA,     9),
	std::pair<int, int>(DHDN,    10),
	std::pair<int, int>(OEG,     11),
	std::pair<int, int>(AGD84,   12),
	std::pair<int, int>(SAD69,   13),
	std::pair<int, int>(PUL42,   14),
	std::pair<int, int>(MGI1901, 15),
	std::pair<int, int>(GGRS87,  16)
});


Cartesian::Cartesian()
	: SRID(WGS84),
	  datum(DATUM_WGS84),
	  x(1.0),
	  y(0.0),
	  z(0.0) { }


/*
 * Constructor receives latitude, longitude and their units on specific CRS
 * which are converted to cartesian coordinates, and then they are converted to CRS WGS84.
 */
Cartesian::Cartesian(double lat, double lon, double height, const CartesianUnits &units, int _SRID)
{
	auto it = SRIDS_DATUMS.find(_SRID);
	if (it == SRIDS_DATUMS.end()) {
		throw MSG_Error("SRID = %d is not supported", _SRID);
	}

	datum = it->second;
	toCartesian(lat, lon, height, units);

	if (_SRID != WGS84) transform2WGS84();
	SRID = WGS84;
}


/*
 * Constructor receives latitude, longitude and their units on WGS84 CRS.
 * which are converted to cartesian coordinates.
 */
Cartesian::Cartesian(double lat, double lon, double height, const CartesianUnits &units)
	: SRID(WGS84),
	  datum(DATUM_WGS84)
{
	toCartesian(lat, lon, height, units);
}


/*
 * Constructor receives a cartesian coordinate.
 * This constructor receive a cartesian coordinate,
 * which was obtained from WGS84 CRS.
 */
Cartesian::Cartesian(double _x, double _y, double _z)
	: SRID(WGS84),
	  datum(DATUM_WGS84),
	  x(_x),
	  y(_y),
	  z(_z) { }


// Applies 7 - Helmert transformation to this point using the datum parameters.
void
Cartesian::transform2WGS84() noexcept
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

	datum = DATUM_WGS84;
}


/*
 * Converts this coordinate from (geodetic) latitude / longitude coordinates to
 * (geocentric) cartesian (x, y, z) coordinates on the CRS specified by SRID.
 *
 * Function receives latitude, longitude, ellipsoid height and their units.
 */
void
Cartesian::toCartesian(double lat, double lon, double height, const CartesianUnits &units)
{
	// If lat and lon are in degrees convert to radians.
	if (units == CartesianUnits::DEGREES) {
		lat = lat * RAD_PER_DEG;
		lon = lon * RAD_PER_DEG;
	}

	if (lat < -PI_HALF || lat > PI_HALF) {
		throw MSG_Error("Latitude out-of-range");
	}

	auto ellipsoid = ellipsoids[datums[datum].ellipsoid];

	double a = ellipsoid.major_axis;
	double e2 = ellipsoid.e2;

	double cos_lat = std::cos(lat);
	double sin_lat = std::sin(lat);
	double v = a / std::sqrt(1 - e2 * sin_lat * sin_lat);

	x = (v + height) * cos_lat * std::cos(lon);
	y = (v + height) * cos_lat * std::sin(lon);
	z = ((1 - e2) * v + height) * sin_lat;
}


bool
Cartesian::operator==(const Cartesian &p) const noexcept
{
	return x == p.x && y == p.y && z == p.z && SRID == p.SRID;
}


bool
Cartesian::operator!=(const Cartesian &p) const noexcept
{
	return x != p.x || y != p.y || z != p.z || SRID != p.SRID;
}


double
Cartesian::operator*(const Cartesian &p) const noexcept
{
	return x * p.x + y * p.y + z * p.z;
}


Cartesian
Cartesian::operator^(const Cartesian &p) const noexcept
{
	return Cartesian(y * p.z - p.y * z, z * p.x - p.z * x, x * p.y - p.x * y);
}


Cartesian&
Cartesian::operator^=(const Cartesian &p) noexcept
{
	double x2 = y * p.z - p.y * z;
	double y2 = z * p.x - p.z * x;
	double z2 = x * p.y - p.x * y;
	x = x2;
	y = y2;
	x = z2;
	return *this;
}


Cartesian
Cartesian::operator+(const Cartesian &p) const noexcept
{
	return Cartesian(x + p.x, y + p.y, z + p.z);
}


Cartesian&
Cartesian::operator+=(const Cartesian &p) noexcept
{
	x += p.x;
	y += p.y;
	z += p.z;
	return *this;
}


Cartesian
Cartesian::operator-(const Cartesian &p) const noexcept
{
	return Cartesian(x - p.x, y - p.y, z - p.z);
}


Cartesian&
Cartesian::operator-=(const Cartesian &p) noexcept
{
	x -= p.x;
	y -= p.y;
	z -= p.z;
	return *this;
}


/*
 * Convert decimal format of lat and lon to Degrees Minutes Seconds
 * Return a string with "lat  lon  height".
 */
std::string
Cartesian::Decimal2Degrees() const
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
	if (lat < 0) {
		direction = "''S";
		dlat = - 1 * dlat;
		mlat = - 1 * mlat;
		slat = - 1 * slat;
	} else {
		direction = "''N";
	}

	std::string res = std::to_string(dlat) + "°" + std::to_string(mlat) + "'" + std::to_string(slat) + direction;
	if (lon < 0) {
		direction = "''W";
		dlon = - 1 * dlon;
		mlon = - 1 * mlon;
		slon = - 1 * slon;
	} else {
		direction = "''E";
	}
	res += "  " + std::to_string(dlon) + "°" + std::to_string(mlon) + "'" + std::to_string(slon) + direction + "  " + std::to_string(height);

	return res;
}


/*
 * Converts (geocentric) cartesian (x, y, z) to (ellipsoidal geodetic) latitude / longitude coordinates.
 * Modified lat and lon in degrees, height in meters.
 */
void
Cartesian::toGeodetic(double &lat, double &lon, double &height) const
{
	lon = std::atan2(y, x);
	double p = std::sqrt(x * x + y * y);

	ellipsoid_t ellipsoid = ellipsoids[datums[datum].ellipsoid];

	double a = ellipsoid.major_axis;
	double e2 = ellipsoid.e2;

	lat = std::atan2(z, p * (1 - e2));
	double sin_lat = std::sin(lat);
	double v = a / std::sqrt(1 - e2 * sin_lat * sin_lat);
	double diff = 1;
	while (diff < -DBL_TOLERANCE || diff > DBL_TOLERANCE) {
		double lat2 = std::atan2(z + e2 * v * sin_lat, p);
		diff = lat - lat2;
		lat = lat2;
		sin_lat = std::sin(lat);
		v = a / std::sqrt(1 - e2 * sin_lat * sin_lat);
	}

	height = (p / std::cos(lat)) - v;
	lat = lat * DEG_PER_RAD;
	lon = lon * DEG_PER_RAD;
}


void
Cartesian::normalize()
{
	double sum = std::sqrt(x * x + y * y + z * z);
	x /= sum;
	y /= sum;
	z /= sum;
}


void
Cartesian::inverse() noexcept
{
	x = -x;
	y = -y;
	z = -z;
}


double
Cartesian::norm() const
{
	return std::sqrt(x * x + y * y + z * z);
}


std::string
Cartesian::as_string() const
{
	return "SRID = " + std::to_string(SRID) + "\n(" + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + ")";
}


bool
Cartesian::is_SRID_supported(int _SRID)
{
	return SRIDS_DATUMS.find(_SRID) != SRIDS_DATUMS.end();
}
