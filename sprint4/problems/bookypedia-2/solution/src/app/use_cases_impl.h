#pragma once

#include "use_cases.h"

namespace app {

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
    std::optional<BookFullInfo> GetBookDetails(const domain::BookId& id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;

    std::string GetAuthorNameById(const domain::AuthorId& id) const;
};

}  // namespace app