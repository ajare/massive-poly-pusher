#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/LineBatch.h>

#include <mpp/mesh/VertexTypeSpecification.h>

// Base data provider classes: one for colour data, and one
// specialization where it's set to None
template<typename PosType, typename ColType = mpp::mesh::DataTypeNone>
class LineBatchDataProvider
{
	size_t mNumLines{ 0 };

public:

	virtual void position(uint32 index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0, typename PosType::builtin_type& x1, typename PosType::builtin_type& y1) = 0;

	virtual void colour(uint32 index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha) = 0;

	virtual mpp::Colour diffuse() = 0;

	void setNumLines(size_t numLines)
	{
		mNumLines = numLines;
	}

	size_t getNumLines() const
	{
		return mNumLines;
	}

	virtual void update(float frameTime) {}
};

template<typename PosType>
class LineBatchDataProvider<PosType, mpp::mesh::DataTypeNone>
{
	size_t mNumLines{ 0 };

public:

	virtual void position(uint32 index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0, typename PosType::builtin_type& x1, typename PosType::builtin_type& y1) = 0;

	virtual mpp::Colour diffuse() = 0;

	void setNumLines(size_t numLines)
	{
		mNumLines = numLines;
	}

	size_t getNumLines() const
	{
		return mNumLines;
	}

	virtual void update(float frameTime) {}
};

// Base class for our 'Test' data provider.  Both this and the specializations (below)
// need to be implemented for each data provider we create.  One for each position/colour
// datatype combination, and one for each position(no colour).
template<typename PosType, typename ColType = mpp::mesh::DataTypeNone>
class TestLineBatchDataProvider : public LineBatchDataProvider<PosType, ColType> {};

// Specialization (to be used) for our data provider
template<>
class TestLineBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeUnsignedByte>
	: public LineBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeUnsignedByte>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	float mTime{ 0.0f };

public:

	TestLineBatchDataProvider(mpp::RenderSystem* renderSystem, size_t numLines)
		: mRenderSystem(renderSystem)
	{
		setNumLines(numLines);
	}

	void position(uint32 index, float& x0, float& y0, float& x1, float& y1)
	{
		float linesX = mRenderSystem->getWindowWidth() * 0.25f;
		float linesW = mRenderSystem->getWindowWidth() * 0.5f;
		float linesY = mRenderSystem->getWindowHeight() * 0.5f;

		size_t numLines = getNumLines();

		x0 = linesX + linesW * ((float)index / numLines);

		y0 =
			sinf(((float)index / numLines) * 6.2832f - mTime) +
			sinf(((float)index / (numLines / 2)) * 6.2832f + 1.2f - mTime) +
			sinf(((float)index / (numLines / 4)) * 6.2832f + 2.4f - mTime) * 0.75f;
		y0 = linesY + 100 + y0 * 10;

		x1 = linesX + linesW * ((float)index / numLines);

		y1 =
			cosf(((float)index / numLines) * 6.2832f - mTime) +
			sinf(((float)index / (numLines / 4)) * 6.2832f + 3.2f - mTime) * 0.5f +
			cosf(((float)index / (numLines / 3)) * 6.2832f + 5.4f - mTime) * 0.5f;
		y1 = linesY - 100 - y1 * 10;
	}

	void colour(uint32 index, uint8& red, uint8& green, uint8& blue, uint8& alpha)
	{
		uint8 colours[]{
			255, 0, 255,
			255, 127, 0,
			0, 255, 127,
			64, 64, 224
		};

		srand(index);
		auto ri = rand() % 4;

		red = colours[ri * 3 + 0];
		green = colours[ri * 3 + 1];
		blue = colours[ri * 3 + 2];
		alpha = 255;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	void update(float frameTime)
	{
		mTime += frameTime;
	}
};

// Specialization with no colour for our data provider
template<>
class TestLineBatchDataProvider<mpp::mesh::DataTypeFloat>
	: public LineBatchDataProvider<mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	float mTime{ 0.0f };

public:

	TestLineBatchDataProvider(mpp::RenderSystem* renderSystem, size_t numLines)
		: mRenderSystem(renderSystem)
	{
		setNumLines(numLines);
	}

	void position(uint32 index, float& x0, float& y0, float& x1, float& y1)
	{
		float linesX = mRenderSystem->getWindowWidth() * 0.25f;
		float linesW = mRenderSystem->getWindowWidth() * 0.5f;
		float linesY = mRenderSystem->getWindowHeight() * 0.5f;

		size_t numLines = getNumLines();

		x0 = linesX + linesW * ((float)index / numLines);

		y0 =
			sinf(((float)index / numLines) * 6.2832f - mTime) +
			sinf(((float)index / (numLines / 2)) * 6.2832f + 1.2f - mTime) +
			sinf(((float)index / (numLines / 4)) * 6.2832f + 2.4f - mTime) * 0.75f;
		y0 = linesY + 100 + y0 * 10;

		x1 = linesX + linesW * ((float)index / numLines);

		y1 =
			cosf(((float)index / numLines) * 6.2832f - mTime) +
			sinf(((float)index / (numLines / 4)) * 6.2832f + 3.2f - mTime) * 0.5f +
			cosf(((float)index / (numLines / 3)) * 6.2832f + 5.4f - mTime) * 0.5f;
		y1 = linesY - 100 - y1 * 10;
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

	void update(float frameTime)
	{
		mTime += frameTime;
	}
};

struct LineBatchRendererParams
{
	bool fixedColourData;
	bool useVertexColours, useDiffuse;
};

template<typename PosType, typename ColType = mpp::mesh::DataTypeNone>
class LineBatchRenderer
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::LineBatch* mBatch{ nullptr };

	LineBatchDataProvider<PosType, ColType>* mDataProvider{ nullptr };

public:

	LineBatchRenderer(std::string const& name,
		LineBatchRendererParams const& params,
		LineBatchDataProvider<PosType, ColType>* dataProvider,
		size_t initialSize,
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		mBatch = new mpp::LineBatch(
			name,
			{
				PosType::vertexDataType(),
				{ params.useVertexColours ? ColType::vertexDataType() : mpp::mesh::Vertex::DataType::None, params.fixedColourData },
				params.useDiffuse,
			},
			initialSize,
			renderSystem,
			resourceMgr);
	}

	virtual ~LineBatchRenderer()
	{
		delete mBatch;
	}

	void create()
	{
		mBatch->load();
		update(mBatch->getCapacity());
	}

	size_t update(size_t count)
	{
		size_t initStart{ ~0u }, batchSize = mBatch->getCount();
		bool newVertices{ false };
		if (count > batchSize)
		{
			initStart = mBatch->getPrimitiveCount(batchSize);
			newVertices = true;
		}

		mBatch->startUpdate(count);

		auto posBuffer = (PosType::builtin_type*)mBatch->getAttributeData("POSITION").first;
		auto posStride = mBatch->getAttributeData("POSITION").second / sizeof(PosType::builtin_type);

		ColType::builtin_type* colBuffer{ nullptr };
		size_t colStride{ 0 };

		if (mBatch->usingColour())
		{
			colBuffer = (ColType::builtin_type*)mBatch->getAttributeData("COLOUR").first;
			colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);
		}

		size_t lineCount = mBatch->getPrimitiveCount(count);
		for (size_t pOffset = 0, cOffset = 0, i = 0; i < lineCount; ++i)
		{
			uint32 primitiveIndex = i;
			bool newVertex = i >= initStart;

			//
			// Position data
			//
			if (!mBatch->positionFixed() || newVertex)
			{
				PosType::builtin_type x0, y0, x1, y1;
				mDataProvider->position(primitiveIndex, x0, y0, x1, y1);

				posBuffer[pOffset + 0] = x0;
				posBuffer[pOffset + 1] = y0;
				pOffset += posStride;

				posBuffer[pOffset + 0] = x1;
				posBuffer[pOffset + 1] = y1;
				pOffset += posStride;
			}

			//
			// Colour data
			//
			if (mBatch->usingColour() && (!mBatch->colourFixed() || newVertex))
			{
				ColType::builtin_type red, green, blue, alpha;
				mDataProvider->colour(primitiveIndex, red, green, blue, alpha);

				colBuffer[cOffset + 0] = red;
				colBuffer[cOffset + 1] = green;
				colBuffer[cOffset + 2] = blue;
				colBuffer[cOffset + 3] = alpha;
				cOffset += colStride;

				colBuffer[cOffset + 0] = red;
				colBuffer[cOffset + 1] = green;
				colBuffer[cOffset + 2] = blue;
				colBuffer[cOffset + 3] = alpha;
				cOffset += colStride;
			}
			else
			{
				cOffset += colStride * 2;
			}
		}

		mBatch->finishUpdate(count, newVertices);
		return mBatch->getCount();
	}

	void render()
	{
		mpp::UniformCollection uniforms;
		if (mBatch->usingDiffuse())
		{
			auto colour = mDataProvider->diffuse();
			uniforms.setUniform("DIFFUSE", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
		}

		mRenderSystem->renderModelImmediate(*mBatch, true, &uniforms);
	}
};

template<typename PosType>
class LineBatchRenderer<PosType, mpp::mesh::DataTypeNone>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::LineBatch* mBatch{ nullptr };

	LineBatchDataProvider<PosType, mpp::mesh::DataTypeNone>* mDataProvider{ nullptr };

public:

	LineBatchRenderer(std::string const& name,
		LineBatchRendererParams const& params,
		LineBatchDataProvider<PosType, mpp::mesh::DataTypeNone>* dataProvider,
		size_t initialSize,
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		mBatch = new mpp::LineBatch(
			name,
			{
				PosType::vertexDataType(),
				{ mpp::mesh::Vertex::DataType::None, true },
				params.useDiffuse,
			},
			initialSize,
			renderSystem,
			resourceMgr);
	}

	virtual ~LineBatchRenderer()
	{
		delete mBatch;
	}

	void create()
	{
		mBatch->load();
		update(mBatch->getCapacity());
	}

	size_t update(size_t count)
	{
		size_t initStart{ ~0u }, batchSize = mBatch->getCount();
		bool newVertices{ false };
		if (count > batchSize)
		{
			initStart = mBatch->getPrimitiveCount(batchSize);
			newVertices = true;
		}

		mBatch->startUpdate(count);

		auto posBuffer = (PosType::builtin_type*)mBatch->getAttributeData("POSITION").first;
		auto posStride = mBatch->getAttributeData("POSITION").second / sizeof(PosType::builtin_type);

		size_t lineCount = mBatch->getPrimitiveCount(count);
		for (size_t pOffset = 0, i = 0; i < lineCount; ++i)
		{
			uint32 primitiveIndex = i;
			bool newVertex = i >= initStart;

			//
			// Position data
			//
			if (!mBatch->positionFixed() || newVertex)
			{
				PosType::builtin_type x0, y0, x1, y1;
				mDataProvider->position(primitiveIndex, x0, y0, x1, y1);

				posBuffer[pOffset + 0] = x0;
				posBuffer[pOffset + 1] = y0;
				pOffset += posStride;

				posBuffer[pOffset + 0] = x1;
				posBuffer[pOffset + 1] = y1;
				pOffset += posStride;
			}
		}

		mBatch->finishUpdate(count, newVertices);
		return mBatch->getCount();
	}

	void render()
	{
		mpp::UniformCollection uniforms;
		if (mBatch->usingDiffuse())
		{
			auto colour = mDataProvider->diffuse();
			uniforms.setUniform("DIFFUSE", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
		}

		mRenderSystem->renderModelImmediate(*mBatch, true, &uniforms);
	}
};

