#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <boost/asio/strand.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cassert>

namespace fs = std::filesystem;

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using StringResponse = http::response<http::string_body>;

inline constexpr std::string_view API_PREFIX = "/api/";
inline constexpr std::string_view MAPS_ENDPOINT = "/api/v1/maps";
inline constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";

inline std::string UrlDecode(std::string_view encoded) {
    std::string result;
    result.reserve(encoded.size());
    
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            char hex[3] = {encoded[i+1], encoded[i+2], '\0'};
            char* endptr;
            long value = strtol(hex, &endptr, 16);
            if (endptr == hex + 2) {
                result.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        } else if (encoded[i] == '+') {
            result.push_back(' ');
            continue;
        }
        result.push_back(encoded[i]);
    }
    
    return result;
} // <- ВОТ ЭТА СКОБКА БЫЛА ПОТЕРЯНА!

inline std::string_view GetMimeType(const fs::path& filepath) {
    std::string ext = filepath.extension().string();
    for (auto& c : ext) c = std::tolower(c);
    
    if (ext == ".htm" || ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".txt") return "text/plain";
    if (ext == ".js") return "text/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".xml") return "application/xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpe" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".ico") return "image/vnd.microsoft.icon";
    if (ext == ".tiff" || ext == ".tif") return "image/tiff";
    if (ext == ".svg" || ext == ".svgz") return "image/svg+xml";
    if (ext == ".mp3") return "audio/mpeg";
    
    return "application/octet-stream";
}

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    explicit RequestHandler(model::Game& game, std::string static_dir, Strand api_strand)
        : game_{game}
        , static_dir_{std::move(static_dir)}
        , api_strand_{std::move(api_strand)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        std::string decoded_target = UrlDecode(target);
        
        if (decoded_target.rfind(API_PREFIX.data(), 0) == 0) {
            auto handle = [self = shared_from_this(), req = std::move(req), send = std::forward<Send>(send), decoded_target = std::move(decoded_target)]() mutable {
                try {
                    assert(self->api_strand_.running_in_this_thread());
                    StringResponse response;
                    
                    if (decoded_target == MAPS_ENDPOINT) {
                        if (req.method() == http::verb::get) {
                            response = self->MakeMapsListResponse(req.version(), req.keep_alive());
                        } else {
                            response = self->MakeApiBadRequestResponse(req.version(), req.keep_alive());
                        }
                    } else if (decoded_target.rfind(MAPS_PREFIX.data(), 0) == 0) {
                        if (req.method() == http::verb::get) {
                            std::string map_id = decoded_target.substr(MAPS_PREFIX.size());
                            response = self->MakeMapResponse(map_id, req.version(), req.keep_alive());
                        } else {
                            response = self->MakeApiBadRequestResponse(req.version(), req.keep_alive());
                        }
                    } else {
                        response = self->MakeApiBadRequestResponse(req.version(), req.keep_alive());
                    }

                    send(std::move(response));
                    
                } catch (...) {
                    send(self->MakeApiBadRequestResponse(req.version(), req.keep_alive()));
                }
            };

            return boost::asio::dispatch(api_strand_, std::move(handle));
        } 
        else {
            StringResponse response;
            if (static_dir_.empty()) {
                response = MakeStaticBadRequestResponse(req.version(), req.keep_alive());
            } else {
                std::string relative_path = decoded_target;
                if (!relative_path.empty() && relative_path[0] == '/') {
                    relative_path.erase(0, 1);
                }
                fs::path full_path = fs::path(static_dir_) / relative_path;
                response = MakeFileResponse(full_path, req.version(), req.keep_alive());
            }
            send(std::move(response));
        }
    }

private:
    model::Game& game_;
    std::string static_dir_;
    Strand api_strand_;

    StringResponse MakeApiBadRequestResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::bad_request, version);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"code\":\"badRequest\",\"message\":\"Bad request\"}";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeApiNotFoundResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::not_found, version);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"code\":\"mapNotFound\",\"message\":\"Map not found\"}";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeStaticBadRequestResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::bad_request, version);
        response.set(http::field::content_type, "text/plain");
        response.body() = "Bad request";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeStaticNotFoundResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::not_found, version);
        response.set(http::field::content_type, "text/plain");
        response.body() = "File not found";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeFileResponse(const fs::path& filepath, unsigned version, bool keep_alive) {
        fs::path target_path = filepath;
        if (fs::is_directory(filepath)) {
            target_path = filepath / "index.html";
        }
        
        if (!fs::exists(target_path)) {
            return MakeStaticNotFoundResponse(version, keep_alive);
        }
        
        std::ifstream file(target_path, std::ios::binary);
        if (!file.is_open()) {
            return MakeStaticNotFoundResponse(version, keep_alive);
        }
        
        std::stringstream ss;
        ss << file.rdbuf();
        
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, GetMimeType(target_path));
        response.body() = ss.str();
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
            return MakeApiNotFoundResponse(version, keep_alive);
        }
        
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, "application/json");
        
        json::object result;
        result["id"] = *map->GetId();
        result["name"] = map->GetName();
        result["roads"] = SerializeRoads(map);
        result["buildings"] = SerializeBuildings(map);
        result["offices"] = SerializeOffices(map);
        
        response.body() = json::serialize(result);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    json::array SerializeRoads(const model::Map* map) const {
        json::array arr;
        for (const auto& road : map->GetRoads()) {
            json::object obj;
            auto start = road.GetStart();
            auto end = road.GetEnd();
            if (road.IsHorizontal()) {
                obj["x0"] = start.x;
                obj["y0"] = start.y;
                obj["x1"] = end.x;
            } else {
                obj["x0"] = start.x;
                obj["y0"] = start.y;
                obj["y1"] = end.y;
            }
            arr.push_back(obj);
        }
        return arr;
    }

    json::array SerializeBuildings(const model::Map* map) const {
        json::array arr;
        for (const auto& building : map->GetBuildings()) {
            json::object obj;
            auto bounds = building.GetBounds();
            obj["x"] = bounds.position.x;
            obj["y"] = bounds.position.y;
            obj["w"] = bounds.size.width;
            obj["h"] = bounds.size.height;
            arr.push_back(obj);
        }
        return arr;
    }

    json::array SerializeOffices(const model::Map* map) const {
        json::array arr;
        for (const auto& office : map->GetOffices()) {
            json::object obj;
            obj["id"] = *office.GetId();
            obj["x"] = office.GetPosition().x;
            obj["y"] = office.GetPosition().y;
            obj["offsetX"] = office.GetOffset().dx;
            obj["offsetY"] = office.GetOffset().dy;
            arr.push_back(obj);
        }
        return arr;
    }
};

}  // namespace http_handler