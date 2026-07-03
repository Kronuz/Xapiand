/*
 * Node-salt hook for the `cuuid` library, wired to Xapiand's Node registry.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "node.h"

namespace cuuid {
	inline std::optional<uint64_t> local_node_hash() {
		auto node = Node::get_local_node();
		return node
			? std::optional<uint64_t>(std::hash<std::string_view>{}(node->lower_name()))
			: std::nullopt;
	}
}
