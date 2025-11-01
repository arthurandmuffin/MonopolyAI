#pragma once
#include <array>
#include <cstdint>
#include <string_view>

enum class TileType: uint8_t {
    Go, Property, Railroad, Utility, Tax, Chance, Community, Jail, FreeParking, GoToJail
};

enum class Colour: uint8_t {
    None, Brown, Blue, Pink, Orange, Red, Yellow, Green, Navy
};

struct TileInfo {
    uint8_t index;
    TileType type;
    std::string_view name;
    Colour colour;
    // Property/utility/railroad-specific id
    uint8_t auxilary_id;
};

struct PropertyInfo {
    uint8_t property_id; // auxilary id from tileinfo
    Colour colour;
    uint16_t purchase_price;
    uint8_t house_cost;
    std::array<uint16_t,6> rent;
};

struct RailroadInfo {
    uint8_t railroad_id;
    uint16_t purchase_price;
    std::array<uint16_t,4> rent;
};

struct UtilityInfo {
    uint8_t utility_ud;
    uint16_t purchase_price;
    std::array<uint16_t,2> multiplier;
};

struct Board {
    // Tiles in order
    std::array<TileInfo,40> tiles{};

    // Catalog of ownable tiles
    std::array<PropertyInfo,22> properties{};
    std::array<RailroadInfo,4> railroads{};
    std::array<UtilityInfo,2> utilities{};

    // lookup
    std::array<int8_t,40> tile_property_index{};
    std::array<int8_t,40> tile_railroad_index{};
    std::array<int8_t,40> tile_utility_index{};

    constexpr const PropertyInfo* propertyByTile(uint8_t tile_id) const {
        int8_t id = tile_property_index[tile_id];
        return (id >= 0) ? &properties[static_cast<size_t>(id)] : nullptr;
    }

    constexpr const int railroadByTile(uint8_t tile_id) const {
        return tile_railroad_index[tile_id];
    }

    constexpr const int utilityByTile(uint8_t tile_id) const {
        return tile_utility_index[tile_id];
    }
};

const Board& board();