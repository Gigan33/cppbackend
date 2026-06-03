#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace json_loader {

model::Game LoadGame(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path);
    }
    
    json data = json::parse(file);
    
    model::Game game;
    
    if (data.contains("maps") && data["maps"].is_array()) {
        for (const auto& map_json : data["maps"]) {
            model::Map map(
                map_json["id"].get<std::string>(),
                map_json["name"].get<std::string>()
            );
            
            // Добавляем дороги
            if (map_json.contains("roads") && map_json["roads"].is_array()) {
                for (const auto& road_json : map_json["roads"]) {
                    if (road_json.contains("x0") && road_json.contains("y0") && road_json.contains("x1")) {
                        // Горизонтальная дорога
                        map.AddRoad(model::Road(
                            model::Point{road_json["x0"].get<int>(), road_json["y0"].get<int>()},
                            model::Point{road_json["x1"].get<int>(), road_json["y0"].get<int>()}
                        ));
                    } else if (road_json.contains("x0") && road_json.contains("y0") && road_json.contains("y1")) {
                        // Вертикальная дорога
                        map.AddRoad(model::Road(
                            model::Point{road_json["x0"].get<int>(), road_json["y0"].get<int>()},
                            model::Point{road_json["x0"].get<int>(), road_json["y1"].get<int>()}
                        ));
                    }
                }
            }
            
            // Добавляем здания
            if (map_json.contains("buildings") && map_json["buildings"].is_array()) {
                for (const auto& building_json : map_json["buildings"]) {
                    map.AddBuilding(model::Building(model::Rectangle(
                        model::Point{building_json["x"].get<int>(), building_json["y"].get<int>()},
                        model::Point{building_json["x"].get<int>() + building_json["w"].get<int>(),
                                     building_json["y"].get<int>() + building_json["h"].get<int>()}
                    )));
                }
            }
            
            // Добавляем офисы
            if (map_json.contains("offices") && map_json["offices"].is_array()) {
                for (const auto& office_json : map_json["offices"]) {
                    map.AddOffice(model::Office(
                        office_json["id"].get<std::string>(),
                        model::Point{office_json["x"].get<int>(), office_json["y"].get<int>()},
                        model::Offset{office_json["offsetX"].get<int>(), office_json["offsetY"].get<int>()}
                    ));
                }
            }
            
            game.AddMap(std::move(map));
        }
    }
    
    return game;
}

}  // namespace json_loader
