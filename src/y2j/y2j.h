/*
 * Derived from y2j (https://github.com/tangrams/y2j), MIT licensed:
 * Copyright (c) 2016,2019 Mapzen (see LICENSE in this directory).
 *
 * Modifications:
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright © 2015-2020 Dubalu LLC
 */

#pragma once

#include "rapidjson/document.h"

namespace y2j {

using JsonDocument = rapidjson::Document;
using JsonValue = rapidjson::Value;

// A document nested more than maxDepth levels (0: no limit) fails with an errorMessage.
JsonDocument yamlParseBytes(const char* bytes, size_t length, const char** errorMessage, size_t* errorLine, size_t maxDepth = 0);

} // namespace y2j
