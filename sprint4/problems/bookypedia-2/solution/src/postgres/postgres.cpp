#include "postgres.h"

#include <boost/uuid/uuid_io.hpp>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// --- Database ---

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    
    // 1. Авторы
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL
);
)"_zv);

    // 2. Книги (с CASCADE удалением при удалении автора)
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title VARCHAR(100) NOT NULL,
    publication_year INTEGER NOT NULL
);
)"_zv);

    // 3. Теги книг
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag VARCHAR(30) NOT NULL
);
)"_zv);

    work.commit();
}

// --- AuthorRepositoryImpl ---

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

bool AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    auto result = work.exec_params("DELETE FROM authors WHERE id = $1;"_zv, id.ToString());
    work.commit();
    return result.affected_rows() > 0;
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAuthors() const {
    pqxx::read_transaction work{connection_};
    auto result = work.exec("SELECT id, name FROM authors ORDER BY name ASC;"_zv);

    std::vector<domain::Author> authors;
    for (const auto& row : result) {
        authors.emplace_back(
            domain::AuthorId::FromString(row["id"].as<std::string>()),
            row["name"].as<std::string>()
        );
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::FindByName(const std::string& name) const {
    pqxx::read_transaction work{connection_};
    auto result = work.exec_params("SELECT id, name FROM authors WHERE name = $1;"_zv, name);
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    return domain::Author{
        domain::AuthorId::FromString(row["id"].as<std::string>()),
        row["name"].as<std::string>()
    };
}

// --- BookRepositoryImpl ---

void BookRepositoryImpl::Save(const domain::Book& book, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    
    // 1. Сохранение/обновление самой книги
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET title=$3, publication_year=$4;
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(), book.GetPublicationYear());

    // 2. Перезапись тегов книги
    work.exec_params("DELETE FROM book_tags WHERE book_id = $1;"_zv, book.GetId().ToString());
    for (const auto& tag : tags) {
        work.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);"_zv, book.GetId().ToString(), tag);
    }

    work.commit();
}

bool BookRepositoryImpl::Delete(const domain::BookId& id) {
    pqxx::work work{connection_};
    auto result = work.exec_params("DELETE FROM books WHERE id = $1;"_zv, id.ToString());
    work.commit();
    return result.affected_rows() > 0;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooks() const {
    pqxx::read_transaction work{connection_};
    auto result = work.exec(R"(
SELECT b.id, b.author_id, b.title, b.publication_year 
FROM books b
JOIN authors a ON b.author_id = a.id
ORDER BY b.title ASC, a.name ASC, b.publication_year ASC;
)"_zv);

    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(
            domain::BookId::FromString(row["id"].as<std::string>()),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["title"].as<std::string>(),
            row["publication_year"].as<int>()
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(const domain::AuthorId& author_id) const {
    pqxx::read_transaction work{connection_};
    auto result = work.exec_params(
        R"(
SELECT id, author_id, title, publication_year FROM books 
WHERE author_id = $1 
ORDER BY publication_year ASC, title ASC;
)"_zv,
        author_id.ToString());

    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(
            domain::BookId::FromString(row["id"].as<std::string>()),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["title"].as<std::string>(),
            row["publication_year"].as<int>()
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::FindBooksByTitle(const std::string& title) const {
    pqxx::read_transaction work{connection_};
    auto result = work.exec_params(R"(
SELECT b.id, b.author_id, b.title, b.publication_year 
FROM books b
JOIN authors a ON b.author_id = a.id
WHERE b.title = $1
ORDER BY a.name ASC, b.publication_year ASC;
)"_zv, title);

    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(
            domain::BookId::FromString(row["id"].as<std::string>()),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["title"].as<std::string>(),
            row["publication_year"].as<int>()
        );
    }
    return books;
}

std::vector<std::string> BookRepositoryImpl::GetBookTags(const domain::BookId& book_id) const {
    pqxx::read_transaction work{connection_};
    auto result = work.exec_params(
        "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag ASC;"_zv,
        book_id.ToString()
    );

    std::vector<std::string> tags;
    for (const auto& row : result) {
        tags.push_back(row["tag"].as<std::string>());
    }
    return tags;
}

}  // namespace postgres