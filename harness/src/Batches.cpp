#include <mpp/UniformCollection.h>

#include "Batches.h"

using namespace std;

mpp::IndexedTriangleBatch* createIndexedTriangleBatch(string const& name, string const& texture, size_t indexedTriangleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr)
{
	auto indexedTriangleBatch = new mpp::IndexedTriangleBatch(
		"TestIndexedTriangles",
		{
			mpp::mesh::Vertex::DataType::Float,
			{ mpp::mesh::Vertex::DataType::Float, false },
			{ mpp::mesh::Vertex::DataType::UnsignedByte, false },
			false
		},
		16,
		indexedTriangleBatchCount,
		texture,
		[](int primCount) { return primCount + 1; },
		renderSystem,
		resourceMgr);

	indexedTriangleBatch->load();
	return indexedTriangleBatch;
}

mpp::TriangleBatch* createTriangleBatch(string const& name, string const& texture, size_t triangleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr)
{
	auto triangleBatch = new mpp::TriangleBatch(
		"TestTriangles",
		{
			mpp::mesh::Vertex::DataType::Float,
			{ mpp::mesh::Vertex::DataType::Float, false },
			{ mpp::mesh::Vertex::DataType::UnsignedByte, false },
			false,
		},
		triangleBatchCount,
		texture,
		renderSystem,
		resourceMgr);

	triangleBatch->load();
	return triangleBatch;
}

mpp::LineBatch* createLineBatch(string const& name, size_t lineBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr)
{
	auto lineBatch = new mpp::LineBatch(
		name,
		{
			mpp::mesh::Vertex::DataType::Float,
			mpp::mesh::Vertex::DataType::UnsignedByte,
			false
		},
		lineBatchCount,
		renderSystem,
		resourceMgr);

	lineBatch->load();
	return lineBatch;
}

mpp::CircleBatch* createCircleBatch(string const& name, size_t circleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr)
{
	auto circleBatch = new mpp::CircleBatch(
		name,
		{
			mpp::CircleBatchOptions::VertexOptions::Auto,
			mpp::mesh::Vertex::DataType::Float,
			mpp::mesh::Vertex::DataType::UnsignedByte,
			false
		},
		16,
		32.0f,
		4.0f,
		true,
		circleBatchCount,
		renderSystem,
		resourceMgr);

	circleBatch->load();
	return circleBatch;
}

size_t updateTriangleBatch(mpp::RenderSystem* renderSystem, mpp::TriangleBatch* triBatch, size_t count, float totalTime)
{
	triBatch->startUpdate(count);

	auto posBuffer = (float*)triBatch->getAttributeData("POSITION").first;
	auto posStride = triBatch->getAttributeData("POSITION").second / sizeof(float);

	auto texBuffer = (float*)triBatch->getAttributeData("TEXCOORDS").first;
	auto texStride = triBatch->getAttributeData("TEXCOORDS").second / sizeof(float);

	auto colBuffer = (uint8*)triBatch->getAttributeData("COLOUR").first;
	auto colStride = triBatch->getAttributeData("COLOUR").second / sizeof(uint8);

	size_t vertexCount = triBatch->getVertexCount(count);
	for (size_t pOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < vertexCount; ++i)
	{
		int primitiveIndex = i / 3;
		int vertexIndex = i % 3;

		// Position
		auto xc = 400 + sinf(totalTime * (primitiveIndex + 1)) * 100;
		auto yc = 300 + cosf(totalTime * (primitiveIndex + 2)) * 100;

		switch (vertexIndex)
		{
		case 0:
			posBuffer[pOffset + 0] = xc - 16.0f;
			posBuffer[pOffset + 1] = yc - 16.0f;
			texBuffer[tOffset + 0] = 0.0f;
			texBuffer[tOffset + 1] = 0.0f;
			break;
		case 1:
			posBuffer[pOffset + 0] = xc + 16.0f;
			posBuffer[pOffset + 1] = yc - 16.0f;
			texBuffer[tOffset + 0] = 1.0f;
			texBuffer[tOffset + 1] = 0.0f;
			break;
		case 2:
			posBuffer[pOffset + 0] = xc;
			posBuffer[pOffset + 1] = yc + 16.0f;
			texBuffer[tOffset + 0] = 0.5f;
			texBuffer[tOffset + 1] = 1.0f;
			break;
		}

		// Colour data
		colBuffer[cOffset + 0] = 255;
		colBuffer[cOffset + 1] = 255;
		colBuffer[cOffset + 2] = 255;
		colBuffer[cOffset + 3] = 255;

		pOffset += posStride;
		tOffset += texStride;
		cOffset += colStride;
	}

	triBatch->finishUpdate(count, false);

	mpp::UniformCollection uniforms;
	if (triBatch->usingDiffuse())
	{
		uniforms.setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	renderSystem->renderModelImmediate(*triBatch, false, &uniforms);
	return triBatch->getCount();
}

size_t updateIndexedTriangleBatch(mpp::RenderSystem* renderSystem, mpp::IndexedTriangleBatch* triBatch, size_t count, float totalTime)
{
	triBatch->startUpdate(count);

	auto posBuffer = (float*)triBatch->getAttributeData("POSITION").first;
	auto posStride = triBatch->getAttributeData("POSITION").second / sizeof(float);

	auto texBuffer = (float*)triBatch->getAttributeData("TEXCOORDS").first;
	auto texStride = triBatch->getAttributeData("TEXCOORDS").second / sizeof(float);

	auto colBuffer = (uint8*)triBatch->getAttributeData("COLOUR").first;
	auto colStride = triBatch->getAttributeData("COLOUR").second / sizeof(uint8);

	size_t vertexCount = triBatch->getVertexCount(count);
	for (size_t pOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < vertexCount; ++i)
	{
		// Position
		if (i == 0)
		{
			posBuffer[pOffset + 0] = 400.0f;
			posBuffer[pOffset + 1] = 300.0f;
			texBuffer[tOffset + 0] = 0.5f;
			texBuffer[tOffset + 1] = 0.5f;
		}
		else
		{
			float angle = ((i - 1) / (float)(vertexCount - 1)) * 2 * 3.14159f;
			float dx = sinf(angle);
			float dy = cosf(angle);

			posBuffer[pOffset + 0] = 400.0f + dx * 30.0f;
			posBuffer[pOffset + 1] = 300.0f + dy * 30.0f;
			texBuffer[tOffset + 0] = dx * 0.5f + 0.5f;
			texBuffer[tOffset + 1] = dy * 0.5f + 0.5f;
		}

		colBuffer[cOffset + 0] = 255;
		colBuffer[cOffset + 1] = 255;
		colBuffer[cOffset + 2] = 255;
		colBuffer[cOffset + 3] = 255;

		pOffset += posStride;
		tOffset += texStride;
		cOffset += colStride;
	}

	uint16_t* indexData = (uint16_t*)triBatch->getIndexData();
	for (size_t i = 0; i < count; ++i)
	{
		indexData[i * 3 + 0] = 0;
		indexData[i * 3 + 1] = (uint16_t)i + 1;
		indexData[i * 3 + 2] = (uint16_t)i + 2;
		if (indexData[i * 3 + 2] > count)
		{
			indexData[i * 3 + 2] = 1;
		}
	}

	triBatch->finishUpdate(count, false);

	mpp::UniformCollection uniforms;
	if (triBatch->usingDiffuse())
	{
		uniforms.setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	renderSystem->renderModelImmediate(*triBatch, false, &uniforms);
	return triBatch->getCount();
}

size_t updateLineBatch(mpp::RenderSystem* renderSystem, mpp::LineBatch* lineBatch, size_t count, float totalTime)
{
	lineBatch->startUpdate(count);

	auto posBuffer = (float*)lineBatch->getAttributeData("POSITION").first;
	auto posStride = lineBatch->getAttributeData("POSITION").second / sizeof(float);

	auto colBuffer = (uint8*)lineBatch->getAttributeData("COLOUR").first;
	auto colStride = lineBatch->getAttributeData("COLOUR").second / sizeof(uint8);

	float linesX = renderSystem->getWindowWidth() * 0.25f;
	float linesW = renderSystem->getWindowWidth() * 0.5f;
	float linesY = renderSystem->getWindowWidth() * 0.5f;
	for (uint32 i = 0; i < count; ++i)
	{
		float y0 =
			sinf(((float)i / count) * 6.2832f - totalTime) +
			sinf(((float)i / (count / 2)) * 6.2832f + 1.2f - totalTime) +
			sinf(((float)i / (count / 4)) * 6.2832f + 2.4f - totalTime) * 0.75f;

		*(posBuffer + 0) = linesX + linesW * ((float)i / count);
		*(posBuffer + 1) = linesY + 100 + y0 * 10;
		*(colBuffer + 0) = 255;
		*(colBuffer + 1) = 255;
		*(colBuffer + 2) = 255;
		*(colBuffer + 3) = 255;

		posBuffer += posStride;
		colBuffer += colStride;

		float y1 =
			cosf(((float)i / count) * 6.2832f - totalTime) +
			sinf(((float)i / (count / 4)) * 6.2832f + 3.2f - totalTime) * 0.5f +
			cosf(((float)i / (count / 3)) * 6.2832f + 5.4f - totalTime) * 0.5f;

		*(posBuffer + 0) = linesX + linesW * ((float)i / count);
		*(posBuffer + 1) = linesY - 100 - y1 * 10;
		*(colBuffer + 0) = 255;
		*(colBuffer + 1) = 255;
		*(colBuffer + 2) = 255;
		*(colBuffer + 3) = 255;

		posBuffer += posStride;
		colBuffer += colStride;
	}

	lineBatch->finishUpdate(count, false);

	mpp::UniformCollection uniforms;
	if (lineBatch->usingDiffuse())
	{
		uniforms.setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	renderSystem->renderModelImmediate(*lineBatch, false, &uniforms);
	return lineBatch->getCount();
}

size_t updateCircleBatch(mpp::RenderSystem* renderSystem, mpp::CircleBatch* circleBatch, size_t count, float totalTime)
{
	circleBatch->startUpdate(count);

	auto posBuffer = (float*)circleBatch->getAttributeData("POSITION").first;
	auto posStride = circleBatch->getAttributeData("POSITION").second / sizeof(float);

	auto optBuffer = (float*)circleBatch->getAttributeData("OPTIONS").first;
	auto optStride = circleBatch->getAttributeData("OPTIONS").second / sizeof(float);

	auto borderBuffer = (uint8_t*)circleBatch->getAttributeData("BORDERCOLOUR").first;
	auto borderStride = circleBatch->getAttributeData("BORDERCOLOUR").second / sizeof(uint8_t);

	auto colourBuffer = (uint8_t*)circleBatch->getAttributeData("INNERCOLOUR").first;
	auto colourStride = circleBatch->getAttributeData("INNERCOLOUR").second / sizeof(uint8_t);

	size_t vertexCount = circleBatch->getVertexCount(circleBatch->getPrimitiveCount(count));
	for (size_t pOffset = 0, sOffset = 0, bOffset = 0, cOffset = 0, i = 0; i < vertexCount; ++i)
	{
		// If we're using points, then store size in position.z, and if not, then
		// generate it (we don't need to store it)
		// But then need to store border

		float radius = 32;// +sinf(totalTime) * 12;

		// Position/rotation data
		if (circleBatch->usingPointSprites())
		{
			// One vertex per quad
			posBuffer[pOffset + 0] = 400 + sinf(totalTime * 0.1f * (i + 1)) * 100;
			posBuffer[pOffset + 1] = 300 + cosf(totalTime * 0.1f * (i + 2)) * 100;
		}
		else
		{
			// Indexed, four vertices per quad
			int primitiveIndex = i / 4;
			int vertexIndex = i % 4;

			auto xc = 400 + sinf(totalTime * (primitiveIndex + 1)) * 100;
			auto yc = 300 + cosf(totalTime * (primitiveIndex + 2)) * 100;

			switch (vertexIndex)
			{
			case 0:
				posBuffer[pOffset + 0] = xc - radius;
				posBuffer[pOffset + 1] = yc - radius;
				posBuffer[pOffset + 2] = 0.0f;
				posBuffer[pOffset + 3] = 0.0f;
				break;
			case 1:
				posBuffer[pOffset + 0] = xc + radius;
				posBuffer[pOffset + 1] = yc - radius;
				posBuffer[pOffset + 2] = 1.0f;
				posBuffer[pOffset + 3] = 0.0f;
				break;
			case 2:
				posBuffer[pOffset + 0] = xc + radius;
				posBuffer[pOffset + 1] = yc + radius;
				posBuffer[pOffset + 2] = 1.0f;
				posBuffer[pOffset + 3] = 1.0f;
				break;
			case 3:
				posBuffer[pOffset + 0] = xc - radius;
				posBuffer[pOffset + 1] = yc + radius;
				posBuffer[pOffset + 2] = 0.0f;
				posBuffer[pOffset + 3] = 1.0f;
				break;
			}
		}

		// Options data
		optBuffer[sOffset + 0] = radius;
		optBuffer[sOffset + 1] = 4.0f;
		optBuffer[sOffset + 2] = 0.5f;
		optBuffer[sOffset + 3] = 0.0f;

		// Border colour data
		borderBuffer[bOffset + 0] = 0;
		borderBuffer[bOffset + 1] = 0;
		borderBuffer[bOffset + 2] = 255;
		borderBuffer[bOffset + 3] = 255;

		// Inner colour data
		colourBuffer[cOffset + 0] = 255;
		colourBuffer[cOffset + 1] = 255;
		colourBuffer[cOffset + 2] = 255;
		colourBuffer[cOffset + 3] = 255;

		pOffset += posStride;
		sOffset += optStride;
		bOffset += borderStride;
		cOffset += colourStride;
	}

	circleBatch->finishUpdate(count, false);

	mpp::UniformCollection uniforms;
	if (circleBatch->usingDiffuse())
	{
		uniforms.setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	renderSystem->renderModelImmediate(*circleBatch, true, &uniforms, circleBatch->getPrimitiveCount(circleBatch->getCount()));
	return circleBatch->getCount();
}