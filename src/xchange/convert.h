#include <string>

#include "../rapidjson/document.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"

#include "../msgpack/src/msgpack.hpp"
#include "rapidjson.hpp"


struct Msgpack {

    struct Document {
        std::string buffer;
        msgpack::unpacked unpacked;
    };

public:
    typedef Document document_type;
    static void load(Document& doc, const std::string& str);
    static std::string save(const rapidjson::Document& doc);
};

struct RapidJSON {
    typedef rapidjson::Document document_type;

public:
	static void load(rapidjson::Document& doc, const std::string& str);
	static std::string save(const Msgpack::Document& sdoc);
};

template <typename Src, typename Dest>
std::string convert(std::string &s_str)
{
    typename Src::document_type doc;
    Src::load(doc, s_str);
    return Dest::save(doc);
}
