#pragma once
#include "http_server.h"
#include "model.h"
#include "model_serialization.h"
#include "records.h"

#include <boost/json.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/log/trivial.hpp>
#include <filesystem>
#include <future>
#include <functional>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <unordered_map>


namespace fs = std::filesystem;

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio;

using StringResponse = http::response<http::string_body>;

inline constexpr std::string_view API_PREFIX = "/api/";
inline constexpr std::string_view MAPS_ENDPOINT = "/api/v1/maps";
inline constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";
inline constexpr std::string_view JOIN_GAME_ENDPOINT = "/api/v1/game/join";
inline constexpr std::string_view PLAYERS_ENDPOINT = "/api/v1/game/players";
inline constexpr std::string_view GAME_STATE_ENDPOINT = "/api/v1/game/state";
inline constexpr std::string_view ACTION_ENDPOINT = "/api/v1/game/player/action";
inline constexpr std::string_view TICK_ENDPOINT = "/api/v1/game/tick";
inline constexpr std::string_view RECORDS_ENDPOINT = "/api/v1/game/records";

inline boost::beast::string_view ToBoostSV(std::string_view sv) noexcept {
    return {sv.data(), sv.size()};
}

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

inline std::pair<std::string, std::string> SplitPathAndQuery(const std::string& target) {
    auto pos = target.find('?');
    if (pos == std::string::npos) return {target, {}};
    return {target.substr(0, pos), target.substr(pos + 1)};
}

inline std::unordered_map<std::string, std::string> ParseQueryParams(const std::string& query) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;
    while (std::getline(iss, pair, '&')) {
        auto eq_pos = pair.find('=');
        if (eq_pos == std::string::npos) continue;
        params[pair.substr(0, eq_pos)] = pair.substr(eq_pos + 1);
    }
    return params;
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

// Проверка защиты от выходя за пределы корневой папки (Path Traversal Protection)
inline bool IsSubpath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

class RequestHandler {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using SaveStateFn = std::function<void(const std::string&, const serialization::SavedState&)>;

    explicit RequestHandler(
        model::Game& game, 
        std::string static_dir, 
        Strand api_strand, 
        bool auto_tick_enabled,
        std::string state_file = {}, 
        std::optional<std::chrono::milliseconds> save_period = std::nullopt,
        SaveStateFn save_state_fn = {}
    )
        : game_{game}
        , static_dir_{std::move(static_dir)}
        , api_strand_{std::move(api_strand)}
        , auto_tick_enabled_{auto_tick_enabled}
        , state_file_{std::move(state_file)}
        , save_period_{save_period}
        , save_state_fn_{std::move(save_state_fn)}
    {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void RestoreState(serialization::SavedState state) {
        net::dispatch(api_strand_, [this, state = std::move(state)]() {
            game_.RestoreState(state);
        });
    }

    serialization::SavedState GetSerializedState() const {
        auto promise = std::make_shared<std::promise<serialization::SavedState>>();
        auto future = promise->get_future();

        net::dispatch(api_strand_, [this, promise]() {
            try {
                promise->set_value(game_.GetSerializedState());
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future.get();
    }

    void SetSaveOptions(std::string state_file_path, 
                        std::optional<std::chrono::milliseconds> save_period,
                        SaveStateFn save_state_fn) {
        state_file_ = std::move(state_file_path);
        save_period_ = save_period;
        save_state_fn_ = std::move(save_state_fn);
    }

     void SetRecordsRepository(std::shared_ptr<records::Repository> repo) {
        records_repo_ = repo;
        game_.SetRetirementCallback([repo](const model::RetiredPlayerRecord& record) {
            std::thread([repo, record]() {
                try {
                    repo->Save(record);
                } catch (const std::exception& ex) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to save retired player record: " << ex.what();
                }
            }).detach();
        });
    }

    void Tick(std::chrono::milliseconds delta_time) {
        double dt_seconds = delta_time.count() / 1000.0;
        game_.Tick(dt_seconds);

        if (save_period_ && !state_file_.empty() && save_state_fn_) {
            time_since_last_save_ += delta_time;
            if (time_since_last_save_ >= *save_period_) {
                time_since_last_save_ = std::chrono::milliseconds{0};

                auto state = game_.GetSerializedState();
                auto filename = state_file_;
                auto save_fn = save_state_fn_;

                std::thread([filename = std::move(filename), 
                            state = std::move(state), 
                            save_fn = std::move(save_fn)]() mutable {
                    try {
                        save_fn(filename, state);
                    } catch (const std::exception& ex) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to auto-save state: " << ex.what();
                    }
                }).detach();
            }
        }
    }

    StringResponse MakeStringResponse(
        http::status status, 
        std::string_view body, 
        unsigned version, 
        bool keep_alive,
        std::string_view content_type = "application/json",
        const std::vector<std::pair<std::string, std::string>>& custom_headers = {},
        std::string_view cache_control = "") 
    {
        StringResponse response(status, version);
        response.set(http::field::content_type, ToBoostSV(content_type));

        if (!cache_control.empty()) {
            response.set(http::field::cache_control, ToBoostSV(cache_control));
        }

        for (const auto& [header, value] : custom_headers) {
            response.set(header, value);
        }

        response.body() = std::string(body);
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);

        return response;
    }

    StringResponse MakeMapsListResponse(unsigned version, bool keep_alive, bool send_body = true) {
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
        
        std::string body_str = json::serialize(arr);
        response.content_length(body_str.size());

        if (send_body) {
            response.body() = std::move(body_str);
        }

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

    StringResponse MakeMapResponse(const std::string& map_id, unsigned version, bool keep_alive, bool send_body = true) {
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
        result["lootTypes"] = map->GetLootTypes();

        if (auto speed = map->GetDogSpeed()) {
            result["dogSpeed"] = speed;
        }
        result["bagCapacity"] = map->GetBagCapacity();

        std::string body_str = json::serialize(result);
        response.content_length(body_str.size());

        if (send_body) {
            response.body() = std::move(body_str);
        }

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
                    const auto& dog = p->GetDog();

                    dog_obj["pos"] = json::array{dog.GetPosition().x, dog.GetPosition().y};
                    dog_obj["speed"] = json::array{dog.GetSpeed().x, dog.GetSpeed().y};
                    dog_obj["dir"] = dog.GetDirectionString();

                    json::array bag_json;
                    for (const auto& item : dog.GetBag()) {
                        json::object item_obj;
                        item_obj["id"] = item.id;
                        item_obj["type"] = item.type;
                        bag_json.push_back(item_obj);
                    }
                    dog_obj["bag"] = bag_json;
                    dog_obj["score"] = dog.GetScore();

                    players_obj[std::to_string(p->GetId())] = dog_obj;
                }
            }

            json::object lost_objects_obj;
            if (current_session) {
                for (const auto& [obj_id, lost_obj] : current_session->GetLostObjects()) {
                    json::object item_obj;
                    item_obj["type"] = lost_obj.type;
                    item_obj["pos"] = json::array{lost_obj.pos.x, lost_obj.pos.y};

                    lost_objects_obj[std::to_string(obj_id)] = item_obj;
                }
            }

            json::object root_obj;
            root_obj["players"] = players_obj;
            root_obj["lostObjects"] = lost_objects_obj;

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
                const model::Map* map = session->GetMap();
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
            error_obj["code"] = "invalidMethod";
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
            const auto& val = obj.at("timeDelta");

            if (val.is_int64()) {
                delta_ms = static_cast<double>(val.as_int64());
            } else if (val.is_uint64()) {
                delta_ms = static_cast<double>(val.as_uint64());
            } else if (val.is_double()) {
                delta_ms = val.as_double();
            } else {
                throw std::invalid_argument("Invalid timeDelta type");
            }

            Tick(std::chrono::milliseconds(static_cast<int64_t>(delta_ms)));

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

    template <typename Body, typename Allocator>
    StringResponse HandleGetRecords(const http::request<Body, http::basic_fields<Allocator>>& req,
                                    const std::string& query, unsigned version, bool keep_alive) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeMethodNotAllowedResponse("GET, HEAD", version, keep_alive);
        }

        auto params = ParseQueryParams(query);
        int start = 0, max_items = 100;

        try {
            if (auto it = params.find("start"); it != params.end() && !it->second.empty())
                start = std::stoi(it->second);
            if (auto it = params.find("maxItems"); it != params.end() && !it->second.empty())
                max_items = std::stoi(it->second);
        } catch (...) {
            return MakeApiBadRequestResponse(version, keep_alive);
        }

        if (max_items > 100 || max_items < 0 || start < 0) {
            return MakeApiBadRequestResponse(version, keep_alive);
        }

        json::array arr;
        if (records_repo_) {
            for (const auto& r : records_repo_->GetRecords(start, max_items)) {
                json::object obj;
                obj["name"] = r.name;
                obj["score"] = r.score;
                obj["playTime"] = r.play_time;
                arr.push_back(obj);
            }
        }

        std::string body_str = json::serialize(arr);
        StringResponse response(http::status::ok, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.content_length(body_str.size());
        if (req.method() == http::verb::get) response.body() = std::move(body_str);
        response.keep_alive(keep_alive);
        return response;
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        std::string decoded_target = UrlDecode(target);
        
        if (decoded_target.rfind(API_PREFIX.data(), 0) == 0) {
            auto handle = [this, req = std::move(req), send = std::forward<Send>(send), decoded_target = std::move(decoded_target)]() mutable {    
                try {
                    assert(api_strand_.running_in_this_thread());

                    auto [path, query] = SplitPathAndQuery(decoded_target);
                    
                    StringResponse response;

                    if (path == MAPS_ENDPOINT) {
                        if (req.method() == http::verb::get || req.method() == http::verb::head) {
                            response = MakeMapsListResponse(req.version(), req.keep_alive(), req.method() == http::verb::get);
                        } else {
                            response = MakeMethodNotAllowedResponse("GET, HEAD", req.version(), req.keep_alive());
                        }
                    } else if (path == JOIN_GAME_ENDPOINT) {
                        response = HandleJoinGame(req, req.version(), req.keep_alive());
                    } else if (path == PLAYERS_ENDPOINT) {
                        response = HandleGetPlayers(req, req.version(), req.keep_alive());
                    } else if (path == GAME_STATE_ENDPOINT) {
                        response = HandleGetGameState(req, req.version(), req.keep_alive());
                    } else if (path == ACTION_ENDPOINT) {
                        response = HandlePlayerAction(req, req.version(), req.keep_alive());
                    } else if (path == RECORDS_ENDPOINT) { // 3. Новая ветка для рекордов
                        response = HandleGetRecords(req, query, req.version(), req.keep_alive());
                    } else if (path == TICK_ENDPOINT) {
                        if (auto_tick_enabled_) {
                            json::object error_obj;
                            error_obj["code"] = "badRequest";
                            error_obj["message"] = "Invalid endpoint";
                            
                            response = MakeStringResponse(http::status::bad_request, 
                                                        json::serialize(error_obj), 
                                                        req.version(), req.keep_alive(), 
                                                        "application/json", {}, "no-cache");
                        } else {
                            response = HandleTickRequest(req, req.version(), req.keep_alive());
                        }
                    } else if (path.rfind(MAPS_PREFIX.data(), 0) == 0) {
                        if (req.method() == http::verb::get || req.method() == http::verb::head) {
                            std::string map_id = path.substr(MAPS_PREFIX.size());
                            response = MakeMapResponse(map_id, req.version(), req.keep_alive(), req.method() == http::verb::get);
                        } else {
                            response = MakeMethodNotAllowedResponse("GET, HEAD", req.version(), req.keep_alive());
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
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                StringResponse response = MakeStringResponse(http::status::method_not_allowed, "Method Not Allowed", 
                                                            req.version(), req.keep_alive(), "text/plain", 
                                                            {{"Allow", "GET, HEAD"}});
                return send(std::move(response));
            } 

            if (static_dir_.empty()) {
                return send(MakeStaticBadRequestResponse(req.version(), req.keep_alive()));
            } 

            std::string relative_path = decoded_target;
            if (!relative_path.empty() && relative_path[0] == '/') {
                relative_path.erase(0, 1);
            }
            
            fs::path base_path = fs::canonical(static_dir_);
            fs::path full_path = fs::weakly_canonical(base_path / relative_path);

            if (!IsSubpath(full_path, base_path)) {
                return send(MakeStaticBadRequestResponse(req.version(), req.keep_alive()));
            }

            if (fs::is_directory(full_path)) {
                full_path /= "index.html";
            }

            if (!fs::exists(full_path)) {
                return send(MakeStaticNotFoundResponse(req.version(), req.keep_alive()));
            }

            http::file_body::value_type body;
            boost::system::error_code ec;
            body.open(full_path.string().c_str(), beast::file_mode::read, ec);

            if (ec) {
                return send(MakeStaticNotFoundResponse(req.version(), req.keep_alive()));
            }

            auto const size = body.size();
            http::response<http::file_body> res{
                std::piecewise_construct,
                std::make_tuple(std::move(body)),
                std::make_tuple(http::status::ok, req.version())};

            auto mime = GetMimeType(full_path);
            res.set(http::field::content_type, ToBoostSV(mime));
            res.content_length(size);
            res.keep_alive(req.keep_alive());

            if (req.method() == http::verb::head) {
                StringResponse head_res(http::status::ok, req.version());
                head_res.set(http::field::content_type, ToBoostSV(mime));
                head_res.content_length(size);
                head_res.keep_alive(req.keep_alive());
                return send(std::move(head_res));
            }

            return send(std::move(res));
        }
    }

private:
    model::Game& game_;
    std::string static_dir_;
    Strand api_strand_;
    bool auto_tick_enabled_;

    std::string state_file_;
    std::optional<std::chrono::milliseconds> save_period_;
    SaveStateFn save_state_fn_;
    std::shared_ptr<records::Repository> records_repo_;
    std::chrono::milliseconds time_since_last_save_{0};

    template <typename Body, typename Allocator, typename Fn>
    StringResponse ExecuteAuthorized(const http::request<Body, http::basic_fields<Allocator>>& req, 
                                     unsigned version, bool keep_alive, Fn&& action) {
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return MakeJoinErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", version, keep_alive);
        }

        std::string_view auth_header = {auth_it->value().data(), auth_it->value().size()};
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
        
        std::string body_str = json::serialize(json_body);
        response.content_length(body_str.size());

        if (send_body) {
            response.body() = std::move(body_str);
        }
        
        response.keep_alive(keep_alive);
        return response;
    }

    StringResponse MakeMethodNotAllowedResponse(std::string_view allow_methods, unsigned version, bool keep_alive, std::string_view msg = "Invalid method") {
        StringResponse response(http::status::method_not_allowed, version);
        response.set(http::field::content_type, "application/json");
        response.set(http::field::cache_control, "no-cache");
        response.set(http::field::allow, ToBoostSV(allow_methods));
        
        json::object err_obj;
        err_obj["code"] = "invalidMethod";
        err_obj["message"] = std::string(msg);
        
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