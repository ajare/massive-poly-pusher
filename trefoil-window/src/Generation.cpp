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

	const size_t numVertices{ 32 };
	for (size_t i = 0; i < trefoil.numFoils; ++i)
	{
		input[i].resize(numVertices * 2 - 2);

		for (size_t j = 0; j < numVertices; ++j)
		{
			float t = j / (float)(numVertices - 1);

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

	// Morphed pane lines
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

			auto trefoil = window->getPaneTrefoil();
			trefoil.radius += 10;

			auto paneVertices = generateTrefoilPane_HM(window, trefoil, Vector2(xPos, yPos), width, height, trefoilOffset);
			copy(paneVertices.begin(), paneVertices.end(), back_inserter(vertices));
		}
		catch (exception&)
		{
			continue;
		}
	}

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
		size_t j = i == 0 ? vertices.size() - 1 : i - 1;
		if (vertex.distanceToSq(vertices[j]) < epsilonSq)
		{
			continue;
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

vector<Vector3> getTriangulation(
	vector<p2t::Point*> const& border,
	vector<vector<p2t::Point*>> const& holes,
	float depth)
{
	p2t::CDT* cdt = new p2t::CDT(border);

	for (auto const& hole : holes)
	{
		auto reversed = hole;
		//reverse(reversed.begin(), reversed.end());
		cdt->AddHole(reversed);
	}

	cdt->Triangulate();

	// Triangulation
	vector<Vector3> triangleData;
	map<p2t::Point*, uint32_t> vertexMap;

	vector<Vector3> finalPoints;
	vector<Vector2> triPoints;
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

			Vector3 point(triPoints[vertexMap[p]].x, triPoints[vertexMap[p]].y, depth);
			point.nx = 0;
			point.ny = 0;
			point.nz = 1;

			finalPoints.push_back(point);
		}
	}

	delete cdt;
	return finalPoints;
}

vector<p2t::Point*> copyPointList(vector<p2t::Point*> const& points)
{
	vector<p2t::Point*> copy;
	copy.reserve(points.size());

	for (auto point : points)
	{
		copy.push_back(new p2t::Point(point->x, point->y));
	}

	return copy;
}

vector<Vector3> generateTriangles(TrefoilWindow const* window, VertexList const& lines)
{
	VertexList points;
	vector<Vector3> finalPoints;

	float thickness = window->getFrameThickness() * 0.5f;

	const size_t BORDER_ID = 0;
	const size_t PANE1BORDER_ID = 1;
	const size_t PANE1_ID = PANE1BORDER_ID + window->getNumPanes();
	const size_t UPPER_ID = PANE1_ID + window->getNumPanes();

	// Preprocess lines 
	for (auto const& loop: lines)
	{
		points.push_back(preprocessLoop(loop));
	}

	// Generate p2d border points
	vector<p2t::Point*> borderPoints;
	vector<p2t::Point*> upperTrefoilPoints;
	vector<vector<p2t::Point*>> paneBorderPoints1, paneBorderPoints2;
	vector<vector<p2t::Point*>> paneInnerPoints;

	for (auto const& p : points[BORDER_ID])
	{
		borderPoints.push_back(new p2t::Point(p.x, p.y));
	}
	if (pointsWinding(points[BORDER_ID]) != Winding::Clockwise)
	{
		reverse(borderPoints.begin(), borderPoints.end());
	}	

	for (auto const& p : points[UPPER_ID])
	{
		upperTrefoilPoints.push_back(new p2t::Point(p.x, p.y));
	}
	if (pointsWinding(points[UPPER_ID]) != Winding::Clockwise)
	{
		reverse(upperTrefoilPoints.begin(), upperTrefoilPoints.end());
	}

	for (size_t i = 0; i < window->getNumPanes(); ++i)
	{
		vector<p2t::Point*> pointsList;
		for (auto const& p : points[PANE1BORDER_ID + i])
		{
			pointsList.push_back(new p2t::Point(p.x, p.y));
		}
		if (pointsWinding(points[PANE1BORDER_ID + i]) != Winding::Clockwise)
		{
			reverse(pointsList.begin(), pointsList.end());
		}

		paneBorderPoints1.push_back(pointsList);
		paneBorderPoints2.push_back(copyPointList(pointsList));

		pointsList.clear();
		for (auto const& p : points[PANE1_ID + i])
		{
			pointsList.push_back(new p2t::Point(p.x, p.y));
		}
		if (pointsWinding(points[PANE1_ID + i]) != Winding::Clockwise)
		{
			reverse(pointsList.begin(), pointsList.end());
		}

		paneInnerPoints.push_back(pointsList);
	}

	// Main
	vector<vector<p2t::Point*>> mainHoles = paneBorderPoints1;
	mainHoles.push_back(upperTrefoilPoints);

	auto mainTriangles = getTriangulation(borderPoints, mainHoles, thickness);
	copy(mainTriangles.begin(), mainTriangles.end(), back_inserter(finalPoints));

	// Panes
	for (size_t i = 0; i < window->getNumPanes(); ++i)
	{
		auto paneTriangles = getTriangulation(paneBorderPoints2[i], { paneInnerPoints[i] }, thickness * 0.5f);
		copy(paneTriangles.begin(), paneTriangles.end(), back_inserter(finalPoints));
	}

	/*
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
	*/

	// Clean up
	for (auto point : borderPoints)
	{
		delete point;
	}

	for (auto point : upperTrefoilPoints)
	{
		delete point;
	}

	for (size_t i = 0; i < window->getNumPanes(); ++i)
	{
		for (auto point : paneBorderPoints1[i])
		{
			delete point;
		}
		for (auto point : paneBorderPoints2[i])
		{
			delete point;
		}
		for (auto point : paneInnerPoints[i])
		{
			delete point;
		}
	}

	return finalPoints;
}