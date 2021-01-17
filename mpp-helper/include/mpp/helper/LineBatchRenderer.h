#pragma once

#include <mpp/BatchRenderer.h>
#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/LineBatch.h>

#include <mpp/mesh/VertexTypeSpecification.h>

#include "Config.h"
#include "LineBatchDataProvider.h"

namespace mpp
{
	namespace helper
	{

		struct LineBatchRendererParams
		{
			bool fixedColourData;
			bool useVertexColours, useDiffuse;
		};

		template<typename PosType, typename ColType = mpp::mesh::DataTypeNone>
		class LineBatchRenderer : public BatchRenderer
		{
			mpp::RenderSystem* mRenderSystem{ nullptr };

			mpp::ResourceManager* mResourceMgr{ nullptr };

			mpp::LineBatch* mBatch{ nullptr };

			std::shared_ptr<LineBatchDataProvider<PosType, ColType>> mDataProvider{ nullptr };

		public:

			LineBatchRenderer(std::string const& name,
				LineBatchRendererParams const& params,
				std::shared_ptr<LineBatchDataProvider<PosType, ColType>> dataProvider,
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
					mDataProvider->getNumPrimitives(),
					renderSystem,
					resourceMgr);
			}

			virtual ~LineBatchRenderer()
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

				auto colBuffer = (ColType::builtin_type*)mBatch->getAttributeData("COLOUR").first;
				auto colStride = mBatch->getAttributeData("COLOUR").second / sizeof(ColType::builtin_type);

				size_t lineCount = mBatch->getPrimitiveCount(count);
				for (size_t pOffset = 0, cOffset = 0, i = 0; i < lineCount; ++i)
				{
					uint32_t primitiveIndex = i;
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
					else
					{
						pOffset += posStride * 2;
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

	}
}