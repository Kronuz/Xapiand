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

#include "test_htm.h"


// Testing the transformation of coordinates between CRS.
int test_cartesian_transforms()
{
	Vector_Transforms SRID_2_WGS84;
	// WGS72 to WGS84  (4322 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1238
	SRID_2_WGS84.push_back({ 4322, 20.0,   10.0, 30.0, "20°0'0.141702''N  10°0'0.554000''E  30.959384"    });
	SRID_2_WGS84.push_back({ 4322, 20.0,  -10.0, 30.0, "20°0'0.141702''N  9°59'59.446000''W  30.959384"   });
	SRID_2_WGS84.push_back({ 4322, -20.0,  10.0, 30.0, "19°59'59.866682''S  10°0'0.554000''E  27.881203"  });
	SRID_2_WGS84.push_back({ 4322, -20.0, -10.0, 30.0, "19°59'59.866682''S  9°59'59.446000''W  27.881203" });

	// NAD83 to WGS84  (4269 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1252
	SRID_2_WGS84.push_back({ 4269, 20.0,   10.0, 30.0, "19°59'59.956556''N  10°0'0.027905''E  30.746560"   });
	SRID_2_WGS84.push_back({ 4269, 20.0,  -10.0, 30.0, "19°59'59.960418''N  9°59'59.960148''W  30.420209" });
	SRID_2_WGS84.push_back({ 4269, -20.0,  10.0, 30.0, "20°0'0.017671''S  10°0'0.027905''E  31.430600"    });
	SRID_2_WGS84.push_back({ 4269, -20.0, -10.0, 30.0, "20°0'0.021534''S  9°59'59.960148''W  31.104249"   });

	// NAD27 to WGS84  (4267 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1173
	SRID_2_WGS84.push_back({ 4267, 20.0,   10.0, 30.0, "20°0'0.196545''N  10°0'5.468256''E  150.554523"    });
	SRID_2_WGS84.push_back({ 4267, 20.0,  -10.0, 30.0, "20°0'0.814568''N  9°59'54.627272''W  98.338209"    });
	SRID_2_WGS84.push_back({ 4267, -20.0,  10.0, 30.0, "19°59'49.440208''S  10°0'5.468256''E  30.171742"   });
	SRID_2_WGS84.push_back({ 4267, -20.0, -10.0, 30.0, "19°59'50.058155''S  9°59'54.627272''W  -22.045563" });

	// OSGB36 to WGS84  (4277 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1314
	SRID_2_WGS84.push_back({ 4277, 20.0,   10.0, 30.0, "20°0'13.337317''N  9°59'53.865759''E  -86.980683"   });
	SRID_2_WGS84.push_back({ 4277, 20.0,  -10.0, 30.0, "20°0'12.801456''N  10°0'0.769107''W  -46.142419"    });
	SRID_2_WGS84.push_back({ 4277, -20.0,  10.0, 30.0, "19°59'40.643875''S  9°59'54.003573''E  -457.728199" });
	SRID_2_WGS84.push_back({ 4277, -20.0, -10.0, 30.0, "19°59'40.212914''S  10°0'0.693312''W  -416.880621"  });

	// TM75 to WGS84  (4300 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1954
	SRID_2_WGS84.push_back({ 4300, 20.0,   10.0, 30.0, "20°0'13.892799''N  9°59'52.446296''E  -87.320347"   });
	SRID_2_WGS84.push_back({ 4300, 20.0,  -10.0, 30.0, "20°0'13.751990''N  10°0'1.815691''W  -44.678652"    });
	SRID_2_WGS84.push_back({ 4300, -20.0,  10.0, 30.0, "19°59'39.325125''S  9°59'51.677477''E  -473.515164" });
	SRID_2_WGS84.push_back({ 4300, -20.0, -10.0, 30.0, "19°59'38.457075''S  10°0'2.530766''W  -430.919043"  });

	// TM65 to WGS84  (4299 to 4326) -> The results are very close to those obtained in the page:
	// http://www.geocachingtoolbox.com/index.php?lang=en&page=coordinateConversion&status=result
	SRID_2_WGS84.push_back({ 4299, 20.0,   10.0, 30.0, "20°0'13.891148''N  9°59'52.446252''E  -87.306642"   });
	SRID_2_WGS84.push_back({ 4299, 20.0,  -10.0, 30.0, "20°0'13.750355''N  10°0'1.815376''W  -44.666252"    });
	SRID_2_WGS84.push_back({ 4299, -20.0,  10.0, 30.0, "19°59'39.326103''S  9°59'51.677433''E  -473.472045" });
	SRID_2_WGS84.push_back({ 4299, -20.0, -10.0, 30.0, "19°59'38.458068''S  10°0'2.530451''W  -430.877230"  });

	// ED79 to WGS84  (4668 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/15752
	SRID_2_WGS84.push_back({ 4668, 20.0,   10.0, 30.0, "19°59'55.589986''N  9°59'57.193708''E  134.068052" });
	SRID_2_WGS84.push_back({ 4668, 20.0,  -10.0, 30.0, "19°59'55.211469''N  10°0'3.833722''W  166.051242"  });
	SRID_2_WGS84.push_back({ 4668, -20.0,  10.0, 30.0, "20°0'2.862582''S  9°59'57.193708''E  215.468007"   });
	SRID_2_WGS84.push_back({ 4668, -20.0, -10.0, 30.0, "20°0'2.484033''S  10°0'3.833722''W  247.450787"    });

	// ED50 to WGS84  (4230 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1133
	SRID_2_WGS84.push_back({ 4230, 20.0,   10.0, 30.0, "19°59'55.539823''N  9°59'57.199681''E  132.458626" });
	SRID_2_WGS84.push_back({ 4230, 20.0,  -10.0, 30.0, "19°59'55.161306''N  10°0'3.839696''W  164.441824"  });
	SRID_2_WGS84.push_back({ 4230, -20.0,  10.0, 30.0, "20°0'2.934649''S  9°59'57.199681''E  215.226660"   });
	SRID_2_WGS84.push_back({ 4230, -20.0, -10.0, 30.0, "20°0'2.556100''S  10°0'3.839696''W  247.209441"    });

	// TOYA to WGS84  (4301 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1230
	SRID_2_WGS84.push_back({ 4301, 20.0,   10.0, 30.0, "20°0'22.962090''N  10°0'18.062821''E  -521.976076"   });
	SRID_2_WGS84.push_back({ 4301, 20.0,  -10.0, 30.0, "20°0'24.921332''N  9°59'43.705140''W  -687.433480"   });
	SRID_2_WGS84.push_back({ 4301, -20.0,  10.0, 30.0, "19°59'41.092892''S  10°0'18.062821''E  -990.556329"  });
	SRID_2_WGS84.push_back({ 4301, -20.0, -10.0, 30.0, "19°59'43.051188''S  9°59'43.705140''W  -1156.025959" });

	// DHDN to WGS84  (4314 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1673
	SRID_2_WGS84.push_back({ 4314, 20.0,   10.0, 30.0, "20°0'7.291150''N  9°59'56.608634''E  48.138765"      });
	SRID_2_WGS84.push_back({ 4314, 20.0,  -10.0, 30.0, "20°0'7.333754''N  9°59'56.393946''W  13.848005"      });
	SRID_2_WGS84.push_back({ 4314, -20.0,  10.0, 30.0, "19°59'42.318425''S  9°59'57.393082''E  -235.013109"  });
	SRID_2_WGS84.push_back({ 4314, -20.0, -10.0, 30.0, "19°59'43.086952''S  9°59'55.697370''W  -269.257292"  });

	// OEG to WGS84  (4229 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1148
	SRID_2_WGS84.push_back({ 4229, 20.0,   10.0, 30.0, "20°0'0.873728''N  10°0'4.503259''E  -13.466677"  });
	SRID_2_WGS84.push_back({ 4229, 20.0,  -10.0, 30.0, "20°0'1.298641''N  9°59'57.049898''W  -49.366075" });
	SRID_2_WGS84.push_back({ 4229, -20.0,  10.0, 30.0, "20°0'1.668233''S  10°0'4.503259''E  -4.574003"   });
	SRID_2_WGS84.push_back({ 4229, -20.0, -10.0, 30.0, "20°0'2.093151''S  9°59'57.049898''W  -40.473350" });

	// AGD84 to WGS84  (4203 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1236
	SRID_2_WGS84.push_back({ 4203, 20.0,   10.0, 30.0, "20°0'5.339442''N  9°59'59.220714''E  -13.586401"    });
	SRID_2_WGS84.push_back({ 4203, 20.0,  -10.0, 30.0, "20°0'5.064184''N  10°0'2.116232''W  2.879302"       });
	SRID_2_WGS84.push_back({ 4203, -20.0,  10.0, 30.0, "19°59'57.371712''S  9°59'59.433464''E  -110.463889" });
	SRID_2_WGS84.push_back({ 4203, -20.0, -10.0, 30.0, "19°59'57.257055''S  10°0'2.001422''W  -93.987306"   });

	// SAD69 to WGS84  (4618 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1864
	SRID_2_WGS84.push_back({ 4618, 20.0,   10.0, 30.0, "19°59'59.357117''N  10°0'0.374382''E  -13.677770" });
	SRID_2_WGS84.push_back({ 4618, 20.0,  -10.0, 30.0, "19°59'59.360979''N  10°0'0.306624''W  -14.004125" });
	SRID_2_WGS84.push_back({ 4618, -20.0,  10.0, 30.0, "20°0'1.862864''S  10°0'0.374382''E  14.368110"    });
	SRID_2_WGS84.push_back({ 4618, -20.0, -10.0, 30.0, "20°0'1.866726''S  10°0'0.306624''W  14.041756"    });

	// PUL42 to WGS84  (4178 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1334
	SRID_2_WGS84.push_back({ 4178, 20.0,   10.0, 30.0, "19°59'57.750301''N  9°59'56.403911''E  92.107732" });
	SRID_2_WGS84.push_back({ 4178, 20.0,  -10.0, 30.0, "19°59'57.019651''N  10°0'3.265190''W  123.917120" });
	SRID_2_WGS84.push_back({ 4178, -20.0,  10.0, 30.0, "20°0'2.270413''S  9°59'57.198773''E  133.835302"  });
	SRID_2_WGS84.push_back({ 4178, -20.0, -10.0, 30.0, "20°0'2.247538''S  10°0'2.616278''W  165.691341"   });

	// MGI1901 to WGS84  (3906 to 4326) -> The results are very close to those obtained in the page:
	// http://www.geocachingtoolbox.com/index.php?lang=en&page=coordinateConversion&status=result
	SRID_2_WGS84.push_back({ 3906, 20.0,   10.0, 30.0, "20°0'8.506072''N  9°59'48.107356''E  -15.039391"    });
	SRID_2_WGS84.push_back({ 3906, 20.0,  -10.0, 30.0, "20°0'7.306781''N  10°0'5.296242''W  -75.952463"     });
	SRID_2_WGS84.push_back({ 3906, -20.0,  10.0, 30.0, "19°59'42.260450''S  9°59'52.463078''E  -364.894519" });
	SRID_2_WGS84.push_back({ 3906, -20.0, -10.0, 30.0, "19°59'44.898670''S  10°0'1.823681''W  -425.555326"  });

	// GGRS87 to WGS84  (4121 to 4326) -> The results are very close to those obtained in the page:
	// http://georepository.com/calculator/convert/operation_id/1272
	SRID_2_WGS84.push_back({ 4121, 20.0,   10.0, 30.0, "20°0'9.581041''N  10°0'3.727855''E  -58.402327"     });
	SRID_2_WGS84.push_back({ 4121, 20.0,  -10.0, 30.0, "20°0'9.869982''N  9°59'58.660140''W  -82.810562"    });
	SRID_2_WGS84.push_back({ 4121, -20.0,  10.0, 30.0, "19°59'54.508366''S  10°0'3.727855''E  -227.104937"  });
	SRID_2_WGS84.push_back({ 4121, -20.0, -10.0, 30.0, "19°59'54.797256''S  9°59'58.660140''W  -251.513821" });


	Vector_Transforms::const_iterator it = SRID_2_WGS84.begin();
	int cont = 0;

	try {
		for ( ;it != SRID_2_WGS84.end(); it++) {
			Cartesian c(it->lat_src, it->lon_src, it->h_src, Cartesian::DEGREES, it->SRID);
			double lat, lon, height;
			c.toGeodetic(lat, lon, height);
			std::string get = c.Decimal2Degrees();
			if (get.compare(it->res) != 0) {
				cont++;
				LOG_ERR(NULL, "ERROR: Resul: %s  Expected: %s\n", get.c_str(), it->res.c_str());
			}
		}
	} catch (const std::exception &e) {
		LOG_ERR(NULL, "ERROR: %s\n", e.what());
		cont++;
	}

	if (cont == 0) {
		LOG(NULL, "Testing the transformation of coordinates between CRS is correct!\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing the transformation of coordinates between CRS has mistakes.\n");
		return 1;
	}
}


// Testing the elimination of points that make the non-convex polygon.
// Python files are generated to view the results.
int test_hullConvex()
{
	int cont = 0;
	std::vector<std::string> files, expect_files, result_files;
	files.push_back("examples/ColoradoPoly.txt");
	expect_files.push_back("examples/ColoradoPoly_expect_convex.txt");
	result_files.push_back("examples/ColoradoPoly_convex_hull.py");
	files.push_back("examples/Georgia.txt");
	expect_files.push_back("examples/Georgia_expect_convex.txt");
	result_files.push_back("examples/Georgia_convex_hull.py");
	files.push_back("examples/MexPoly.txt");
	expect_files.push_back("examples/MexPoly_expect_convex.txt");
	result_files.push_back("examples/MexPoly_convex_hull.py");
	files.push_back("examples/Nave.txt");
	expect_files.push_back("examples/Nave_expect_convex.txt");
	result_files.push_back("examples/Nave_convex_hull.py");
	files.push_back("examples/Poly.txt");
	expect_files.push_back("examples/Poly_expect_convex.txt");
	result_files.push_back("examples/Poly_convex_hull.py");
	files.push_back("examples/Poly2.txt");
	expect_files.push_back("examples/Poly2_expect_convex.txt");
	result_files.push_back("examples/Poly2_convex_hull.py");
	files.push_back("examples/Strip.txt");
	expect_files.push_back("examples/Strip_expect_convex.txt");
	result_files.push_back("examples/Strip_convex_hull.py");
	files.push_back("examples/Utah.txt");
	expect_files.push_back("examples/Utah_expect_convex.txt");
	result_files.push_back("examples/Utah_convex_hull.py");

	std::vector<std::string>::const_iterator it_f(files.begin());
	std::vector<std::string>::const_iterator it_e(expect_files.begin());
	std::vector<std::string>::const_iterator it_r(result_files.begin());

	for ( ;it_f != files.end(); it_f++, it_e++, it_r++) {
		std::ofstream fs(*it_r);
		fs << "from mpl_toolkits.mplot3d import Axes3D\n";
		fs << "from mpl_toolkits.mplot3d.art3d import Poly3DCollection\n";
		fs << "import matplotlib.pyplot as plt\n\n\n";
		fs << "ax = Axes3D(plt.figure())\n\n";

		std::ifstream readFile(*it_f);
		std::ifstream readEFile(*it_e);

		char output[50];
		double lat, lon;
		std::vector<Cartesian> pts;
		int contLL = 0;

		if (readFile.is_open() && readEFile.is_open()) {
			while (!readFile.eof()) {
				readFile >> output;
				if (contLL == 0) {
					lat = atof(output);
					contLL++;
				} else {
					lon = atof(output);
					Cartesian c(lat, lon, 0, Cartesian::DEGREES);
					pts.push_back(c);
					contLL = 0;
				}
			}

			try {
				Geometry g(pts);
				std::string x_s, y_s, z_s;
				double x1, y1, z1;
				int i = 1;

				std::vector<Cartesian>::iterator it_o = pts.begin();
				fs << "\n# Original Points\n";
				for ( ;it_o != pts.end(); it_o++) {
					(*it_o).normalize();
					if (i == 1) {
						x1 = (*it_o).x;
						y1 = (*it_o).y;
						z1 = (*it_o).z;
						i++;
						fs << "x1 = " << x1 << ";\n" << "y1 = " << y1 << ";\n" << "z1 = " << z1 << ";\n";
					}
					fs << "x = [" << (*it_o).x << "];\ny = [" << (*it_o).y << "];\nz = [" << (*it_o).z << "]\n";
					fs << "ax.plot3D(x, y, z, 'ro', lw = 2.0, ms = 6);\n";
				}

				std::vector<Cartesian>::const_iterator it = g.corners.begin();
				fs << "# Points for the hull convex\n";
				i = 1;
				for ( ;it != g.corners.end(); it++) {
					x_s += std::to_string((*it).x) + ", ";
					y_s += std::to_string((*it).y) + ", ";
					z_s += std::to_string((*it).z) + ", ";
					if (i == 1) {
						x1 = (*it).x;
						y1 = (*it).y;
						z1 = (*it).z;
						i++;
					}
					std::string coord_get = std::to_string((*it).x) + " " + std::to_string((*it).y) + " " + std::to_string((*it).z);
					std::string coord_exp;
					if (!readEFile.eof()) {
						std::getline(readEFile, coord_exp);
						if (coord_exp.compare(coord_get) != 0) {
							cont++;
							LOG_ERR(NULL, "ERROR: Result(%s) Expect(%s).\n", coord_get.c_str(), coord_exp.c_str());
						}
					} else {
						cont++;
						LOG_ERR(NULL, "ERROR: Expected less corners.\n");
						break;
					}
				}

				if (!readEFile.eof()) {
					cont++;
					LOG_ERR(NULL, "ERROR: Expected more corners.\n");
					break;
				}

				fs << "x = [" << x_s << x1 << "];\ny = [" << y_s << y1 << "];\nz = [" << z_s << z1 << "]\n";
				fs << "ax.plot3D(x, y, z, '-', lw = 2.0, ms = 12, mfc = 'white', mec = 'black');\n";

				fs << "ax.set_xlabel('x')\nax.set_ylabel('y')\nax.set_zlabel('z')\n";
				fs << "plt.show()\nplt.ion()\n";
			} catch(const std::exception &e) {
				LOG_ERR(NULL, "ERROR: %s\n", e.what());
				cont++;
			}
		} else {
			LOG_ERR(NULL, "ERROR: File %s or %s not found.\n", (*it_f).c_str(), (*it_e).c_str());
			cont ++;
		}

		fs.close();
	}

	if (cont == 0) {
		LOG(NULL, "Testing Geometry Hull Convex is correct!, run with python examples/{file}_convex_hull.py to see the hull convex.\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing Geometry Hull Convex has mistakes.\n");
		return 1;
	}
}


// Testing HTM for Polygons.
// Python files are generated to view the results.
int test_HTM_chull()
{
	double error = 0.2;
	bool partials = true;
	int cont = 0;

	std::vector<std::string> files, expect_files, result_files;
	files.push_back("examples/ColoradoPoly.txt");
	expect_files.push_back("examples/ColoradoPoly_expect_HTM.txt");
	result_files.push_back("examples/ColoradoPoly_polygon_HTM.py");
	files.push_back("examples/Georgia.txt");
	expect_files.push_back("examples/Georgia_expect_HTM.txt");
	result_files.push_back("examples/Georgia_polygon_HTM.py");
	files.push_back("examples/MexPoly.txt");
	expect_files.push_back("examples/MexPoly_expect_HTM.txt");
	result_files.push_back("examples/MexPoly_polygon_HTM.py");
	files.push_back("examples/Nave.txt");
	expect_files.push_back("examples/Nave_expect_HTM.txt");
	result_files.push_back("examples/Nave_polygon_HTM.py");
	files.push_back("examples/Poly.txt");
	expect_files.push_back("examples/Poly_expect_HTM.txt");
	result_files.push_back("examples/Poly_polygon_HTM.py");
	files.push_back("examples/Poly2.txt");
	expect_files.push_back("examples/Poly2_expect_HTM.txt");
	result_files.push_back("examples/Poly2_polygon_HTM.py");
	files.push_back("examples/Poly3.txt");
	expect_files.push_back("examples/Poly3_expect_HTM.txt");
	result_files.push_back("examples/Poly3_polygon_HTM.py");
	files.push_back("examples/Strip.txt");
	expect_files.push_back("examples/Strip_expect_HTM.txt");
	result_files.push_back("examples/Strip_polygon_HTM.py");
	files.push_back("examples/Utah.txt");
	expect_files.push_back("examples/Utah_expect_HTM.txt");
	result_files.push_back("examples/Utah_polygon_HTM.py");

	std::vector<std::string>::const_iterator it_f(files.begin());
	std::vector<std::string>::const_iterator it_e(expect_files.begin());
	std::vector<std::string>::const_iterator it_r(result_files.begin());

	for ( ;it_f != files.end(); it_f++, it_e++, it_r++) {
		std::ifstream readFile(*it_f);
		std::ifstream readEFile(*it_e);

		char output[50];
		double lat, lon;
		std::vector<Cartesian> pts;
		int contLL = 0;

		if (readFile.is_open() && readEFile.is_open()) {
			while (!readFile.eof()) {
				readFile >> output;
				if (contLL == 0) {
					lat = atof(output);
					contLL++;
				} else {
					lon = atof(output);
					Cartesian c(lat, lon, 0, Cartesian::DEGREES);
					pts.push_back(c);
					contLL = 0;
				}
			}

			try {
				Geometry g(pts);

				HTM _htm(partials, error, g);
				_htm.run();

				std::vector<std::string>::const_iterator itn = _htm.names.begin();
				for ( ;itn != _htm.names.end(); itn++) {
					std::string trixel_exp;
					if (!readEFile.eof()) {
						std::getline(readEFile, trixel_exp);
						if (trixel_exp.compare(*itn) != 0) {
							cont++;
							LOG_ERR(NULL, "ERROR: File(%s) Result(%s) Expect(%s).\n", (*it_f).c_str(), (*itn).c_str(), trixel_exp.c_str());
						}
					} else {
						cont++;
						LOG_ERR(NULL, "ERROR: Expected less trixels.\n");
						break;
					}
				}

				if (!readEFile.eof()) {
					cont++;
					LOG_ERR(NULL, "ERROR: Expected more trixels.\n");
					break;
				}

				_htm.writePython3D(*it_r);
			} catch(const std::exception &e) {
				LOG_ERR(NULL, "ERROR: %s\n", e.what());
				cont++;
			}
		} else {
			LOG_ERR(NULL, "ERROR: File %s or %s not found.\n", (*it_f).c_str(), (*it_e).c_str());
			cont ++;
		}
	}

	if (cont == 0) {
		LOG(NULL, "Testing HTM polygon is correct!, run with python examples/{file}_polygon_HTM.py to see the trixels that cover the hull convex.\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing polygon HTM has mistakes.\n");
		return 1;
	}
}


// Testing HTM for bounding circles.
// Python files are generated to view the results.
int test_HTM_circle()
{
	int cont = 0;
	std::string name("examples/Circles_HTM.txt");
	std::ifstream readFile(name);
	char output[50];

	if (readFile.is_open()) {
		while (!readFile.eof()) {
			readFile >> output;
			double error = atof(output);
			readFile >> output;
			double partials = (atoi(output) > 0) ? true : false;
			readFile >> output;
			double lat = atof(output);
			readFile >> output;
			double lon = atof(output);
			readFile >> output;
			double radius = atof(output);
			readFile >> output;
			std::string file_expect(output);
			readFile >> output;
			std::string file_result(output);

			std::ifstream readEFile(file_expect);
			if (readEFile.is_open()) {
				try {
					Cartesian center(lat, lon, 0, Cartesian::DEGREES);
					Constraint c(center, radius);
					Geometry g(c);

					HTM _htm(partials, error, g);
					_htm.run();

					std::vector<std::string>::const_iterator itn = _htm.names.begin();
					for ( ;itn != _htm.names.end(); itn++) {
						std::string trixel_exp;
						if (!readEFile.eof()) {
							std::getline(readEFile, trixel_exp);
							if (trixel_exp.compare(*itn) != 0) {
								cont++;
								LOG_ERR(NULL, "ERROR: File (%s) Result(%s) Expect(%s).\n", file_expect.c_str(), (*itn).c_str(), trixel_exp.c_str());
							}
						} else {
							cont++;
							LOG_ERR(NULL, "ERROR: Expected less trixels.\n");
							break;
						}
					}

					if (!readEFile.eof()) {
						cont++;
						LOG_ERR(NULL, "ERROR: Expected more trixels.\n");
						break;
					}

					_htm.writePython3D(file_result);
				} catch(const std::exception &e) {
					LOG_ERR(NULL, "ERROR: %s\n", e.what());
					cont++;
				}
			} else {
				LOG_ERR(NULL, "ERROR: File %s not found.\n", file_expect.c_str());
				cont ++;
			}
		}
	} else {
		LOG_ERR(NULL, "ERROR: File %s not found.\n", name.c_str());
		cont ++;
	}

	if (cont == 0) {
		LOG(NULL, "Testing HTM bounding circle is correct!, run with python examples/Circle{#}_HTM.py to see the trixels that cover the bounding circle.\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing HTM bounding circle has mistakes.\n");
		return 1;
	}
}