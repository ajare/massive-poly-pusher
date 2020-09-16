#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/LineBatch.h>
#include <mpp/QuadBatch.h>
#include <mpp/CircleBatch.h>
#include <mpp/TriangleBatch.h>
#include <mpp/IndexedTriangleBatch.h>

mpp::TriangleBatch* createTriangleBatch(std::string const& name, std::string const& texture, size_t triangleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

mpp::IndexedTriangleBatch* createIndexedTriangleBatch(std::string const& name, std::string const& texture, size_t indexedTriangleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

mpp::LineBatch* createLineBatch(std::string const& name, size_t lineBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

mpp::QuadBatch2* createQuadBatch(std::string const& name, std::string const& texture, size_t quadBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

mpp::CircleBatch* createCircleBatch(std::string const& name, size_t circleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

size_t updateTriangleBatch(mpp::RenderSystem* renderSystem, mpp::TriangleBatch* triBatch, size_t count, float totalTime);

size_t updateIndexedTriangleBatch(mpp::RenderSystem* renderSystem, mpp::IndexedTriangleBatch* triBatch, size_t count, float totalTime);

size_t updateLineBatch(mpp::RenderSystem* renderSystem, mpp::LineBatch* lineBatch, size_t count, float totalTime);

size_t updateQuadBatch(mpp::RenderSystem* renderSystem, mpp::QuadBatch2* quadBatch, size_t count, float totalTime);

size_t updateCircleBatch(mpp::RenderSystem* renderSystem, mpp::CircleBatch* circleBatch, size_t count, float totalTime);
