//
//  TypesFieldProcessor.h
//  Xapiand
//
//  Created by Jose Madrigal on 07/04/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#ifndef __Xapiand__TypesFieldProcessor__
#define __Xapiand__TypesFieldProcessor__

#include <string.h>
#include <sstream>
#include <xapian.h>

#include "database.h"

class NumericFieldProcessor: public Xapian::FieldProcessor {
    std::string prefix;
	std::string field;

public:
    Xapian::Query operator()(const std::string &str);
	NumericFieldProcessor(std::string prefix, std::string field);
};

class LatLongFieldProcessor: public Xapian::FieldProcessor {
	std::string prefix;
	std::string field;
    
public:
    Xapian::Query operator()(const std::string &str);
    LatLongFieldProcessor(std::string prefix, std::string field);
};

class LatLongDistanceFieldProcessor: public Xapian::FieldProcessor {
	std::string prefix;
	std::string field;
	
public:
    Xapian::Query operator()(const std::string &str);
	LatLongDistanceFieldProcessor(std::string prefix, std::string field);
};

class BooleanFieldProcessor: public Xapian::FieldProcessor {
	std::string prefix;
	std::string field;
	
public:
	Xapian::Query operator()(const std::string &str);
	BooleanFieldProcessor(std::string prefix, std::string field);
};


#endif /* defined(__Xapiand__TypesFieldProcessor__) */
