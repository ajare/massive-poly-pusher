#include <mpp/UniformCollection.h>

#include "Batches.h"

using namespace std;

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