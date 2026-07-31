#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
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
inline constexpr std::string_view JOIN_GAME_ENDPOINT = "/api/v1/game/join";
inline constexpr std::string_view PLAYERS_ENDPOINT = "/api/v1/game/players";
inline constexpr std::string_view GAME_STATE_ENDPOINT = "/api/v1/game/state";
inline constexpr std::string_view ACTION_ENDPOINT = "/api/v1/game/player/action";
inline constexpr std::string_view TICK_ENDPOINT = "/api/v1/game/tick";

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
}

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

class RequestHandler {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    explicit RequestHandler(model::Game& game, std::string static_dir, Strand api_strand)
        : game_{game}
        , static_dir_{std::move(static_dir)}
        , api_strand_{std::move(api_strand)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    // Вспомогательный метод для ручных ответов
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned version, bool keep_alive,
                                      std::string_view content_type = "application/json",
                                      const std::vector<std::pair<std::string, std::string>>& custom_headers = {},
                                      std::string_view cache_control = "") {
        StringResponse response(status, version);
        response.set(http::field::content_type, content_type);
        if (!cache_control.empty()) {
            response.set(http::field::cache_control, cache_control);
        }
        for (const auto& [header, value] : custom_headers) {
            response.set(header, value);
        }
        response.body() = std::string(body);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    // --- ОПРЕДЕЛЕНИЯ МЕТОДОВ ОБРАБОТКИ ЗАПРОСОВ (ТЕПЕРЬ СРАЗУ ТУТ) ---

    StringResponse MakeMapsListResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        
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

    StringResponse MakeApiBadRequestResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::bad_request, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.body() = "{\"code\":\"badRequest\",\"message\":\"Bad request\"}";
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
        response.set(http::field::cache_control, "no-cache");
        
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

    template <typename Body, typename Allocator>
    StringResponse HandleJoinGame(const http::request<Body, http::basic_fields<Allocator>>& req, unsigned version, bool keep_alive) {
        if (req.method() != http::verb::post) {
            return MakeMethodNotAllowedResponse("POST", version, keep_alive, "Only POST method is allowed");
        }

        try {
            auto json_doc = json::parse(req.body());
            if (!json_doc.is_object()) {
                return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", version, keep_alive);
            }

            const auto& obj = json_doc.as_object();
            if (!obj.contains("userName") || !obj.contains("mapId")) {
                return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Invalid mapId or userName", version, keep_alive);
            }

            std::string user_name = json::value_to<std::string>(obj.at("userName"));
            std::string map_id = json::value_to<std::string>(obj.at("mapId"));

            auto trimmed_name = user_name;
            trimmed_name.erase(std::remove_if(trimmed_name.begin(), trimmed_name.end(), ::isspace), trimmed_name.end());
            if (user_name.empty() || trimmed_name.empty()) {
                return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", version, keep_alive);
            }

            auto [token, player_id] = game_.JoinGame(map_id, user_name);

            json::object res_obj;
            res_obj["authToken"] = *token;
            res_obj["playerId"] = player_id;

            return MakeJsonResponse(http::status::ok, res_obj, true, version, keep_alive);

        } catch (const std::invalid_argument& e) {
            std::string err_str = e.what();
            if (err_str == "mapNotFound") {
                return MakeJoinErrorResponse(http::status::not_found, "mapNotFound", "Map not found", version, keep_alive);
            }
            return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Bad request", version, keep_alive);
        } catch (...) {
            return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", version, keep_alive);
        }
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGetPlayers(const http::request<Body, http::basic_fields<Allocator>>& req, unsigned version, bool keep_alive) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeMethodNotAllowedResponse("GET, HEAD", version, keep_alive);
        }

        return ExecuteAuthorized(req, version, keep_alive, [this, &req, version, keep_alive](auto player) {
            json::object root_obj;
            auto current_session = player->GetSession();

            for (const auto& p : game_.GetPlayers()) {
                if (p->GetSession() == current_session) {
                    json::object player_obj;
                    player_obj["name"] = p->GetName();
                    root_obj[std::to_string(p->GetId())] = player_obj;
                }
            }

            return MakeJsonResponse(http::status::ok, root_obj, req.method() == http::verb::get, version, keep_alive);
        });
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGetGameState(const http::request<Body, http::basic_fields<Allocator>>& req, unsigned version, bool keep_alive) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeMethodNotAllowedResponse("GET, HEAD", version, keep_alive);
        }

        return ExecuteAuthorized(req, version, keep_alive, [this, &req, version, keep_alive](auto player) {
            json::object players_obj;
            auto current_session = player->GetSession();

            for (const auto& p : game_.GetPlayers()) {
                if (p->GetSession() == current_session) {
                    json::object dog_obj;
                    
                    json::array pos_arr{p->GetDog().GetPosition().x, p->GetDog().GetPosition().y};
                    dog_obj["pos"] = pos_arr;

                    json::array speed_arr{p->GetDog().GetSpeed().ux, p->GetDog().GetSpeed().uy};
                    dog_obj["speed"] = speed_arr;

                    dog_obj["dir"] = p->GetDog().GetDirectionString();

                    players_obj[std::to_string(p->GetId())] = dog_obj;
                }
            }

            json::object root_obj;
            root_obj["players"] = players_obj;

            return MakeJsonResponse(http::status::ok, root_obj, req.method() == http::verb::get, version, keep_alive);
        });
    }

    template <typename Body, typename Allocator>
    StringResponse HandlePlayerAction(const http::request<Body, http::basic_fields<Allocator>>& req, unsigned version, bool keep_alive) {
        if (req.method() != http::verb::post) {
            return MakeMethodNotAllowedResponse("POST", version, keep_alive);
        }

        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ct_it->value() != "application/json") {
            return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", version, keep_alive);
        }

        return ExecuteAuthorized(req, version, keep_alive, [this, &req, version, keep_alive](auto player) {
            try {
                auto json_doc = json::parse(req.body());
                if (!json_doc.is_object() || !json_doc.as_object().contains("move")) {
                    return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version, keep_alive);
                }

                std::string move_action = json::value_to<std::string>(json_doc.as_object().at("move"));
                
                if (move_action != "L" && move_action != "R" && move_action != "U" && move_action != "D" && move_action != "") {
                    return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version, keep_alive);
                }

                auto session = player->GetSession();
                const model::Map* map = game_.FindMap(session->GetMapId());
                double speed = map ? map->GetDogSpeed() : game_.GetDefaultDogSpeed();

                player->GetDog().Move(move_action, speed);

                json::object root_obj;
                return MakeJsonResponse(http::status::ok, root_obj, true, version, keep_alive);

            } catch (...) {
                return MakeJoinErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", version, keep_alive);
            }
        });
    }

    template <typename Body, typename Allocator>
    StringResponse HandleTickRequest(const http::request<Body, http::basic_fields<Allocator>>& req, 
                                    unsigned version, bool keep_alive) {
        if (req.method() != http::verb::post) {
            json::object error_obj;
            error_obj["code"] = "invalidMethod";  // <-- Исправили код ошибки
            error_obj["message"] = "Invalid method";
            return MakeStringResponse(http::status::method_not_allowed, 
                                    json::serialize(error_obj), 
                                    version, keep_alive, 
                                    "application/json", 
                                    {{"Allow", "POST"}}, 
                                    "no-cache");
        }

        try {
            auto json_doc = json::parse(req.body());
            if (!json_doc.is_object()) {
                throw std::invalid_argument("Not an object");
            }

            const auto& obj = json_doc.as_object();
            if (!obj.contains("timeDelta")) {
                throw std::invalid_argument("Missing timeDelta");
            }

            double delta_ms = 0.0;
            if (obj.at("timeDelta").is_int64()) {
                delta_ms = static_cast<double>(obj.at("timeDelta").as_int64());
            } else {
                throw std::invalid_argument("Invalid timeDelta type");
            }

            double dt = delta_ms / 1000.0;
            game_.Tick(dt);

            return MakeStringResponse(http::status::ok, 
                                    "{}", 
                                    version, keep_alive, 
                                    "application/json", 
                                    {}, 
                                    "no-cache");

        } catch (const std::exception& e) {
            json::object error_obj;
            error_obj["code"] = "invalidArgument";
            error_obj["message"] = "Failed to parse tick request JSON";
            
            return MakeStringResponse(http::status::bad_request, 
                                    json::serialize(error_obj), 
                                    version, keep_alive, 
                                    "application/json", 
                                    {}, 
                                    "no-cache");
        }
    }

    // Главный распределитель запросов
    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        std::string decoded_target = UrlDecode(target);
        
        if (decoded_target.rfind(API_PREFIX.data(), 0) == 0) {
            auto handle = [this, req = std::move(req), send = std::forward<Send>(send), decoded_target = std::move(decoded_target)]() mutable {    
                try {
                    assert(api_strand_.running_in_this_thread());
                    
                    StringResponse response;
                    
                    if (decoded_target == MAPS_ENDPOINT) {
                        if (req.method() == http::verb::get) {
                            response = MakeMapsListResponse(req.version(), req.keep_alive());
                        } else {
                            response = MakeApiBadRequestResponse(req.version(), req.keep_alive());
                        }
                    } else if (decoded_target == JOIN_GAME_ENDPOINT) {
                        response = HandleJoinGame(req, req.version(), req.keep_alive());
                    } else if (decoded_target == PLAYERS_ENDPOINT) {
                        response = HandleGetPlayers(req, req.version(), req.keep_alive());
                    } else if (decoded_target == GAME_STATE_ENDPOINT) {
                        response = HandleGetGameState(req, req.version(), req.keep_alive());
                    } else if (decoded_target == ACTION_ENDPOINT) {
                        response = HandlePlayerAction(req, req.version(), req.keep_alive());
                    } else if (decoded_target == TICK_ENDPOINT) {
                        response = HandleTickRequest(req, req.version(), req.keep_alive());
                    } else if (decoded_target.rfind(MAPS_PREFIX.data(), 0) == 0) {
                        if (req.method() == http::verb::get) {
                            std::string map_id = decoded_target.substr(MAPS_PREFIX.size());
                            response = MakeMapResponse(map_id, req.version(), req.keep_alive());
                        } else {
                            response = MakeApiBadRequestResponse(req.version(), req.keep_alive());
                        }
                    } else {
                        response = MakeApiBadRequestResponse(req.version(), req.keep_alive());
                    }

                    send(std::move(response));
                    
                } catch (...) {
                    send(MakeApiBadRequestResponse(req.version(), req.keep_alive()));
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

    template <typename Body, typename Allocator, typename Fn>
    StringResponse ExecuteAuthorized(const http::request<Body, http::basic_fields<Allocator>>& req, 
                                     unsigned version, bool keep_alive, Fn&& action) {
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return MakeJoinErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version, keep_alive);
        }

        std::string_view auth_header = auth_it->value();
        std::string_view bearer_prefix = "Bearer ";
        if (auth_header.rfind(bearer_prefix, 0) != 0) {
            return MakeJoinErrorResponse(http::status::unauthorized, "invalidToken", "Invalid token", version, keep_alive);
        }

        std::string token_str(auth_header.substr(bearer_prefix.size()));
        token_str.erase(std::remove_if(token_str.begin(), token_str.end(), ::isspace), token_str.end());

        if (token_str.empty() || token_str.size() != 32) {
            return MakeJoinErrorResponse(http::status::unauthorized, "invalidToken", "Invalid token", version, keep_alive);
        }

        model::Token token{token_str};
        auto player = game_.FindPlayerByToken(token);
        if (!player) {
            return MakeJoinErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", version, keep_alive);
        }

        return action(player);
    }

    StringResponse MakeJsonResponse(http::status status, const json::object& json_body, bool send_body, unsigned version, bool keep_alive) {
        StringResponse response(status, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        
        if (send_body) {
            response.body() = json::serialize(json_body);
        }
        
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeMethodNotAllowedResponse(std::string_view allow_methods, unsigned version, bool keep_alive, std::string_view msg = "Invalid method") {
        StringResponse response(http::status::method_not_allowed, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.set(http::field::allow, allow_methods.data());
        
        json::object err_obj;
        err_obj["code"] = "invalidMethod";
        err_obj["message"] = msg.data();
        
        response.body() = json::serialize(err_obj);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeJoinErrorResponse(http::status status, std::string_view code, std::string_view message, unsigned version, bool keep_alive) {
        StringResponse response(status, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        
        json::object err_obj;
        err_obj["code"] = code.data();
        err_obj["message"] = message.data();
        
        response.body() = json::serialize(err_obj);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeApiNotFoundResponse(unsigned version, bool keep_alive) {
        StringResponse response(http::status::not_found, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
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