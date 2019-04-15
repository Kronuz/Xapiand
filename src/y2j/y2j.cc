#include "y2j.h"
#include "yaml.h"
#include "rapidjson/pointer.h"
#include "rapidjson/stringbuffer.h"
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#ifndef Y2J_DEBUG
#define Y2J_DEBUG 0
#endif

namespace y2j {

static const char* TAG_NULL = "tag:yaml.org,2002:null";
static const char* TAG_BOOL = "tag:yaml.org,2002:bool";
static const char* TAG_INT = "tag:yaml.org,2002:int";
static const char* TAG_FLOAT = "tag:yaml.org,2002:float";
static const char* TAG_STR = "tag:yaml.org,2002:str";
static const char* tagErrorString = "Scalar tag could not be resolved.";
static const char* aliasErrorString = "Encountered an unidentified alias";
static const char complexKeyString[] = "COMPLEX YAML KEYS ARE NOT SUPPORTED";

using JsonPointer = rapidjson::Pointer;
using Handler = rapidjson::Document;

struct Collection {
    std::string member; // Current member being processed in this collection.
    size_t count = 0; // Current number of elements in this collection.
    bool isMapping = true; // Whether this collection is a mapping or sequence.
};

struct Anchor {
    std::string name;
    JsonPointer value;
};

struct Alias {
    JsonPointer anchor, reference;
};

struct Generator {

    Generator(const char* bytes, size_t length, const char** errorMessage, size_t* errorLine) :
        errorMessage(errorMessage),
        errorLine(errorLine) {
        yaml_parser_initialize(&parser);
        yaml_parser_set_input_string(&parser, (const unsigned char*)bytes, length);
        yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
        yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    }

    ~Generator() {
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    std::vector<Collection> collections;
    std::vector<Anchor> anchors;
    std::vector<Alias> aliases;
    size_t complexKeyDepth = 0;
    const char** errorMessage;
    size_t* errorLine;

    size_t getSeqLength() {
        assert(!collections.empty());
        assert(!collections.back().isMapping);
        return collections.back().count;
    }

    size_t getMapLength() {
        assert(!collections.empty());
        assert(collections.back().isMapping);
        return collections.back().count / 2;
    }

    bool entryIsMapKey() {
        return event.type != YAML_MAPPING_END_EVENT &&
            !collections.empty() &&
            collections.back().isMapping &&
            collections.back().count % 2 == 0;
    }

    void pushCollection(bool isMapping) {
        // Push a new collection onto the stack.
        collections.emplace_back();

        // Set the values for the new collection.
        collections.back().isMapping = isMapping;
    }

    void popCollection() {
        // Remove the current collection from the back of the stack.
        collections.pop_back();

        // A new collection is an entry in its parent, so increment the count.
        if (!collections.empty()) {
            ++collections.back().count;
        }
    }

    JsonPointer getJsonPointer() {
        // Create a pointer starting from the root collection.
        JsonPointer pointer;
        for (const auto& c : collections) {
            if (!c.member.empty()) {
                pointer = pointer.Append(c.member.c_str(), c.member.length());
            } else {
                pointer = pointer.Append(c.count);
            }
        }
        return pointer;
    }

    void handleAnchor(const char* anchor) {
        if (anchor) {
            anchors.push_back({ std::string(anchor), getJsonPointer() });
        }
    }

    bool handleAlias(Handler& handler, yaml_event_t event) {
        const char* anchor = (char*)event.data.alias.anchor;
        JsonPointer* pointer = nullptr;
        for (auto it = anchors.rbegin(); it != anchors.rend(); it++) {
            if (it->name == anchor) {
                pointer = &it->value;
                break;
            }
        }
        if (!pointer) {
            // This means we tried to find an anchor that was never defined.
            *errorLine = event.start_mark.line;
            *errorMessage = aliasErrorString;
            return false;
        }
        // Create a JSON pointer to the current node and add it to a list of
        // references, then push a null as a placeholder.
        aliases.push_back({ *pointer, getJsonPointer() });
        return handler.Null();
    }

    bool operator()(Handler& handler) {

        bool ok = true;

        while (ok && event.type != YAML_STREAM_END_EVENT) {

            yaml_event_delete(&event);

            if (!yaml_parser_parse(&parser, &event)) {
                if (errorMessage) {
                    *errorMessage = parser.problem;
                }
                if (errorLine) {
                    *errorLine = parser.context_mark.line;
                }
                ok = false;
                break;
            }

            #if Y2J_DEBUG
            printEvent(event);
            #endif

            if (complexKeyDepth > 0) {
                switch (event.type) {
                case YAML_SEQUENCE_START_EVENT:
                case YAML_MAPPING_START_EVENT:
                    complexKeyDepth++;
                    break;
                case YAML_SEQUENCE_END_EVENT:
                case YAML_MAPPING_END_EVENT:
                    complexKeyDepth--;
                    break;
                default:
                    break;
                }
                if (complexKeyDepth == 0) {
                    handler.Key(complexKeyString, sizeof(complexKeyString), true);
                    collections.back().count++;
                }
            } else if (entryIsMapKey()) {
                switch (event.type) {
                case YAML_SEQUENCE_START_EVENT:
                case YAML_MAPPING_START_EVENT:
                    complexKeyDepth++;
                    break;
                case YAML_ALIAS_EVENT:
                    ok = handleAlias(handler, event);
                    collections.back().count++;
                    break;
                case YAML_SCALAR_EVENT:
                    collections.back().member.assign((char*)event.data.scalar.value, event.data.scalar.length);
                    handleAnchor((char*) event.data.scalar.anchor);
                    ok = handler.Key((char*)event.data.scalar.value, event.data.scalar.length, true);
                    collections.back().count++;
                    break;
                default:
                    // No other types of events should occur in a map key.
                    assert(false);
                    break;
                }
            } else {
                switch (event.type) {
                case YAML_NO_EVENT:
                case YAML_STREAM_START_EVENT:
                case YAML_STREAM_END_EVENT:
                case YAML_DOCUMENT_START_EVENT:
                case YAML_DOCUMENT_END_EVENT:
                    break;
                case YAML_SEQUENCE_START_EVENT:
                    ok = handler.StartArray();
                    handleAnchor((char*)event.data.sequence_start.anchor);
                    pushCollection(false);
                    break;
                case YAML_SEQUENCE_END_EVENT:
                    ok = handler.EndArray(getSeqLength());
                    popCollection();
                    break;
                case YAML_MAPPING_START_EVENT:
                    ok = handler.StartObject();
                    handleAnchor((char*)event.data.mapping_start.anchor);
                    pushCollection(true);
                    break;
                case YAML_MAPPING_END_EVENT:
                    ok = handler.EndObject(getMapLength());
                    popCollection();
                    break;
                case YAML_ALIAS_EVENT:
                    ok = handleAlias(handler, event);
                    collections.back().count++;
                    break;
                case YAML_SCALAR_EVENT:
                    handleAnchor((char*)event.data.scalar.anchor);
                    ok = parseScalar(handler, event);
                    collections.back().count++;
                    break;
                }
            }
        }

        return ok;
    }

    bool parseScalar(Handler& handler, yaml_event_t event) {

        const char* tag = (char*)event.data.scalar.tag;
        const char* value = (char*)event.data.scalar.value;
        size_t length = event.data.scalar.length;
        bool parsed = false;
        bool ok = true;

        if (tag) {
            if (strcmp(tag, TAG_NULL) == 0) {
                ok = parseNull(handler, value, length, &parsed);
            } else if (strcmp(tag, TAG_BOOL) == 0) {
                ok = parseBool(handler, value, length, &parsed);
            } else if (strcmp(tag, TAG_INT) == 0) {
                ok = parseInteger(handler, value, length, &parsed);
            } else if (strcmp(tag, TAG_FLOAT) == 0) {
                ok = parseFloat(handler, value, length, &parsed);
            } else if (strcmp(tag, TAG_STR) == 0) {
                ok = handler.String(value, length, true);
                parsed = true;
            }
            if (!parsed) {
                *errorMessage = tagErrorString;
                *errorLine = event.start_mark.line;
                ok = false;
            }
            return ok;
        }

        if (event.data.scalar.style != YAML_PLAIN_SCALAR_STYLE) {
            // See [1] below.
            return handler.String(value, length, true);
        }

        if (length == 0) {
            return handler.Null();
        }

        switch (value[0]) {
        case '~':
        case 'n':
        case 'N':
            ok = parseNull(handler, value, length, &parsed);
            break;
        case 't':
        case 'T':
            ok = parseTrue(handler, value, length, &parsed);
            break;
        case 'f':
        case 'F':
            ok = parseFalse(handler, value, length, &parsed);
            break;
        default:
            ok = parseNumber(handler, value, length, &parsed);
            break;
        }
        if (ok && !parsed) {
            ok = handler.String(value, length, true);
        }
        return ok;
    }

    bool parseNull(Handler& handler, const char* value, size_t length, bool* parsed) {
        if ((length == 1 && value[0] == '~') ||
            (length == 4 && (strcmp(value, "null") == 0 || strcmp(value, "Null") == 0 || strcmp(value, "NULL") == 0))) {
            *parsed = true;
            return handler.Null();
        }
        return true;
    }

    bool parseTrue(Handler& handler, const char* value, size_t length, bool* parsed) {
        if (length == 4 && (strcmp(value, "true") == 0 || strcmp(value, "True") == 0 || strcmp(value, "TRUE") == 0)) {
            *parsed = true;
            return handler.Bool(true);
        }
        return true;
    }

    bool parseFalse(Handler& handler, const char* value, size_t length, bool* parsed) {
        if (length == 5 && (strcmp(value, "false") == 0 || strcmp(value, "False") == 0 || strcmp(value, "FALSE") == 0)) {
            *parsed = true;
            return handler.Bool(false);
        }
        return true;
    }

    bool parseBool(Handler& handler, const char* value, size_t length, bool* parsed) {
        bool ok = parseTrue(handler, value, length, parsed);
        if (ok && !*parsed) {
            return parseFalse(handler, value, length, parsed);
        }
        return ok;
    }

    bool parseInteger(Handler& handler, const char* value, size_t length, bool* parsed) {
        // Check for hexadecimal:
        //   0x [0-9a-fA-F]+
        // Or octal:
        //   0o [0-7]+
        const char* end = value + length;
        int base = 10;
        if (length > 2 && value[0] == '0') {
            if (value[1] == 'x') {
                base = 16;
                value += 2;
            } else if (value[1] == 'o') {
                base = 8;
                value += 2;
            }
        }
        // TODO: Optimize parsing of integers.
        char* pos = nullptr;
        int64_t i = strtoll(value, &pos, base);
        if (pos == end) {
            *parsed = true;
            return handler.Int64(i);
        }
        return true;
    }

    bool parseFloat(Handler& handler, const char* value, size_t length, bool* parsed) {
        const char* start = value;
        const char* end = value + length;
        // Check for NaN:
        //   (\.nan | \.NaN | \.NAN)
        if (length == 4 && (strcmp(start, ".nan") == 0 || strcmp(start, ".NaN") == 0 || strcmp(start, ".NAN") == 0)) {
            *parsed = true;
            return handler.Double(NAN);
        }
        // Check for Inf:
        //   [-+]? ( \.inf | \.Inf | \.INF )
        bool minus = (*start == '-');
        if (minus || *start == '+') {
            start++;
        }
        if (end - start == 4 && (strcmp(start, ".inf") == 0 || strcmp(start, ".Inf") == 0 || strcmp(start, ".INF") == 0)) {
            *parsed = true;
            return handler.Double(minus ? -INFINITY : INFINITY);
        }
        start = value;
        // TODO: Optimize parsing of doubles.
        char* pos = nullptr;
        double d = strtod(start, &pos);
        if (pos == end) {
            *parsed = true;
            return handler.Double(d);
        }
        return true;
    }

    bool parseNumber(Handler& handler, const char* value, size_t length, bool* parsed) {
        bool ok = parseInteger(handler, value, length, parsed);
        if (ok && !*parsed) {
            return parseFloat(handler, value, length, parsed);
        }
        return ok;
    }

    #if Y2J_DEBUG
    void printEvent(yaml_event_t& event) {
        static int indent = 0;
        if (event.type == YAML_DOCUMENT_START_EVENT) { indent = 0; }
        if (event.type == YAML_SEQUENCE_END_EVENT || event.type == YAML_MAPPING_END_EVENT) { indent -= 2; }
        printf("%*s", indent, "");
        if (complexKeyDepth > 0) {
            printf("?\n");
        } else {
            switch (event.type) {
            case YAML_NO_EVENT: printf("No event!\n"); break;
            case YAML_STREAM_START_EVENT: printf("Start Stream\n"); break;
            case YAML_STREAM_END_EVENT: printf("End Stream\n"); break;
            case YAML_DOCUMENT_START_EVENT: printf("Start Document\n"); break;
            case YAML_DOCUMENT_END_EVENT: printf("End Document\n"); break;
            case YAML_SEQUENCE_START_EVENT: printf("[\n"); break;
            case YAML_SEQUENCE_END_EVENT: printf("] (members: %lu)\n", getSeqLength()); break;
            case YAML_MAPPING_START_EVENT: printf("{\n"); break;
            case YAML_MAPPING_END_EVENT: printf("} (members: %lu)\n", getMapLength()); break;
            case YAML_ALIAS_EVENT: printf("Alias (anchor %s)\n", event.data.alias.anchor); break;
            case YAML_SCALAR_EVENT: printf(entryIsMapKey() ? "\"%s\":\n" : "\"%s\"\n", event.data.scalar.value); break;
            }
        }
        if (event.type == YAML_SEQUENCE_START_EVENT || event.type == YAML_MAPPING_START_EVENT) { indent += 2; }
    }
    #endif
};

JsonDocument yamlParseBytes(const char* bytes, size_t length, const char** errorMessage, size_t* errorOffset) {

    Generator generator(bytes, length, errorMessage, errorOffset);
    JsonDocument document;
    document.Populate(generator);

    // Apply aliases.
    for (const auto& alias : generator.aliases) {
        #if Y2J_DEBUG
        rapidjson::StringBuffer anchor, reference;
        alias.anchor.Stringify(anchor);
        alias.reference.Stringify(reference);
        printf("Applying anchor: %s reference: %s\n", anchor.GetString(), reference.GetString());
        #endif
        const JsonValue* value = alias.anchor.Get(document);
        assert(value);
        alias.reference.Set(document, *value);
    }
    return document;
}

} // namespace y2j

// [1] The conditional here implies that any scalar value that is quoted in any
// way or that uses a multi-line format will be deduced as a string type. This
// is not part of the YAML 1.2 spec. However, the alternative would be to allow
// quoted values like "76" to be interpreted as number types. That would differ
// from the semantics of quoted values in JSON (which are always strings) and so
// in keeping with the notion of YAML 1.2 as a strict superset of JSON, I have
// enforced that quoted values become strings. -MEB 11/2016
