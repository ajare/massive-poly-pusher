#pragma once

#include "mpp/Config.h"

namespace mpp
{
	// Eagerly loads every engine-owned PBR and particle shader. Source accessors
	// also call this function so context-free tests can inspect the same files.
	_MPPAPI void initialiseBuiltInShaderResources();
}
