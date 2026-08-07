#include <iostream>
#include <string>
#include <optional>
#include <pqxx/pqxx>
#include <boost/json.hpp>

namespace json = boost::json;
using namespace std::literals;
#include <boost/json/src.hpp>

void InitDatabase(pqxx::connection& conn) {
    pqxx::work tx{conn};
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            id SERIAL PRIMARY KEY,
            title VARCHAR(100) NOT NULL,
            author VARCHAR(100) NOT NULL,
            year INTEGER NOT NULL,
            isbn CHAR(13) UNIQUE
        );
    )"_pqxx);
    tx.commit();
}

bool AddBook(pqxx::connection& conn, 
             const std::string& title, 
             const std::string& author, 
             int year, 
             const std::optional<std::string>& isbn) {
    try {
        pqxx::work tx{conn};
        tx.exec_params(
            "INSERT INTO books (title, author, year, isbn) VALUES ($1, $2, $3, $4);",
            title, author, year, isbn
        );
        tx.commit();
        return true;
    } catch (const pqxx::sql_error& e) {
        // При нарушении UNIQUE ограничения (или любой другой ошибке SQL) возвращаем false
        return false;
    } catch (const std::exception& e) {
        return false;
    }
}

json::array GetAllBooks(pqxx::connection& conn) {
    pqxx::read_transaction tx{conn};

    auto result = tx.exec(R"(
        SELECT id, title, author, year, isbn 
        FROM books 
        ORDER BY year DESC, title ASC, author ASC, isbn ASC NULLS LAST;
    )"_pqxx);

    json::array books;
    for (const auto& row : result) {
        json::object book;
        book["id"] = row["id"].as<int>();
        book["title"] = row["title"].c_str();
        book["author"] = row["author"].c_str();
        book["year"] = row["year"].as<int>();

        if (row["isbn"].is_null()) {
            book["ISBN"] = nullptr;
        } else {
            // Убираем возможные пробелы от CHAR(13)
            std::string isbn_str = row["isbn"].c_str();
            while (!isbn_str.empty() && isbn_str.back() == ' ') {
                isbn_str.pop_back();
            }
            book["ISBN"] = isbn_str;
        }
        
        books.push_back(std::move(book));
    }
    
    return books;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: book_manager <db-connection-string>\n";
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn{argv[1]};
        InitDatabase(conn);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            try {
                auto req = json::parse(line).as_object();
                std::string action = req.at("action").as_string().c_str();

                if (action == "add_book") {
                    auto payload = req.at("payload").as_object();
                    std::string title = payload.at("title").as_string().c_str();
                    std::string author = payload.at("author").as_string().c_str();
                    int year = static_cast<int>(payload.at("year").as_int64());

                    std::optional<std::string> isbn;
                    if (!payload.at("ISBN").is_null()) {
                        isbn = payload.at("ISBN").as_string().c_str();
                    }

                    bool ok = AddBook(conn, title, author, year, isbn);
                    
                    json::object resp;
                    resp["result"] = ok;
                    std::cout << json::serialize(resp) << std::endl;

                } else if (action == "all_books") {
                    json::array books = GetAllBooks(conn);
                    std::cout << json::serialize(books) << std::endl;

                } else if (action == "exit") {
                    break;
                }
            } catch (const std::exception& e) {
                continue;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}