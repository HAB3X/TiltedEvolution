#pragma once

#include <cmath>
#include <cstdint>

// Where a party member or a ping is from the player, for the party bar and ping lines: a distance in metres and one of
// eight compass points.
namespace PartyCompass
{
// Clockwise from north, the order the UI's translations use
enum Point : uint8_t
{
    kNorth,
    kNorthEast,
    kEast,
    kSouthEast,
    kSouth,
    kSouthWest,
    kWest,
    kNorthWest,
};

// The compass point from one spot toward another, in world coordinates: +Y is north and +X east, the world map's
// up and right (UESP, "Skyrim:Coordinates")
inline Point Toward(float aFromX, float aFromY, float aToX, float aToY) noexcept
{
    constexpr float kPi = 3.14159265358979f;
    // atan2(east, north) is the bearing clockwise from north, -pi to pi
    const float cBearing = std::atan2(aToX - aFromX, aToY - aFromY);
    const int cSector = static_cast<int>(std::lround(cBearing / (kPi / 4.f)));
    return static_cast<Point>((cSector + 8) % 8);
}

// One game unit is about 1.428 cm (UESP, "Skyrim:Units")
inline float ToMeters(float aGameUnits) noexcept
{
    return aGameUnits * 0.01428f;
}

inline float Distance(float aFromX, float aFromY, float aFromZ, float aToX, float aToY, float aToZ) noexcept
{
    const float cX = aToX - aFromX;
    const float cY = aToY - aFromY;
    const float cZ = aToZ - aFromZ;
    return std::sqrt(cX * cX + cY * cY + cZ * cZ);
}
} // namespace PartyCompass
