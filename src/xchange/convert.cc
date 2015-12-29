#include "convert.h"

using namespace rapidjson;
using namespace msgpack;

void Msgpack::load(Document& doc, const std::string& str)
{
	doc.buffer.assign(str);
    msgpack::unpack(&doc.unpacked, doc.buffer.data(), doc.buffer.size());
}

std::string Msgpack::save(const rapidjson::Document& doc)
{
        msgpack::sbuffer sbuf;  // simple buffer
        msgpack::pack(&sbuf, doc);
        return std::string(sbuf.data(), sbuf.size());
}


void RapidJSON::load(rapidjson::Document& doc, const std::string& str)
{
    doc.Parse(str.data());
}


std::string RapidJSON::save(const Msgpack::Document& sdoc)
{
    rapidjson::Document doc;

    sdoc.unpacked.get().convert(&doc);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}
