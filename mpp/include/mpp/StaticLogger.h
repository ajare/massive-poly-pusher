#pragma once

#include "mpp/Config.h"

namespace mpp
{
	void _MPPAPI enable_static_log(bool enabled);

	bool _MPPAPI is_static_log_enabled();

	void static_log_message(std::string const& message);
}