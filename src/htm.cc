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


#include "HTM.h"


// Constructor for HTM earth.
// If partials_ then return triangles partials.
// Error: [0.1, 0.5], specific error according to the diameter of the circle
// or the circle that adjusts the area Polygono.
// Geometry region.
HTM::HTM(bool partials_, double error, Geometry &region_) : region(region_), partials(partials_)
{
	ids = std::vector<uInt64>();
	names = std::vector<std::string>();
	// Get the error with respect to the radius.
	error = (error > 0.5) ? 1 : (error < 0.1) ? 0.2 : 2 * error;
	double errorD =  error * region.getRadius();
	max_level = MAX_LEVEL;
	for (int i = 0; i <= MAX_LEVEL; i++) {
		if (ERROR_NIVEL[i] < errorD || i == MAX_LEVEL) {
			max_level = i;
			break;
		}
	}
}


// Finds the inicial trixel containing the coord.
uInt64
HTM::startTrixel(Cartesian &v0, Cartesian &v1, Cartesian &v2, const Cartesian &coord, std::string &name)
{
	uInt64 trixel_id = 0;
	if(coord.x > 0 && coord.y >= 0) {
		trixel_id = (coord.z >= 0) ? N3 : S0;
	} else if(coord.x <= 0 && coord.y > 0) {
		trixel_id = (coord.z >= 0) ? N2 : S1;
	} else if(coord.x < 0 && coord.y <= 0) {
		trixel_id = (coord.z >= 0) ? N1 : S2;
	} else if(coord.x >= 0 && coord.y < 0) {
		trixel_id = (coord.z >= 0) ? N0 : S3;
	} else {
		trixel_id = (coord.z >= 0) ? N3 : S0;
	}

	// The enumeration begins in 8L.
	int num = (int) (trixel_id - 8L);
	v0 = start_vertices[start_trixels[num].v0];
	v1 = start_vertices[start_trixels[num].v1];
	v2 = start_vertices[start_trixels[num].v2];
	name = start_trixels[num].name;;

	return trixel_id;
}


// Finds the midpoint of two edges.
void
HTM::midPoint(const Cartesian &v0, const Cartesian &v1, Cartesian &w)
{
	w = v0 + v1;
	w.normalize();
	return;
}


// Given a coord, return its HTM id
uInt64
HTM::cartesian2id(Cartesian &coord, std::string &name)
{
	uInt64 start_id;
	uInt64 id;
	Cartesian v0, v1, v2;
	Cartesian w0, w1, w2;

	start_id = startTrixel(v0, v1, v2, coord, name);

	id = start_id;

	// Search in children's trixel
	short depth = max_level;
	while (depth-- > 0) {
		id <<= 2;
		midPoint(v0, v1, w2);
		midPoint(v1, v2, w0);
		midPoint(v2, v0, w1);
		if(insideVector(v0, w2, w1, coord)) {
			name += "0";
			v1 = w2;
			v2 = w1;
		} else if(insideVector(v1, w0, w2, coord)) {
			name += "1";
			id |= 1;
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if(insideVector(v2, w1, w0, coord)) {
			name += "2";
			id |= 2;
			v0 = v2;
			v1 = w1;
			v2 = w0;
		} else if(insideVector(w0, w1, w2, coord)) {
			name += "3";
			id |= 3;
			v0 = w0;
			v1 = w1;
			v2 = w2;
		} else {
			throw Error("It can not find cartesian coordinate's id");
		}
	}

	return id;
}


// Return 1 if trixel's vertex are inside, otherwise return 0.
int
HTM::insideVertex(const Cartesian &v)
{
	std::vector<Constraint>::const_iterator it = region.constraints.begin();
	for ( ; it != region.constraints.end(); it++) {
		if ((*it).center * v <= (*it).distance) return 0;
	}
	return 1;
}


// Verify if a trixel is inside, outside or partial of the convex.
int
HTM::verifyTrixel(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2)
{
	int sum = insideVertex(v0) + insideVertex(v1) + insideVertex(v2);

	if (sum == 1 || sum == 2) return HTM_PARTIAL;

	// If corners are inside.
	//   Case 1: the sign is NEG and there is a hole, the trixel will be mark as HTM_PARTIAL.
	//   Case 2: the sign is NEG and there are intersections, the trixel will be mark as HTM_PARTIAL.
	//   Other case: HTM_FULL.
	if (sum == 3) {
		// We need to test whether there is a hole inside the triangle for negative halfspace’s boundary circle.
		// If there is a hole the trixel is flag to PARTIAL.
		if (region.constraints.at(0).sign == NEG) {
			if (thereisHole(v0, v1, v2)) return HTM_PARTIAL;
			// Test whether one of the negative halfspace’s boundary circles intersects with one of the edges of the triangle.
			if (intersectEdge(v0, v1, v2)) return HTM_PARTIAL;
		}
		return HTM_FULL;
	}

	if (region.constraints.at(0).sign == NEG || !boundingCircle(v0, v1, v2)) return HTM_OUTSIDE;

	// If region is a bounding circle
	if (region.constraints.at(0).sign == POS || region.constraints.size() == 1) {
		// The constraint intersect with a side.
		if (intersectEdge(v0, v1, v2)) {
			return HTM_PARTIAL;
			// Constraint in triangle.
		} else if (insideVector(v0, v1, v2, region.boundingCircle.center)) {
				return HTM_PARTIAL;
		} else {
			return HTM_OUTSIDE;
		}
	// If region it is a Polygon
	} else if (testEdgePolygon(v0, v1, v2)) {
		return HTM_PARTIAL;
	} else {
		return HTM_OUTSIDE;
	}
}


// Return if a trixel is intersecting or inside of a polygon.
bool
HTM::testEdgePolygon(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2)
{
	// We need to check each polygon's side against the 3 triangle edges.
	// If any of the triangle's edges has its intersection INSIDE the polygon's side, return true.
	// Otherwise return if a corner is inside.

	Cartesian coords[3];
	double length[3];  // length of edge in radians.
	Cartesian start_e[3]; // Start edge.
	Cartesian end_e[3]; // End edge.

	// For the first edge
	coords[0] = v0 ^ v1;
	length[0] = acos(v0 * v1);
	start_e[0] = v0;
	end_e[0] = v1;

	// For the second edge
	coords[1] = v1 ^ v2;
	length[1] = acos(v1 * v2);
	start_e[1] = v1;
	end_e[1] = v2;

	// For the third edge
	coords[2] = v2 ^ v0;
	length[2] = acos(v2 * v0);
	start_e[2] = v2;
	end_e[2] = v0;

	// Checking each polygon's side against the 3 triangle edges for intersections.
	for (int i = 0; i < region.corners.size(); i++) {
		int j = (i == region.corners.size() - 1) ? 0 : i + 1;
		Cartesian aux;
		double d1, d2;
		double dij = acos(region.corners.at(i) * region.corners.at(j));  // Distance between points i and j.

		// Calculate the intersection with the 3 triangle's edges.
		for (int k = 0; k < 3; k++) {
			aux = coords[k] ^ (region.corners.at(i) ^ region.corners.at(j));
			aux.normalize();
			// If the intersection is inside the edge of the convex, its distance to the corners
			// is smaller than the side of Polygon. This test has to be done for:
			//     convex's edge and
			//     triangle's edge.
			for (int kk = 0; kk < 2; kk++) {
				d1 = acos(region.corners.at(i) * aux); // distance to the corner i
				d2 = acos(region.corners.at(j) * aux); // distance to the corner j
				// Test with the convex's edge
				if (d1 - dij < DBL_TOLERANCE && d2 - dij < DBL_TOLERANCE) {
					d1 = acos(start_e[k] * aux);
					d2 = acos(end_e[k] * aux);
					// Test with triangle's edge.
					if ((d1 - length[k]) < DBL_TOLERANCE && (d2 - length[k]) < DBL_TOLERANCE) {
						return true;
					}
				}
				aux = aux.get_inverse(); // Do the same for the other intersection
			}
		}
	}
	return insideVector(v0, v1, v2, region.corners.at(0));
}


// Return whether there is a hole inside the triangle.
bool
HTM::thereisHole(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2)
{
	if (((v0 ^ v1) * region.boundingCircle.center) >= 0.0 ||
		((v1 ^ v2) * region.boundingCircle.center) >= 0.0 ||
		((v2 ^ v0) * region.boundingCircle.center) >= 0.0)
		return false;
	return true;
}


// Return if v is inside of a trixel.
bool
HTM::insideVector(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2, const Cartesian &v) {
	if (((v0 ^ v1) * v) < 0 ||
		((v1 ^ v2) * v) < 0 ||
		((v2 ^ v0) * v) < 0)
		return false;
	return true;
}


// Test whether one of the halfspace’s boundary circles intersects with
// one of the edges of the triangle.
bool
HTM::intersectEdge(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2)
{
	if (intersection(v0, v1, region.boundingCircle) ||
		intersection(v1, v2, region.boundingCircle) ||
		intersection(v2, v0, region.boundingCircle)) return true;
	return false;
}


// Return true if there is a intersection between trixel and convex.
bool
HTM::intersection(const Cartesian &v1, const Cartesian &v2, const Constraint &c)
{
	double gamma1 = v1 * c.center;
	double gamma2 = v2 * c.center;
	double cos_t = v1 * v2;
	double square_u = (1 - cos_t) / (1 + cos_t);

	double _a = - square_u * (gamma1 + c.distance);
	double _b = gamma1 * (square_u - 1) + gamma2 * (square_u + 1);
	double _c = gamma1 - c.distance;
	double aux = (_b * _b) - (4 * _a * _c);

	if (aux < 0.0 || (_a > -DBL_TOLERANCE && _a < DBL_TOLERANCE)) return false;


	aux = sqrt(aux);
	_a = 2 * _a;
	_b = -_b;
	double r1 = (_b + aux) / _a;
	double r2 = (_b - aux) / _a;

	if ((r1 >= 0.0 && r1 <= 1.0) || (r2 >= 0.0 && r2 <= 1.0)) return true;

	return false;
}


// Return if there is an overlap between trixel and convex, calculating the bounding circle of the trixel.
bool
HTM::boundingCircle(const Cartesian &v0, const Cartesian &v1, const Cartesian &v2)
{
	Cartesian vb = (v1 - v0) ^ (v2 - v1);
	vb.normalize();
	double phi_d = acos(v0 * vb);

	double tetha = acos(vb * region.boundingCircle.center);
	if (tetha >= (phi_d + region.boundingCircle.arcangle)) return false;
	return true;
}


void
HTM::lookupTrixels(int level, std::string name, const Cartesian &v0, const Cartesian &v1, const Cartesian &v2, uInt64 id_, int Pp, bool first_FULL)
{
	// Finish the recursion.
	if (--level < 0) {
		if (partials) {
			ids.push_back(id_);
			names.push_back(name);
		}
		return;
	}

	uInt64 offsprings_id[4], id = id_ << 2;
	int P, F, type_trixels[4];

	offsprings_id[0] = id;
	offsprings_id[1] = id + 1;
	offsprings_id[2] = id + 2;
	offsprings_id[3] = id + 3;

	Cartesian w0, w1, w2;
	midPoint(v0, v1, w2);
	midPoint(v1, v2, w0);
	midPoint(v2, v0, w1);

	type_trixels[0] = verifyTrixel(v0, w2, w1);
	type_trixels[1] = verifyTrixel(v1, w0, w2);
	type_trixels[2] = verifyTrixel(v2, w1, w0);
	type_trixels[3] = verifyTrixel(w0, w1, w2);

	// Number of full and partial subtrixels.
	F = (type_trixels[0] == HTM_FULL)  + (type_trixels[1] == HTM_FULL)  + (type_trixels[2] == HTM_FULL)  + (type_trixels[3] == HTM_FULL);
	P = (type_trixels[0] == HTM_PARTIAL) + (type_trixels[1] == HTM_PARTIAL) + (type_trixels[2] == HTM_PARTIAL) + (type_trixels[3] == HTM_PARTIAL);

	if (F == 4) {
		ids.push_back(id_);
		names.push_back(name);
		return;
	}

	// Save the full subtrixels' id
	if (type_trixels[0] == HTM_FULL) {
		ids.push_back(offsprings_id[0]);
		names.push_back(name + "0");
	}
	if (type_trixels[1] == HTM_FULL) {
		ids.push_back(offsprings_id[1]);
		names.push_back(name + "1");
	}
	if (type_trixels[2] == HTM_FULL) {
		ids.push_back(offsprings_id[2]);
		names.push_back(name + "2");
	}
	if (type_trixels[3] == HTM_FULL) {
		ids.push_back(offsprings_id[3]);
		names.push_back(name + "3");
	}

	// Recursion
	if (type_trixels[0] == HTM_PARTIAL) lookupTrixels(level, name + "0", v0, w2, w1, offsprings_id[0], P, first_FULL);
	if (type_trixels[1] == HTM_PARTIAL) lookupTrixels(level, name + "1", v1, w0, w2, offsprings_id[1], P, first_FULL);
	if (type_trixels[2] == HTM_PARTIAL) lookupTrixels(level, name + "2", v2, w1, w0, offsprings_id[2], P, first_FULL);
	if (type_trixels[3] == HTM_PARTIAL) lookupTrixels(level, name + "3", w0, w1, w2, offsprings_id[3], P, first_FULL);
	return;
}


void
HTM::run()
{
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2]))
		lookupTrixels(max_level, start_trixels[0].name, start_vertices[start_trixels[0].v0], start_vertices[start_trixels[0].v1], start_vertices[start_trixels[0].v2], start_trixels[0].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2]))
		lookupTrixels(max_level, start_trixels[1].name, start_vertices[start_trixels[1].v0], start_vertices[start_trixels[1].v1], start_vertices[start_trixels[1].v2], start_trixels[1].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2]))
		lookupTrixels(max_level, start_trixels[2].name, start_vertices[start_trixels[2].v0], start_vertices[start_trixels[2].v1], start_vertices[start_trixels[2].v2], start_trixels[2].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2]))
		lookupTrixels(max_level, start_trixels[3].name, start_vertices[start_trixels[3].v0], start_vertices[start_trixels[3].v1], start_vertices[start_trixels[3].v2], start_trixels[3].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2]))
		lookupTrixels(max_level, start_trixels[4].name, start_vertices[start_trixels[4].v0], start_vertices[start_trixels[4].v1], start_vertices[start_trixels[4].v2], start_trixels[4].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2]))
		lookupTrixels(max_level, start_trixels[5].name, start_vertices[start_trixels[5].v0], start_vertices[start_trixels[5].v1], start_vertices[start_trixels[5].v2], start_trixels[5].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2]))
		lookupTrixels(max_level, start_trixels[6].name, start_vertices[start_trixels[6].v0], start_vertices[start_trixels[6].v1], start_vertices[start_trixels[6].v2], start_trixels[6].id, 0, true);
	if (HTM_OUTSIDE != verifyTrixel(start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2]))
		lookupTrixels(max_level, start_trixels[7].name, start_vertices[start_trixels[7].v0], start_vertices[start_trixels[7].v1], start_vertices[start_trixels[7].v2], start_trixels[7].id, 0, true);
}


//Save id and its ranges.
void
HTM::saveRangeID(uInt64 id, const std::string &name)
{
	int i, mask;
	uInt64 min, max;

	short level = name.size() - 2;
	if (level < max_level) {
		mask = (max_level - level) << 1;
		min = id << mask;
		max = min + ((uInt64) 1 << mask) - 1;
	} else {
		min = max = id;
	}
	ids.push_back(id);
	names.push_back(name);
	ranges.push_back({min, max});
}


void
HTM::getCorners(const std::string &name, Cartesian &v0, Cartesian &v1, Cartesian &v2)
{
	Cartesian w0, w1, w2;
	int trixel = (int)(name.at(1) - '0');

	if (name.at(0) == 'S') {
		v0 = start_vertices[start_trixels[trixel].v0];
		v1 = start_vertices[start_trixels[trixel].v1];
		v2 = start_vertices[start_trixels[trixel].v2];
	} else {
		trixel = trixel + 4;
		v0 = start_vertices[start_trixels[trixel].v0];
		v1 = start_vertices[start_trixels[trixel].v1];
		v2 = start_vertices[start_trixels[trixel].v2];
	}

	int i = 2, len = (int)name.size();
	while (i < len) {
		midPoint(v0, v1, w2);
		midPoint(v1, v2, w0);
		midPoint(v2, v0, w1);
		switch (name.at(i)) {
			case '0':
				v1 = w2;
				v2 = w1;
				break;
			case '1':
				v0 = v1;
				v1 = w0;
				v2 = w2;
				break;
			case '2':
				v0 = v2;
				v1 = w1;
				v2 = w0;
				break;
			case '3':
				v0 = w0;
				v1 = w1;
				v2 = w2;
				break;
		}
		i++;
	}
	return;
}


std::string
HTM::getCircle3D(int points)
{
	double inc = RAD_PER_CIRCUMFERENCE / points;
	std::string x0, y0, z0;
	char x0s[DIGITS];
	char y0s[DIGITS];
	char z0s[DIGITS];
	snprintf(x0s, DIGITS, "%.50f", region.boundingCircle.center.x);
	snprintf(y0s, DIGITS, "%.50f", region.boundingCircle.center.y);
	snprintf(z0s, DIGITS, "%.50f", region.boundingCircle.center.z);
	std::string xs = "x = [" + std::string(x0s) + "]\n";
	xs += "y = [" + std::string(y0s) + "]\n";
	xs += "z = [" + std::string(z0s) + "]\n";
	xs += "ax.plot3D(x, y, z, 'ko', linewidth = 2.0)\n\n";
	std::string ys, zs;
	if (region.boundingCircle.sign == POS) {
		xs += "x = [";
		ys = "y = [";
		zs = "z = [";
		Cartesian a = (region.boundingCircle.center.y == 0) ? Cartesian(0, 1, 0) : Cartesian(1.0, - (region.boundingCircle.center.x + region.boundingCircle.center.z) / region.boundingCircle.center.y, 1.0);
		a.normalize();
		Cartesian b = a ^ region.boundingCircle.center;

		Cartesian vc;
		for (double t = 0; t <= RAD_PER_CIRCUMFERENCE; t += inc) {
			double rc = region.boundingCircle.arcangle * cos(t);
			double rs = region.boundingCircle.arcangle * sin(t);
			vc.x = region.boundingCircle.center.x + rc * a.x + rs * b.x;
			vc.y = region.boundingCircle.center.y + rc * a.y + rs * b.y;
			vc.z = region.boundingCircle.center.z + rc * a.z + rs * b.z;
			char vcx[DIGITS];
			char vcy[DIGITS];
			char vcz[DIGITS];
			snprintf(vcx, DIGITS, "%.50f", vc.x);
			snprintf(vcy, DIGITS, "%.50f", vc.y);
			snprintf(vcz, DIGITS, "%.50f", vc.z);
			xs += std::string(vcx) + ", ";
			ys += std::string(vcy) + ", ";
			zs += std::string(vcz) + ", ";
			if (t == 0.0) {
				x0 = std::string(vcx) + ", ";
				y0 = std::string(vcy) + ", ";
				z0 = std::string(vcz) + ", ";
			}
		}
		xs += x0 + "]\n";
		ys += y0 + "]\n";
		zs += z0 + "]\n";
	}

	return xs + ys + zs;
}


void
HTM::writePython3D(const std::string &file)
{
	std::ofstream fs(file);
	std::string x("x = ["), y("y = ["), z("z = [");
	int numCorners = region.corners.size() - 1;

	fs << "from mpl_toolkits.mplot3d import Axes3D\n";
	fs << "from mpl_toolkits.mplot3d.art3d import Poly3DCollection\n";
	fs << "import matplotlib.pyplot as plt\n\n\n";
	fs << "ax = Axes3D(plt.figure())\n";
	if (numCorners >= 2) {
		for (int i = 0; i < numCorners; i++) {
			char vx[DIGITS];
			char vy[DIGITS];
			char vz[DIGITS];
			snprintf(vx, DIGITS, "%.50f", region.corners.at(i).x);
			snprintf(vy, DIGITS, "%.50f", region.corners.at(i).y);
			snprintf(vz, DIGITS, "%.50f", region.corners.at(i).z);
			x += std::string(vx) + ", ";
			y += std::string(vy) + ", ";
			z += std::string(vz) + ", ";
		}
		char v0x[DIGITS];
		char v0y[DIGITS];
		char v0z[DIGITS];
		snprintf(v0x, DIGITS, "%.50f", region.corners.at(numCorners).x);
		snprintf(v0y, DIGITS, "%.50f", region.corners.at(numCorners).y);
		snprintf(v0z, DIGITS, "%.50f", region.corners.at(numCorners).z);
		char v1x[DIGITS];
		char v1y[DIGITS];
		char v1z[DIGITS];
		snprintf(v1x, DIGITS, "%.50f", region.corners.at(0).x);
		snprintf(v1y, DIGITS, "%.50f", region.corners.at(0).y);
		snprintf(v1z, DIGITS, "%.50f", region.corners.at(0).z);
		x += std::string(v0x) + ", " + std::string(v1x) + "]\n";
		y += std::string(v0y) + ", " + std::string(v1y) + "]\n";
		z += std::string(v0z) + ", " + std::string(v1z) + "]\n";
		fs << (x + y + z) << "ax.plot3D(x, y, z, 'k-', linewidth = 2.0)\n\n";
	} else {
		fs << getCircle3D(100) << "ax.plot3D(x, y, z, 'k-', linewidth = 2.0)\n\n";
	}

	std::vector<std::string>::const_iterator itn = names.begin();
	for ( ;itn != names.end(); itn++) {
		Cartesian v0, v1, v2;
		getCorners((*itn), v0, v1, v2);
		char v0x[DIGITS];
		char v0y[DIGITS];
		char v0z[DIGITS];
		snprintf(v0x, DIGITS, "%.50f", v0.x);
		snprintf(v0y, DIGITS, "%.50f", v0.y);
		snprintf(v0z, DIGITS, "%.50f", v0.z);
		char v1x[DIGITS];
		char v1y[DIGITS];
		char v1z[DIGITS];
		snprintf(v1x, DIGITS, "%.50f", v1.x);
		snprintf(v1y, DIGITS, "%.50f", v1.y);
		snprintf(v1z, DIGITS, "%.50f", v1.z);
		char v2x[DIGITS];
		char v2y[DIGITS];
		char v2z[DIGITS];
		snprintf(v2x, DIGITS, "%.50f", v2.x);
		snprintf(v2y, DIGITS, "%.50f", v2.y);
		snprintf(v2z, DIGITS, "%.50f", v2.z);
		x = "x = [" + std::string(v0x) + ", " + std::string(v1x) + ", " + std::string(v2x) + ", " + std::string(v0x) + "]\n";
		y = "y = [" + std::string(v0y) + ", " + std::string(v1y) + ", " + std::string(v2y) + ", " + std::string(v0y) + "]\n";
		z = "z = [" + std::string(v0z) + ", " + std::string(v1z) + ", " + std::string(v2z) + ", " + std::string(v0z) + "]\n";
		fs << (x + y + z) << "ax.plot3D(x, y, z, 'r-')\n\n";
	}
	fs << "plt.ion()\nplt.grid()\nplt.show()";
	fs.close();
}