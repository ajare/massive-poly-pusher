#pragma once

#include <mpp/BatchRenderer.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/QuadBatch.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Config.h"
#include "QuadBatchDataProvider.h"

namespace mpp
{
	namespace helper
	{

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
		class QuadBatchRenderer : public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::QuadBatch* mBatch{ nullptr };

			std::shared_ptr<QuadBatchDataProvider<PosType, TexType, ColType>> mDataProvider{ nullptr };

		public:

			QuadBatchRenderer(std::string const& name,
				QuadBatchRendererParams const& params,
				std::shared_ptr<QuadBatchDataProvider<PosType, TexType, ColType>> dataProvider,
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
						mDataProvider->getNumQuads(),
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
						mDataProvider->getNumQuads(),
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
						mDataProvider->getNumQuads(),
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

			void create() override
			{
				mBatch->load();
				update(mBatch->getCapacity());
			}

			size_t update(size_t count) override
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

			void render() override
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
			: public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::QuadBatch* mBatch{ nullptr };

			std::shared_ptr<QuadBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>> mDataProvider;

		public:

			QuadBatchRenderer(std::string const& name,
				QuadBatchRendererParams const& params,
				std::shared_ptr<QuadBatchDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>> dataProvider,
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
						mDataProvider->getNumPrimitives(),
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
						mDataProvider->getNumPrimitives(),
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
						mDataProvider->getNumPrimitives(),
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

	}
}