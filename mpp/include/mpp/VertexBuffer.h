#pragma once

#include "Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include <memory>

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/VertexBufferDefinition.h"
#include "mpp/mesh/VertexBufferStorageType.h"

namespace mpp
{
	class RenderSystem;

	class _MPPAPI VertexBuffer
	{
		struct Attribute
		{
			int id;
			int componentSize, sizeInBytes;
			GLenum dataType;
			int offsetInBytes;
			bool normalise;
		};

	private:

		GLuint mVBO;

		RenderSystem* mwRenderSystem;

		mesh::VertexBufferStorageType mStorageType;

		int mVertexCount;
		
		int mVertexStride;

		bool mStreaming;

		bool mStatic;
		
		std::vector<Attribute> mAttributes;

		std::vector<int8_t> mData;

		size_t mMaxDataSize;

	private:

		void allocate(size_t size);

	public:

		VertexBuffer(RenderSystem* renderSystem, mesh::VertexBufferStorageType storageType, int vertexCount, int vertexStride, bool streaming, bool staticData, std::shared_ptr<const int8_t> data);

		virtual ~VertexBuffer();

		void setAttribute(int id, mesh::Vertex::DataType dataType, int componentSize, int offset, bool normalise);

		int getNumVertices() const;
			
		int getVertexStride() const;

		bool isStatic() const;

		int getNumAttributes() const;

		void enableAttribute(int index, bool enable);

		std::vector<int8_t>& getBufferData();

		void mapBufferData(int numVertices);

		void mapBufferData(int startVertex, int numVertices);

		void bind();

		void unbind();

		void load();

		void unload();
	};

}