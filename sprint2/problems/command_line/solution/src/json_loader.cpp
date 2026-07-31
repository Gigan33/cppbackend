#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace json_loader {
namespace helper {

void ParseRoads(const json::object& map_obj, model::Map& map) {
    const auto* roads_ptr = map_obj.if_contains("roads");
    if (!roads_ptr || !roads_ptr->is_array()) return;

    for (const auto& road_json : roads_ptr->as_array()) {
        auto road = road_json.as_object();
        int x0 = road.at("x0").as_int64();
        int y0 = road.at("y0").as_int64();
        
        if (const auto* x1_ptr = road.if_contains("x1")) {
            map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1_ptr->as_int64()));
        } else {
            map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, road.at("y1").as_int64()));
        }
    }
}

void ParseBuildings(const json::object& map_obj, model::Map& map) {
    const auto* buildings_ptr = map_obj.if_contains("buildings");
    if (!buildings_ptr || !buildings_ptr->is_array()) return;

    for (const auto& building_json : buildings_ptr->as_array()) {
        auto building = building_json.as_object();
        int x = building.at("x").as_int64();
        int y = building.at("y").as_int64();
        int w = building.at("w").as_int64();
        int h = building.at("h").as_int64();
        map.AddBuilding(model::Building({{x, y}, {w, h}}));
    }
}

void ParseOffices(const json::object& map_obj, model::Map& map) {
    const auto* offices_ptr = map_obj.if_contains("offices");
    if (!offices_ptr || !offices_ptr->is_array()) return;

    for (const auto& office_json : offices_ptr->as_array()) {
        auto office = office_json.as_object();
        std::string office_id = office.at("id").as_string().c_str();
        int x = office.at("x").as_int64();
        int y = office.at("y").as_int64();
        int offsetX = office.at("offsetX").as_int64();
        int offsetY = office.at("offsetY").as_int64();
        map.AddOffice(model::Office(
            model::Office::Id{std::move(office_id)},
            {x, y},
            {offsetX, offsetY}
        ));
    }
}

model::Map ParseMap(const json::value& map_json, double default_speed) {
    auto map_obj = map_json.as_object();
    
    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();
    model::Map map(model::Map::Id{std::move(id)}, std::move(name));

    if (const auto* map_speed_ptr = map_obj.if_contains("dogSpeed")) {
        map.SetDogSpeed(map_speed_ptr->as_double());
    } else {
        map.SetDogSpeed(default_speed);
    }

    ParseRoads(map_obj, map);
    ParseBuildings(map_obj, map);
    ParseOffices(map_obj, map);

    return map;
}

} // namespace helper

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    json::value root = json::parse(ss.str());
    auto obj = root.as_object();
    
    model::Game game;

    double default_speed = 1.0;
    if (const auto* speed_ptr = obj.if_contains("defaultDogSpeed")) {
        default_speed = speed_ptr->as_double();
    }
    game.SetDefaultDogSpeed(default_speed);

    if (const auto* maps_ptr = obj.if_contains("maps")) {
        if (maps_ptr->is_array()) {
            for (const auto& map_json : maps_ptr->as_array()) {
                game.AddMap(helper::ParseMap(map_json, default_speed));
            }
        }
    }
    
    return game;
}

}  // namespace json_loader