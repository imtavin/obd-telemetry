#include "../protocol/ProtocolTypes.h"
#include <cstdint>
#include <vector>
#pragma pack(push, 1)
struct Packet
{
    uint8_t header;
    std::vector<uint8_t> payload;
};