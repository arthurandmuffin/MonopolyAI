#include "board.hpp"

namespace{
constexpr uint8_t NA = 255;

constexpr std::array<TileInfo,40> AllTiles = {{
    {0, TileType::Go, "Go", Colour::None, NA},
    {1, TileType::Property, "Mediterranean Avenue", Colour::Brown, 0 },
    {2, TileType::Community, "Community Chest", Colour::None, NA },
    {3, TileType::Property, "Baltic Avenue", Colour::Brown, 1 },
    {4, TileType::Tax, "Income Tax", Colour::None, NA },
    {5, TileType::Railroad, "Reading Railroad", Colour::None, 0 },
    {6, TileType::Property, "Oriental Avenue", Colour::Blue,2 },
    {7, TileType::Chance, "Chance", Colour::None, NA },
    {8, TileType::Property, "Vermont Avenue", Colour::Blue,3 },
    {9, TileType::Property, "Connecticut Avenue", Colour::Blue,4 },
    {10, TileType::Jail, "Just Visiting / In Jail", Colour::None, NA },
    {11, TileType::Property, "St. Charles Place", Colour::Pink, 5 },
    {12, TileType::Utility, "Electric Company", Colour::None, 0 },
    {13, TileType::Property, "States Avenue", Colour::Pink, 6 },
    {14, TileType::Property, "Virginia Avenue", Colour::Pink, 7 },
    {15, TileType::Railroad, "Pennsylvania Railroad", Colour::None, 1 },
    {16, TileType::Property, "St. James Place", Colour::Orange, 8 },
    {17, TileType::Community, "Community Chest", Colour::None, NA },
    {18, TileType::Property, "Tennessee Avenue", Colour::Orange, 9 },
    {19, TileType::Property, "New York Avenue", Colour::Orange, 10 },
    {20, TileType::FreeParking, "Free Parking", Colour::None, NA },
    {21, TileType::Property, "Kentucky Avenue", Colour::Red, 11 },
    {22, TileType::Chance, "Chance", Colour::None, NA },
    {23, TileType::Property, "Indiana Avenue", Colour::Red, 12 },
    {24, TileType::Property, "Illinois Avenue", Colour::Red, 13 },
    {25, TileType::Railroad, "B. & O. Railroad", Colour::None, 2 },
    {26, TileType::Property, "Atlantic Avenue", Colour::Yellow, 14 },
    {27, TileType::Property, "Ventnor Avenue", Colour::Yellow, 15 },
    {28, TileType::Utility, "Water Works", Colour::None, 1 },
    {29, TileType::Property, "Marvin Gardens", Colour::Yellow, 16 },
    {30, TileType::GoToJail, "Go To Jail", Colour::None, NA },
    {31, TileType::Property, "Pacific Avenue", Colour::Green, 17 },
    {32, TileType::Property, "North Carolina Avenue", Colour::Green, 18 },
    {33, TileType::Community, "Community Chest", Colour::None, NA },
    {34, TileType::Property, "Pennsylvania Avenue", Colour::Green, 19 },
    {35, TileType::Railroad, "Short Line", Colour::None, 3 },
    {36, TileType::Chance, "Chance", Colour::None, NA },
    {37, TileType::Property, "Park Place", Colour::Navy,20 },
    {38, TileType::Tax, "Luxury Tax", Colour::None, NA },
    {39, TileType::Property, "Boardwalk", Colour::Navy,21 },
}};

constexpr std::array<PropertyInfo, 22> AllProperties = {{
    {0, Colour::Brown, 60, 50, {2, 10, 30, 90, 160, 250}},
    {1, Colour::Brown, 60, 50, {4, 20, 60, 180, 320, 450}},
    {2, Colour::Blue, 100, 50, {6, 30, 90, 270, 400, 550}},
    {3, Colour::Blue, 100, 50, {6, 30, 90, 270, 400, 550}},
    {4, Colour::Blue, 120, 50, {8, 40, 100, 300, 450, 600}},
    {5, Colour::Pink, 140, 100, {10, 50, 150, 450, 625, 750}},
    {6, Colour::Pink, 140, 100, {10, 50, 150, 450, 625, 750}},
    {7, Colour::Pink, 160, 100, {12, 60, 180, 500, 700, 900}},
    {8, Colour::Orange, 180, 100, {14, 70, 200, 550, 750, 950}},
    {9, Colour::Orange, 180, 100, {14, 70, 200, 550, 750, 950}},
    {10, Colour::Orange, 200, 100, {16, 80, 220, 600, 800, 1000}},
    {11, Colour::Red, 220, 150, {18, 90, 250, 700, 875, 1050}},
    {12, Colour::Red, 220, 150, {18, 90, 250, 700, 875, 1050}},
    {13, Colour::Red, 240, 150, {20, 100, 300, 750, 925, 1100}},
    {14, Colour::Yellow, 260, 150, {22, 110, 330, 800, 975, 1150}},
    {15, Colour::Yellow, 260, 150, {22, 110, 330, 800, 975, 1150}},
    {16, Colour::Yellow, 280, 150, {24, 120, 360, 850, 1025, 1200}},
    {17, Colour::Green, 300, 200, {26, 130, 390, 900, 1100, 1275}},
    {18, Colour::Green, 300, 200, {26, 130, 390, 900, 1100, 1275}},
    {19, Colour::Green, 320, 200, {28, 150, 450, 1000, 1200, 1400}},
    {20, Colour::Navy, 350, 200, {35, 175, 500, 1100, 1300, 1500}},
    {21, Colour::Navy, 400, 200, {50, 200, 600, 1400, 1700, 2000}}
}};

constexpr std::array<RailroadInfo,4> AllRailRoads = {{
  {0, 200, {25, 50, 100, 200}},
  {1, 200, {25, 50, 100, 200}},
  {2, 200, {25, 50, 100, 200}},
  {3, 200, {25, 50, 100, 200}},
}};

constexpr std::array<UtilityInfo,2> AllUtilities = {{
  {0, 150, {4, 10}}, 
  {1, 150, {4, 10}},
}};

constexpr std::array<int8_t, 4> RailroadPositions = {5, 15, 25, 35};

constexpr std::array<int8_t, 2> UtilityPositions = {12, 28};

constexpr std::array<double, 40> TileProbabilities = {
    3.0961,  // Go
    2.1314,  // Mediterranean Avenue
    1.8849,  // Community Chest
    2.1624,  // Baltic Avenue
    2.3285,  // Income Tax
    2.9631,  // Reading Railroad
    2.2621,  // Oriental Avenue
    0.8650,  // Chance
    2.3210,  // Vermont Avenue
    2.3003,  // Connecticut Avenue
    6.2194,  // Visiting Jail
    2.7017,  // St. Charles Place
    2.6040,  // Electric Company
    2.3721,  // States Avenue
    2.4649,  // Virginia Avenue
    2.9200,  // Pennsylvania Railroad
    2.7924,  // St. James Place
    2.5945,  // Community Chest
    2.9356,  // Tennessee Avenue
    3.0852,  // New York Avenue
    2.8836,  // Free Parking
    2.8358,  // Kentucky Avenue
    1.0480,  // Chance
    2.7357,  // Indiana Avenue
    3.1858,  // Illinois Avenue
    3.0659,  // B & O Railroad
    2.7072,  // Atlantic Avenue
    2.6789,  // Ventnor Avenue
    2.8074,  // Water Works
    2.5861,  // Marvin Gardens
    0.0000,  // Go To Jail
    2.6774,  // Pacific Avenue
    2.6252,  // North Carolina Avenue
    2.3661,  // Community Chest
    2.5006,  // Pennsylvania Avenue
    2.4326,  // Short Line
    0.8669,  // Chance
    2.1864,  // Park Place
    2.1799,  // Luxury Tax
    2.6260,  // Boardwalk
};

constexpr std::array<int8_t,40> makeTilePropertyIndex() {
    std::array<int8_t,40> tile_property_index{};
    tile_property_index.fill(-1);
    for (const auto& tile : AllTiles) {
        if (tile.type == TileType::Property) {
            tile_property_index[tile.index] = static_cast<int8_t>(tile.auxilary_id);
        }
    }
    return tile_property_index;
}

constexpr std::array<int8_t,40> makeTileRailroadIndex() {
    std::array<int8_t,40> tile_railroad_index{};
    tile_railroad_index.fill(-1);
    for (const auto& tile : AllTiles) {
        if (tile.type == TileType::Railroad) {
            tile_railroad_index[tile.index] = static_cast<int8_t>(tile.auxilary_id);
        }
    }
    return tile_railroad_index;
}

constexpr std::array<int8_t,40> makeTileUtilityIndex() {
    std::array<int8_t,40> tile_utility_index{};
    tile_utility_index.fill(-1);
    for (const auto& tile : AllTiles) {
        if (tile.type == TileType::Utility) {
            tile_utility_index[tile.index] = static_cast<int8_t>(tile.auxilary_id);
        }
    }
    return tile_utility_index;
}

constexpr std::array<ColourGroup, 9> makeColourToTiles() {
    std::array<ColourGroup, 9> result{};
    for (const auto& tile : AllTiles) {
        if (tile.type == TileType::Property) {
            auto c = static_cast<size_t>(tile.colour);
            ColourGroup& g = result[c];
            g.colour = tile.colour;
            g.tiles[g.count++] = tile.index;
        }
    }
    return result;
}

struct BoardHolder {
    Board board;
    constexpr BoardHolder() : board{} {
        board.tiles = AllTiles;
        board.properties = AllProperties;
        board.railroads = AllRailRoads;
        board.utilities = AllUtilities;

        board.railroad_positions = RailroadPositions;
        board.utility_positions = UtilityPositions;
        board.tile_probability = TileProbabilities;

        board.tile_property_index = makeTilePropertyIndex();
        board.tile_railroad_index = makeTileRailroadIndex();
        board.tile_utility_index = makeTileUtilityIndex();

        board.colour_to_tiles = makeColourToTiles();
    }
};

inline BoardHolder board_holder{};
}

const Board& board() {
    return board_holder.board;
}