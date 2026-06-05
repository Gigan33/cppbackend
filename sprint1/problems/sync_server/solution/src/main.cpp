#ifdef WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

// Типы
using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

// Структура для Content-Type
struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

// Создание ответа
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

// Обработка запроса
StringResponse HandleRequest(StringRequest&& req) {
    std::string target(req.target());
    
    // Удаляем ведущий '/'
    if (!target.empty() && target[0] == '/') {
        target.erase(0, 1);
    }
    
    std::string body = "Hello, " + target;
    
    if (req.method() == http::verb::get) {
        return MakeStringResponse(http::status::ok, body, req.version(), req.keep_alive());
    } 
    else if (req.method() == http::verb::head) {
        auto response = MakeStringResponse(http::status::ok, "", req.version(), req.keep_alive());
        response.content_length(body.size());  // HEAD: Content-Length как у GET
        return response;
    } 
    else {
        auto response = MakeStringResponse(http::status::method_not_allowed, "Invalid method", 
                                            req.version(), req.keep_alive());
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }
}

// Чтение запроса
std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    http::read(socket, buffer, req, ec);
    
    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    if (ec) {
        throw std::runtime_error("Failed to read request: "s + ec.message());
    }
    return req;
}

// Обработка соединения
template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request) {
    try {
        beast::flat_buffer buffer;
        while (auto request = ReadRequest(socket, buffer)) {
            StringResponse response = handle_request(std::move(*request));
            http::write(socket, response);
            if (response.need_eof()) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
    try {
        net::io_context ioc;
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        tcp::acceptor acceptor(ioc, {address, port});
        
        // Выводим строчку, что сервер запущен (требование тестов)
        std::cout << "Server has started..." << std::endl;
        
        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::thread t([](tcp::socket s) { 
                HandleConnection(s, HandleRequest); 
            }, std::move(socket));
            t.detach();
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
