#include <map>

#include <poly2tri/poly2tri.h>

#include "clipper.hpp"
#include "Generation.h"
#include "Vector2.h"
#include "CentripetalCatmullRomSpline.h"
#include "BezierSpline.h"

using namespace std;

float minX(vector<Vector2> const& vertices, uint32_t* index)
{
	float extent{ 1e10f };
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		if (vertices[i].x < extent)
		{
			extent = vertices[i].x;
			if (index)
			{
				*index = i;
			}
		}
	}

	return extent;
}

float minY(vector<Vector2> const& vertices, uint32_t* index)
{
	float extent{ 1e10f };
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		if (vertices[i].y < extent)
		{
			extent = vertices[i].y;
			if (index)
			{
				*index = i;
			}
		}
	}

	return extent;
}

float maxX(vector<Vector2> const& vertices, uint32_t* index)
{
	float extent{ -1e10f };
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		if (vertices[i].x > extent)
		{
			extent = vertices[i].x;
			if (index)
			{
				*index = i;
			}
		}
	}

	return extent;
}

float maxY(vector<Vector2> const& vertices, uint32_t* index = nullptr)
{
	float extent{ -1e10f };
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		if (vertices[i].y > extent)
		{
			extent = vertices[i].y;
			if (index)
			{
				*index = i;
			}
		}
	}

	return extent;
}

Winding pointsWinding(vector<Vector2> const& points)
{
	float sum = 0.0f;

	size_t numPoints = points.size();
	for (size_t i = 0; i < numPoints; ++i)
	{
		auto const& point1 = points[i];
		auto const& point2 = points[(i + 1) % numPoints];

		sum += (point2.x - point1.x) * (point2.y + point1.y);
	}

	return sum >= 0 ? Winding::Clockwise : Winding::Anticlockwise;
}

VertexList generateTrefoil(TrefoilWindow const* window, Trefoil const& trefoil, Vector2 const& position)
{
	ClipperLib::Clipper clipper;
	ClipperLib::Paths input(trefoil.numFoils);

	float clipperScale{ 1000.0f };
	/*
	float r = trefoil.radius;
	float rStretch = 2.0f;
	auto tr = window->getTrefoilControlRadius();

	auto const& cpos1 = window->getTrefoilControlPosition();
	Vector2 cdir1(1, -1); cdir1.normalise(); cdir1 *= tr;

	Vector2 cpos2(cpos1.x, -cpos1.y);
	Vector2 cdir2(-1, -1); cdir2.normalise(); cdir2 *= tr;

	vector<Vector2> points
	{
		{ 0.0f * r, 1.0f * r },
		(cpos1 - cdir1) * r,
		(cpos1 + cdir1) * r,
		{ 1.0f * r, 0.0f * r },
		(cpos2 - cdir2) * r,
		(cpos2 + cdir2) * r,
		{ 0.0f * r, -1.0f * r }
	};

	points[4].y *= rStretch;
	points[5].y *= rStretch;
	points[6].y *= rStretch;

	BezierSpline spline(points);
	float splineLength = spline.getLength();
	*/
	const size_t numVertices{ 32 };
	for (size_t i = 0; i < trefoil.numFoils; ++i)
	{
		input[i].resize(numVertices * 2 - 2);

		for (size_t j = 0; j < numVertices; ++j)
		{
			float t = j / (float)(numVertices - 1);
			/*
			Vector2 v = spline.getPosition(splineLength * t);
			v.y += trefoil.distance;

			Vector2 m(-v.x, v.y);
			*/

			// Teardrop 1
			// x = sin(t) * sin(t/2)^m
			// y = cos(t)

			// Teardrop 2
			// a * x * x − (1 − y)^3 * (1 + y) = 0 


			// Pear
			// b^2 * y^2 = x^3(a - x)
			// y = sqrt(x^3(a - x)/b * b)
			/*
			float a = 4;
			float y = t * 2 - 1;
			float omt = 1 - y;
			float x0 = sqrtf(omt * omt * omt * (1 + y) / a);
			float x1 = -x0;

			x0 *= trefoil.radius;
			x1 *= trefoil.radius;

			y = -y;
			y *= trefoil.radius;// *0.75f;
			y += trefoil.distance;
			*/
			float x0 = sinf(t * 3.14159f) * trefoil.radius;
			float y = cos(t * 3.14159f) * trefoil.radius;
			y += trefoil.distance;
			float x1 = -x0;

			Vector2 v(x0, y);
			Vector2 m(x1, y);
			
			v.rotateClockwise(trefoil.foilOffset + i * 360.0f / trefoil.numFoils);
			m.rotateClockwise(trefoil.foilOffset + i * 360.0f / trefoil.numFoils);
			
			v += position;
			m += position;

			// Scale for clipper as it uses ints
			v *= clipperScale;
			input[i][j] = ClipperLib::IntPoint((int)v.x, (int)v.y);

			// Mirror
			if (j > 0 && j < numVertices - 1)
			{
				m *= clipperScale;
				input[i][numVertices * 2 - 2 - j] = ClipperLib::IntPoint((int)m.x, (int)m.y);
			}
		}
	}

	// Clip
	ClipperLib::Paths work[2];
	work[0].push_back(input[0]);
	for (size_t i = 1; i < trefoil.numFoils; ++i)
	{
		auto workId = i % 2;

		clipper.AddPaths(work[1 - workId], ClipperLib::ptSubject, true);
		clipper.AddPath(input[i], ClipperLib::ptClip, true);
		clipper.Execute(ClipperLib::ctUnion, work[workId]);
		clipper.Clear();
	}

	// Clip against triangle
	ClipperLib::Path triPath;
	for (size_t i = 0; i < trefoil.numFoils; ++i)
	{
		Vector2 v(0, trefoil.distance);

		v.rotateClockwise(trefoil.foilOffset + i * 360.0f / trefoil.numFoils);
		v += position;

		v *= clipperScale;

		triPath.push_back(ClipperLib::IntPoint((int)v.x, (int)v.y));
	}

	clipper.AddPaths(work[1 - (trefoil.numFoils % 2)], ClipperLib::ptSubject, true);
	clipper.AddPath(triPath, ClipperLib::ptClip, true);
	clipper.Execute(ClipperLib::ctUnion, work[trefoil.numFoils % 2]);
	clipper.Clear();

	// Get ouput
	VertexList vertices;
	for (auto const& path: work[trefoil.numFoils % 2])
	{
		vector<Vector2> foil;

		for (auto const& point : path)
		{
			foil.push_back(Vector2(point.X / clipperScale, point.Y / clipperScale));
		}

		// Remove holes
		if (pointsWinding(foil) == Winding::Anticlockwise)
		{
			vertices.push_back(foil);
		}
	}

	return vertices;
}

vector<vector<Vector2>> generateTrefoilPane_Classic(TrefoilWindow const* window, Trefoil const& trefoil, Vector2 const& position, float paneHeight)
{
	// Calculate trefoil position: get max Y
	auto vertices = generateTrefoil(window, trefoil, Vector2::ZERO);

	float trefoilMaxY{ -1e10f };
	for (auto const& path : vertices)
	{
		auto maxy = maxY(path);
		if (maxy > trefoilMaxY)
		{
			trefoilMaxY = maxy;
		}
	}

	// Move trefoil up
	for (auto& path : vertices)
	{
		for (auto& vertex : path)
		{
			vertex += position;
			vertex.y += (paneHeight - trefoilMaxY);
		}
	}

	// Get the x-extents
	int minPath{ -1 }, maxPath{ -1 };
	int minIndex{ -1 }, maxIndex{ -1 };
	float minExtent{ 1e10f }, maxExtent{ -1e10f };
	float extentY1{ -1e10f }, extentY2{ -1e10f }, maxY{ -1e10f };

	for (size_t pathIndex = 0; pathIndex < vertices.size(); ++pathIndex)
	{
		auto const& path = vertices[pathIndex];

		for (size_t vertexIndex = 0; vertexIndex < path.size(); ++vertexIndex)
		{
			if (path[vertexIndex].x < minExtent)
			{
				minPath = pathIndex;
				minIndex = vertexIndex;
				minExtent = path[vertexIndex].x;

				extentY1 = path[vertexIndex].y;
			}
			if (path[vertexIndex].x > maxExtent)
			{
				maxPath = pathIndex;
				maxIndex = vertexIndex;
				maxExtent = path[vertexIndex].x;

				extentY2 = path[vertexIndex].y;
			}

			if (path[vertexIndex].y > maxY)
			{
				maxY = path[vertexIndex].y;
			}
		}
	}

	// Can't have them in different paths
	if (minPath != maxPath)
	{
		throw exception();
	}

	float extentSize = maxExtent - minExtent;

	// Remove all vertices beween extents, except for two
	vector<vector<Vector2>> result;

	for (size_t pathIndex = 0; pathIndex < vertices.size(); ++pathIndex)
	{
		auto const& path = vertices[pathIndex];
		if (pathIndex != minPath)
		{
			result.push_back(path);
		}
		else
		{
			vector<Vector2> panel;

			if (minIndex > maxIndex)
			{
				swap(minIndex, maxIndex);
			}

			copy(path.begin() + minIndex, path.begin() + maxIndex, back_inserter(panel));

			// Add drops at end
			float drop = paneHeight - (maxY - extentY1);
			panel.push_back(panel.back() + Vector2(0, -drop));
			panel.push_back(panel.back() + Vector2(extentSize, 0));

			result.push_back(panel);
		}
	}

	return result;
}

VertexList generateTrefoilPane_HM(TrefoilWindow const* window, Trefoil const& trefoil, Vector2 const& position, float paneWidth, float paneHeight, float trefoilOffset)
{
	// Calculate trefoil position: get max Y
	auto trefoilVertices = generateTrefoil(window, trefoil, Vector2(0, paneHeight + trefoilOffset));

	// HM-style trefoil is done by union of a regular trefoil, a circle, and a rectangle
	float paneUpperY = paneHeight - paneWidth / 2.0f;
	vector<Vector2> circleVertices;

	// Top
	/* Semicircle
	for (size_t i = 0; i < 100; ++i)
	{
		Vector2 p(0, paneWidth / 2.0f);
		p.rotateClockwise(360 * i / 99.0f);

		p.y += paneUpperY;
		circleVertices.push_back(p);
	}
	*/

	for (size_t i = 0; i < 25; ++i)
	{
		Vector2 p(-paneWidth / 2, 0);
		p.rotateClockwiseAround(Vector2(p.x + trefoil.radius, p.y), 90 * i / 24.0f);

		p.y += paneUpperY;
		circleVertices.push_back(p);
	}
	for (size_t i = 0; i < 25; ++i)
	{
		Vector2 p(paneWidth / 2, 0);
		p.rotateAnticlockwiseAround(Vector2(p.x - trefoil.radius, p.y), 90 - 90 * i / 24.0f);

		p.y += paneUpperY;
		circleVertices.push_back(p);
	}

	// Rectangle
	vector<Vector2> rectVertices
	{
		Vector2(-paneWidth / 2, paneUpperY),
		Vector2(paneWidth / 2, paneUpperY),
		Vector2(paneWidth / 2, 0),
		Vector2(-paneWidth / 2, 0)
	};

	ClipperLib::Clipper clipper;
	ClipperLib::Paths trefoilPaths(trefoilVertices.size()), output;
	ClipperLib::Path circlePath, rectPath;
	float clipperScale{ 1000.0f };

	for (size_t i = 0; i < trefoilVertices.size(); ++i)
	{
		for (auto const& vertex : trefoilVertices[i])
		{
			trefoilPaths[i] << ClipperLib::IntPoint((int)(vertex.x * clipperScale), (int)(vertex.y * clipperScale));
		}
	}

	for (auto const& vertex: circleVertices)
	{
		circlePath << ClipperLib::IntPoint((int)(vertex.x * clipperScale), (int)(vertex.y * clipperScale));
	}

	for (auto const& vertex : rectVertices)
	{
		rectPath << ClipperLib::IntPoint((int)(vertex.x * clipperScale), (int)(vertex.y * clipperScale));
	}

	ClipperLib::Paths tempOut;

	clipper.AddPath(circlePath, ClipperLib::ptSubject, true);
	clipper.AddPath(rectPath, ClipperLib::ptClip, true);
	clipper.Execute(ClipperLib::ctUnion, tempOut);
	clipper.Clear();

	clipper.AddPaths(tempOut, ClipperLib::ptSubject, true);
	clipper.AddPaths(trefoilPaths, ClipperLib::ptClip, true);
	clipper.Execute(ClipperLib::ctUnion, output);

	// Get output
	VertexList vertices;
	for (auto const& path: output)
	{
		vector<Vector2> verts;

		for (auto const& point: path)
		{
			auto vert = Vector2(point.X / clipperScale, point.Y / clipperScale);
			vert += position;

			verts.push_back(vert);
		}

		// Remove holes
		if (pointsWinding(verts) == Winding::Anticlockwise)
		{
			vertices.push_back(verts);
		}
	}

	return vertices;
}

vector<Vector2> generateBorder(TrefoilWindow const* window, float x, float y, float width, float height, float shoulderHeight, float arcScale)
{
	vector<Vector2> vertices;

	// Generate control points
	vector<Vector2> splinePoints = window->getArcVertices(0, width, height, shoulderHeight, arcScale);

	CentripetalCatmullRomSpline spline(splinePoints);

	auto length = spline.getLength();
	size_t numPoints{ 50 };
	for (size_t i = 0; i < numPoints; ++i)
	{
		float t = i / (float)(numPoints - 1);
		auto pos = spline.getPosition(length * t);
		pos.x += x;
		pos.y += y;

		vertices.push_back(pos);
	}

	vertices.push_back(Vector2(x + width / 2, y));
	vertices.push_back(Vector2(x - width / 2, y));

	for (size_t i = 0; i < numPoints; ++i)
	{
		float t = i / (float)(numPoints - 1);
		auto pos = spline.getPosition(length * (1.0f - t));
		pos.x = -pos.x;
		pos.x += x;
		pos.y += y;

		vertices.push_back(pos);
	}

	return vertices;
}

VertexList generateLines(TrefoilWindow const* window)
{
	VertexList vertices;

	// Border
	auto borderVertices = generateBorder(window,
		0,
		0,
		window->getWidth(),
		window->getHeight(),
		window->getShoulderHeight(),
		1);
	vertices.push_back(borderVertices);

	// Pane borders
	auto pane1borderVertices = generateBorder(window,
		-(window->getPaneWidth() + window->getPaneSpacing()) / 2.0f,
		10,
		window->getPaneWidth() + 20,
		window->getPaneHeight() + window->getPaneTrefoilOffset() + window->getPaneTrefoil().getHeight(),
		window->getPaneHeight(),
		0.3f);
	vertices.push_back(pane1borderVertices);

	auto pane2borderVertices = generateBorder(window,
		(window->getPaneWidth() + window->getPaneSpacing()) / 2.0f,
		10,
		window->getPaneWidth() + 20,
		window->getPaneHeight() + window->getPaneTrefoilOffset() + window->getPaneTrefoil().getHeight(),
		window->getPaneHeight(),
		0.3f);
	vertices.push_back(pane2borderVertices);

	// Panes
	for (size_t i = 0; i < window->getNumPanes(); ++i)
	{
		VertexList paneVertices;
		try
		{
			float width = window->getPaneWidth();
			float xPos = -(window->getWidth() / 2.0f) +
				window->getPaneSideOffset() +
				(i + 0.5f) * width +
				i * window->getPaneSpacing();

			float yPos = window->getPaneBaseOffset();
			float height = window->getPaneHeight();
			float trefoilOffset = window->getPaneTrefoilOffset();

			auto paneVertices = generateTrefoilPane_HM(window, window->getPaneTrefoil(), Vector2(xPos, yPos), width, height, trefoilOffset);
			copy(paneVertices.begin(), paneVertices.end(), back_inserter(vertices));
		}
		catch (exception&)
		{
			continue;
		}
	}

	// Upper trefoil
	auto upperVertices = generateTrefoil(window, window->getUpperTrefoil(), Vector2(0, window->getUpperTrefoilPosition()));
	copy(upperVertices.begin(), upperVertices.end(), back_inserter(vertices));

	return vertices;
}

bool pointsFormLine(vector<Vector2> const& points, int offset)
{
	auto numPoints = points.size();

	auto const& point1 = points[offset + 0];
	auto const& point2 = points[(offset + 1) % numPoints];
	auto const& point3 = points[(offset + 2) % numPoints];

	float area = fabs(point1.x * (point2.y - point3.y) + point2.x * (point3.y - point1.y) + point3.x * (point1.y - point2.y));
	return area < 0.0001f;
}

vector<Vector2> preprocessLoop(vector<Vector2> const& vertices)
{
	vector<Vector2> points;

	float epsilonSq = 0.0001f;

	for (size_t i = 0; i < vertices.size(); ++i)
	{
		Vector2 const& vertex = vertices[i];

		// Don't allow multiple vertices too close to each other as this can crash the triangulator.
		if (vertex.distanceToSq(vertices[i - 1]) < epsilonSq)
		{
			continue;
		}

		// Check last point against first
		if (i == vertices.size() - 1)
		{
			if (vertex.distanceToSq(vertices[0]) < epsilonSq)
			{
				continue;
			}
		}

		// Don't allow collinear points, ie three or more on a straight line.
		if (i > 0)
		{
			if (pointsFormLine(vertices, i - 1))
			{
				continue;
			}
		}

		points.push_back(vertex);
	}

	return points;
}

uint32_t addVertex(float x, float y, vector<Vector2>& vertices)
{
	auto index = vertices.size();
	
	vertices.push_back(Vector2(x, y));
	return index;
}

vector<Vector3> generateTriangles(TrefoilWindow const* window, VertexList const& lines)
{
	VertexList points;

	float thickness = window->getFrameThickness() * 0.5f;

	const size_t BORDER_ID = 0;
	const size_t PANE1BORDER_ID = 1;
	const size_t PANE1_ID = PANE1BORDER_ID + window->getNumPanes();
	const size_t UPPER_ID = PANE1_ID + window->getNumPanes();

	for (auto const& loop: lines)
	{
		points.push_back(preprocessLoop(loop));
	}

	vector<p2t::Point*> borderPoints;
	for (auto const& p: points[BORDER_ID])
	{
		borderPoints.push_back(new p2t::Point(p.x, p.y));
	}

	// Triangulate
	p2t::CDT* cdt = new p2t::CDT(borderPoints);

	vector<vector<p2t::Point*>> paneBorderList;
	for (size_t i = 0; i < window->getNumPanes(); ++i)
	{
		vector<p2t::Point*> paneBorderPoints;
		for (auto it = points[PANE1BORDER_ID + i].rbegin(); it != points[PANE1BORDER_ID + i].rend(); ++it)
		{
			auto const& p = *it;
			paneBorderPoints.push_back(new p2t::Point(p.x, p.y));
		}

		cdt->AddHole(paneBorderPoints);
		paneBorderList.push_back(paneBorderPoints);
	}

	vector<p2t::Point*> upperTrefoilPoints;
	for (auto it = points[UPPER_ID].rbegin(); it != points[UPPER_ID].rend(); ++it)
	{
		auto const& p = *it;
		upperTrefoilPoints.push_back(new p2t::Point(p.x, p.y));
	}

	cdt->AddHole(upperTrefoilPoints);

	cdt->Triangulate();

	// Extract triangle data
	map<p2t::Point*, uint32_t> vertexMap;

	vector<Vector2> triPoints;
	vector<Vector3> finalPoints;
	auto const& triangles = cdt->GetTriangles();
	for (auto const& triangle : triangles)
	{
		for (int i = 0; i < 3; ++i)
		{
			p2t::Point* p = triangle->GetPoint(i);
			if (vertexMap.find(p) == vertexMap.end())
			{
				// Add new vertex
				vertexMap[p] = addVertex((float)p->x, (float)p->y, triPoints);
			}

			Vector3 point(triPoints[vertexMap[p]].x, triPoints[vertexMap[p]].y, thickness);
			point.nx = 0;
			point.ny = 0;
			point.nz = 1;

			finalPoints.push_back(point);
		}
	}

	// Copy for back
	auto numVertices = finalPoints.size();
	for (size_t i = 0; i < numVertices; ++i)
	{
		auto fp = finalPoints[i];

		fp.pz -= window->getFrameThickness();
		fp.nz = -fp.nz;

		finalPoints.push_back(fp);
	}

	// Fill in gap between

	// Tidy up
	delete cdt;

	for (auto p: borderPoints)
	{
		delete p;
	}

	for (auto const& pb: paneBorderList)
	{
		for (auto p: pb)
		{
			delete p;
		}
	}

	for (auto p : upperTrefoilPoints)
	{
		delete p;
	}

	// Border side
	float frameThickness = window->getFrameThickness();
	for (size_t i = 0; i < points[BORDER_ID].size(); ++i)
	{
		auto const& thisPoint = points[BORDER_ID][i];
		auto const& nextPoint = i == points[BORDER_ID].size() - 1 ? points[BORDER_ID][0] : points[BORDER_ID][i + 1];

		Vector2 perp = -(nextPoint - thisPoint).perpendicular();

		// 0
		Vector3 thisP(thisPoint.x, thisPoint.y, -thickness);
		thisP.nx = perp.x;
		thisP.ny = perp.y;
		thisP.nz = 0;
		thisP.r = 0.0f;
		thisP.g = 1.0f;
		thisP.b = 0.0f;

		finalPoints.push_back(thisP);

		// 1
		Vector3 nextP(nextPoint.x, nextPoint.y, -thickness);
		nextP.nx = perp.x;
		nextP.ny = perp.y;
		nextP.nz = 0;
		nextP.r = 0.0f;
		nextP.g = 1.0f;
		nextP.b = 0.0f;

		finalPoints.push_back(nextP);

		// 2
		nextP.pz = thickness;
		finalPoints.push_back(nextP);

		// 3
		finalPoints.push_back(nextP);

		// 4
		thisP.pz = thickness;
		finalPoints.push_back(thisP);

		// 5
		thisP.pz = -thickness;
		finalPoints.push_back(thisP);
	}

	// Pane sides
	for (size_t i = 0; i < window->getNumPanes(); ++i)
	{
		auto const& pp = points[PANE1BORDER_ID + i];
		for (size_t j = 0; j < pp.size(); ++j)
		{
			auto const& thisPoint = pp[j];
			auto const& nextPoint = j == pp.size() - 1 ? pp[0] : pp[j + 1];

			Vector2 perp = -(nextPoint - thisPoint).perpendicular();

			// 0
			Vector3 thisP(thisPoint.x, thisPoint.y, -thickness);
			thisP.nx = perp.x;
			thisP.ny = perp.y;
			thisP.nz = 0;
			thisP.r = 0.0f;
			thisP.g = 0.0f;
			thisP.b = 1.0f;

			finalPoints.push_back(thisP);

			// 1
			Vector3 nextP(nextPoint.x, nextPoint.y, -thickness);
			nextP.nx = perp.x;
			nextP.ny = perp.y;
			nextP.nz = 0;
			nextP.r = 0.0f;
			nextP.g = 0.0f;
			nextP.b = 1.0f;

			finalPoints.push_back(nextP);

			// 2
			nextP.pz = thickness;
			finalPoints.push_back(nextP);

			// 3
			finalPoints.push_back(nextP);

			// 4
			thisP.pz = thickness;
			finalPoints.push_back(thisP);

			// 5
			thisP.pz = -thickness;
			finalPoints.push_back(thisP);
		}
	}

	// Upper trefoil inside
	for (size_t i = 0; i < points[UPPER_ID].size(); ++i)
	{
		auto const& thisPoint = points[UPPER_ID][i];
		auto const& nextPoint = i == points[UPPER_ID].size() - 1 ? points[UPPER_ID][0] : points[UPPER_ID][i + 1];

		Vector2 perp = (nextPoint - thisPoint).perpendicular();

		// 0
		Vector3 thisP(thisPoint.x, thisPoint.y, -thickness);
		thisP.nx = perp.x;
		thisP.ny = perp.y;
		thisP.nz = 0;
		thisP.r = 1.0f;
		thisP.g = 0.0f;
		thisP.b = 0.0f;

		finalPoints.push_back(thisP);

		// 1
		Vector3 nextP(nextPoint.x, nextPoint.y, -thickness);
		nextP.nx = perp.x;
		nextP.ny = perp.y;
		nextP.nz = 0;
		nextP.r = 1.0f;
		nextP.g = 0.0f;
		nextP.b = 0.0f;

		finalPoints.push_back(nextP);

		// 2
		nextP.pz = thickness;
		finalPoints.push_back(nextP);

		// 3
		finalPoints.push_back(nextP);

		// 4
		thisP.pz = thickness;
		finalPoints.push_back(thisP);

		// 5
		thisP.pz = -thickness;
		finalPoints.push_back(thisP);
	}

	return finalPoints;
}