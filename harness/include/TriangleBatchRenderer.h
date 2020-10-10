#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TriangleBatch.h>

#include <mpp/mesh/VertexTypeSpecification.h>

template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
class TriangleBatchDataProvider
{
	size_t mNumTriangles{ 0 };

public:

	virtual void position(uint32 index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0,
		typename PosType::builtin_type& x1, typename PosType::builtin_type& y1,
		typename PosType::builtin_type& x2, typename PosType::builtin_type& y2) = 0;

	virtual void texcoords(uint32 index, typename PosType::builtin_type& u0, typename PosType::builtin_type& v0,
		typename PosType::builtin_type& u1, typename PosType::builtin_type& v1,
		typename PosType::builtin_type& u2, typename PosType::builtin_type& v2) {}

	virtual void colour(uint32 index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha) = 0;

	virtual mpp::Colour diffuse() = 0;

	void setNumTriangles(size_t numTriangles)
	{
		mNumTriangles = numTriangles;
	}

	size_t getNumTriangles() const
	{
		return mNumTriangles;
	}

	virtual void update(float frameTime) {}
};

template<typename PosType, typename TexType>
class TriangleBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>
{
	size_t mNumTriangles{ 0 };

public:

	virtual void position(uint32 index, typename PosType::builtin_type& x0, typename PosType::builtin_type& y0,
		typename PosType::builtin_type& x1, typename PosType::builtin_type& y1,
		typename PosType::builtin_type& x2, typename PosType::builtin_type& y2) = 0;

	virtual void texcoords(uint32 index, typename PosType::builtin_type& u0, typename PosType::builtin_type& v0,
		typename PosType::builtin_type& u1, typename PosType::builtin_type& v1,
		typename PosType::builtin_type& u2, typename PosType::builtin_type& v2) {}

	virtual mpp::Colour diffuse() = 0;

	void setNumTriangles(size_t numTriangles)
	{
		mNumTriangles = numTriangles;
	}

	size_t getNumTriangles() const
	{
		return mNumTriangles;
	}

	virtual void update(float frameTime) {}
};

// Base class for our 'Test' data provider.  Both this and the specializations (below)
// need to be implemented for each data provider we create.  One for each position/colour
// datatype combination, and one for each position(no colour).
template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
class TestTriangleBatchDataProvider : public TriangleBatchDataProvider<PosType, TexType, ColType> {};

// Specialization (to be used) for our data provider
template<>
class TestTriangleBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeUnsignedByte>
	: public TriangleBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeUnsignedByte>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	float mTime{ 0.0f };

public:

	TestTriangleBatchDataProvider(mpp::RenderSystem* renderSystem, size_t numTriangles)
		: mRenderSystem(renderSystem)
	{
		setNumTriangles(numTriangles);
	}

	void position(uint32 index, float& x0, float& y0, float& x1, float& y1, float& x2, float& y2)
	{
		x0 = 400 + sinf((index + 1) * mTime / 10.0f) * 100;
		y0 = 300 + cosf((index + 2) * mTime / 10.0f) * 100;

		x1 = x0 - 16;
		y1 = y0 - 32;

		x2 = x0 + 16;
		y2 = y0 - 32;
	}

	void texcoords(uint32 index, float& u0, float& v0, float& u1, float& v1, float& u2, float& v2)
	{
		u0 = 0.5f;
		v0 = 1.0f;
		u1 = 0.0f;
		v1 = 0.0f;
		u2 = 1.0f;
		v2 = 0.0f;
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

		red = 255; colours[ri * 3 + 0];
		green = 255; colours[ri * 3 + 1];
		blue = 255; colours[ri * 3 + 2];
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
class TestTriangleBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat>
	: public TriangleBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	float mTime{ 0.0f };

public:

	TestTriangleBatchDataProvider(mpp::RenderSystem* renderSystem, size_t numTriangles)
		: mRenderSystem(renderSystem)
	{
		setNumTriangles(numTriangles);
	}

	void position(uint32 index, float& x0, float& y0, float& x1, float& y1, float& x2, float& y2)
	{
		x0 = 400 + sinf((index + 1) * mTime / 10.0f) * 100;
		y0 = 300 + cosf((index + 2) * mTime / 10.0f) * 100;

		x1 = x0 - 16;
		y1 = y0 - 32;

		x2 = x0 + 16;
		y2 = y0 - 32;
	}

	void texcoords(uint32 index, float& u0, float& v0, float& u1, float& v1, float& u2, float& v2)
	{
		u0 = 0.5f;
		v0 = 1.0f;
		u1 = 0.0f;
		v1 = 0.0f;
		u2 = 1.0f;
		v2 = 0.0f;
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

struct TriangleBatchRendererParams
{
	bool fixedTextureData, fixedColourData;
	bool useDiffuse;
};

template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
class TriangleBatchRenderer
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::TriangleBatch* mBatch{ nullptr };

	TriangleBatchDataProvider<PosType, TexType, ColType>* mDataProvider{ nullptr };

public:

	TriangleBatchRenderer(std::string const& name,
		TriangleBatchRendererParams const& params,
		TriangleBatchDataProvider<PosType, TexType, ColType>* dataProvider,
		mpp::ResourcePtr texture,
		size_t initialSize,
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		mBatch = new mpp::TriangleBatch(
			name,
			{
				PosType::vertexDataType(),
				{ TexType::vertexDataType(), params.fixedTextureData },
				{ ColType::vertexDataType(), params.fixedColourData },
				params.useDiffuse
			},
			texture,
			initialSize,
			renderSystem,
			resourceMgr);
	}

	virtual ~TriangleBatchRenderer()
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

		TexType::builtin_type* texBuffer{ nullptr };
		size_t texStride{ 0 };

		if (mBatch->usingTexture())
		{
			texBuffer = (TexType::builtin_type*)mBatch->getAttributeData("TEXCOORDS").first;
			texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexType::builtin_type);
		}

		auto colBuffer = (uint8*)mBatch->getAttributeData("COLOUR").first;
		auto colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);

		size_t triangleCount = mBatch->getPrimitiveCount(count);
		for (size_t pOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < triangleCount; ++i)
		{
			uint32 primitiveIndex = i;
			bool newVertex = i >= initStart;

			//
			// Position data
			//
			if (!mBatch->positionFixed() || newVertex)
			{
				PosType::builtin_type x0, y0, x1, y1, x2, y2;
				mDataProvider->position(primitiveIndex, x0, y0, x1, y1, x2, y2);

				posBuffer[pOffset + 0] = x0;
				posBuffer[pOffset + 1] = y0;
				pOffset += posStride;

				posBuffer[pOffset + 0] = x1;
				posBuffer[pOffset + 1] = y1;
				pOffset += posStride;

				posBuffer[pOffset + 0] = x2;
				posBuffer[pOffset + 1] = y2;
				pOffset += posStride;
			}
			else
			{ 
				pOffset += posStride * 3;
			}

			//
			// Texture data
			//
			if (mBatch->usingTexture() && (!mBatch->texcoordsFixed() || newVertex))
			{
				TexType::builtin_type u0, v0, u1, v1, u2, v2;
				mDataProvider->texcoords(primitiveIndex, u0, v0, u1, v1, u2, v2);

				texBuffer[tOffset + 0] = u0;
				texBuffer[tOffset + 1] = v0;
				tOffset += texStride;

				texBuffer[tOffset + 0] = u1;
				texBuffer[tOffset + 1] = v1;
				tOffset += texStride;

				texBuffer[tOffset + 0] = u2;
				texBuffer[tOffset + 1] = v2;
				tOffset += texStride;
			}
			else
			{
				tOffset += texStride * 3;
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

				colBuffer[cOffset + 0] = red;
				colBuffer[cOffset + 1] = green;
				colBuffer[cOffset + 2] = blue;
				colBuffer[cOffset + 3] = alpha;
				cOffset += colStride;
			}
			else
			{
				cOffset += colStride * 3;
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

template<typename PosType, typename TexType>
class TriangleBatchRenderer<PosType, TexType, mpp::mesh::DataTypeNone>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::TriangleBatch* mBatch{ nullptr };

	TriangleBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>* mDataProvider{ nullptr };

public:

	TriangleBatchRenderer(std::string const& name,
		TriangleBatchRendererParams const& params,
		TriangleBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>* dataProvider,
		mpp::ResourcePtr texture,
		size_t initialSize,
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		mBatch = new mpp::TriangleBatch(
			name,
			{
				PosType::vertexDataType(),
				{ TexType::vertexDataType(), params.fixedTextureData },
				{ mpp::mesh::Vertex::DataType::None, true },
				params.useDiffuse
			},
			texture,
			initialSize,
			renderSystem,
			resourceMgr);
	}

	virtual ~TriangleBatchRenderer()
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

		TexType::builtin_type* texBuffer{ nullptr };
		size_t texStride{ 0 };

		if (mBatch->usingTexture())
		{
			texBuffer = (TexType::builtin_type*)mBatch->getAttributeData("TEXCOORDS").first;
			texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexType::builtin_type);
		}

		size_t triangleCount = mBatch->getPrimitiveCount(count);
		for (size_t pOffset = 0, tOffset = 0, i = 0; i < triangleCount; ++i)
		{
			uint32 primitiveIndex = i;
			bool newVertex = i >= initStart;

			//
			// Position data
			//
			if (!mBatch->positionFixed() || newVertex)
			{
				PosType::builtin_type x0, y0, x1, y1, x2, y2;
				mDataProvider->position(primitiveIndex, x0, y0, x1, y1, x2, y2);

				posBuffer[pOffset + 0] = x0;
				posBuffer[pOffset + 1] = y0;
				pOffset += posStride;

				posBuffer[pOffset + 0] = x1;
				posBuffer[pOffset + 1] = y1;
				pOffset += posStride;

				posBuffer[pOffset + 0] = x2;
				posBuffer[pOffset + 1] = y2;
				pOffset += posStride;
			}
			else
			{
				pOffset += posStride * 3;
			}

			//
			// Texture data
			//
			if (!mBatch->texcoordsFixed() || newVertex)
			{
				TexType::builtin_type u0, v0, u1, v1, u2, v2;
				mDataProvider->texcoords(primitiveIndex, u0, v0, u1, v1, u2, v2);

				texBuffer[tOffset + 0] = u0;
				texBuffer[tOffset + 1] = v0;
				tOffset += texStride;

				texBuffer[tOffset + 0] = u1;
				texBuffer[tOffset + 1] = v1;
				tOffset += texStride;

				texBuffer[tOffset + 0] = u2;
				texBuffer[tOffset + 1] = v2;
				tOffset += texStride;
			}
			else
			{
				tOffset += texStride * 3;
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

