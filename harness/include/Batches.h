#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/LineBatch.h>
#include <mpp/CircleBatch.h>
#include <mpp/TriangleBatch.h>
#include <mpp/IndexedTriangleBatch.h>

mpp::TriangleBatch* createTriangleBatch(std::string const& name, std::string const& texture, size_t triangleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

mpp::IndexedTriangleBatch* createIndexedTriangleBatch(std::string const& name, std::string const& texture, size_t indexedTriangleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

mpp::CircleBatch* createCircleBatch(std::string const& name, size_t circleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

size_t updateTriangleBatch(mpp::RenderSystem* renderSystem, mpp::TriangleBatch* triBatch, size_t count, float totalTime);

size_t updateIndexedTriangleBatch(mpp::RenderSystem* renderSystem, mpp::IndexedTriangleBatch* triBatch, size_t count, float totalTime);

size_t updateCircleBatch(mpp::RenderSystem* renderSystem, mpp::CircleBatch* circleBatch, size_t count, float totalTime);
