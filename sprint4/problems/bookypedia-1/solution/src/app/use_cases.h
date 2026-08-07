#pragma once

#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    // Возвращает true при успехе, false при ошибке (пустое имя / дубликат)
    virtual bool AddAuthor(const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors() const = 0;

    virtual void AddBook(const domain::AuthorId& author_id, const std::string& title, int pub_year) = 0;
    virtual std::vector<domain::Book> GetBooks() const = 0;
    virtual std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app