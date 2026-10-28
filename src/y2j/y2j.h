#pragma once

#include "rapidjson/document.h"

namespace y2j {

using JsonDocument = rapidjson::Document;
using JsonValue = rapidjson::Value;

// A document nested more than maxDepth levels (0: no limit) fails with an errorMessage.
JsonDocument yamlParseBytes(const char* bytes, size_t length, const char** errorMessage, size_t* errorLine, size_t maxDepth = 0);

} // namespace y2j
