#pragma once

#include <format>

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
			bool indexedVertices{ false };
		};

		template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
		class TriangleBatch2DRenderer : public BatchRenderer
		{
		protected:
			
			typedef TriangleBatch2DDataProvider<PosType, TexType, ColType> T2DDataProvider;

			typedef std::pair<mpp::TriangleBatch*, std::shared_ptr<T2DDataProvider>> BatchEntry;

		protected:

			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			std::vector<BatchEntry> mBatches;

		public:

			TriangleBatch2DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<T2DDataProvider> dataProvider,
				mpp::ResourcePtr textureOrMaterial,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: BatchRenderer()
				, mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
			{
				auto batch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P2D,
						params.useMaterialNotTexture,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ ColType::vertexDataType(), params.fixedColourData },
						params.useDiffuse,
						params.indexedVertices
					},
					textureOrMaterial,
					dataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);

				mBatches.push_back(make_pair(batch, dataProvider));

				mUniforms->setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			}

			virtual ~TriangleBatch2DRenderer()
			{
				for (auto entry : mBatches)
				{
					delete entry.first;
				}
			}

			void create() override
			{
				for (auto entry : mBatches)
				{
					auto [batch, dataProvider] = entry;

					batch->create();
				}

				update();
			}

			size_t update() override
			{
				size_t totalCount{ 0 };
				auto numBatches = (uint32_t)mBatches.size();

				for (uint32_t batchIndex = 0; batchIndex < numBatches; ++batchIndex)
				{
					auto entry = mBatches[batchIndex];
					auto [batch, dataProvider] = entry;

					size_t count = dataProvider->getNumPrimitives();
					size_t initStart{ ~0u }, batchSize = batch->getCount();
					bool newVertices{ false };
					if (count > batchSize)
					{
						initStart = batch->getPrimitiveCount(batchSize);
						newVertices = true;
					}

					batch->startUpdate(count);

					typedef typename PosType::builtin_type PosTypeBuiltin;
					typedef typename TexType::builtin_type TexTypeBuiltin;
					typedef typename ColType::builtin_type ColTypeBuiltin;

					auto posBuffer = (PosTypeBuiltin*)batch->getAttributeData("POSITION").first;
					auto posStride = batch->getAttributeData("POSITION").second / sizeof(PosTypeBuiltin);

					TexTypeBuiltin* texBuffer{ nullptr };
					size_t texStride{ 0 };

					if (batch->usingTexture())
					{
						texBuffer = (TexTypeBuiltin*)batch->getAttributeData("TEXCOORDS").first;
						texStride = batch->getAttributeData("TEXCOORDS").second / sizeof(TexTypeBuiltin);
					}

					auto colBuffer = (ColTypeBuiltin*)batch->getAttributeData("COLOUR").first;
					auto colStride = batch->getAttributeData("COLOUR").second / sizeof(ColTypeBuiltin);

					size_t triangleCount = batch->getPrimitiveCount(count);
					for (size_t pOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < triangleCount; ++i)
					{
						auto primitiveIndex = (uint32_t)i;
						bool newVertex = i >= initStart;

						//
						// Position data
						//
						if (!batch->positionFixed() || newVertex)
						{
							PosTypeBuiltin x0, y0, x1, y1, x2, y2;
							dataProvider->position(batchIndex, primitiveIndex, x0, y0, x1, y1, x2, y2);

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
						if (texBuffer && batch->usingTexture() && (!batch->texcoordsFixed() || newVertex))
						{
							TexTypeBuiltin u0, v0, u1, v1, u2, v2;
							dataProvider->texcoords(batchIndex, primitiveIndex, u0, v0, u1, v1, u2, v2);

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
							ColTypeBuiltin red, green, blue, alpha;
							dataProvider->colour(batchIndex, primitiveIndex, red, green, blue, alpha);

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
					totalCount += batch->getCount();
				}

				return totalCount;
			}

			void render() override
			{
				auto numBatches = (uint32_t)mBatches.size();
				for (uint32_t batchIndex = 0; batchIndex < numBatches; ++batchIndex)
				{
					auto [batch, dataProvider] = mBatches[batchIndex];

					if (batch->usingDiffuse())
					{
						auto colour = dataProvider->diffuse(batchIndex);
						mUniforms->updateUniform("DIFFUSE", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
					}

					auto const& model = static_cast<Model const&>(*batch->getModel().get());
					mRenderSystem->renderModelBatched(model, true);
				}
			}
		};

		template<typename PosType, typename TexType>
		class TriangleBatch2DRenderer<PosType, TexType, mpp::mesh::DataTypeNone> : public BatchRenderer
		{
		protected:

			typedef TriangleBatch2DDataProvider<PosType, TexType, mpp::mesh::DataTypeNone> T2DDataProvider;

			typedef std::pair<mpp::TriangleBatch*, std::shared_ptr<T2DDataProvider>> BatchEntry;

		protected:

			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			std::vector<BatchEntry> mBatches;

		public:

			TriangleBatch2DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<T2DDataProvider> dataProvider,
				mpp::ResourcePtr textureOrMaterial,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: BatchRenderer()
				, mRenderSystem(renderSystem)
				, mResourceMgr(resourceMgr)
			{
				auto batch = new mpp::TriangleBatch(
					name,
					{
						TriangleBatchOptions::Dimension::P2D,
						params.useMaterialNotTexture,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ mpp::mesh::Vertex::DataType::None, true },
						params.useDiffuse,
						params.indexedVertices
					},
					textureOrMaterial,
					dataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);

				mBatches.push_back(make_pair(batch, dataProvider));

				mUniforms->setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			}

			virtual ~TriangleBatch2DRenderer()
			{
				for (auto entry : mBatches)
				{
					delete entry.first;
				}
			}

			void create() override
			{
				for (auto entry : mBatches)
				{
					auto [batch, dataProvider] = entry;

					batch->create();
				}

				update();
			}

			size_t update(size_t count) override
			{
				size_t totalCount{ 0 };
				auto numBatches = (uint32_t)mBatches.size();

				for (uint32_t batchIndex = 0; batchIndex < numBatches; ++batchIndex)
				{
					auto entry = mBatches[batchIndex];
					auto [batch, dataProvider] = entry;

					size_t count = dataProvider->getNumPrimitives();

					size_t initStart{ ~0u }, batchSize = batch->getCount();
					bool newVertices{ false };
					if (count > batchSize)
					{
						initStart = batch->getPrimitiveCount(batchSize);
						newVertices = true;
					}

					batch->startUpdate(count);

					typedef typename PosType::builtin_type PosTypeBuiltin;
					typedef typename TexType::builtin_type TexTypeBuiltin;

					auto posBuffer = (PosTypeBuiltin*)batch->getAttributeData("POSITION").first;
					auto posStride = batch->getAttributeData("POSITION").second / sizeof(PosTypeBuiltin);

					TexTypeBuiltin* texBuffer{ nullptr };
					size_t texStride{ 0 };

					if (batch->usingTexture())
					{
						texBuffer = (TexTypeBuiltin*)batch->getAttributeData("TEXCOORDS").first;
						texStride = batch->getAttributeData("TEXCOORDS").second / sizeof(TexTypeBuiltin);
					}

					size_t triangleCount = batch->getPrimitiveCount(count);
					for (size_t pOffset = 0, tOffset = 0, i = 0; i < triangleCount; ++i)
					{
						uint32_t primitiveIndex = i;
						bool newVertex = i >= initStart;

						//
						// Position data
						//
						if (!batch->positionFixed() || newVertex)
						{
							PosTypeBuiltin x0, y0, x1, y1, x2, y2;
							dataProvider->position(batchIndex, primitiveIndex, x0, y0, x1, y1, x2, y2);

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
						if (batch->usingTexture() && (!batch->texcoordsFixed() || newVertex))
						{
							TexTypeBuiltin u0, v0, u1, v1, u2, v2;
							dataProvider->texcoords(batchIndex, primitiveIndex, u0, v0, u1, v1, u2, v2);

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

					batch->finishUpdate(count, newVertices);
					totalCount += batch->getCount();
				}

				return totalCount;
			}

			void render() override
			{
				auto numBatches = (uint32_t)mBatches.size();

				for (uint32_t batchIndex = 0; batchIndex < numBatches; ++batchIndex)
				{
					auto [batch, dataProvider] = mBatches[batchIndex];

					if (batch->usingDiffuse())
					{
						auto colour = dataProvider->diffuse(batchIndex);
						mUniforms->updateUniform("DIFFUSE", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
					}

					mRenderSystem->renderModelImmediate(*batch, true, mParams);
				}
			}
		};

		template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
		class TriangleMultiBatch2DRenderer : public TriangleBatch2DRenderer<PosType, TexType, ColType>
		{
			TriangleBatchRendererParams mTriParams;

		public:

			TriangleMultiBatch2DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<typename TriangleBatch2DRenderer<PosType, TexType, ColType>::T2DDataProvider> dataProvider,
				mpp::ResourcePtr textureOrMaterial,
				mpp::RenderSystem* renderSystem,
				mpp::ResourceManager* resourceMgr)
				: TriangleBatch2DRenderer(name, params, dataProvider, textureOrMaterial, renderSystem, resourceMgr)
				, mTriParams(params)
			{
			}

			uint32_t addBatch(mpp::ResourcePtr textureOrMaterial, bool useMaterialNotTexture, std::shared_ptr<typename TriangleBatch2DRenderer<PosType, TexType, ColType>::T2DDataProvider> dataProvider, bool create)
			{
				auto index = (uint32_t)this->mBatches.size();

				auto batch = new mpp::TriangleBatch(
					std::format("{}_{}", this->mName, index),
					{
						TriangleBatchOptions::Dimension::P2D,
						useMaterialNotTexture,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), mTriParams.fixedTextureData },
						{ ColType::vertexDataType(), mTriParams.fixedColourData },
						mTriParams.useDiffuse,
						mTriParams.indexedVertices
					},
					textureOrMaterial,
					dataProvider->getNumPrimitives(),
					this->mRenderSystem,
					this->mResourceMgr);

				this->mBatches.push_back(make_pair(batch, dataProvider));

				if (create)
				{
					batch->create();
				}

				return index;
			}
		};

		template<typename PosType, typename TexType, typename ColType = mpp::mesh::DataTypeNone>
		class TriangleBatch3DRenderer : public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::TriangleBatch* mBatch{ nullptr };

			std::shared_ptr<TriangleBatch3DDataProvider<PosType, TexType, ColType>> mDataProvider{ nullptr };

		public:

			TriangleBatch3DRenderer(std::string const& name,
				TriangleBatchRendererParams const& params,
				std::shared_ptr<TriangleBatch3DDataProvider<PosType, TexType, ColType>> dataProvider,
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
						TriangleBatchOptions::Dimension::P3D,
						params.useMaterialNotTexture,
						PosType::vertexDataType(),
						{ TexType::vertexDataType(), params.fixedTextureData },
						{ ColType::vertexDataType(), params.fixedColourData },
						params.useDiffuse,
						params.indexedVertices
					},
					textureOrMaterial,
					mDataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);

				mUniforms->setUniform("DIFFUSE", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			}

			virtual ~TriangleBatch3DRenderer()
			{
				delete mBatch;
			}

			ResourcePtr getModel()
			{
				return mBatch->getModel();
			}

			void create() override
			{
				mBatch->create();
				update();
			}

			size_t update() override
			{
				size_t count = mDataProvider->getNumPrimitives();
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

				auto nmlBuffer = (PosTypeBuiltin*)mBatch->getAttributeData("NORMAL").first;
				auto nmlStride = mBatch->getAttributeData("NORMAL").second / sizeof(PosTypeBuiltin);

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
				for (size_t pOffset = 0, nOffset = 0, tOffset = 0, cOffset = 0, i = 0; i < triangleCount; ++i)
				{
					auto primitiveIndex = (uint32_t)i;
					bool newVertex = i >= initStart;

					//
					// Position and normal data
					//
					if (!mBatch->positionFixed() || newVertex)
					{
						PosTypeBuiltin x0, y0, z0, x1, y1, z1, x2, y2, z2;
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

						float nx0, ny0, nz0, nx1, ny1, nz1, nx2, ny2, nz2;
						mDataProvider->normal(primitiveIndex, nx0, ny0, nz0, nx1, ny1, nz1, nx2, ny2, nz2);

						nmlBuffer[nOffset + 0] = nx0;
						nmlBuffer[nOffset + 1] = ny0;
						nmlBuffer[nOffset + 2] = nz0;
						nOffset += nmlStride;

						nmlBuffer[nOffset + 0] = nx1;
						nmlBuffer[nOffset + 1] = ny1;
						nmlBuffer[nOffset + 2] = nz1;
						nOffset += nmlStride;

						nmlBuffer[nOffset + 0] = nx2;
						nmlBuffer[nOffset + 1] = ny2;
						nmlBuffer[nOffset + 2] = nz2;
						nOffset += nmlStride;
					}
					else
					{
						pOffset += posStride * 3;
						nOffset += nmlStride * 3;
					}

					//
					// Texture data
					//
					if (texBuffer && mBatch->usingTexture() && (!mBatch->texcoordsFixed() || newVertex))
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

	}
}