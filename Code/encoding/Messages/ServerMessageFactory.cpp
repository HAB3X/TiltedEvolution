#pragma once

#include <Messages/ServerMessageFactory.h>
#include <WireProtocol.h>

static std::function<UniquePtr<ServerMessage>(TiltedPhoques::Buffer::Reader& aReader)> s_serverMessageExtractor[kServerOpcodeMax];

namespace details
{
static struct ServerMessageFactoryInit
{
    ServerMessageFactoryInit()
    {
        auto extractor = [](auto& x)
        {
            using T = typename std::remove_reference_t<decltype(x)>::Type;

            s_serverMessageExtractor[T::Opcode] = [](TiltedPhoques::Buffer::Reader& aReader)
            {
                auto ptr = TiltedPhoques::MakeUnique<T>();
                ptr->DeserializeRaw(aReader);
                return TiltedPhoques::CastUnique<ServerMessage>(std::move(ptr));
            };

            return false;
        };

        ServerMessageFactory::Visit(extractor);
    }
} s_ServerMessageFactoryInit;
} // namespace details

UniquePtr<ServerMessage> ServerMessageFactory::Extract(TiltedPhoques::Buffer::Reader& aReader) const noexcept
{
    uint64_t data;
    aReader.ReadBits(data, sizeof(ServerOpcode) * 8);

    const auto opcode = Wire::FromWire(static_cast<uint8_t>(data));
    if (!opcode) [[unlikely]]
        return {nullptr};

    return s_serverMessageExtractor[*opcode](aReader);
}
