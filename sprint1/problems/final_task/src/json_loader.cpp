#include "json_loader.h"
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace json_loader {

namespace {

// Проверка наличия поля
bool HasField(const json::object& obj, const std::string& field_name) {
    return obj.contains(field_name);
}

// Получение строкового поля с проверкой
std::string GetStringField(const json::object& obj, const std::string& field_name) {
    if (!HasField(obj, field_name)) {
        throw std::runtime_error("Missing required field: " + field_name);
    }
    const auto& value = obj.at(field_name);
    if (!value.is_string()) {
        throw std::runtime_error("Field " + field_name + " must be a string");
    }
    return std::string(value.as_string().c_str());
}

// Получение целочисленного поля с проверкой
int GetIntField(const json::object& obj, const std::string& field_name) {
    if (!HasField(obj, field_name)) {
        throw std::runtime_error("Missing required field: " + field_name);
    }
    const auto& value = obj.at(field_name);
    if (!value.is_int64()) {
        throw std::runtime_error("Field " + field_name + " must be an integer");
    }
    return static_cast<int>(value.as_int64());
}

// Загрузка дорог
void LoadRoads(const json::object& map_obj, model::Map& map) {
    if (!HasField(map_obj, "roads")) return;
    const auto& roads_value = map_obj.at("roads");
    if (!roads_value.is_array()) {
        throw std::runtime_error("Roads must be an array");
    }
    
    for (const auto& road_value : roads_value.as_array()) {
        if (!road_value.is_object()) {
            throw std::runtime_error("Each road must be an object");
        }
        
        const json::object& road = road_value.as_object();
        int x0 = GetIntField(road, "x0");
        int y0 = GetIntField(road, "y0");
        
        if (HasField(road, "x1")) {
            // Горизонтальная дорога
            int x1 = GetIntField(road, "x1");
            map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
        } else if (HasField(road, "y1")) {
            // Вертикальная дорога
            int y1 = GetIntField(road, "y1");
            map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
        } else {
            throw std::runtime_error("Road must have either x1 or y1");
        }
    }
}

// Загрузка зданий
void LoadBuildings(const json::object& map_obj, model::Map& map) {
    if (!HasField(map_obj, "buildings")) return;
    const auto& buildings_value = map_obj.at("buildings");
    if (!buildings_value.is_array()) {
        throw std::runtime_error("Buildings must be an array");
    }
    
    for (const auto& building_value : buildings_value.as_array()) {
        if (!building_value.is_object()) {
            throw std::runtime_error("Each building must be an object");
        }
        
        const json::object& building = building_value.as_object();
        int x = GetIntField(building, "x");
        int y = GetIntField(building, "y");
        int w = GetIntField(building, "w");
        int h = GetIntField(building, "h");
        
        map.AddBuilding(model::Building({{x, y}, {w, h}}));
    }
}

// Загрузка офисов
void LoadOffices(const json::object& map_obj, model::Map& map) {
    if (!HasField(map_obj, "offices")) return;
    const auto& offices_value = map_obj.at("offices");
    if (!offices_value.is_array()) {
        throw std::runtime_error("Offices must be an array");
    }
    
    for (const auto& office_value : offices_value.as_array()) {
        if (!office_value.is_object()) {
            throw std::runtime_error("Each office must be an object");
        }
        
        const json::object& office = office_value.as_object();
        std::string office_id = GetStringField(office, "id");
        int x = GetIntField(office, "x");
        int y = GetIntField(office, "y");
        int offsetX = GetIntField(office, "offsetX");
        int offsetY = GetIntField(office, "offsetY");
        
        map.AddOffice(model::Office(
            model::Office::Id{std::move(office_id)},
            {x, y},
            {offsetX, offsetY}
        ));
    }
}

// Загрузка одной карты
model::Map LoadMap(const json::object& map_obj) {
    std::string id = GetStringField(map_obj, "id");
    std::string name = GetStringField(map_obj, "name");
    
    model::Map map(model::Map::Id{std::move(id)}, std::move(name));
    
    LoadRoads(map_obj, map);
    LoadBuildings(map_obj, map);
    LoadOffices(map_obj, map);
    
    return map;
}

} // anonymous namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + json_path.string());
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    std::string json_str = ss.str();
    
    json::value root;
    try {
        root = json::parse(json_str);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON config: " + std::string(e.what()));
    }
    
    if (!root.is_object()) {
        throw std::runtime_error("JSON root must be an object");
    }
    
    const json::object& obj = root.as_object();
    
    model::Game game;
    
    if (HasField(obj, "maps") && obj.at("maps").is_array()) {
        for (const auto& map_value : obj.at("maps").as_array()) {
            if (!map_value.is_object()) {
                throw std::runtime_error("Each map must be an object");
            }
            game.AddMap(LoadMap(map_value.as_object()));
        }
    }
    
    return game;
}

}  // namespace json_loader
