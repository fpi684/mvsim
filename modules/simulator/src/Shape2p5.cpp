/*+-------------------------------------------------------------------------+
  |                       MultiVehicle simulator (libmvsim)                 |
  |                                                                         |
  | Copyright (C) 2014-2023  Jose Luis Blanco Claraco                       |
  | Copyright (C) 2017  Borys Tymchenko (Odessa Polytechnic University)     |
  | Distributed under 3-clause BSD License                                  |
  |   See COPYING                                                           |
  +-------------------------------------------------------------------------+ */

#include <box2d/b2_settings.h>	// b2_maxPolygonVertices
#include <mrpt/maps/CSimplePointsMap.h>
#include <mrpt/math/TBoundingBox.h>
#include <mrpt/math/TLine2D.h>
#include <mrpt/math/TObject2D.h>
#include <mrpt/math/geometry.h>
#include <mvsim/Shape2p5.h>

#include <cmath>
#include <iostream>
#include <queue>

// Uncomment only for development debugging
#define DEBUG_DUMP_ALL_TEMPORARY_GRIDS

#ifdef DEBUG_DUMP_ALL_TEMPORARY_GRIDS
#include <mrpt/opengl/COpenGLScene.h>
#include <mrpt/opengl/CSetOfLines.h>
#include <mrpt/opengl/CTexturedPlane.h>
#include <mrpt/opengl/stock_objects.h>
#endif

using namespace mvsim;

using mrpt::math::TPoint2Df;
using mrpt::math::TPoint3Df;

constexpr uint8_t CELL_UNDEFINED = 0x80;
constexpr uint8_t CELL_OCCUPIED = 0x00;
constexpr uint8_t CELL_FREE = 0xff;
constexpr uint8_t CELL_VISITED = 0x40;

double Shape2p5::volume() const
{
	return std::abs(mrpt::math::signedArea(*contour_)) *
		   std::abs(zMin_ - zMax_);
}

void Shape2p5::mergeWith(const Shape2p5& s)
{
	ASSERT_(!s.contour_->empty());

#if 0
	std::vector<mrpt::math::TPoint3Df> pts;
	for (const auto& p : s.contour) pts.emplace_back(p.x, p.y, s.zMin);
	pts.emplace_back(pts.front().x, pts.front().y, s.zMax);
	mergeWith(pts);
#endif
}

void Shape2p5::mergeWith(const std::vector<mrpt::math::TPoint3Df>& pts)
{
	ASSERT_(!contour_->empty());

#if 0
	std::vector<mrpt::math::TPoint3Df> allPts;
	for (const auto& p : contour) allPts.emplace_back(p.x, p.y, zMin);
	allPts.emplace_back(allPts.front().x, allPts.front().y, zMax);

	for (const auto& p : pts) allPts.emplace_back(p);

	*this = CreateConvexHullFromPoints(allPts);
#endif
}

const mrpt::math::TPolygon2D& Shape2p5::getContour() const
{
	if (!contour_) computeShape();
	return *contour_;
}

void Shape2p5::buildInit(
	const mrpt::math::TPoint2Df& bbMin, const mrpt::math::TPoint2Df& bbMax,
	int numCells)
{
	contour_.reset();  // start from scratch

	// grid resolution:
	const float r = (bbMax - bbMin).norm() / numCells;
	// border to ensure we have a free row/col all around the shape
	const float b = r * 1.5f;
	grid_.emplace(bbMin.x - b, bbMax.x + b, bbMin.y - b, bbMax.y + b, r);

	zMin_ = std::numeric_limits<float>::max();
	zMax_ = -std::numeric_limits<float>::max();
	grid_->fill(CELL_UNDEFINED);
}

void Shape2p5::buildAddPoint(const mrpt::math::TPoint3Df& pt)
{
	mrpt::keep_max(zMax_, pt.z);
	mrpt::keep_min(zMin_, pt.z);
	uint8_t* c = grid_->cellByPos(pt.x, pt.y);
	ASSERT_(c);
	*c = CELL_OCCUPIED;
}

void Shape2p5::buildAddTriangle(const mrpt::opengl::TTriangle& t)
{
	const float step = grid_->getResolution();

	for (int i0 = 0; i0 < 3; i0++)
	{
		const int i1 = (i0 + 1) % 3;
		const auto& v0 = t.vertex(i0);
		const auto& v1 = t.vertex(i1);

		const auto v01 = v1 - v0;

		const int nSteps = static_cast<int>(std::ceil(v01.norm() / step));

		mrpt::math::TPoint3Df p = v0;
		const mrpt::math::TVector3Df Ap = v01 * (1.0f / nSteps);

		for (int s = 0; s < nSteps; s++, p += Ap)
		{
			uint8_t* c = grid_->cellByPos(p.x, p.y);
			if (!c) continue;

			*c = CELL_OCCUPIED;
			mrpt::keep_max(zMax_, p.z);
			mrpt::keep_min(zMin_, p.z);
		}
	}
}

// Computes contour_ from the contents in grid_
void Shape2p5::computeShape() const
{
	ASSERT_(grid_);
	ASSERT_(!contour_);

	// 1) Flood-fill the grid with "FREE color" to allow detecting the outer
	// shape:
	internalGridFloodFill();

	// 2) Detect the outer contour with full grid resolution:
	const mrpt::math::TPolygon2D rawGridContour = internalGridContour();

	// 3) convex hull:
	// Eventually, b2Box library will use convex hull anyway, so let's use it
	// here too:

	// 4) Polygon pruning until edge count is <= b2_maxPolygonVertices

	// 5) Save result if output structure:
	contour_.emplace();
	contour_->push_back({-0.25, -0.25});
	contour_->push_back({-0.25, 0.25});
	contour_->push_back({0.25, 0.25});
	contour_->push_back({0.25, -0.25});

// DEBUG:
#ifdef DEBUG_DUMP_ALL_TEMPORARY_GRIDS
	{
		mrpt::opengl::COpenGLScene scene;

		auto glGrid = mrpt::opengl::CTexturedPlane::Create();
		glGrid->setPlaneCorners(
			grid_->getXMin(), grid_->getXMax(), grid_->getYMin(),
			grid_->getYMax());

		mrpt::math::CMatrixDouble mat;
		grid_->getAsMatrix(mat);

		mrpt::img::CImage im;
		im.setFromMatrix(mat, false /* matrix is [0,255]*/);

		glGrid->assignImage(im);

		scene.insert(mrpt::opengl::stock_objects::CornerXYZSimple());
		scene.insert(glGrid);

		auto lambdaRenderPoly = [&scene](
									const mrpt::math::TPolygon2D& p,
									const mrpt::img::TColor& color, double z) {
			auto glPoly = mrpt::opengl::CSetOfLines::Create();
			glPoly->setColor_u8(color);
			const auto N = p.size();
			for (size_t j = 0; j < N; j++)
			{
				const size_t j1 = (j + 1) % N;
				const auto& p0 = p.at(j);
				const auto& p1 = p.at(j1);
				glPoly->appendLine(p0.x, p0.y, z, p1.x, p1.y, z);
			}
			scene.insert(glPoly);
		};

		lambdaRenderPoly(*contour_, {0xff, 0x00, 0x00}, 0.10);
		lambdaRenderPoly(rawGridContour, {0x00, 0xff, 0x00}, 0.05);

		static int i = 0;
		scene.saveToFile(mrpt::format("collision_grid_%03i.3Dscene", i++));
	}
#endif

	grid_.reset();
}

void Shape2p5::setShapeManual(
	const mrpt::math::TPolygon2D& contour, const float zMin, const float zMax)
{
	grid_.reset();
	contour_ = contour;
	zMin_ = zMin;
	zMax_ = zMax;
}

void Shape2p5::internalGridFloodFill() const
{
	ASSERT_(grid_);

	// Algorithm:
	// Heckbert, Paul S (1990). "IV.10: A Seed Fill Algorithm". In Glassner,
	// Andrew S (ed.). Graphics Gems. Academic Press. pp. 275–277.
	// https://en.wikipedia.org/wiki/Flood_fill

	const int cxMax = grid_->getSizeX() - 1;
	const int cyMax = grid_->getSizeY() - 1;

	const auto Inside = [&](int x, int y) {
		if (x < 0 || y < 0) return false;
		if (x > cxMax || y > cyMax) return false;
		uint8_t* c = grid_->cellByIndex(x, y);
		if (!c) return false;

		return *c == CELL_UNDEFINED;
	};

	const auto Set = [&](int x, int y) {
		if (x < 0 || y < 0) return;
		if (x > cxMax || y > cyMax) return;
		uint8_t* c = grid_->cellByIndex(x, y);
		if (!c) return;
		*c = CELL_FREE;
	};

	const int x0 = 0, y0 = 0;  // start pixel for flood fill.

	/*
	fn fill(x, y):
		if not Inside(x, y) then return
		let s = new empty stack or queue
		Add (x, y) to s
		while s is not empty:
			Remove an (x, y) from s
			let lx = x
			while Inside(lx - 1, y):
				Set(lx - 1, y)
				lx = lx - 1
			while Inside(x, y):
				Set(x, y)
				x = x + 1
		  scan(lx, x - 1, y + 1, s)
		  scan(lx, x - 1, y - 1, s)

	fn scan(lx, rx, y, s):
		let span_added = false
		for x in lx .. rx:
			if not Inside(x, y):
				span_added = false
			else if not span_added:
				Add (x, y) to s
				span_added = true
	*/

	if (!Inside(x0, y0)) return;

	struct Coord
	{
		Coord() = default;
		Coord(int X, int Y) : x_(X), y_(Y) {}

		int x_ = 0, y_ = 0;
	};

	std::queue<Coord> s;

	const auto lambdaScan = [&s, &Inside](int lx, int rx, int y) {
		bool spanAdded = false;
		for (int x = lx; x <= rx; x++)
		{
			if (!Inside(x, y))
			{
				spanAdded = false;
			}
			else if (!spanAdded)
			{
				s.emplace(x, y);
				spanAdded = true;
			}
		}
	};

	s.emplace(x0, y0);
	while (!s.empty())
	{
		auto [x, y] = s.front();
		s.pop();
		int lx = x;
		while (Inside(lx - 1, y))
		{
			Set(lx - 1, y);
			lx--;
		}
		while (Inside(x, y))
		{
			Set(x, y);
			x++;
		}
		lambdaScan(lx, x - 1, y + 1);
		lambdaScan(lx, x - 1, y - 1);
	}
}

// Detects the outter polygon of the grid, after having been flood filled.
mrpt::math::TPolygon2D Shape2p5::internalGridContour() const
{
	ASSERT_(grid_);

	mrpt::math::TPolygon2D p;

	const int nx = grid_->getSizeX();
	const int ny = grid_->getSizeY();

	auto lambdaCellIsBorder = [&](int cx, int cy) {
		auto* c = grid_->cellByIndex(cx, cy);
		if (!c) return false;
		if (*c != CELL_OCCUPIED) return false;

		// check 4 neighbors:
		if (auto* cS = grid_->cellByIndex(cx, cy - 1); cS && *cS == CELL_FREE)
			return true;
		if (auto* cN = grid_->cellByIndex(cx, cy + 1); cN && *cN == CELL_FREE)
			return true;
		if (auto* cE = grid_->cellByIndex(cx + 1, cy); cE && *cE == CELL_FREE)
			return true;
		if (auto* cW = grid_->cellByIndex(cx - 1, cy); cW && *cW == CELL_FREE)
			return true;

		return false;
	};

	// 1) Look for the first CELL_OCCUPIED cell:
	int cx = 0, cy = 0;
	while (*grid_->cellByIndex(cx, cy) != CELL_OCCUPIED)
	{
		cx++;
		if (cx >= nx)
		{
			cx = 0;
			cy++;
			ASSERT_(cy < ny);
		}
	}

	// 2) Iterate:
	//    - mark current cell as CELL_VISITED, add to polygon.
	//    - Look in 8 neighbors for a CELL_OCCUPIED with a CELL_FREE cell in
	//    one of its 4 main directions.
	for (;;)
	{
		auto* c = grid_->cellByIndex(cx, cy);
		ASSERT_(c);
		*c = CELL_VISITED;

		// save into polygon too:
		p.emplace_back(grid_->idx2x(cx), grid_->idx2y(cy));

		bool cellDone = false;
		for (int ix = -1; ix <= 1 && !cellDone; ix++)
		{
			for (int iy = -1; iy <= 1 && !cellDone; iy++)
			{
				if (lambdaCellIsBorder(cx + ix, cy + iy))
				{
					// Save for next iter:
					cellDone = true;
					cx = cx + ix;
					cy = cy + iy;
					break;
				}
			}
		}
		if (!cellDone) break;
	}

	return p;
}
