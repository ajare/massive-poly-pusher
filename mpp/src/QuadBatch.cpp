#include <cmath>

#include "utils/MemTracker.h"

#include "mpp/QuadBatch.h"
#include "mpp/DefaultShaders.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		QuadBatchOptions const& options, 
		bool sameSize,
		ResourcePtr texture,
		size_t initialCapacity,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, defaultVertexShader, defaultFragmentShader, "quad", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
		, mOptions(options)
		, mSameSize(sameSize)
		, mTexture(texture)
		, mTextureRenderer(nullptr)
		, mPointSize((float)options.maxSizeX)
	{
		setPrimitiveOptions();
	}

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		QuadBatchOptions const& options,
		bool sameSize,
		ResourcePtr texture,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: QuadBatch(name, options, sameSize, texture, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, renderSystem, resourceMgr)
	{
	}

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		QuadBatchOptions const& options,
		bool sameSize,
		TextureRendererPtr textureRenderer,
		size_t initialCapacity,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, defaultVertexShader, defaultFragmentShader, "quad", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
		, mOptions(options)
		, mSameSize(sameSize)
		, mTexture(nullptr)
		, mTextureRenderer(textureRenderer)
		, mPointSize((float)options.maxSizeX)
	{
		setPrimitiveOptions();
	}

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		QuadBatchOptions const& options,
		bool sameSize,
		TextureRendererPtr textureRenderer,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: QuadBatch(name, options, sameSize, textureRenderer, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, renderSystem, resourceMgr)
	{
	}

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		QuadBatchOptions const& options,
		bool sameSize,
		size_t initialCapacity,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Batch(name, initialCapacity, defaultVertexShader, defaultFragmentShader, "quad", options.colourAttrib, options.useDiffuse, renderSystem, resourceMgr)
		, mOptions(options)
		, mSameSize(sameSize)
		, mTexture(nullptr)
		, mTextureRenderer(nullptr)
		, mPointSize((float)options.maxSizeX)
	{
		setPrimitiveOptions();
	}

	/*
	 * Constructor.
	 *
	 */
	QuadBatch::QuadBatch(string const& name,
		QuadBatchOptions const& options,
		bool sameSize,
		size_t initialCapacity,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: QuadBatch(name, options, sameSize, initialCapacity, VertexShader2dTemplate, FragmentShader2dTemplate, renderSystem, resourceMgr)
	{
	}

	void QuadBatch::setPrimitiveOptions()
	{
		// Set vertex options
		float size = (float)max(mOptions.maxSizeX, mOptions.maxSizeY);
		bool square = mSameSize && mOptions.maxSizeX == mOptions.maxSizeY;

		Caps const& caps = getRenderSystem()->getCaps();
		if (mOptions.primitiveOptions == QuadBatchOptions::PrimitiveOptions::Points)
		{
			if (mOptions.rotate && !usingTexture())
			{
				THROW_MPP("Cannot use point sprites when rotating with no texture.", __LINE__, __FILE__, __func__);
			}
			if (!square)
			{
				THROW_MPP("Cannot use point sprites for non-square quads.", __LINE__, __FILE__, __func__);
			}
			if (caps.pointSizeRange[0] > size || caps.pointSizeRange[1] < size)
			{
				THROW_MPP("Cannot use point sprites for theses sizes of quad.", __LINE__, __FILE__, __func__);
			}
		}

		if (mOptions.primitiveOptions != QuadBatchOptions::PrimitiveOptions::Triangles)
		{
			mOptions.primitiveOptions = QuadBatchOptions::PrimitiveOptions::Points;
		}

		if (usingTexture() && mOptions.texcoordAttrib.dataType == mesh::Vertex::DataType::None)
		{
			THROW_MPP("Must specify a texcoord type when using a texture.", __LINE__, __FILE__, __func__);
		}
	}

	mesh::Primitive::Type QuadBatch::getPrimitiveType() const
	{
		return usingPointSprites() ? mesh::Primitive::Type::Points : mesh::Primitive::Type::Triangles;
	}

	bool QuadBatch::indexedVertices() const
	{
		return !usingPointSprites();
	}

	/*
	 * Create indices for a primitive.
	 *
	 */
	void QuadBatch::createIndexData(vector<uint8_t>& data, uint32_t start, size_t count)
	{
		size_t vertexSize{ 6 * (mOptions.indexWidth / 8) };
		data.resize(count * vertexSize);

		uint32_t* ptr = (uint32_t*)&data[start * vertexSize]; // Indices will be 16 or 32-bit, so use 32 to cover both
		int indexBytes = mOptions.indexWidth / 8;

		for (uint32_t i = start; i < count; ++i)
		{
			if (indexBytes == 2)
			{
				*ptr = (i * 4 + 0) + ((i * 4 + 1) << 16); ptr++;
				*ptr = (i * 4 + 2) + ((i * 4 + 2) << 16); ptr++;
				*ptr = (i * 4 + 3) + ((i * 4 + 0) << 16); ptr++;
			}
			else if (indexBytes == 4)
			{
				*ptr = i * 4 + 0; ptr++;
				*ptr = i * 4 + 1; ptr++;
				*ptr = i * 4 + 2; ptr++;
				*ptr = i * 4 + 2; ptr++;
				*ptr = i * 4 + 3; ptr++;
				*ptr = i * 4 + 0; ptr++;
			}
		}
	}

	/*
	 * Get the number of vertices required, given the number of primitives.
	 *
	 */
	size_t QuadBatch::getVertexCount(size_t primitiveCount) const
	{
		// Assume that if not using point sprites, primitiveCount must be a
		// multiple of 2.
		return primitiveCount * (usingPointSprites() ? 1 : 2);
	}

	void QuadBatch::createMeshSpecification(mesh::Primitive::Type primitiveType)
	{
		/*
		Position holds position (xy) and centroid (zw) for when we're rotating and
		using triangles.
		
		User-defined Rotation holds rotation (xy) for when we're rotating

		Texcoords hold texcoords when we have a single texture (xy) and full xyzw
		when we're using point sprites and an atlas

		Colour is optional, but always xyzw
		*/

		mSpecification = mesh::MeshSpecification(primitiveType);
		auto dynamicLayout = mSpecification.createVertexBufferAttributeLayout(false);
		mesh::VertexBufferAttributeLayout* staticLayout{ nullptr };

		// For position, if we're rotating with triangles, then we need to store the
		// angle in .zw, as it needs the same type as position.
		if (rotating() && usingTriangles())
		{
			dynamicLayout->createAttribute(mesh::Vertex::Component::Position4, mOptions.positionType, false);
		}
		else
		{
			dynamicLayout->createAttribute(mesh::Vertex::Component::Position2, mOptions.positionType, false);
		}

		// If we're not rotating, store rotation as static data
		mesh::VertexBufferAttributeLayout* rotationLayout{ nullptr };
		if (!rotating())
		{
			if (!staticLayout)
			{
				staticLayout = mSpecification.createVertexBufferAttributeLayout(true);
			}

			rotationLayout = staticLayout;
		}
		else
		{
			rotationLayout = dynamicLayout;
		}

		rotationLayout->createAttribute(mesh::Vertex::Component::UserDefined2, "ROTATION", mesh::Vertex::DataType::Float, false);

		// Texture coords
		mesh::VertexBufferAttributeLayout* texcoordLayout{ nullptr };
		if (mOptions.texcoordAttrib.fixedValues)
		{
			if (!staticLayout)
			{
				staticLayout = mSpecification.createVertexBufferAttributeLayout(true);
			}

			texcoordLayout = staticLayout;
		}
		else
		{
			texcoordLayout = dynamicLayout;
		}

		if (usingPointSprites())
		{
			// Only need texcoords if we're using a texture atlas (and implicitly, a texture), otherwise
			// gl_PointCoord is used
			if (usingTextureAtlas())
			{
				texcoordLayout->createAttribute(mesh::Vertex::Component::TexCoord4, mOptions.texcoordAttrib.dataType, false);
			}
		}
		else
		{
			if (usingTexture())
			{
				texcoordLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mOptions.texcoordAttrib.dataType, false);
			}
		}
		
		// Colour
		if (mOptions.colourAttrib.dataType != mesh::Vertex::DataType::None)
		{
			mesh::VertexBufferAttributeLayout* colourLayout{ nullptr };
			if (mOptions.colourAttrib.fixedValues)
			{
				if (!staticLayout)
				{
					staticLayout = mSpecification.createVertexBufferAttributeLayout(true);
				}

				colourLayout = staticLayout;
			}
			else
			{
				colourLayout = dynamicLayout;
			}

			colourLayout->createAttribute(mesh::Vertex::Component::Colour4, mOptions.colourAttrib.dataType, mesh::Vertex::isDataTypeNormalisable(mOptions.colourAttrib.dataType));
		}
	}

	/*
	 * Create the data required.
	 *
	 */
	void QuadBatch::createImpl()
	{
		// Set primitive options
		auto primitiveType = getPrimitiveType();
		int primitiveCount = getPrimitiveCount(getCapacity());

		createMeshSpecification(primitiveType);

		// Set program flags
		uint32_t flags = 0
			| (usingPointSprites() ? MPP_PROGRAM_TAGS_PRIM_POINTS : MPP_PROGRAM_TAGS_PRIM_TRIANGLES)
			| (usingTexture() ? MPP_PROGRAM_TAGS_TEXTURE1 : 0)
			| (usingTextureAtlas() ? MPP_PROGRAM_TAGS_ATLAS : 0)
			| (rotating() ? MPP_PROGRAM_TAGS_ROTATION : 0)
			| (usingDiffuse() ? MPP_PROGRAM_TAGS_DIFFUSE : 0);

		// Material
		if (mTextureRenderer)
		{
			mTexture = mTextureRenderer->createRenderTexture(mOptions.maxSizeX, mOptions.maxSizeY);
		}

		auto materialResource = createMaterial(getName() + "_QuadBatch", mTexture, flags);
		size_t vertexCount = getVertexCount(primitiveCount);

		Mesh* mesh{ nullptr };
		if (indexedVertices())
		{
			vector<uint8_t> indices;
			createIndexData(indices, 0, getCapacity());

			mesh = new Mesh(
				getRenderSystem(),
				getName(),
				materialResource,
				primitiveType,
				primitiveCount,
				mOptions.indexWidth,
				indices,
				mesh::VertexBufferStorageType::Dynamic,
				mPointSize);
		}
		else
		{
			mesh = new Mesh(
				getRenderSystem(),
				getName(),
				materialResource,
				primitiveType,
				primitiveCount,
				mesh::VertexBufferStorageType::Dynamic,
				mPointSize);
		}

		for (size_t i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = mSpecification.getVertexBufferAttributeLayout(i);
			createVertexBuffer(i, mesh, vertexCount, layout.isStatic());
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
	}

	size_t QuadBatch::getPrimitiveCount(size_t objectCount) const
	{
		return objectCount * (usingPointSprites() ? 1 : 2);
	}

	int QuadBatch::getMaxDimX() const
	{
		return mOptions.maxSizeX;
	}

	int QuadBatch::getMaxDimY() const
	{
		return mOptions.maxSizeY;
	}

	bool QuadBatch::usingPointSprites() const
	{
		return mOptions.primitiveOptions == QuadBatchOptions::PrimitiveOptions::Points;
	}

	bool QuadBatch::usingTriangles() const
	{
		return mOptions.primitiveOptions == QuadBatchOptions::PrimitiveOptions::Triangles;
	}

	bool QuadBatch::rotating() const
	{
		return mOptions.rotate;
	}

	bool QuadBatch::usingTexture() const
	{
		return mTexture || mTextureRenderer;
	}

	bool QuadBatch::usingTextureAtlas() const
	{
		return mTexture && mTexture->getType() == "TextureAtlas";
	}

	bool QuadBatch::positionFixed() const
	{
		return false;
	}

	bool QuadBatch::rotationFixed() const
	{
		return !mOptions.rotate;
	}

	bool QuadBatch::texcoordsFixed() const
	{
		return mOptions.texcoordAttrib.fixedValues;
	}

	bool QuadBatch::colourFixed() const
	{
		return mOptions.colourAttrib.fixedValues;
	}

	ResourcePtr QuadBatch::getTexture()
	{
		return mTexture;
	}

}