#pragma once

#include "use_cases.h"
#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    bool AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() const override;

    void AddBook(const domain::AuthorId& author_id, const std::string& title, int pub_year) override;
    std::vector<domain::Book> GetBooks() const override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app