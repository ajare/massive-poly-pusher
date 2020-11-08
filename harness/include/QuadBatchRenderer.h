#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/QuadBatch.h>
#include <mpp/TextureRenderer.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "TriangleBatchRenderer.h"

class CircleDataProvider : public TriangleBatchDataProvider<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>
{
	float mTime{ 0.0f };

public:

	CircleDataProvider(size_t numTriangles)
	{
		setNumTriangles(numTriangles);
	}

	void position(uint32_t index, float& x0, float& y0, float& x1, float& y1, float& x2, float& y2)
	{
		x0 = 8;
		y0 = 8;

		x1 = 8 + sinf(2 * 3.14159f * index / 36.0f) * 7.0f;
		y1 = 8 + cosf(2 * 3.14159f * index / 36.0f) * 7.0f;

		x2 = 8 + sinf(2 * 3.14159f * (index + 1) / 36.0f) * 7.0f;
		y2 = 8 + cosf(2 * 3.14159f * (index + 1) / 36.0f) * 7.0f;
	}

	void colour(uint32_t index, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha)
	{
		red = uint8_t((sinf(mTime) * 2 - 1.0f) * 192 + 64);
		green = 255;
		blue = 224 - uint8_t((cosf(mTime) * 2 - 1.0f) * 128);
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

class CircleRenderer : public mpp::TextureRenderer
{
	std::shared_ptr<TriangleBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>> mTriRenderer{ nullptr };

	std::shared_ptr<CircleDataProvider> mDataProvider;

private:

	void render(int width, int height)
	{
		mTriRenderer->render();
	}

public:

	CircleRenderer(std::string const& name, mpp::RenderSystem* renderSystem, mpp::ResourceManager* resourceMgr)
		: mpp::TextureRenderer(name, renderSystem, resourceMgr)
	{
		mDataProvider = std::make_shared<CircleDataProvider>(36);
		
		TriangleBatchRendererParams params
		{
			true, 
			false, 
			false
		};

		mTriRenderer = std::make_shared<TriangleBatchRenderer<mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeFloat, mpp::mesh::DataTypeUnsignedByte>>(
			"CircleRenderer",
			params,
			mDataProvider,
			nullptr,
			36,
			mRenderSystem,
			mResourceMgr);

		mTriRenderer->create();
	}

	void update(float frameTime)
	{
		mDataProvider->update(frameTime);
		mTriRenderer->update(36);
	}
};

template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
class QuadBatchDataProvider
{
	size_t mNumQuads{ 0 };

public:

	virtual void position(uint32_t index, typename PosType::builtin_type& x, typename PosType::builtin_type& y) = 0;

	virtual void angle(uint32_t index, float& angle) = 0;

	virtual void textureAtlasTexcoords(uint32_t index, typename TexType::builtin_type& u0, typename TexType::builtin_type& v0, typename TexType::builtin_type& u1, typename TexType::builtin_type& v1) = 0;

	virtual void colour(uint32_t index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha) = 0;

	virtual mpp::Colour diffuse() = 0;

	void setNumQuads(size_t numQuads)
	{
		mNumQuads = numQuads;
	}

	size_t getNumQuads() const
	{
		return mNumQuads;
	}

	virtual void update(float frameTime) {}
};

template<typename PosType, typename TexType>
class QuadBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>
{
	size_t mNumQuads{ 0 };

public:

	virtual void position(uint32_t index, typename PosType::builtin_type& x, typename PosType::builtin_type& y) = 0;

	virtual void angle(uint32_t index, float& angle) = 0;

	virtual void textureAtlasTexcoords(uint32_t index, typename TexType::builtin_type& u0, typename TexType::builtin_type& v0, typename TexType::builtin_type& u1, typename TexType::builtin_type& v1) = 0;

	virtual mpp::Colour diffuse() = 0;

	void setNumQuads(size_t numQuads)
	{
		mNumQuads = numQuads;
	}

	size_t getNumQuads() const
	{
		return mNumQuads;
	}

	virtual void update(float frameTime) {}
};

// Base class for our 'Test' data provider.  Both this and the specializations (below)
// need to be implemented for each data provider we create.  One for each position/colour
// datatype combination, and one for each position(no colour).
template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
class TestQuadBatchDataProvider : public QuadBatchDataProvider<PosType, TexType, ColType> {};

// Specialization (to be used) for our data provider
template<>
class TestQuadBatchDataProvider<
	mpp::mesh::DataTypeFloat, 
	mpp::mesh::DataTypeFloat, 
	mpp::mesh::DataTypeUnsignedByte>
	: public QuadBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeUnsignedByte>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	float mTime{ 0.0f };

public:

	TestQuadBatchDataProvider(mpp::RenderSystem* renderSystem, size_t numQuads)
		: mRenderSystem(renderSystem)
	{
		setNumQuads(numQuads);
	}

	void position(uint32_t index, float& x, float& y)
	{
		x = 400 + sinf((index + 1) * mTime / 10.0f) * 100;
		y = 300 + cosf((index + 2) * mTime / 10.0f) * 100;
	}

	void angle(uint32_t index, float& angle)
	{
		angle = index * mTime;
	}

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		const float txWidth = 1.0f / 8;

		u0 = txWidth * 0;
		v0 = 0.0f;
		u1 = txWidth * 1;
		v1 = 1.0f;
	}

	void colour(uint32_t index, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha)
	{
		uint8_t colours[]{
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
class TestQuadBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat>
	: public QuadBatchDataProvider<
	mpp::mesh::DataTypeFloat,
	mpp::mesh::DataTypeFloat>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	float mTime{ 0.0f };

public:

	TestQuadBatchDataProvider(mpp::RenderSystem* renderSystem, size_t numQuads)
		: mRenderSystem(renderSystem)
	{
		setNumQuads(numQuads);
	}

	void position(uint32_t index, float& x, float& y)
	{
		x = 400 + sinf((index + 1) * mTime / 10.0f) * 100;
		y = 300 + cosf((index + 2) * mTime / 10.0f) * 100;
	}

	void angle(uint32_t index, float& angle)
	{
		angle = index * mTime;
	}

	void textureAtlasTexcoords(uint32_t index, float& u0, float& v0, float& u1, float& v1)
	{
		const float txWidth = 1.0f / 8;

		u0 = txWidth * 0;
		v0 = 0.0f;
		u1 = txWidth * 1;
		v1 = 1.0f;
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

class QuadBatchRendererParams
{
	mpp::QuadBatchOptions::PrimitiveOptions mPrimitiveOptions;

	bool mFixedTextureData, mFixedColourData;
	
	bool mUseVertexColours, mUseDiffuse;

	bool mSameSize;
	
	bool mRotate;

	size_t mWidth, mHeight;

	size_t mIndexWidth;

	mpp::ResourcePtr mTexture;

	mpp::TextureRendererPtr mTextureRenderer;

public:

	QuadBatchRendererParams(
		mpp::QuadBatchOptions::PrimitiveOptions primitiveOptions,
		bool fixedTextureData,
		bool fixedColourData,
		bool useVertexColours,
		bool useDiffuse,
		bool rotate,
		size_t width,
		size_t height,
		bool sameSize,
		size_t indexWidth)
		: mPrimitiveOptions(primitiveOptions)
		, mFixedTextureData(fixedTextureData)
		, mFixedColourData(fixedColourData)
		, mUseVertexColours(useVertexColours)
		, mUseDiffuse(useDiffuse)
		, mRotate(rotate)
		, mSameSize(sameSize)
		, mWidth(width)
		, mHeight(height)
		, mIndexWidth(indexWidth)
		, mTexture(nullptr)
		, mTextureRenderer(nullptr)
	{
	}

	QuadBatchRendererParams(
		mpp::QuadBatchOptions::PrimitiveOptions primitiveOptions,
		bool fixedTextureData,
		bool fixedColourData,
		bool useVertexColours,
		bool useDiffuse,
		bool rotate,
		size_t width,
		size_t height,
		bool sameSize,
		size_t indexWidth,
		mpp::ResourcePtr texture)
		: mPrimitiveOptions(primitiveOptions)
		, mFixedTextureData(fixedTextureData)
		, mFixedColourData(fixedColourData)
		, mUseVertexColours(useVertexColours)
		, mUseDiffuse(useDiffuse)
		, mRotate(rotate)
		, mSameSize(sameSize)
		, mWidth(width)
		, mHeight(height)
		, mIndexWidth(indexWidth)
		, mTexture(texture)
		, mTextureRenderer(nullptr)
	{
	}

	QuadBatchRendererParams(
		mpp::QuadBatchOptions::PrimitiveOptions primitiveOptions,
		bool fixedTextureData,
		bool fixedColourData,
		bool useVertexColours,
		bool useDiffuse,
		bool rotate,
		size_t width,
		size_t height,
		bool sameSize,
		size_t indexWidth,
		mpp::TextureRendererPtr textureRenderer)
		: mPrimitiveOptions(primitiveOptions)
		, mFixedTextureData(fixedTextureData)
		, mFixedColourData(fixedColourData)
		, mUseVertexColours(useVertexColours)
		, mUseDiffuse(useDiffuse)
		, mRotate(rotate)
		, mSameSize(sameSize)
		, mWidth(width)
		, mHeight(height)
		, mIndexWidth(indexWidth)
		, mTexture(nullptr)
		, mTextureRenderer(textureRenderer)
	{
	}

	mpp::QuadBatchOptions::PrimitiveOptions getPrimitiveOptions() const
	{
		return mPrimitiveOptions;
	}

	bool fixedTextureData() const
	{
		return mFixedTextureData;
	}

	bool fixedColourData() const
	{
		return mFixedColourData;
	}

	bool useVertexColours() const
	{
		return mUseVertexColours;
	}
	
	bool useDiffuse() const
	{
		return mUseDiffuse;
	}
	
	bool rotate() const
	{
		return mRotate;
	}
	
	size_t getWidth() const
	{
		return mWidth;
	}

	size_t getHeight() const
	{
		return mHeight;
	}

	bool sameSize() const
	{
		return mSameSize;
	}

	size_t getIndexWidth() const
	{
		return mIndexWidth;
	}

	mpp::ResourcePtr getTexture() const
	{
		return mTexture;
	}

	mpp::TextureRendererPtr getTextureRenderer() const
	{
		return mTextureRenderer;
	}
};

template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
class QuadBatchRenderer
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::QuadBatch* mBatch{ nullptr };

	std::shared_ptr<QuadBatchDataProvider<PosType, TexType, ColType>> mDataProvider{ nullptr };

public:

	QuadBatchRenderer(std::string const& name, 
		QuadBatchRendererParams const& params, 
		std::shared_ptr<QuadBatchDataProvider<PosType, TexType, ColType>> dataProvider, 
		size_t initialSize, 
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		if (params.getTexture())
		{
			mBatch = new mpp::QuadBatch(
				name,
				{
					params.getPrimitiveOptions(),
					PosType::vertexDataType(),
					{ TexType::vertexDataType(), params.fixedTextureData() },
					{ params.useVertexColours() ? ColType::vertexDataType() : mpp::mesh::Vertex::DataType::None, params.fixedColourData() },
					params.useDiffuse(),
					params.rotate(),
					params.getWidth(),
					params.getHeight(),
					params.getIndexWidth()
				},
				params.getSameSize(),
				params.getTexture(),
				initialSize,
				renderSystem,
				resourceMgr);
		}
		else if (params.getTextureRenderer())
		{
			mBatch = new mpp::QuadBatch(
				name,
				{
					params.getPrimitiveOptions(),
					PosType::vertexDataType(),
					{ TexType::vertexDataType(), params.fixedTextureData() },
					{ params.useVertexColours() ? ColType::vertexDataType() : mpp::mesh::Vertex::DataType::None, params.fixedColourData() },
					params.useDiffuse(),
					params.rotate(),
					params.getWidth(),
					params.getHeight(),
					params.getIndexWidth()
				},
				params.getSameSize(),
				params.getTextureRenderer(),
				initialSize,
				renderSystem,
				resourceMgr);
		}
		else
		{
			mBatch = new mpp::QuadBatch(
				name,
				{
					params.getPrimitiveOptions(),
					PosType::vertexDataType(),
					{ TexType::vertexDataType(), params.fixedTextureData() },
					{ params.useVertexColours() ? ColType::vertexDataType() : mpp::mesh::Vertex::DataType::None, params.fixedColourData() },
					params.useDiffuse(),
					params.rotate(),
					params.getWidth(),
					params.getHeight(),
					params.getIndexWidth()
				},
				params.getSameSize(),
				initialSize,
				renderSystem,
				resourceMgr);
		}
	}

	virtual ~QuadBatchRenderer()
	{
		delete mBatch;
	}

	mpp::QuadBatch* getBatch()
	{
		return mBatch;
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
			initStart = mBatch->getVertexCount(mBatch->getPrimitiveCount(batchSize));
			newVertices = true;
		}

		mBatch->startUpdate(count);

		float radiusX = mBatch->getMaxDimX() / 2.0f;
		float radiusY = mBatch->getMaxDimY() / 2.0f;

		auto posBuffer = (PosType::builtin_type*)mBatch->getAttributeData("POSITION").first;
		auto posStride = mBatch->getAttributeData("POSITION").second / sizeof(PosType::builtin_type);

		auto rotBuffer = (PosType::builtin_type*)mBatch->getAttributeData("ROTATION").first;
		auto rotStride = mBatch->getAttributeData("ROTATION").second / sizeof(PosType::builtin_type);

		TexType::builtin_type* texBuffer{ nullptr };
		size_t texStride{ 0 };

		if (mBatch->usingTexture())
		{
			texBuffer = (TexType::builtin_type*)mBatch->getAttributeData("TEXCOORDS").first;
			texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexType::builtin_type);
		}

		auto colBuffer = (uint8_t*)mBatch->getAttributeData("COLOUR").first;
		auto colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);

		size_t vertexCount = mBatch->getVertexCount(mBatch->getPrimitiveCount(count));
		for (size_t pOffset = 0, rOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < vertexCount; ++i)
		{
			uint32_t primitiveIndex = mBatch->usingPointSprites() ? i : i / 4;
			bool newVertex = i >= initStart;

			//
			// Position data
			//
			if (!mBatch->positionFixed() || newVertex)
			{
				PosType::builtin_type x, y;
				mDataProvider->position(primitiveIndex, x, y);

				if (mBatch->usingPointSprites())
				{
					// One vertex per quad
					posBuffer[pOffset + 0] = x;
					posBuffer[pOffset + 1] = y;
				}
				else
				{
					// Indexed, four vertices per quad
					int vertexIndex = i % 4;

					switch (vertexIndex)
					{
					case 0:
						posBuffer[pOffset + 0] = x - radiusX;
						posBuffer[pOffset + 1] = y - radiusY;
						break;
					case 1:
						posBuffer[pOffset + 0] = x + radiusX;
						posBuffer[pOffset + 1] = y - radiusY;
						break;
					case 2:
						posBuffer[pOffset + 0] = x + radiusX;
						posBuffer[pOffset + 1] = y + radiusY;
						break;
					case 3:
						posBuffer[pOffset + 0] = x - radiusX;
						posBuffer[pOffset + 1] = y + radiusY;
						break;
					}
				}

				if (mBatch->rotating() && mBatch->usingTriangles())
				{
					// Centroid for rotation in vertex shader
					posBuffer[pOffset + 2] = x;
					posBuffer[pOffset + 3] = y;
				}
			}

			//
			// Rotation data
			//
			if (!mBatch->rotationFixed() || newVertex)
			{
				PosType::builtin_type angle;
				mDataProvider->angle(primitiveIndex, angle);

				rotBuffer[rOffset + 0] = sinf(angle);
				rotBuffer[rOffset + 1] = cosf(angle);
			}

			//
			// Texture data
			//
			if (!mBatch->texcoordsFixed() || newVertex)
			{
				if (mBatch->usingPointSprites())
				{
					if (mBatch->usingTextureAtlas())
					{
						TexType::builtin_type u0, v0, u1, v1;
						mDataProvider->textureAtlasTexcoords(primitiveIndex, u0, v0, u1, v1);

						texBuffer[tOffset + 0] = u0;
						texBuffer[tOffset + 1] = v0;
						texBuffer[tOffset + 2] = u1;
						texBuffer[tOffset + 3] = v1;
					}
				}
				else
				{
					if (mBatch->usingTexture())
					{
						int vertexIndex = i % 4;

						TexType::builtin_type u0, v0, u1, v1;
						mDataProvider->textureAtlasTexcoords(primitiveIndex, u0, v0, u1, v1);

						// Indexed, four vertices per quad
						if (mBatch->usingTextureAtlas())
						{
							switch (vertexIndex)
							{
							case 0:
								texBuffer[tOffset + 0] = u0;
								texBuffer[tOffset + 1] = v0;
								break;
							case 1:
								texBuffer[tOffset + 0] = u1;
								texBuffer[tOffset + 1] = v0;
								break;
							case 2:
								texBuffer[tOffset + 0] = u1;
								texBuffer[tOffset + 1] = v1;
								break;
							case 3:
								texBuffer[tOffset + 0] = u0;
								texBuffer[tOffset + 1] = v1;
								break;
							}
						}
						else
						{
							switch (vertexIndex)
							{
							case 0:
								texBuffer[tOffset + 0] = 0.0f;
								texBuffer[tOffset + 1] = 0.0f;
								break;
							case 1:
								texBuffer[tOffset + 0] = 1.0f;
								texBuffer[tOffset + 1] = 0.0f;
								break;
							case 2:
								texBuffer[tOffset + 0] = 1.0f;
								texBuffer[tOffset + 1] = 1.0f;
								break;
							case 3:
								texBuffer[tOffset + 0] = 0.0f;
								texBuffer[tOffset + 1] = 1.0f;
								break;
							}
						}
					}
				}
			}

			//
			// Colour data
			//
			if (!mBatch->colourFixed() || newVertex)
			{
				ColType::builtin_type red, green, blue, alpha;
				mDataProvider->colour(primitiveIndex, red, green, blue, alpha);

				colBuffer[cOffset + 0] = red;
				colBuffer[cOffset + 1] = green;
				colBuffer[cOffset + 2] = blue;
				colBuffer[cOffset + 3] = alpha;
			}

			// Next vertex
			pOffset += posStride;
			rOffset += rotStride;
			tOffset += texStride;
			cOffset += colStride;
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
class QuadBatchRenderer<PosType, TexType, mpp::mesh::DataTypeNone>
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::QuadBatch* mBatch{ nullptr };

	std::shared_ptr<QuadBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>> mDataProvider;

public:

	QuadBatchRenderer(std::string const& name,
		QuadBatchRendererParams const& params,
		std::shared_ptr<QuadBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>> dataProvider,
		size_t initialSize,
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		if (params.getTexture())
		{
			mBatch = new mpp::QuadBatch(
				name,
				{
					params.getPrimitiveOptions(),
					PosType::vertexDataType(),
					{ TexType::vertexDataType(), params.fixedTextureData() },
					{ mpp::mesh::Vertex::DataType::None, true },
					params.useDiffuse(),
					params.rotate(),
					params.getWidth(),
					params.getHeight(),
					params.getIndexWidth()
				},
				params.sameSize(),
				params.getTexture(),
				initialSize,
				renderSystem,
				resourceMgr);
		}
		else if (params.getTextureRenderer())
		{
			mBatch = new mpp::QuadBatch(
				name,
				{
					params.getPrimitiveOptions(),
					PosType::vertexDataType(),
					{ TexType::vertexDataType(), params.fixedTextureData() },
					{ mpp::mesh::Vertex::DataType::None, true },
					params.useDiffuse(),
					params.rotate(),
					params.getWidth(),
					params.getHeight(),
					params.getIndexWidth()
				},
				params.sameSize(),
				params.getTextureRenderer(),
				initialSize,
				renderSystem,
				resourceMgr);
		}
		else
		{
			mBatch = new mpp::QuadBatch(
				name,
				{
					params.getPrimitiveOptions(),
					PosType::vertexDataType(),
					{ TexType::vertexDataType(), params.fixedTextureData() },
					{ mpp::mesh::Vertex::DataType::None, true },
					params.useDiffuse(),
					params.rotate(),
					params.getWidth(),
					params.getHeight(),
					params.getIndexWidth()
				},
				params.sameSize(),
				initialSize,
				renderSystem,
				resourceMgr);
		}
	}

	virtual ~QuadBatchRenderer()
	{
		delete mBatch;
	}

	mpp::QuadBatch* getBatch()
	{
		return mBatch;
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
			initStart = mBatch->getVertexCount(mBatch->getPrimitiveCount(batchSize));
			newVertices = true;
		}

		mBatch->startUpdate(count);

		float radiusX = mBatch->getMaxDimX() / 2.0f;
		float radiusY = mBatch->getMaxDimY() / 2.0f;

		auto posBuffer = (PosType::builtin_type*)mBatch->getAttributeData("POSITION").first;
		auto posStride = mBatch->getAttributeData("POSITION").second / sizeof(PosType::builtin_type);

		auto rotBuffer = (PosType::builtin_type*)mBatch->getAttributeData("ROTATION").first;
		auto rotStride = mBatch->getAttributeData("ROTATION").second / sizeof(PosType::builtin_type);

		TexType::builtin_type* texBuffer{ nullptr };
		size_t texStride{ 0 };

		if (mBatch->usingTexture())
		{
			texBuffer = (TexType::builtin_type*)mBatch->getAttributeData("TEXCOORDS").first;
			texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexType::builtin_type);
		}

		size_t vertexCount = mBatch->getVertexCount(mBatch->getPrimitiveCount(count));
		for (size_t pOffset = 0, rOffset = 0, tOffset = 0, i = 0; i < vertexCount; ++i)
		{
			uint32_t primitiveIndex = mBatch->usingPointSprites() ? i : i / 4;
			bool newVertex = i >= initStart;

			//
			// Position data
			//
			if (!mBatch->positionFixed() || newVertex)
			{
				PosType::builtin_type x, y;
				mDataProvider->position(primitiveIndex, x, y);

				if (mBatch->usingPointSprites())
				{
					// One vertex per quad
					posBuffer[pOffset + 0] = x;
					posBuffer[pOffset + 1] = y;
				}
				else
				{
					// Indexed, four vertices per quad
					int vertexIndex = i % 4;

					switch (vertexIndex)
					{
					case 0:
						posBuffer[pOffset + 0] = x - radiusX;
						posBuffer[pOffset + 1] = y - radiusY;
						break;
					case 1:
						posBuffer[pOffset + 0] = x + radiusX;
						posBuffer[pOffset + 1] = y - radiusY;
						break;
					case 2:
						posBuffer[pOffset + 0] = x + radiusX;
						posBuffer[pOffset + 1] = y + radiusY;
						break;
					case 3:
						posBuffer[pOffset + 0] = x - radiusX;
						posBuffer[pOffset + 1] = y + radiusY;
						break;
					}
				}

				if (mBatch->rotating() && mBatch->usingTriangles())
				{
					// Centroid for rotation in vertex shader
					posBuffer[pOffset + 2] = x;
					posBuffer[pOffset + 3] = y;
				}
			}

			//
			// Rotation data
			//
			if (!mBatch->rotationFixed() || newVertex)
			{
				PosType::builtin_type angle;
				mDataProvider->angle(primitiveIndex, angle);

				rotBuffer[rOffset + 0] = sinf(angle);
				rotBuffer[rOffset + 1] = cosf(angle);
			}

			//
			// Texture data
			//
			if (!mBatch->texcoordsFixed() || newVertex)
			{
				if (mBatch->usingPointSprites())
				{
					if (mBatch->usingTextureAtlas())
					{
						TexType::builtin_type u0, v0, u1, v1;
						mDataProvider->textureAtlasTexcoords(primitiveIndex, u0, v0, u1, v1);

						texBuffer[tOffset + 0] = u0;
						texBuffer[tOffset + 1] = v0;
						texBuffer[tOffset + 2] = u1;
						texBuffer[tOffset + 3] = v1;
					}
				}
				else
				{
					if (mBatch->usingTexture())
					{
						int vertexIndex = i % 4;

						TexType::builtin_type u0, v0, u1, v1;
						mDataProvider->textureAtlasTexcoords(primitiveIndex, u0, v0, u1, v1);

						// Indexed, four vertices per quad
						if (mBatch->usingTextureAtlas())
						{
							switch (vertexIndex)
							{
							case 0:
								texBuffer[tOffset + 0] = u0;
								texBuffer[tOffset + 1] = v0;
								break;
							case 1:
								texBuffer[tOffset + 0] = u1;
								texBuffer[tOffset + 1] = v0;
								break;
							case 2:
								texBuffer[tOffset + 0] = u1;
								texBuffer[tOffset + 1] = v1;
								break;
							case 3:
								texBuffer[tOffset + 0] = u0;
								texBuffer[tOffset + 1] = v1;
								break;
							}
						}
						else
						{
							switch (vertexIndex)
							{
							case 0:
								texBuffer[tOffset + 0] = 0.0f;
								texBuffer[tOffset + 1] = 0.0f;
								break;
							case 1:
								texBuffer[tOffset + 0] = 1.0f;
								texBuffer[tOffset + 1] = 0.0f;
								break;
							case 2:
								texBuffer[tOffset + 0] = 1.0f;
								texBuffer[tOffset + 1] = 1.0f;
								break;
							case 3:
								texBuffer[tOffset + 0] = 0.0f;
								texBuffer[tOffset + 1] = 1.0f;
								break;
							}
						}
					}
				}
			}

			// Next vertex
			pOffset += posStride;
			rOffset += rotStride;
			tOffset += texStride;
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

