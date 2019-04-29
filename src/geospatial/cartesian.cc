/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "cartesian.h"

#include <cmath>
#include <unordered_map>
#include <vector>

#include "strings.hh"


/*
 * More ellipsoids available in:
 * http://earth-info.nga.mil/GandG/coordsys/datums/ellips.txt
 * http://icvficheros.icv.gva.es/ICV/geova/erva/Utilidades/jornada_ETRS89/1_ANTECEDENTES_IGN.pdf
 * http://www.geocachingtoolbox.com/?page=datumEllipsoidDetails
 */
static const ellipsoid_t ellipsoids[12] = {
	// Used by GPS and default in this aplication.
	{ "World Geodetic System 1984 (WE)",     6378137,     6356752.314245179, 0.00669437999014131699613723 },
	{ "Geodetic Reference System 1980 (RF)", 6378137,     6356752.314140356, 0.00669438002290078762535911 },
	{ "Airy 1830 (AA)",                      6377563.396, 6356256.909237285, 0.00667053999998536347457648 },
	{ "Modified Airy (AM)",                  6377340.189, 6356034.447938534, 0.00667053999998536347457648 },
	// Hayford 1909.
	{ "International 1924 (IN)",             6378388,     6356911.946127946, 0.00672267002233332199662165 },
	{ "Bessel 1841 (BR)",                    6377397.155, 6356078.962818188, 0.00667437223180214468008836 },
	{ "Helmert 1906 (HE)",                   6378200,     6356818.169627891, 0.00669342162296594322796213 },
	{ "Australian National (AN)",            6378160,     6356774.719195305, 0.00669454185458763715976614 },
	// The most used in Mexico. http://www.inegi.org.mx/inegi/SPC/doc/internet/Sistema_de_Coordenadas.pdf
	{ "Clarke 1866 (CC)",                    6378206.4,   6356583.799998980, 0.00676865799760964394479365 },
	// Also called GRS 1967 Modified
	{ "South American 1969 (SA)",            6378160,     6356774.719195305, 0.00669454185458763715976614 },
	{ "Krassovsky 1940 (KA)",                6378245,     6356863.018773047, 0.00669342162296594322796213 },
	{ "Worl Geodetic System 1972 (WD)",      6378135,     6356750.520016093, 0.00669431777826672197122802 }
};


/*
 * Datums: with associated ellipsoid and Helmert transform parameters to convert given CRS
 * to WGS84 CRS.
 *
 * More are available from:
 * http://earth-info.nga.mil/GandG/coordsys/datums/NATO_DT.pdfs
 * http://georepository.com/search/by-name/?query=&include_world=on
 */
static const std::unordered_map<int, datum_t> map_datums({
	// World Geodetic System 1984 (WGS84)
	// EPSG_SRID: 4326
	// Code NATO: WGE
	// Code Ellip: WE
	{ WGS84, { "World Geodetic System 1984 (WGS84)", ellipsoids[0], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } },
	// World Geodetic System 1972
	// EPSG_SRID: 4322
	// Code NATO: WGC-7
	// Code Ellip: WD
	{ WGS72, { "Worl Geodetic System 1972 (WGS72)", ellipsoids[11], 0.0, 0.0, 4.5, 0.0, 0.0, (0.554 / 3600.0) * RAD_PER_DEG, 0.219 / 1E6} },
	// North American Datum 1983 USA - Hawaii - main islands
	// EPSG_SRID: 4269
	// Code NATO: NAR(H)
	// Code Ellip: RF
	{ NAD83, { "North American Datum 1983 US - Hawaii (NAD83)", ellipsoids[1], 1, 1, -1, 0.0, 0.0, 0.0, 0.0 } },
	// NORTH AMERICAN 1927 USA - CONUS - onshore
	// EPSG_SRID: 4267
	// Code NATO: NAS(C)
	// Code Ellip: CC
	{ NAD27, { "North American 1927 US-CONUS (NAD27)", ellipsoids[8], -8, 160, 176, 0.0, 0.0, 0.0, 0.0 } },
	// Ordnance Survey Great Britain 1936 - UK - Great Britain; Isle of Man
	// EPSG_SRID: 4277
	// Code NATO: OGB-7
	// Code Ellip: AA
	{ OSGB36, { "Ordnance Survey Great Britain 1936 (OSGB36)", ellipsoids[2], 446.448, -125.157, 542.06, (0.150 / 3600.0) * RAD_PER_DEG, (0.247 / 3600.0) * RAD_PER_DEG, (0.8421 / 3600.0) * RAD_PER_DEG, -20.4894 / 1E6 } },
	// IRELAND 1975, Europe - Ireland (Republic and Ulster) - onshore
	// EPSG_SRID: 4300
	// Code Ellip: AM
	{ TM75, { "Ireland 1975 (TM75)", ellipsoids[3], 482.5, -130.6, 564.6, (-1.042 / 3600.0) * RAD_PER_DEG, (-0.214 / 3600.0) * RAD_PER_DEG, (-0.631 / 3600.0) * RAD_PER_DEG, 8.150 / 1E6 } },
	// IRELAND 1965, Europe - Ireland (Republic and Ulster) - onshore
	// EPSG_SRID: 4299
	// Code NATO: IRL-7
	// Code Ellip: AM
	{ TM65, { "Ireland 1965 (TM65)", ellipsoids[3], 482.530, -130.596, 564.557, (-1.042 / 3600.0) * RAD_PER_DEG, (-0.214 / 3600.0) * RAD_PER_DEG, (-0.631 / 3600.0) * RAD_PER_DEG, 8.150 / 1E6 } },
	// European Datum 1979 (ED79), Europe - west
	// EPSG_SRID: 4668
	// Code Ellip: IN
	// http://georepository.com/transformation_15752/ED79-to-WGS-84-1.html
	{ ED79, { "European Datum 1979 (ED79)", ellipsoids[4], -86, -98, -119, 0.0, 0.0, 0.0, 0.0 } },
	// European Datum 1950, Europe - west (DMA ED50 mean)
	// EPSG_SRID: 4230
	// Code NATO: EUR(M)
	// Code Ellip: IN
	// http://georepository.com/transformation_1133/ED50-to-WGS-84-1.html
	{ ED50, { "European Datum 1950 (ED50)", ellipsoids[4], -87, -98, -121, 0.0, 0.0, 0.0, 0.0 } },
	// Tokyo Japan, Asia - Japan and South Korea
	// EPSG_SRID: 4301
	// Code NATO: TOY(A)
	// Code Ellip: BR
	{ TOYA, { "Tokyo Japan (TOYA)", ellipsoids[5], -148, 507, 685, 0.0, 0.0, 0.0, 0.0 } },
	// DHDN (RAUENBERG), Germany - West Germany all states
	// EPSG_SRID: 4314
	// Code NATO: RAU-7
	// Code Ellip: BR
	{ DHDN, { "Deutsches Hauptdreiecksnetz (DHDN)", ellipsoids[5], 582, 105, 414, (1.04 / 3600.0) * RAD_PER_DEG, (0.35 / 3600.0) * RAD_PER_DEG, (-3.08 / 3600.0) * RAD_PER_DEG, 8.3 /1E6 } },
	// OLD EGYPTIAN 1907 - Egypt.
	// EPSG_SRID: 4229
	// Code NATO: OEG
	// Code Ellip: HE
	{ OEG, { "Egypt 1907 (OEG)", ellipsoids[6], -130, 110, -13, 0.0, 0.0, 0.0, 0.0 } },
	// AUSTRALIAN GEODETIC 1984, Australia - all states
	// EPSG_SRID: 4203
	// Code NATO: AUG-7
	// Code Ellip: AN
	{ AGD84, { "Australian Geodetic 1984 (AGD84)", ellipsoids[7], -116, -50.47, 141.69, (0.23 / 3600.0) * RAD_PER_DEG, (0.39 / 3600.0) * RAD_PER_DEG, (0.344 / 3600.0) * RAD_PER_DEG, 0.0983 / 1E6 } },
	// SOUTH AMERICAN 1969 - South America - SAD69 by country
	// EPSG_SRID: 4618
	// Code NATO: SAN(M)
	// Code Ellip: SA
	{ SAD69, { "South American 1969 (SAD69)", ellipsoids[9], -57, 1, -41, 0.0, 0.0, 0.0, 0.0 } },
	// PULKOVO 1942 - Germany - East Germany all states
	// EPSG_SRID: 4178
	// Code NATO: PUK-7
	// Code Ellip: KA
	{ PUL42, { "Pulkovo 1942 (PUL42)", ellipsoids[10], 21.58719, -97.541, -60.925, (1.01378 / 3600.0) * RAD_PER_DEG, (0.58117 / 3600.0) * RAD_PER_DEG, (0.2348 / 3600.0) * RAD_PER_DEG, -4.6121 / 1E6 } },
	// HERMANNSKOGEL, Former Yugoslavia.
	// EPSG_SRID: 3906
	// Code NATO: HER-7
	// Code Ellip: BR
	{ MGI1901, { "MGI 1901 (MGI1901)", ellipsoids[5], 515.149, 186.233, 511.959, (5.49721 / 3600.0) * RAD_PER_DEG, (3.51742 / 3600.0) * RAD_PER_DEG, (-12.948 / 3600.0) * RAD_PER_DEG, 0.782 / 1E6 } },
	// GGRS87, Greece
	// EPSG_SRID: 4121
	// Code NATO: GRX
	// Code Ellip: RF
	{ GGRS87, { "GGRS87", ellipsoids[1], -199.87, 74.79, 246.62, 0.0, 0.0, 0.0, 0.0  }}
});


// Cartesian with Lat: 0, Lon: 0, height: 0.
Cartesian::Cartesian()
	: SRID(WGS84),
	  x(map_datums.at(SRID).ellipsoid.major_axis),
	  y(0.0),
	  z(0.0) { }


/*
 * Constructor receives latitude, longitude and their units on specific CRS
 * which are converted to cartesian coordinates, and then they are converted to CRS WGS84.
 */
Cartesian::Cartesian(double lat, double lon, double height, Units units, int _SRID)
	: SRID(_SRID)
{
	toCartesian(lat, lon, height, units);
	if (SRID != WGS84) {
		toWGS84();
	}
}


/*
 * Constructor receives a cartesian coordinate.
 * This constructor receive a cartesian coordinate,
 * which was obtained from WGS84 CRS.
 */
Cartesian::Cartesian(double _x, double _y, double _z, int _SRID)
	: SRID(_SRID),
	  x(_x),
	  y(_y),
	  z(_z)
{
	if (SRID != WGS84) {
		toWGS84();
	}
}


bool
Cartesian::operator==(const Cartesian& p) const noexcept
{
	return x == p.x && y == p.y && z == p.z && SRID == p.SRID;
}


bool
Cartesian::operator!=(const Cartesian& p) const noexcept
{
	return !operator==(p);
}


bool
Cartesian::operator<(const Cartesian& p) const noexcept
{
	return y < p.y || (y == p.y && x < p.x) || (y == p.y && x == p.x && z < p.z);
}


bool
Cartesian::operator>(const Cartesian& p) const noexcept
{
	return y > p.y || (y == p.y && x > p.x) || (y == p.y && x == p.x && z > p.z);
}


double
Cartesian::operator*(const Cartesian& p) const noexcept
{
	return x * p.x + y * p.y + z * p.z;
}


Cartesian&
Cartesian::operator*(double _scale) noexcept
{
	x *= _scale;
	y *= _scale;
	z *= _scale;
	return *this;
}


Cartesian
Cartesian::operator^(const Cartesian& p) const noexcept
{
	return {y * p.z - p.y * z, z * p.x - p.z * x, x * p.y - p.x * y};
}


Cartesian&
Cartesian::operator^=(const Cartesian& p) noexcept
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
Cartesian::operator+(const Cartesian& p) const noexcept
{
	return {x + p.x, y + p.y, z + p.z};
}


Cartesian&
Cartesian::operator+=(const Cartesian& p) noexcept
{
	x += p.x;
	y += p.y;
	z += p.z;
	return *this;
}


Cartesian
Cartesian::operator-(const Cartesian& p) const noexcept
{
	return {x - p.x, y - p.y, z - p.z};
}


Cartesian&
Cartesian::operator-=(const Cartesian& p) noexcept
{
	x -= p.x;
	y -= p.y;
	z -= p.z;
	return *this;
}


/*
 * Converts (geocentric) cartesian (x, y, z) with any datum to WGS84 datum.
 *
 * Applies 7 - Helmert transformation to this point using the datum parameters.
 */
void
Cartesian::toWGS84()
{
	try {
		const auto& datum = map_datums.at(SRID);
		double s_1 = datum.s + 1;

		// Apply transform.
		double x2 = datum.tx + s_1 * (x - datum.rz * y + datum.ry * z);
		double y2 = datum.ty + s_1 * (datum.rz * x + y - datum.rx * z);
		double z2 = datum.tz + s_1 * (- datum.ry * x + datum.rx * y + z);

		x = x2;
		y = y2;
		z = z2;

		SRID = WGS84;
	} catch (const std::out_of_range&) {
		THROW(CartesianError, "SRID = {} is not supported", SRID);
	}
}


/*
 * Converts this coordinate from (geodetic) latitude / longitude coordinates to
 * (geocentric) cartesian (x, y, z) coordinates on the CRS specified by SRID.
 *
 * Reference: Conversion between Cartesian and geodetic coordinates
 *   on a rotational ellipsoid by solving a system of nonlinear equations.
 *   http://www.iag-aig.org/attach/989c8e501d9c5b5e2736955baf2632f5/V60N2_5FT.pdf
 *
 * Function receives latitude, longitude, ellipsoid height and their units.
 */
void
Cartesian::toCartesian(double lat, double lon, double height, Units units)
{
	try {
		const auto& datum = map_datums.at(SRID);
		// If lat and lon are in degrees convert to radians.
		if (units == Units::DEGREES) {
			lat = lat * RAD_PER_DEG;
			lon = lon * RAD_PER_DEG;
		}

		if (lat < -PI_HALF || lat > PI_HALF) {
			THROW(CartesianError, "Latitude out-of-range");
		}

		double a = datum.ellipsoid.major_axis;
		double e2 = datum.ellipsoid.e2;

		double cos_lat = std::cos(lat);
		double sin_lat = std::sin(lat);
		// Radius of curvature in the prime vertical.
		double N = a / std::sqrt(1 - e2 * sin_lat * sin_lat);

		x = (N + height) * cos_lat * std::cos(lon);
		y = (N + height) * cos_lat * std::sin(lon);
		z = ((1 - e2) * N + height) * sin_lat;
	} catch (const std::out_of_range&) {
		THROW(CartesianError, "SRID = {} is not supported", SRID);
	}
}


/*
 * Converts (geocentric) cartesian (x, y, z) to (ellipsoidal geodetic) latitude, longitude, height coordinates.
 * Sets lat and lon in degrees, height in meters.
 *
 * Reference:
 *   + A COMPARISON OF METHODS USED IN RECTANGULAR TO GEODETIC COORDINATE TRANSFORMATIONS.
 *     http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.139.7504&rep=rep1&type=pdf
 *   + Conversion between Cartesian and geodetic coordinates on a rotational ellipsoid
 *     by solving a system of nonlinear equations
 *     http://www.iag-aig.org/attach/989c8e501d9c5b5e2736955baf2632f5/V60N2_5FT.pdf
 *
 * Used method: Lin and Wang (1995)
 */
std::tuple<double, double, double>
Cartesian::toGeodetic() const
{
	const auto& datum = map_datums.at(SRID);

	double _x = scale * x;
	double _y = scale * y;
	double _z = scale * z;
	double p2 = _x * _x + _y * _y;
	// Distance from the polar axis to the point.
	double p = std::sqrt(p2);
	double z2 = _z * _z;
	double a = datum.ellipsoid.major_axis;
	double b = datum.ellipsoid.minor_axis;

	double a2 = a * a;
	double b2 = b * b;
	double aux = a2 * z2 + b2 * p2;
	double m0 = (a * b * aux * std::sqrt(aux) - a2 * b2 * aux) / (2 * (a2 * a2 * z2 + b2 * b2 * p2));
	double dm = 2 * m0;
	for (int itr = 0; itr < 10; ++itr) {
		double f_a = a + dm / a, f_b = b + dm / b;
		double f_a2 = f_a * f_a, f_b2 = f_b * f_b;
		double fm = p2 / f_a2 + z2 / f_b2 - 1;
		double dfm = - 4 * (p2 / (a * f_a2 * f_a) + z2 / (b * f_b2 * f_b));
		double h = fm / dfm;
		if (h > -DBL_TOLERANCE && h < DBL_TOLERANCE) {
			break;
		}
		m0 = m0 - h;
		dm = 2 * m0;
	}

	double pe = p / (1 + dm / a2);
	double ze = _z / (1 + dm / b2);

	double lat = std::atan2(a2 * ze, b2 * pe) * DEG_PER_RAD;
	double lon = 2 * std::atan2(_y, _x + p) * DEG_PER_RAD;
	double height = std::sqrt(std::pow(pe - p, 2) + std::pow(ze - _z, 2));
	if ((p + std::abs(_z)) < (pe + std::abs(ze))) {
		height = - height;
	}

	return std::make_tuple(lat, lon, height);
}


/*
 * Converts (geocentric) cartesian (x, y, z) to (ellipsoidal geodetic) latitude / longitude coordinates.
 * Sets lat and lon in degrees.
 *
 * Reference:
 *   + A COMPARISON OF METHODS USED IN RECTANGULAR TO GEODETIC COORDINATE TRANSFORMATIONS.
 *     http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.139.7504&rep=rep1&type=pdf
 *   + Conversion between Cartesian and geodetic coordinates on a rotational ellipsoid
 *     by solving a system of nonlinear equations
 *     http://www.iag-aig.org/attach/989c8e501d9c5b5e2736955baf2632f5/V60N2_5FT.pdf
 *
 * Used method: Lin and Wang (1995)
 */
std::pair<double, double>
Cartesian::toLatLon() const
{
	const auto& datum = map_datums.at(SRID);

	double _x = scale * x;
	double _y = scale * y;
	double _z = scale * z;
	double p2 = _x * _x + _y * _y;
	// Distance from the polar axis to the point.
	double p = std::sqrt(p2);
	double z2 = _z * _z;
	double a = datum.ellipsoid.major_axis;
	double b = datum.ellipsoid.minor_axis;

	double a2 = a * a;
	double b2 = b * b;
	double aux = a2 * z2 + b2 * p2;
	double m0 = (a * b * aux * std::sqrt(aux) - a2 * b2 * aux) / (2 * (a2 * a2 * z2 + b2 * b2 * p2));
	double dm = 2 * m0;
	for (int itr = 0; itr < 10; ++itr) {
		double f_a = a + dm / a, f_b = b + dm / b;
		double f_a2 = f_a * f_a, f_b2 = f_b * f_b;
		double fm = p2 / f_a2 + z2 / f_b2 - 1;
		double dfm = - 4 * (p2 / (a * f_a2 * f_a) + z2 / (b * f_b2 * f_b));
		double h = fm / dfm;
		if (h > -DBL_TOLERANCE && h < DBL_TOLERANCE) {
			break;
		}
		m0 = m0 - h;
		dm = 2 * m0;
	}

	double pe = p / (1 + dm / a2);
	double ze = _z / (1 + dm / b2);

	double lat = std::atan2(a2 * ze, b2 * pe) * DEG_PER_RAD;
	double lon = 2 * std::atan2(_y, _x + p) * DEG_PER_RAD;

	return std::make_pair(lat, lon);
}


/*
 * Converts (geocentric) cartesian (x, y, z) to Degrees Minutes Seconds.
 */
std::string
Cartesian::toDegMinSec() const
{
	const auto geodetic = toGeodetic();
	auto dlat = (int)std::get<0>(geodetic);
	auto dlon = (int)std::get<1>(geodetic);
	auto mlat = (int)((std::get<0>(geodetic) - dlat) * 60.0);
	auto mlon = (int)((std::get<1>(geodetic) - dlon) * 60.0);
	double slat = (std::get<0>(geodetic) - dlat - mlat / 60.0) * 3600.0;
	double slon = (std::get<1>(geodetic) - dlon - mlon / 60.0) * 3600.0;

	std::string direction;
	if (std::get<0>(geodetic) < 0) {
		direction.assign("''S");
		dlat = - 1 * dlat;
		mlat = - 1 * mlat;
		slat = - 1 * slat;
	} else {
		direction.assign("''N");
	}

	std::string res;
	res.reserve(60);
	res.append(std::to_string(dlat)).append("°");
	res.append(std::to_string(mlat)).append("'");
	res.append(std::to_string(slat)).append(direction);
	if (std::get<1>(geodetic) < 0) {
		direction.assign("''W");
		dlon = - 1 * dlon;
		mlon = - 1 * mlon;
		slon = - 1 * slon;
	} else {
		direction.assign("''E");
	}
	res.append("  ").append(std::to_string(dlon)).append("°");
	res.append(std::to_string(mlon)).append("'");
	res.append(std::to_string(slon)).append(direction).append("  ").append(std::to_string(std::get<2>(geodetic)));

	return res;
}


Cartesian&
Cartesian::normalize()
{
	scale = norm();
	if (scale < DBL_TOLERANCE) {
		THROW(CartesianError, "Norm is zero ({})", scale);
	}
	x /= scale;
	y /= scale;
	z /= scale;
	return *this;
}


Cartesian&
Cartesian::inverse() noexcept
{
	x = -x;
	y = -y;
	z = -z;
	return *this;
}


double
Cartesian::norm() const
{
	return std::sqrt(x * x + y * y + z * z);
}


std::string
Cartesian::to_string() const
{
	return strings::format("{} ({:.6} {:.6} {:.6})", DEFAULT_CRS, x * scale, y * scale, z * scale);
}


bool
Cartesian::is_SRID_supported(int _SRID)
{
	return map_datums.find(_SRID) != map_datums.end();
}
