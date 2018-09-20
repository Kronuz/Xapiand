#pragma once

#include <memory>
#include <vector>


#include "registry.h"

namespace prometheus {
namespace detail {
	std::string SerializeGet(Registry& registry);
}
}
