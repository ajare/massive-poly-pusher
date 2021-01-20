#pragma once

#include <mpp/BatchRenderer.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TriangleBatch.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Config.h"
#include "TriangleBatchDataProvider.h"

namespace mpp
{
	namespace helper
	{

		struct TriangleBatchRendererParams
		{
			bool fixedTextureData, fixedColourData;
			bool useDiffuse;
		};

		template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
		class TriangleBatch2DRenderer : public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::TriangleBatch* mBatch{ nullptr };

			std::shared_ptr<TriangleBatch2DDataProvider<PosType, TexType, ColType>> mDataProvider{ nullptr };

		public:

			TriangleBatch2DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<TriangleBatch2DDataProvider<PosType, TexType, ColType>> dataProvider,
				mpp::ResourcePtr texture,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
				, mDataProvider(dataProvider)
			{
				mBatch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P2D,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ ColType::vertexDataType(), params.fixedColourData },
						params.useDiffuse
					},
					texture,
					mDataProvider->getNumTriangles(),
					renderSystem,
					resourceMgr);
			}

			virtual ~TriangleBatch2DRenderer()
			{
				delete mBatch;
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

				auto colBuffer = (uint8_t*)mBatch->getAttributeData("COLOUR").first;
				auto colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);

				size_t triangleCount = mBatch->getPrimitiveCount(count);
				for (size_t pOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < triangleCount; ++i)
				{
					uint32_t primitiveIndex = i;
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
		class TriangleBatch2DRenderer<PosType, TexType, mpp::mesh::DataTypeNone> : public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::TriangleBatch* mBatch{ nullptr };

			std::shared_ptr<TriangleBatch2DDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>> mDataProvider{ nullptr };

		public:

			TriangleBatch2DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<TriangleBatch2DDataProvider<PosType, TexType, mpp::mesh::DataTypeNone>> dataProvider,
				mpp::ResourcePtr texture,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
				, mDataProvider(dataProvider)
			{
				mBatch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P2D,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ mpp::mesh::Vertex::DataType::None, true },
						params.useDiffuse
					},
					texture,
					mDataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);
			}

			virtual ~TriangleBatch2DRenderer()
			{
				delete mBatch;
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
					uint32_t primitiveIndex = i;
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

		template<typename PosType, typename TexType, typename ColType>
		class TriangleBatch3DRenderer : public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::ResourcePtr mBatch;

			std::shared_ptr<TriangleBatch3DDataProvider<PosType, TexType, ColType>> mDataProvider{ nullptr };

		public:

			TriangleBatch3DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<TriangleBatch3DDataProvider<PosType, TexType, ColType>> dataProvider,
				mpp::ResourcePtr texture,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
				, mDataProvider(dataProvider)
			{
				auto batch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P3D,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ ColType::vertexDataType(), params.fixedColourData },
						false
					},
					texture,
					mDataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);

				mBatch = ResourcePtr(batch);
			}

			virtual ~TriangleBatch3DRenderer()
			{
			}

			void create() override
			{
				auto batch = static_cast<TriangleBatch*>(mBatch.get());

				batch->load();
				update(batch->getCapacity());
			}

			size_t update(size_t count) override
			{
				auto batch = static_cast<TriangleBatch*>(mBatch.get());

				size_t initStart{ ~0u }, batchSize = batch->getCount();
				bool newVertices{ false };
				if (count > batchSize)
				{
					initStart = batch->getPrimitiveCount(batchSize);
					newVertices = true;
				}

				batch->startUpdate(count);

				auto posBuffer = (PosType::builtin_type*)batch->getAttributeData("POSITION").first;
				auto posStride = batch->getAttributeData("POSITION").second / sizeof(PosType::builtin_type);

				auto norBuffer = (PosType::builtin_type*)batch->getAttributeData("NORMAL").first;
				auto norStride = batch->getAttributeData("NORMAL").second / sizeof(PosType::builtin_type);

				TexType::builtin_type* texBuffer{ nullptr };
				size_t texStride{ 0 };

				if (batch->usingTexture())
				{
					texBuffer = (TexType::builtin_type*)batch->getAttributeData("TEXCOORDS").first;
					texStride = batch->getAttributeData("TEXCOORDS").second / sizeof(TexType::builtin_type);
				}

				auto colBuffer = (uint8_t*)batch->getAttributeData("COLOUR").first;
				auto colStride = batch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);

				size_t triangleCount = batch->getPrimitiveCount(count);
				for (size_t pOffset = 0, nOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < triangleCount; ++i)
				{
					uint32_t primitiveIndex = i;
					bool newVertex = i >= initStart;

					//
					// Position data
					//
					if (!batch->positionFixed() || newVertex)
					{
						PosType::builtin_type x0, y0, z0, x1, y1, z1, x2, y2, z2;
						mDataProvider->position(primitiveIndex, x0, y0, z0, x1, y1, z1, x2, y2, z2);

						posBuffer[pOffset + 0] = x0;
						posBuffer[pOffset + 1] = y0;
						posBuffer[pOffset + 2] = z0;
						pOffset += posStride;

						posBuffer[pOffset + 0] = x1;
						posBuffer[pOffset + 1] = y1;
						posBuffer[pOffset + 2] = z1;
						pOffset += posStride;

						posBuffer[pOffset + 0] = x2;
						posBuffer[pOffset + 1] = y2;
						posBuffer[pOffset + 2] = z2;
						pOffset += posStride;
					}
					else
					{
						pOffset += posStride * 3;
					}

					//
					// Normal data
					//
					if (!batch->positionFixed() || newVertex)
					{
						PosType::builtin_type x0, y0, z0, x1, y1, z1, x2, y2, z2;
						mDataProvider->normal(primitiveIndex, x0, y0, z0, x1, y1, z1, x2, y2, z2);

						norBuffer[nOffset + 0] = x0;
						norBuffer[nOffset + 1] = y0;
						norBuffer[nOffset + 2] = z0;
						nOffset += norStride;

						norBuffer[nOffset + 0] = x1;
						norBuffer[nOffset + 1] = y1;
						norBuffer[nOffset + 2] = z1;
						nOffset += norStride;

						norBuffer[nOffset + 0] = x2;
						norBuffer[nOffset + 1] = y2;
						norBuffer[nOffset + 2] = z2;
						nOffset += norStride;
					}
					else
					{
						nOffset += norStride * 3;
					}

					//
					// Texture data
					//
					if (batch->usingTexture() && (!batch->texcoordsFixed() || newVertex))
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
					if (batch->usingColour() && (!batch->colourFixed() || newVertex))
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

				batch->finishUpdate(count, newVertices);
				return batch->getCount();
			}

			void render() override
			{
			}

			ResourcePtr getModel()
			{
				return mBatch;
			}
		};

	}
}