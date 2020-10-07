#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>
#include <mpp/LineBatch.h>
#include <mpp/CircleBatch.h>
#include <mpp/TriangleBatch.h>

mpp::CircleBatch* createCircleBatch(std::string const& name, size_t circleBatchCount, mpp::RenderSystem* renderSystem, mpp::ResourceManager *resourceMgr);

size_t updateCircleBatch(mpp::RenderSystem* renderSystem, mpp::CircleBatch* circleBatch, size_t count, float totalTime);
