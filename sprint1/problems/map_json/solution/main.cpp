#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include "json_loader.h"
#include "model.h"

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

using json = nlohmann::json;

std::atomic<bool> stop_server{false};

void SignalHandler(int sig) {
    std::cout << "\nSignal " << sig << " received. Shutting down..." << std::endl;
    stop_server = true;
}

using StringResponse = http::response<http::string_body>;

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

StringResponse HandleMapsRequest(const model::Game& game, unsigned version, bool keep_alive) {
    StringResponse response(http::status::ok, version);
    response.set(http::field::content_type, "application/json");
    
    json maps_array = json::array();
    for (const auto& map : game.GetMaps()) {
        maps_array.push_back({
            {"id", map.GetId()},
            {"name", map.GetName()}
        });
    }
    
    response.body() = maps_array.dump();
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

StringResponse HandleMapRequest(const model::Game& game, const std::string& map_id, 
                                unsigned version, bool keep_alive) {
    const model::Map* map = game.FindMap(map_id);
    
    if (!map) {
        return MakeNotFoundResponse(version, keep_alive);
    }
    
    StringResponse response(http::status::ok, version);
    response.set(http::field::content_type, "application/json");
    
    json map_json;
    map_json["id"] = map->GetId();
    map_json["name"] = map->GetName();
    
    json roads_array = json::array();
    for (const auto& road : map->GetRoads()) {
        const auto& start = road.GetStart();
        const auto& end = road.GetEnd();
        
        if (start.x == end.x) {
            roads_array.push_back({
                {"x0", start.x},
                {"y0", start.y},
                {"y1", end.y}
            });
        } else {
            roads_array.push_back({
                {"x0", start.x},
                {"y0", start.y},
                {"x1", end.x}
            });
        }
    }
    map_json["roads"] = roads_array;
    
    json buildings_array = json::array();
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        buildings_array.push_back({
            {"x", bounds.p1.x},
            {"y", bounds.p1.y},
            {"w", bounds.p2.x - bounds.p1.x},
            {"h", bounds.p2.y - bounds.p1.y}
        });
    }
    map_json["buildings"] = buildings_array;
    
    json offices_array = json::array();
    for (const auto& office : map->GetOffices()) {
        offices_array.push_back({
            {"id", office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        });
    }
    map_json["offices"] = offices_array;
    
    response.body() = map_json.dump();
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, const model::Game& game)
        : socket_(std::move(socket)), game_(game) {}

    void Run() {
        ReadRequest();
    }

private:
    void ReadRequest() {
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, request_,
            [self](beast::error_code ec, std::size_t) {
                if (!ec) {
                    self->HandleRequest();
                }
            });
    }

    void HandleRequest() {
        std::string target(request_.target());
        StringResponse response;
        
        if (target.rfind("/api/", 0) != 0) {
            response = MakeBadRequestResponse(request_.version(), request_.keep_alive());
        }
        else if (target == "/api/v1/maps") {
            if (request_.method() == http::verb::get) {
                response = HandleMapsRequest(game_, request_.version(), request_.keep_alive());
            } else {
                response = MakeBadRequestResponse(request_.version(), request_.keep_alive());
            }
        }
        else if (target.rfind("/api/v1/maps/", 0) == 0) {
            if (request_.method() == http::verb::get) {
                std::string map_id = target.substr(13);
                response = HandleMapRequest(game_, map_id, request_.version(), request_.keep_alive());
            } else {
                response = MakeBadRequestResponse(request_.version(), request_.keep_alive());
            }
        }
        else {
            response = MakeBadRequestResponse(request_.version(), request_.keep_alive());
        }
        
        WriteResponse(std::move(response));
    }

    void WriteResponse(StringResponse&& response) {
        auto self = shared_from_this();
        http::async_write(socket_, response,
            [self](beast::error_code ec, std::size_t) {
                if (!ec) {
                    self->ReadRequest();
                }
            });
    }

    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    const model::Game& game_;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: game_server <config.json>" << std::endl;
        return 1;
    }
    
    try {
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        
        std::string config_path = argv[1];
        model::Game game = json_loader::LoadGame(config_path);
        
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, {net::ip::make_address("0.0.0.0"), 8080});
        
        std::cout << "Server has started..." << std::endl;
        std::cout << "Listening on http://0.0.0.0:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Асинхронный accept
        std::function<void(beast::error_code, tcp::socket)> accept_handler;
        accept_handler = [&](beast::error_code ec, tcp::socket socket) {
            if (!ec && !stop_server) {
                std::make_shared<Session>(std::move(socket), game)->Run();
            }
            if (!stop_server) {
                acceptor.async_accept(accept_handler);
            }
        };
        
        acceptor.async_accept(accept_handler);
        
        // Запускаем io_context в отдельном потоке
        std::thread io_thread([&ioc]() {
            ioc.run();
        });
        
        // Ждём сигнал остановки
        while (!stop_server) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Останавливаем io_context
        ioc.stop();
        io_thread.join();
        
        std::cout << "Server stopped gracefully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
