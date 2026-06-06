#include "json_loader.h"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    std::string json_str = ss.str();
    
    json::value root = json::parse(json_str);
    json::object obj = root.as_object();
    
    model::Game game;
    
    if (obj.contains("maps") && obj["maps"].is_array()) {
        for (const auto& map_json : obj["maps"].as_array()) {
            json::object map_obj = map_json.as_object();
            
            std::string id = map_obj["id"].as_string().c_str();
            std::string name = map_obj["name"].as_string().c_str();
            model::Map map(model::Map::Id{std::move(id)}, std::move(name));
            
            // Дороги
            if (map_obj.contains("roads") && map_obj["roads"].is_array()) {
                for (const auto& road_json : map_obj["roads"].as_array()) {
                    json::object road = road_json.as_object();
                    int x0 = road["x0"].as_int64();
                    int y0 = road["y0"].as_int64();
                    
                    if (road.contains("x1")) {
                        // Горизонтальная
                        map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, road["x1"].as_int64()));
                    } else {
                        // Вертикальная
                        map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, road["y1"].as_int64()));
                    }
                }
            }
            
            // Здания
            if (map_obj.contains("buildings") && map_obj["buildings"].is_array()) {
                for (const auto& building_json : map_obj["buildings"].as_array()) {
                    json::object building = building_json.as_object();
                    int x = building["x"].as_int64();
                    int y = building["y"].as_int64();
                    int w = building["w"].as_int64();
                    int h = building["h"].as_int64();
                    map.AddBuilding(model::Building({{x, y}, {w, h}}));
                }
            }
            
            // Офисы
            if (map_obj.contains("offices") && map_obj["offices"].is_array()) {
                for (const auto& office_json : map_obj["offices"].as_array()) {
                    json::object office = office_json.as_object();
                    std::string office_id = office["id"].as_string().c_str();
                    int x = office["x"].as_int64();
                    int y = office["y"].as_int64();
                    int offsetX = office["offsetX"].as_int64();
                    int offsetY = office["offsetY"].as_int64();
                    map.AddOffice(model::Office(
                        model::Office::Id{std::move(office_id)},
                        {x, y},
                        {offsetX, offsetY}
                    ));
                }
            }
            
            game.AddMap(std::move(map));
        }
    }
    
    return game;
}

}  // namespace json_loader
