#pragma once

#include "rapidjson/document.h"

namespace y2j {

using JsonDocument = rapidjson::Document;
using JsonValue = rapidjson::Value;

JsonDocument yamlParseBytes(const char* bytes, size_t length, const char** errorMessage, size_t* errorLine);

} // namespace y2j
