#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/QuadBatch.h>

#include <mpp/mesh/VertexTypeSpecification.h>

template<typename PosType, typename TexType, typename ColType>
class QuadBatchDataProvider
{
public:

	virtual void position(uint32 index, typename PosType::builtin_type& x, typename PosType::builtin_type& y) = 0;

	virtual void angle(uint32 index, float& angle) = 0;

	virtual void textureAtlasTexcoords(uint32 index, typename TexType::builtin_type& u0, typename TexType::builtin_type& v0, typename TexType::builtin_type& u1, typename TexType::builtin_type& v1) = 0;

	virtual void colour(uint32 index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha) = 0;

	virtual mpp::Colour diffuse() = 0;

	virtual void update(float frameTime) {}
};

template<typename PosType, typename TexType, typename ColType>
class TestQuadBatchDataProvider : public QuadBatchDataProvider<PosType, TexType, ColType>
{
public:

	void position(uint32 index, typename PosType::builtin_type& x, typename PosType::builtin_type& y)
	{
	}

	void angle(uint32 index, float& angle)
	{
	}

	void textureAtlasTexcoords(uint32 index, typename TexType::builtin_type& u0, typename TexType::builtin_type& v0, typename TexType::builtin_type& u1, typename TexType::builtin_type& v1)
	{
	}

	void colour(uint32 index, typename ColType::builtin_type& red, typename ColType::builtin_type& green, typename ColType::builtin_type& blue, typename ColType::builtin_type& alpha)
	{
	}

	mpp::Colour diffuse()
	{
		return mpp::Colour::White;
	}

};

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
	float mTime{ 0.0f };

public:

	void position(uint32 index, float& x, float& y)
	{
		x = 400 + sinf((index + 1) * mTime / 10.0f) * 100;
		y = 300 + cosf((index + 2) * mTime / 10.0f) * 100;
	}

	void angle(uint32 index, float& angle)
	{
		angle = index * mTime;
	}

	void textureAtlasTexcoords(uint32 index, float& u0, float& v0, float& u1, float& v1)
	{
		const float txWidth = 1.0f / 8;

		u0 = txWidth * 0;
		v0 = 0.0f;
		u1 = txWidth * 1;
		v1 = 1.0f;
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

struct QuadBatchRendererParams
{
	mpp::QuadBatchOptions::PrimitiveOptions primitiveOptions;
	bool fixedTextureData, fixedColourData;
	bool useVertexColours, useDiffuse;
	bool rotate;
	size_t width, height;
	size_t indexWidth;
};

template<typename PosType, typename TexType, typename ColType>
class QuadBatchRenderer
{
	mpp::RenderSystem* mRenderSystem{ nullptr };

	mpp::ResourceManager* mResourceMgr{ nullptr };

	mpp::QuadBatch* mBatch{ nullptr };

	QuadBatchDataProvider<PosType, TexType, ColType>* mDataProvider{ nullptr };

public:

	QuadBatchRenderer(std::string const& name, 
		QuadBatchRendererParams const& params, 
		QuadBatchDataProvider<PosType, TexType, ColType>* dataProvider, 
		mpp::ResourcePtr texture, 
		size_t initialSize, 
		mpp::RenderSystem* renderSystem,
		mpp::ResourceManager* resourceMgr)
		: mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
		, mDataProvider(dataProvider)
	{
		bool sameSize = params.width == params.height;
		if (texture)
		{
			// Need to load texture first, to get its sizes
			texture->load();

			if (texture->getType() == "Texture")
			{
				auto t = dynamic_cast<mpp::Texture*>(texture.get());
				sameSize = t->getWidth() == t->getHeight();
			}
			else
			{
				auto t = dynamic_cast<mpp::TextureAtlas*>(texture.get());
				sameSize = t->getWidth() / t->getImagesX() == t->getHeight() / t->getImagesY();
			}
		}

		mBatch = new mpp::QuadBatch(
			name,
			{
				params.primitiveOptions,
				PosType::vertexDataType(),
				{ TexType::vertexDataType(), params.fixedTextureData },
				{ params.useVertexColours ? ColType::vertexDataType() : mpp::mesh::Vertex::DataType::None, params.fixedColourData },
				params.useDiffuse,
				params.rotate,
				params.width,
				params.height,
				params.indexWidth
			},
			sameSize,
			texture,
			initialSize,
			renderSystem,
			resourceMgr);
	}

	virtual ~QuadBatchRenderer()
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

		TexType::builtin_type* texBuffer;
		size_t texStride{ 0 };

		if (mBatch->usingTexture())
		{
			texBuffer = (TexType::builtin_type*)mBatch->getAttributeData("TEXCOORDS").first;
			texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexType::builtin_type);
		}

		ColType::builtin_type* colBuffer;
		size_t colStride{ 0 };

		if (mBatch->usingColour())
		{
			colBuffer = (uint8*)mBatch->getAttributeData("COLOUR").first;
			colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);
		}

		size_t vertexCount = mBatch->getVertexCount(mBatch->getPrimitiveCount(count));
		for (size_t pOffset = 0, rOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < vertexCount; ++i)
		{
			uint32 primitiveIndex = mBatch->usingPointSprites() ? i : i / 4;
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
			if (mBatch->usingColour() && (!mBatch->colourFixed() || newVertex))
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

