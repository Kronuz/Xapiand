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

#ifndef XAPIAND_INCLUDED_CARTESIAN_H
#define XAPIAND_INCLUDED_CARTESIAN_H

#include <stdio.h>
#include <math.h>
#include <string>
#include <map>
#include <vector>
#include "exception.h"

// Code of ellipsoids were obtained of
// http://earth-info.nga.mil/GandG/coordsys/datums/ellips.txt
// Ellipsoids
#define WE 0
#define RF 1
#define AA 2
#define AM 3
#define IN 4
#define BR 5
#define HE 6
#define AN 7
#define CC 8
#define SA 9
#define KA 10
#define WD 11

// This SRIDs were obtained of http://www.epsg.org/, However we can use differents datums.
// The datums used were obtained of
// http://earth-info.nga.mil/GandG/coordsys/datums/NATO_DT.pdf
// CRS  SRID
#define WGS84   4326 // Cartesian uses this Coordinate Reference System (CRS).
#define WGS72   4322
#define NAD83   4269
#define NAD27   4267
#define OSGB36  4277
#define TM75    4300
#define TM65    4299
#define ED79    4668
#define ED50    4230
#define TOYA    4301
#define DHDN    4314
#define OEG     4229
#define AGD84   4203
#define SAD69   4618
#define PUL42   4178
#define MGI1901 3906
#define GGRS87  4121

// Double tolerance.
const double DBL_TOLERANCE = 1e-15;

// Constant used for converting degrees to radians and back
const double RAD_PER_DEG = 0.01745329251994329576923691;
const double DEG_PER_RAD = 57.2957795130823208767981548;

// Constant used to verify the range of latitude.
const double PI_HALF = 1.57079632679489661923132169;


// The formulas used for the conversions were obtained from:
// "A guide to coordinate systems in Great Britain".
class Cartesian {

	// The simple geometric shape which most closely approximates the shape of the Earth is a
	// biaxial ellipsoid.
	typedef struct ellipsoid_s {
		std::string name;
		double major_axis;
		double minor_axis;
		double e2; //eccentricity squared = 2f - f^2
	} ellipsoid_t;

	typedef struct datum_s {
		std::string name; // Datum name
		int ellipsoid; // Ellipsoid used
		double tx; // meters
		double ty; // meters
		double tz; // meters
		double rx; // radians
		double ry; // radians
		double rz; // radians
		double s;  // scale factor s / 1E6
	} datum_t;


	std::map<int, int> create_map() {
		std::map<int, int> m;
		m.insert(std::pair<int, int>(WGS84,    0));
		m.insert(std::pair<int, int>(WGS72,    1));
		m.insert(std::pair<int, int>(NAD83,    2));
		m.insert(std::pair<int, int>(NAD27,    3));
		m.insert(std::pair<int, int>(OSGB36,   4));
		m.insert(std::pair<int, int>(TM75,     5));
		m.insert(std::pair<int, int>(TM65,     6));
		m.insert(std::pair<int, int>(ED79,     7));
		m.insert(std::pair<int, int>(ED50,     8));
		m.insert(std::pair<int, int>(TOYA,     9));
		m.insert(std::pair<int, int>(DHDN,    10));
		m.insert(std::pair<int, int>(OEG,     11));
		m.insert(std::pair<int, int>(AGD84,   12));
		m.insert(std::pair<int, int>(SAD69,   13));
		m.insert(std::pair<int, int>(PUL42,   14));
		m.insert(std::pair<int, int>(MGI1901, 15));
		m.insert(std::pair<int, int>(GGRS87,  16));
		return m;
	}

	std::map<int, int> SRIDS_DATUMS = create_map();

	// More ellipsoids available in:
	// http://earth-info.nga.mil/GandG/coordsys/datums/ellips.txt
	// http://icvficheros.icv.gva.es/ICV/geova/erva/Utilidades/jornada_ETRS89/1_ANTECEDENTES_IGN.pdf
	// http://www.geocachingtoolbox.com/?page=datumEllipsoidDetails
	ellipsoid_t ellipsoids[12] = {
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

	// Datums: with associated ellipsoid and Helmert transform parameters to convert given CRS
	// to WGS84 CRS.
	//
	// More are available from:
	// http://earth-info.nga.mil/GandG/coordsys/datums/NATO_DT.pdfs
	// http://georepository.com/search/by-name/?query=&include_world=on
	datum_t datums[17] = {
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

	public:
		enum LatLongUnits {
			RADIANS,
			DEGREES
		};

		Cartesian(double lat, double lon, double height, LatLongUnits units, int SRID);
		Cartesian(double lat, double lon, double height, LatLongUnits units);

		// Dot product
		double operator*(const Cartesian &p) const;
		// Vector product
		Cartesian operator^(const Cartesian &p) const;
		Cartesian operator+(const Cartesian &p) const;
		Cartesian operator-(const Cartesian &p) const;
		void toGeodetic(double &lat, double &lon, double &height);
		bool operator==(const Cartesian &p) const;
		Cartesian& operator=(const Cartesian &p);
		void normalize();
		double norm();
		Cartesian get_inverse();
		std::string as_string() const;
		char* c_str() const;
		double getX() const;
		double getY() const;
		double getZ() const;
		int getSRID() const;
		int getDatum() const;
		// tan(y / x)
		double atan2(double y, double x);
		std::string Decimal2Degrees();

	private:
		double x;
		double y;
		double z;
		int SRID;
		int datum;

		Cartesian(double x, double y, double z);

		void transform2WGS84();
		void toCartesian(double lat, double lon, double height, LatLongUnits units);
};


#endif /* XAPIAND_INCLUDED_CARTESIAN_H */