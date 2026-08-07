#pragma once

#include <pqxx/pqxx>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::Author& author) override;
    bool Delete(const domain::AuthorId& id) override;
    std::vector<domain::Author> GetAuthors() const override;
    std::optional<domain::Author> FindByName(const std::string& name) const override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::Book& book, const std::vector<std::string>& tags = {}) override;
    bool Delete(const domain::BookId& id) override;
    std::vector<domain::Book> GetBooks() const override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override;
    std::vector<domain::Book> FindBooksByTitle(const std::string& title) const override;
    std::vector<std::string> GetBookTags(const domain::BookId& book_id) const override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection)
        : connection_{std::move(connection)} {}

    AuthorRepositoryImpl& GetAuthors() { return authors_; }
    BookRepositoryImpl& GetBooks() { return books_; }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres