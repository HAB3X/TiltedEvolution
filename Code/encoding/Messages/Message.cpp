#include <Messages/Message.h>

#include <WireProtocol.h>

void ClientMessage::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    // We don't read the opcode, the factory will do it
}

void ClientMessage::DeserializeDifferential(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
}

ClientOpcode ClientMessage::GetOpcode() const noexcept
{
    return m_opcode;
}

void ClientMessage::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    ClientMessage::SerializeRaw(aWriter);
    SerializeRaw(aWriter);
    SerializeDifferential(aWriter);
}

void ClientMessage::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    // A message the server's protocol lacks is never sent (TransportService::Send); 0xFF is no message to any receiver
    aWriter.WriteBits(Wire::ToWire(m_opcode).value_or(0xFF), sizeof(m_opcode) * 8);
}

void ClientMessage::SerializeDifferential(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
}

void ServerMessage::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    // We don't read the opcode, the factory will do it
}

void ServerMessage::DeserializeDifferential(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
}

ServerOpcode ServerMessage::GetOpcode() const noexcept
{
    return m_opcode;
}

void ServerMessage::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    ServerMessage::SerializeRaw(aWriter);
    SerializeRaw(aWriter);
    SerializeDifferential(aWriter);
}

void ServerMessage::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Wire::ToWire(m_opcode).value_or(0xFF), sizeof(m_opcode) * 8);
}

void ServerMessage::SerializeDifferential(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
}
