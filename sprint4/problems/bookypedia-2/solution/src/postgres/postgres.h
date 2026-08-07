#pragma once

#include <pqxx/pqxx>
#include <optional>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::Author& author);
    bool Delete(const domain::AuthorId& id);
    std::vector<domain::Author> GetAuthors() const;
    std::optional<domain::Author> FindByName(const std::string& name) const;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::Book& book, const std::vector<std::string>& tags = {});
    bool Delete(const domain::BookId& id);
    
    std::vector<domain::Book> GetBooks() const;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const;
    std::vector<domain::Book> FindBooksByTitle(const std::string& title) const;
    std::vector<std::string> GetBookTags(const domain::BookId& book_id) const;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() {
        return books_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres