#ifndef PARSEDMESSAGE_H
#define PARSEDMESSAGE_H

#include <string>
#include "../protocol/ProtocolTypes.h"

struct ParsedMessage
{
    ProtocolTypes type;
    std::string payload;
};

#endif