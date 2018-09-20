#include "handler.h"
#include "serializer.h"
#include "text_serializer.h"

namespace prometheus {
namespace detail {

std::string SerializeGet(Registry& registry) {
	auto metrics = registry.Collect();
	auto serializer = std::unique_ptr<Serializer>{new TextSerializer()};
	// auto content_type = std::string{"text/plain"};
	auto body = serializer->Serialize(metrics);
	return body;
}
}  // namespace detail
}  // namespace prometheus
