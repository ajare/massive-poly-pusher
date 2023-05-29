#pragma once

#include <mpp/BatchRenderer.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/TriangleBatch.h>
#include <mpp/ModelRenderParams.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Config.h"
#include "TriangleBatchDataProvider.h"

namespace mpp
{
	namespace helper
	{

		struct TriangleBatchRendererParams
		{
			bool useMaterialNotTexture{ false };
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
				mpp::ResourcePtr textureOrMaterial,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: BatchRenderer()
				, mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
				, mDataProvider(dataProvider)
			{
				mBatch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P2D,
						params.useMaterialNotTexture,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ ColType::vertexDataType(), params.fixedColourData },
						params.useDiffuse
					},
					textureOrMaterial,
					mDataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);

				mUniforms->setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			}

			virtual ~TriangleBatch2DRenderer()
			{
				delete mBatch;
			}

			void create() override
			{
				mBatch->create();
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

				typedef typename PosType::builtin_type PosTypeBuiltin;
				typedef typename TexType::builtin_type TexTypeBuiltin;
				typedef typename ColType::builtin_type ColTypeBuiltin;

				auto posBuffer = (PosTypeBuiltin*)mBatch->getAttributeData("POSITION").first;
				auto posStride = mBatch->getAttributeData("POSITION").second / sizeof(PosTypeBuiltin);

				TexTypeBuiltin* texBuffer{ nullptr };
				size_t texStride{ 0 };

				if (mBatch->usingTexture())
				{
					texBuffer = (TexTypeBuiltin*)mBatch->getAttributeData("TEXCOORDS").first;
					texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexTypeBuiltin);
				}

				auto colBuffer = (ColTypeBuiltin*)mBatch->getAttributeData("COLOUR").first;
				auto colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColTypeBuiltin);

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
						PosTypeBuiltin x0, y0, x1, y1, x2, y2;
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
						TexTypeBuiltin u0, v0, u1, v1, u2, v2;
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
						ColTypeBuiltin red, green, blue, alpha;
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
				if (mBatch->usingDiffuse())
				{
					auto colour = mDataProvider->diffuse();
					mUniforms->updateUniform("DIFFUSE", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
				}

				auto const& model = static_cast<Model const&>(*mBatch->getModel().get());
				mRenderSystem->renderModelBatched(model, true);
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
				mpp::ResourcePtr textureOrMaterial,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: BatchRenderer()
				, mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
				, mDataProvider(dataProvider)
			{
				mBatch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P2D,
						params.useMaterialNotTexture,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ mpp::mesh::Vertex::DataType::None, true },
						params.useDiffuse
					},
					textureOrMaterial,
					mDataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);

				mUniforms->setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
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

				typedef typename PosType::builtin_type PosTypeBuiltin;
				typedef typename TexType::builtin_type TexTypeBuiltin;

				auto posBuffer = (PosTypeBuiltin*)mBatch->getAttributeData("POSITION").first;
				auto posStride = mBatch->getAttributeData("POSITION").second / sizeof(PosTypeBuiltin);

				TexTypeBuiltin* texBuffer{ nullptr };
				size_t texStride{ 0 };

				if (mBatch->usingTexture())
				{
					texBuffer = (TexTypeBuiltin*)mBatch->getAttributeData("TEXCOORDS").first;
					texStride = mBatch->getAttributeData("TEXCOORDS").second / sizeof(TexTypeBuiltin);
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
						PosTypeBuiltin x0, y0, x1, y1, x2, y2;
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
						TexTypeBuiltin u0, v0, u1, v1, u2, v2;
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
				if (mBatch->usingDiffuse())
				{
					auto colour = mDataProvider->diffuse();
					mUniforms->updateUniform("DIFFUSE", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
				}

				mRenderSystem->renderModelImmediate(*mBatch, true, mParams);
			}
		};

	}
}