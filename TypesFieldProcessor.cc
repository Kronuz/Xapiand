//
//  TypesFieldProcessor.cpp
//  Xapiand
//
//  Created by Jose Madrigal on 07/04/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#include "TypesFieldProcessor.h"

NumericFieldProcessor::NumericFieldProcessor(std::string prefix_, std::string field_): prefix(prefix_), field(field_){}

Xapian::Query NumericFieldProcessor::operator()(const std::string &str)
{
    Database *database = NULL;
    return Xapian::Query(prefix + database->serialise(field, str));
}


LatLongFieldProcessor::LatLongFieldProcessor(std::string prefix_, std::string field_): prefix(prefix_), field(field_){ }

Xapian::Query
LatLongFieldProcessor::operator()(const std::string &str)
{
    Database *database = NULL;
    return Xapian::Query(prefix + database->serialise(field, str));
}


LatLongDistanceFieldProcessor::LatLongDistanceFieldProcessor(std::string prefix_, std::string field_): prefix(prefix_), field(field_) {}

Xapian::Query
LatLongDistanceFieldProcessor::operator()(const std::string &str)
{
    Database *database = NULL;
    return Xapian::Query(prefix + database->serialise(field, str));
}


BooleanFieldProcessor::BooleanFieldProcessor(std::string prefix_, std::string field_): prefix(prefix_), field(field_){}

Xapian::Query BooleanFieldProcessor::operator()(const std::string &str)
{
	Database *database = NULL;
	return Xapian::Query(prefix + database->serialise(field, str));
}