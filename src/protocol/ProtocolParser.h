#include "../packet/Packet.h"
#include "../packet/ParsedMessage.h"

namespace ProtocolParser
{

    static ParsedMessage parse(const Packet &rawPacket)
    {
        ParsedMessage msg;

        msg.type = static_cast<ProtocolTypes>(rawPacket.header);

        if (!rawPacket.payload.empty())
        {
            msg.payload = std::string(rawPacket.payload.begin(), rawPacket.payload.end());
        }
        else
        {
            msg.payload = "";
        }
        return msg;
    }
}