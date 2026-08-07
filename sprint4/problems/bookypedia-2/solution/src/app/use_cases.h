#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

struct BookFullInfo {
    domain::BookId id;
    domain::AuthorId author_id;
    std::string title;
    std::string author_name;
    int publication_year = 0;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual ~UseCases() = default;

    // Авторы
    virtual domain::AuthorId AddAuthor(const std::string& name) = 0;
    virtual bool DeleteAuthor(const domain::AuthorId& id) = 0;
    virtual bool EditAuthor(const domain::AuthorId& id, const std::string& new_name) = 0;
    virtual std::vector<domain::Author> GetAuthors() const = 0;
    virtual std::optional<domain::Author> FindAuthorByName(const std::string& name) const = 0;

    // Книги
    virtual void AddBook(const domain::AuthorId& author_id, const std::string& title, int pub_year, const std::vector<std::string>& tags) = 0;
    virtual bool DeleteBook(const domain::BookId& id) = 0;
    virtual bool EditBook(const domain::BookId& id, const std::string& new_title, int new_pub_year, const std::vector<std::string>& new_tags) = 0;
    virtual std::vector<BookFullInfo> GetBooks() const = 0;
    virtual std::vector<BookFullInfo> GetAuthorBooks(const domain::AuthorId& author_id) const = 0;
    virtual std::vector<BookFullInfo> FindBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<BookFullInfo> GetBookDetails(const domain::BookId& id) const = 0;
};

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {}

    domain::AuthorId AddAuthor(const std::string& name) override;
    bool DeleteAuthor(const domain::AuthorId& id) override;
    bool EditAuthor(const domain::AuthorId& id, const std::string& new_name) override;
    std::vector<domain::Author> GetAuthors() const override;
    std::optional<domain::Author> FindAuthorByName(const std::string& name) const override;

    void AddBook(const domain::AuthorId& author_id, const std::string& title, int pub_year, const std::vector<std::string>& tags) override;
    bool DeleteBook(const domain::BookId& id) override;
    bool EditBook(const domain::BookId& id, const std::string& new_title, int new_pub_year, const std::vector<std::string>& new_tags) override;
    std::vector<BookFullInfo> GetBooks() const override;
    std::vector<BookFullInfo> GetAuthorBooks(const domain::AuthorId& author_id) const override;
    std::vector<BookFullInfo> FindBooksByTitle(const std::string& title) const override;
    std::optional<BookFullInfo> GetBookDetails(const domain::BookId& id) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;

    std::string GetAuthorNameById(const domain::AuthorId& id) const;
};

}  // namespace app