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

#include "test_geospatial.h"

#include "../src/geospatial/ewkt.h"
#include "../src/log.h"
#include "../src/fs.hh"
#include "utils.h"


const std::string path_test_geospatial = std::string(FIXTURES_PATH) + "/examples/geospatial/";
const std::string python_geospatial = "python_files/geospatial/";


constexpr bool partials = true;
constexpr double error = HTM_MIN_ERROR;


/*
 * Testing the transformation of coordinates between CRS.
 */
int testCartesianTransforms() {
	INIT_LOG
	struct test_transform_t {
		// Source CRS.
		int SRID;
		double lat;
		double lon;
		double height;
		// Target CRS.
		std::string res;
	};

	std::vector<test_transform_t> SRID_2_WGS84;
	// WGS72 to WGS84  (4322 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1238
	SRID_2_WGS84.push_back({ 4322,  20.0,  10.0, 30.0, "20°0'0.141702''N  10°0'0.554000''E  30.959384"    });
	SRID_2_WGS84.push_back({ 4322,  20.0, -10.0, 30.0, "20°0'0.141702''N  9°59'59.446000''W  30.959384"   });
	SRID_2_WGS84.push_back({ 4322, -20.0,  10.0, 30.0, "19°59'59.866682''S  10°0'0.554000''E  27.881203"  });
	SRID_2_WGS84.push_back({ 4322, -20.0, -10.0, 30.0, "19°59'59.866682''S  9°59'59.446000''W  27.881203" });

	// NAD83 to WGS84  (4269 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1252
	SRID_2_WGS84.push_back({ 4269,  20.0,  10.0, 30.0, "19°59'59.956556''N  10°0'0.027905''E  30.746560"  });
	SRID_2_WGS84.push_back({ 4269,  20.0, -10.0, 30.0, "19°59'59.960418''N  9°59'59.960148''W  30.420209" });
	SRID_2_WGS84.push_back({ 4269, -20.0,  10.0, 30.0, "20°0'0.017671''S  10°0'0.027905''E  31.430600"    });
	SRID_2_WGS84.push_back({ 4269, -20.0, -10.0, 30.0, "20°0'0.021534''S  9°59'59.960148''W  31.104249"   });

	// NAD27 to WGS84  (4267 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1173
	SRID_2_WGS84.push_back({ 4267,  20.0,  10.0, 30.0, "20°0'0.196545''N  10°0'5.468256''E  150.554523"    });
	SRID_2_WGS84.push_back({ 4267,  20.0, -10.0, 30.0, "20°0'0.814568''N  9°59'54.627272''W  98.338209"    });
	SRID_2_WGS84.push_back({ 4267, -20.0,  10.0, 30.0, "19°59'49.440208''S  10°0'5.468256''E  30.171742"   });
	SRID_2_WGS84.push_back({ 4267, -20.0, -10.0, 30.0, "19°59'50.058155''S  9°59'54.627272''W  -22.045563" });

	// OSGB36 to WGS84  (4277 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1314
	SRID_2_WGS84.push_back({ 4277,  20.0,  10.0, 30.0, "20°0'13.337317''N  9°59'53.865759''E  -86.980683"   });
	SRID_2_WGS84.push_back({ 4277,  20.0, -10.0, 30.0, "20°0'12.801456''N  10°0'0.769107''W  -46.142419"    });
	SRID_2_WGS84.push_back({ 4277, -20.0,  10.0, 30.0, "19°59'40.643875''S  9°59'54.003573''E  -457.728199" });
	SRID_2_WGS84.push_back({ 4277, -20.0, -10.0, 30.0, "19°59'40.212914''S  10°0'0.693312''W  -416.880621"  });

	// TM75 to WGS84  (4300 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1954
	SRID_2_WGS84.push_back({ 4300,  20.0,  10.0, 30.0, "20°0'13.892799''N  9°59'52.446296''E  -87.320347"   });
	SRID_2_WGS84.push_back({ 4300,  20.0, -10.0, 30.0, "20°0'13.751990''N  10°0'1.815691''W  -44.678652"    });
	SRID_2_WGS84.push_back({ 4300, -20.0,  10.0, 30.0, "19°59'39.325125''S  9°59'51.677477''E  -473.515164" });
	SRID_2_WGS84.push_back({ 4300, -20.0, -10.0, 30.0, "19°59'38.457075''S  10°0'2.530766''W  -430.919043"  });

	// TM65 to WGS84  (4299 to 4326) -> The results are very close to those obtained in the page:
	// http://www.geocachingtoolbox.com/index.php?lang=en&page=coordinateConversion&status=result
	SRID_2_WGS84.push_back({ 4299,  20.0,  10.0, 30.0, "20°0'13.891148''N  9°59'52.446252''E  -87.306642"   });
	SRID_2_WGS84.push_back({ 4299,  20.0, -10.0, 30.0, "20°0'13.750355''N  10°0'1.815376''W  -44.666252"    });
	SRID_2_WGS84.push_back({ 4299, -20.0,  10.0, 30.0, "19°59'39.326103''S  9°59'51.677433''E  -473.472045" });
	SRID_2_WGS84.push_back({ 4299, -20.0, -10.0, 30.0, "19°59'38.458068''S  10°0'2.530451''W  -430.877230"  });

	// ED79 to WGS84  (4668 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/15752
	SRID_2_WGS84.push_back({ 4668,  20.0,  10.0, 30.0, "19°59'55.589986''N  9°59'57.193708''E  134.068052" });
	SRID_2_WGS84.push_back({ 4668,  20.0, -10.0, 30.0, "19°59'55.211469''N  10°0'3.833722''W  166.051242"  });
	SRID_2_WGS84.push_back({ 4668, -20.0,  10.0, 30.0, "20°0'2.862582''S  9°59'57.193708''E  215.468007"   });
	SRID_2_WGS84.push_back({ 4668, -20.0, -10.0, 30.0, "20°0'2.484033''S  10°0'3.833722''W  247.450787"    });

	// ED50 to WGS84  (4230 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1133
	SRID_2_WGS84.push_back({ 4230,  20.0,  10.0, 30.0, "19°59'55.539823''N  9°59'57.199681''E  132.458626" });
	SRID_2_WGS84.push_back({ 4230,  20.0, -10.0, 30.0, "19°59'55.161306''N  10°0'3.839696''W  164.441824"  });
	SRID_2_WGS84.push_back({ 4230, -20.0,  10.0, 30.0, "20°0'2.934649''S  9°59'57.199681''E  215.226660"   });
	SRID_2_WGS84.push_back({ 4230, -20.0, -10.0, 30.0, "20°0'2.556100''S  10°0'3.839696''W  247.209441"    });

	// TOYA to WGS84  (4301 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1230
	SRID_2_WGS84.push_back({ 4301,  20.0,  10.0, 30.0, "20°0'22.962090''N  10°0'18.062821''E  -521.976076"   });
	SRID_2_WGS84.push_back({ 4301,  20.0, -10.0, 30.0, "20°0'24.921332''N  9°59'43.705140''W  -687.433480"   });
	SRID_2_WGS84.push_back({ 4301, -20.0,  10.0, 30.0, "19°59'41.092892''S  10°0'18.062821''E  -990.556329"  });
	SRID_2_WGS84.push_back({ 4301, -20.0, -10.0, 30.0, "19°59'43.051188''S  9°59'43.705140''W  -1156.025959" });

	// DHDN to WGS84  (4314 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1673
	SRID_2_WGS84.push_back({ 4314,  20.0,  10.0, 30.0, "20°0'7.291150''N  9°59'56.608634''E  48.138765"      });
	SRID_2_WGS84.push_back({ 4314,  20.0, -10.0, 30.0, "20°0'7.333754''N  9°59'56.393946''W  13.848005"      });
	SRID_2_WGS84.push_back({ 4314, -20.0,  10.0, 30.0, "19°59'42.318425''S  9°59'57.393082''E  -235.013109"  });
	SRID_2_WGS84.push_back({ 4314, -20.0, -10.0, 30.0, "19°59'43.086952''S  9°59'55.697370''W  -269.257292"  });

	// OEG to WGS84  (4229 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1148
	SRID_2_WGS84.push_back({ 4229,  20.0,  10.0, 30.0, "20°0'0.873728''N  10°0'4.503259''E  -13.466677"  });
	SRID_2_WGS84.push_back({ 4229,  20.0, -10.0, 30.0, "20°0'1.298641''N  9°59'57.049898''W  -49.366075" });
	SRID_2_WGS84.push_back({ 4229, -20.0,  10.0, 30.0, "20°0'1.668233''S  10°0'4.503259''E  -4.574003"   });
	SRID_2_WGS84.push_back({ 4229, -20.0, -10.0, 30.0, "20°0'2.093151''S  9°59'57.049898''W  -40.473350" });

	// AGD84 to WGS84  (4203 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1236
	SRID_2_WGS84.push_back({ 4203,  20.0,  10.0, 30.0, "20°0'5.339442''N  9°59'59.220714''E  -13.586401"    });
	SRID_2_WGS84.push_back({ 4203,  20.0, -10.0, 30.0, "20°0'5.064184''N  10°0'2.116232''W  2.879302"       });
	SRID_2_WGS84.push_back({ 4203, -20.0,  10.0, 30.0, "19°59'57.371712''S  9°59'59.433464''E  -110.463889" });
	SRID_2_WGS84.push_back({ 4203, -20.0, -10.0, 30.0, "19°59'57.257055''S  10°0'2.001422''W  -93.987306"   });

	// SAD69 to WGS84  (4618 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1864
	SRID_2_WGS84.push_back({ 4618,  20.0,  10.0, 30.0, "19°59'59.357117''N  10°0'0.374382''E  -13.677770" });
	SRID_2_WGS84.push_back({ 4618,  20.0, -10.0, 30.0, "19°59'59.360979''N  10°0'0.306624''W  -14.004125" });
	SRID_2_WGS84.push_back({ 4618, -20.0,  10.0, 30.0, "20°0'1.862864''S  10°0'0.374382''E  14.368110"    });
	SRID_2_WGS84.push_back({ 4618, -20.0, -10.0, 30.0, "20°0'1.866726''S  10°0'0.306624''W  14.041756"    });

	// PUL42 to WGS84  (4178 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1334
	SRID_2_WGS84.push_back({ 4178,  20.0,  10.0, 30.0, "19°59'57.750301''N  9°59'56.403911''E  92.107732" });
	SRID_2_WGS84.push_back({ 4178,  20.0, -10.0, 30.0, "19°59'57.019651''N  10°0'3.265190''W  123.917120" });
	SRID_2_WGS84.push_back({ 4178, -20.0,  10.0, 30.0, "20°0'2.270413''S  9°59'57.198773''E  133.835302"  });
	SRID_2_WGS84.push_back({ 4178, -20.0, -10.0, 30.0, "20°0'2.247538''S  10°0'2.616278''W  165.691341"   });

	// MGI1901 to WGS84  (3906 to 4326) -> The results are very close to those obtained in the page:
	// http://www.geocachingtoolbox.com/index.php?lang=en&page=coordinateConversion&status=result
	SRID_2_WGS84.push_back({ 3906,  20.0,  10.0, 30.0, "20°0'8.506072''N  9°59'48.107356''E  -15.039391"    });
	SRID_2_WGS84.push_back({ 3906,  20.0, -10.0, 30.0, "20°0'7.306781''N  10°0'5.296242''W  -75.952463"     });
	SRID_2_WGS84.push_back({ 3906, -20.0,  10.0, 30.0, "19°59'42.260450''S  9°59'52.463078''E  -364.894519" });
	SRID_2_WGS84.push_back({ 3906, -20.0, -10.0, 30.0, "19°59'44.898670''S  10°0'1.823681''W  -425.555326"  });

	// GGRS87 to WGS84  (4121 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1272
	SRID_2_WGS84.push_back({ 4121,  20.0,  10.0, 30.0, "20°0'9.581041''N  10°0'3.727855''E  -58.402327"     });
	SRID_2_WGS84.push_back({ 4121,  20.0, -10.0, 30.0, "20°0'9.869982''N  9°59'58.660140''W  -82.810562"    });
	SRID_2_WGS84.push_back({ 4121, -20.0,  10.0, 30.0, "19°59'54.508366''S  10°0'3.727855''E  -227.104937"  });
	SRID_2_WGS84.push_back({ 4121, -20.0, -10.0, 30.0, "19°59'54.797256''S  9°59'58.660140''W  -251.513821" });

	int cont = 0;

	try {
		for (const auto& test : SRID_2_WGS84) {
			Cartesian c(test.lat, test.lon, test.height, Cartesian::Units::DEGREES, test.SRID);
			const auto deg_min_sec = c.toDegMinSec();
			if (deg_min_sec != test.res) {
				L_ERR("ERROR: Resul: %s  Expected: %s", deg_min_sec, test.res);
				++cont;
			}
		}
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", exc.what());
		++cont;
	}

	if (cont == 0) {
		L_DEBUG("Testing the transformation of coordinates between CRS is correct!");
		RETURN(0);
	} else {
		L_ERR("ERROR: Testing the transformation of coordinates between CRS has mistakes.");
		RETURN(1);
	}
}


/*
 * Testing Graham Scan Algorithm.
 */
int testGrahamScanAlgorithm() {
	INIT_LOG
	int cont = 0;
	std::vector<std::string> tests({ "ColoradoPoly", "Georgia", "Utah" });

	// Make the path for the python files generated.
	build_path_index(python_geospatial + "convex_hull/");

	for (const auto& test : tests) {
		std::string source_file = path_test_geospatial + "convex_hull/" + test + ".txt";
		std::string expected_file = path_test_geospatial + "convex_hull/" + test + "_expect_convex.txt";
		std::ifstream source_points(source_file);
		std::ifstream expected(expected_file);
		if (source_points.is_open() && expected.is_open()) {
			std::vector<Cartesian> points;
			char output[50];
			while (!source_points.eof()) {
				source_points >> output;
				double lat = std::stod(output);
				source_points >> output;
				double lon = std::stod(output);
				Cartesian c(lat, lon, 0, Cartesian::Units::DEGREES);
				points.push_back(std::move(c));
			}

			const auto convex_points = Polygon::ConvexPolygon::graham_scan(std::vector<Cartesian>(points.begin(), points.end()));
			for (const auto& point : convex_points) {
				std::string coord_get;
				coord_get.append(std::to_string(point.x)).push_back(' ');
				coord_get.append(std::to_string(point.y)).push_back(' ');
				coord_get.append(std::to_string(point.z));
				if (!expected.eof()) {
					std::string coord_exp;
					std::getline(expected, coord_exp);
					if (coord_exp != coord_get) {
						++cont;
						L_ERR("ERROR: Result(%s) Expect(%s).", coord_get, coord_exp);
					}
				} else {
					++cont;
					L_ERR("ERROR: Expected less corners.");
					break;
				}
			}

			if (!expected.eof()) {
				++cont;
				L_ERR("ERROR: Expected more corners.");
				break;
			}

			expected.close();
			source_points.close();

			HTM::writeGrahamScanMap(python_geospatial + "convex_hull/" + test + "GM.py", test + "GM.html", points, convex_points, path_test_geospatial);
			HTM::writeGrahamScan3D(python_geospatial + "convex_hull/" + test + "3D.py", points, convex_points);
		} else {
			L_ERR("ERROR: File %s or %s not found.", source_file, expected_file);
			++cont;
		}
	}

	if (cont == 0) {
		L_DEBUG("Testing Geometry Hull Convex is correct!");
		RETURN(0);
	} else {
		L_ERR("ERROR: Testing Geometry Hull Convex has mistakes.");
		RETURN(1);
	}
}


inline Point getPoint() {
	// Catedral Morelia
	return Point(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES));
}


inline MultiPoint getMultiPoint() {
	MultiPoint multipoint;
	// Catedral Morelia
	multipoint.add(Point(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES)));
	// Zoologico Morelia
	multipoint.add(Point(Cartesian(19.684201, -101.194725, 0, Cartesian::Units::DEGREES)));
	// Av. Michoacán
	multipoint.add(Point(Cartesian(19.708061, -101.207265, 0, Cartesian::Units::DEGREES)));
	return multipoint;
}


inline Circle getCircle() {
	return Circle(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES), 1000);
}


inline Convex getConvex() {
	Convex convex;
	convex.add(Circle(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES), 1000));
	convex.add(Circle(Cartesian(19.708061, -101.207265, 0, Cartesian::Units::DEGREES), 1000));
	convex.add(Circle(Cartesian(19.715503, -101.194015, 0, Cartesian::Units::DEGREES), 1000));
	return convex;
}


inline Polygon getPolygon() {
	Polygon polygon(Geometry::Type::POLYGON, std::vector<Cartesian>({
		Cartesian(19.682206, -101.226447, 0, Cartesian::Units::DEGREES),
		Cartesian(19.708061, -101.207265, 0, Cartesian::Units::DEGREES),
		Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES),
		Cartesian(19.684201, -101.194725, 0, Cartesian::Units::DEGREES),
		Cartesian(19.678558, -101.208605, 0, Cartesian::Units::DEGREES),
	}));
	polygon.add(std::vector<Cartesian>({
		Cartesian(19.731249, -101.193327, 0, Cartesian::Units::DEGREES),
		Cartesian(19.660095, -101.213948, 0, Cartesian::Units::DEGREES),
		Cartesian(19.687726, -101.183904, 0, Cartesian::Units::DEGREES),
		Cartesian(19.731249, -101.193327, 0, Cartesian::Units::DEGREES),
	}));
	polygon.add(std::vector<Cartesian>({
		Cartesian(19.692047, -101.217750, 0, Cartesian::Units::DEGREES),
		Cartesian(19.697119, -101.183902, 0, Cartesian::Units::DEGREES),
		Cartesian(19.716354, -101.172173, 0, Cartesian::Units::DEGREES),
		Cartesian(19.692047, -101.217750, 0, Cartesian::Units::DEGREES),
	}));
	return polygon;
}


inline MultiCircle getMultiCircle() {
	MultiCircle multicircle;
	multicircle.add(Circle(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES), 1000));
	multicircle.add(Circle(Cartesian(19.708061, -101.207265, 0, Cartesian::Units::DEGREES), 1000));
	multicircle.add(Circle(Cartesian(19.715503, -101.194015, 0, Cartesian::Units::DEGREES), 1000));
	return multicircle;
}


inline MultiConvex getMultiConvex() {
	MultiConvex multiconvex;
	multiconvex.add(getConvex());

	Convex convex;
	convex.add(Circle(Cartesian(19.721603, -101.225874, 0, Cartesian::Units::DEGREES), 500));
	convex.add(Circle(Cartesian(19.718179, -101.222280, 0, Cartesian::Units::DEGREES), 500));
	convex.add(Circle(Cartesian(19.720820, -101.218673, 0, Cartesian::Units::DEGREES), 500));
	multiconvex.add(std::move(convex));

	return multiconvex;
}


inline MultiPolygon getMultiPolygon() {
	MultiPolygon multipolygon;

	Polygon polygon(Geometry::Type::CHULL, std::vector<Cartesian>({
		Cartesian(19.689145, -101.211355, 0, Cartesian::Units::DEGREES),
		Cartesian(19.682206, -101.226447, 0, Cartesian::Units::DEGREES),
		Cartesian(19.708061, -101.207265, 0, Cartesian::Units::DEGREES),
		Cartesian(19.690554, -101.214786, 0, Cartesian::Units::DEGREES),
		Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES),
		Cartesian(19.684201, -101.194725, 0, Cartesian::Units::DEGREES),
		Cartesian(19.678558, -101.208605, 0, Cartesian::Units::DEGREES),
		Cartesian(19.687163, -101.216246, 0, Cartesian::Units::DEGREES),
	}));
	polygon.add(std::vector<Cartesian>({
		Cartesian(19.689145, -101.211355, 0, Cartesian::Units::DEGREES),
		Cartesian(19.690554, -101.214786, 0, Cartesian::Units::DEGREES),
		Cartesian(19.687163, -101.216246, 0, Cartesian::Units::DEGREES),
		Cartesian(19.685756, -101.220635, 0, Cartesian::Units::DEGREES),
		Cartesian(19.696039, -101.210120, 0, Cartesian::Units::DEGREES),
		Cartesian(19.685132, -101.201934, 0, Cartesian::Units::DEGREES),
	}));
	multipolygon.add(std::move(polygon));

	multipolygon.add(Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({
		Cartesian(19.731249, -101.193327, 0, Cartesian::Units::DEGREES),
		Cartesian(19.660095, -101.213948, 0, Cartesian::Units::DEGREES),
		Cartesian(19.687726, -101.183904, 0, Cartesian::Units::DEGREES),
		Cartesian(19.731249, -101.193327, 0, Cartesian::Units::DEGREES),
	})));

	multipolygon.add(Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({
		Cartesian(19.692047, -101.217750, 0, Cartesian::Units::DEGREES),
		Cartesian(19.697119, -101.183902, 0, Cartesian::Units::DEGREES),
		Cartesian(19.716354, -101.172173, 0, Cartesian::Units::DEGREES),
		Cartesian(19.692047, -101.217750, 0, Cartesian::Units::DEGREES),
	})));

	return multipolygon;
}


inline Collection getCollection() {
	Collection collection;
	collection.add_point(getPoint());
	collection.add_multipoint(getMultiPoint());
	collection.add_circle(getCircle());
	collection.add_polygon(getPolygon());
	collection.add_multicircle(getMultiCircle());

	Intersection intersection;
	intersection.add(std::make_shared<Convex>(getConvex()));
	intersection.add(std::make_shared<MultiPolygon>(getMultiPolygon()));
	collection.add_intersection(std::move(intersection));

	return collection;
}


inline Intersection getIntersection() {
	Intersection intersection;
	intersection.add(std::make_shared<Convex>(getConvex()));

	Collection collection;
	collection.add_circle(getCircle());
	collection.add_polygon(getPolygon());
	intersection.add(std::make_shared<Collection>(std::move(collection)));

	return intersection;
}


inline int verify_trixels_ranges(const std::shared_ptr<Geometry>& geometry, const std::vector<std::string>& trixels, const std::vector<range_t>& ranges) {
	int cont = 0;

	// Test trixels to ranges
	std::vector<range_t> _ranges;
	for (const auto& trixel : trixels) {
		HTM::insertGreaterRange(_ranges, HTM::getRange(trixel));
	}
	if (_ranges != ranges) {
		L_ERR("ERROR: Different ranges [%zu %zu]", ranges.size(), _ranges.size());
		++cont;
	}

	// Test ranges to trixels
	auto _trixels = HTM::getTrixels(ranges);
	if (_trixels != trixels) {
		L_ERR("ERROR: Different trixels [%zu %zu]", trixels.size(), _trixels.size());
		++cont;
	}

	// test to EWKT.
	const auto str_ewkt = geometry->toEWKT();
	EWKT ewkt(str_ewkt);
	const auto& _geometry = ewkt.getGeometry();
	_trixels = _geometry->getTrixels(partials, error);
	HTM::simplifyTrixels(_trixels);
	if (_trixels != trixels) {
		L_ERR("ERROR: Geometry::toEWKT is not working\nEWKT: %s\nRec. EWKT: %s", str_ewkt, _geometry->toEWKT());
		++cont;
	}

	return cont;
}


int testPoint() {
	INIT_LOG
	auto point = std::make_shared<Point>(getPoint());
	auto trixels = point->getTrixels(partials, error);
	auto ranges = point->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "Point3D.py", point, trixels);
	HTM::writeGoogleMap(python_geospatial + "PointGM.py", "PointGM.html", point, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(point, trixels, ranges));
}


int testMultiPoint() {
	INIT_LOG
	auto multipoint = std::make_shared<MultiPoint>(getMultiPoint());
	multipoint->simplify();
	auto trixels = multipoint->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = multipoint->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "MultiPoint3D.py", multipoint, trixels);
	HTM::writeGoogleMap(python_geospatial + "MultiPointGM.py", "MultiPointGM.html", multipoint, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(multipoint, trixels, ranges));
}


int testCircle() {
	INIT_LOG
	int cont = 0;
	{
		// Test all the globe
		auto circle = std::make_shared<Circle>(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES), 20015114);
		auto trixels = circle->getTrixels(partials, error);
		HTM::simplifyTrixels(trixels);
		auto ranges = circle->getRanges(partials, error);
		HTM::writePython3D(python_geospatial + "AllCircle3D.py", circle, trixels);
		HTM::writeGoogleMap(python_geospatial + "AllCircleGM.py", "AllCircleGM.html", circle, trixels, path_test_geospatial);
		if (verify_trixels_ranges(circle, trixels, ranges) != 0) {
			++cont;
			L_ERR("Testing circle (all the globe) is not working");
		}
	}
	{
		// Test negative circle
		auto circle = std::make_shared<Circle>(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES), 15011335.5);
		auto trixels = circle->getTrixels(partials, error);
		HTM::simplifyTrixels(trixels);
		auto ranges = circle->getRanges(partials, error);
		HTM::writePython3D(python_geospatial + "NegCircle3D.py", circle, trixels);
		HTM::writeGoogleMap(python_geospatial + "NegCircleGM.py", "NegCircleGM.html", circle, trixels, path_test_geospatial);
		if (verify_trixels_ranges(circle, trixels, ranges) != 0) {
			++cont;
			L_ERR("Testing negative circle is not working");
		}
	}
	{
		// Test positive circle.
		auto circle = std::make_shared<Circle>(Cartesian(-23.6994215, 133.873049, 0, Cartesian::Units::DEGREES), 1500);
		auto trixels = circle->getTrixels(partials, error);
		HTM::simplifyTrixels(trixels);
		auto ranges = circle->getRanges(partials, error);
		HTM::writePython3D(python_geospatial + "PosCircle3D.py", circle, trixels);
		HTM::writeGoogleMap(python_geospatial + "PosCircleGM.py", "PosCircleGM.html", circle, trixels, path_test_geospatial);
		if (verify_trixels_ranges(circle, trixels, ranges) != 0) {
			++cont;
			L_ERR("Testing positive circle is not working");
		}
	}
	{
		// Test positive circle
		auto circle = std::make_shared<Circle>(getCircle());
		auto trixels = circle->getTrixels(partials, error);
		HTM::simplifyTrixels(trixels);
		auto ranges = circle->getRanges(partials, error);
		HTM::writePython3D(python_geospatial + "PosCircle3D2.py", circle, trixels);
		HTM::writeGoogleMap(python_geospatial + "PosCircleGM2.py", "PosCircleGM2.html", circle, trixels, path_test_geospatial);
		if (verify_trixels_ranges(circle, trixels, ranges) != 0) {
			++cont;
			L_ERR("Testing positive circle is not working");
		}
	}
	RETURN(cont);
}


int testConvex() {
	INIT_LOG
	auto convex = std::make_shared<Convex>(getConvex());
	convex->simplify();
	auto trixels = convex->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = convex->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "Convex3D.py", convex, trixels);
	HTM::writeGoogleMap(python_geospatial + "ConvexGM.py", "ConvexGM.html", convex, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(convex, trixels, ranges));
}


int testPolygon() {
	INIT_LOG
	auto polygon = std::make_shared<Polygon>(getPolygon());
	polygon->simplify();
	auto trixels = polygon->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = polygon->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "Polygon3D.py", polygon, trixels);
	HTM::writeGoogleMap(python_geospatial + "PolygonGM.py", "PolygonGM.html", polygon, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(polygon, trixels, ranges));
}


int testMultiCircle() {
	INIT_LOG
	auto multicircle = std::make_shared<MultiCircle>(getMultiCircle());
	multicircle->simplify();
	auto trixels = multicircle->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = multicircle->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "MultiCircle3D.py", multicircle, trixels);
	HTM::writeGoogleMap(python_geospatial + "MultiCircleGM.py", "MultiCircleGM.html", multicircle, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(multicircle, trixels, ranges));
}


int testMultiConvex() {
	INIT_LOG
	auto multiconvex = std::make_shared<MultiConvex>(getMultiConvex());
	multiconvex->simplify();
	auto trixels = multiconvex->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = multiconvex->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "MultiConvex3D.py", multiconvex, trixels);
	HTM::writeGoogleMap(python_geospatial + "MultiConvexGM.py", "MultiConvexGM.html", multiconvex, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(multiconvex, trixels, ranges));
}


int testMultiPolygon() {
	INIT_LOG
	auto multipolygon = std::make_shared<MultiPolygon>(getMultiPolygon());
	multipolygon->simplify();
	auto trixels = multipolygon->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = multipolygon->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "MultiPolygon3D.py", multipolygon, trixels);
	HTM::writeGoogleMap(python_geospatial + "MultiPolygonGM.py", "MultiPolygonGM.html", multipolygon, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(multipolygon, trixels, ranges));
}


int testCollection() {
	INIT_LOG
	auto collection = std::make_shared<Collection>(getCollection());
	collection->simplify();
	auto trixels = collection->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = collection->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "Collection3D.py", collection, trixels);
	HTM::writeGoogleMap(python_geospatial + "CollectionGM.py", "CollectionGM.html", collection, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(collection, trixels, ranges));
}


int testIntersection() {
	INIT_LOG
	auto intersection = std::make_shared<Intersection>(getIntersection());
	intersection->simplify();
	auto trixels = intersection->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	auto ranges = intersection->getRanges(partials, error);
	HTM::writePython3D(python_geospatial + "Intersection3D.py", intersection, trixels);
	HTM::writeGoogleMap(python_geospatial + "IntersectionGM.py", "IntersectionGM.html", intersection, trixels, path_test_geospatial);
	RETURN(verify_trixels_ranges(intersection, trixels, ranges));
}
