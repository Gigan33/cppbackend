#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using StringResponse = http::response<http::string_body>;

// Константы для эндпоинтов
inline constexpr std::string_view API_PREFIX = "/api/";
inline constexpr std::string_view MAPS_ENDPOINT = "/api/v1/maps";
inline constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        StringResponse response;
        
        if (target.rfind(API_PREFIX.data(), 0) != 0) {
            response = MakeBadRequestResponse(req.version(), req.keep_alive());
        }
        else if (target == MAPS_ENDPOINT) {
            if (req.method() == http::verb::get) {
                response = MakeMapsListResponse(req.version(), req.keep_alive());
            } else {
                response = MakeBadRequestResponse(req.version(), req.keep_alive());
            }
        }
        else if (target.rfind(MAPS_PREFIX.data(), 0) == 0) {
            if (req.method() == http::verb::get) {
                std::string map_id = target.substr(MAPS_PREFIX.size());
                response = MakeMapResponse(map_id, req.version(), req.keep_alive());
            } else {
                response = MakeBadRequestResponse(req.version(), req.keep_alive());
            }
        }
        else {
            response = MakeBadRequestResponse(req.version(), req.keep_alive());
        }
        
        send(std::move(response));
    }

private:
    // Вспомогательные функции для сериализации
    json::array SerializeRoads(const model::Map* map) const {
        json::array roads_arr;
        for (const auto& road : map->GetRoads()) {
            json::object road_obj;
            auto start = road.GetStart();
            auto end = road.GetEnd();
            if (road.IsHorizontal()) {
                road_obj["x0"] = start.x;
                road_obj["y0"] = start.y;
                road_obj["x1"] = end.x;
            } else {
                road_obj["x0"] = start.x;
                road_obj["y0"] = start.y;
                road_obj["y1"] = end.y;
            }
            roads_arr.push_back(road_obj);
        }
        return roads_arr;
    }

    json::array SerializeBuildings(const model::Map* map) const {
        json::array buildings_arr;
        for (const auto& building : map->GetBuildings()) {
            json::object building_obj;
            auto bounds = building.GetBounds();
            building_obj["x"] = bounds.position.x;
            building_obj["y"] = bounds.position.y;
            building_obj["w"] = bounds.size.width;
            building_obj["h"] = bounds.size.height;
            buildings_arr.push_back(building_obj);
        }
        return buildings_arr;
    }

    json::array SerializeOffices(const model::Map* map) const {
        json::array offices_arr;
        for (const auto& office : map->GetOffices()) {
            json::object office_obj;
            office_obj["id"] = *office.GetId();
            office_obj["x"] = office.GetPosition().x;
            office_obj["y"] = office.GetPosition().y;
            office_obj["offsetX"] = office.GetOffset().dx;
            office_obj["offsetY"] = office.GetOffset().dy;
            offices_arr.push_back(office_obj);
        }
        return offices_arr;
    }

    json::object SerializeMap(const model::Map* map) const {
        json::object result;
        result["id"] = *map->GetId();
        result["name"] = map->GetName();
        result["roads"] = SerializeRoads(map);
        result["buildings"] = SerializeBuildings(map);
        result["offices"] = SerializeOffices(map);
        return result;
    }

    // Функции формирования ответов
    StringResponse MakeBadRequestResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::bad_request, version);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"code\":\"badRequest\",\"message\":\"Bad request\"}";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeNotFoundResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::not_found, version);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"code\":\"mapNotFound\",\"message\":\"Map not found\"}";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeMapsListResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, "application/json");
        
        json::array arr;
        for (const auto& map : game_.GetMaps()) {
            json::object map_obj;
            map_obj["id"] = *map.GetId();
            map_obj["name"] = map.GetName();
            arr.push_back(map_obj);
        }
        
        response.body() = json::serialize(arr);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeMapResponse(const std::string& map_id, unsigned version, bool keep_alive) {
        const model::Map* map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            return MakeNotFoundResponse(version, keep_alive);
        }
        
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, "application/json");
        
        json::object result = SerializeMap(map);
        
        response.body() = json::serialize(result);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    model::Game& game_;
};

}  // namespace http_handler
