#include "use_cases.h"

#include <algorithm>
#include <stdexcept>

namespace app {

domain::AuthorId UseCasesImpl::AddAuthor(const std::string& name) {
    auto id = domain::AuthorId::New();
    authors_.Save({id, name});
    return id;
}

bool UseCasesImpl::DeleteAuthor(const domain::AuthorId& id) {
    return authors_.Delete(id);
}

bool UseCasesImpl::EditAuthor(const domain::AuthorId& id, const std::string& new_name) {
    authors_.Save({id, new_name});
    return true;
}

std::vector<domain::Author> UseCasesImpl::GetAuthors() const {
    return authors_.GetAuthors();
}

std::optional<domain::Author> UseCasesImpl::FindAuthorByName(const std::string& name) const {
    return authors_.FindByName(name);
}

void UseCasesImpl::AddBook(const domain::AuthorId& author_id, const std::string& title, int pub_year, const std::vector<std::string>& tags) {
    auto id = domain::BookId::New();
    books_.Save({id, author_id, title, pub_year}, tags);
}

bool UseCasesImpl::DeleteBook(const domain::BookId& id) {
    return books_.Delete(id);
}

bool UseCasesImpl::EditBook(const domain::BookId& id, const std::string& new_title, int new_pub_year, const std::vector<std::string>& new_tags) {
    auto details = GetBookDetails(id);
    if (!details) return false;
    books_.Save({id, details->author_id, new_title, new_pub_year}, new_tags);
    return true;
}

std::string UseCasesImpl::GetAuthorNameById(const domain::AuthorId& id) const {
    for (const auto& author : authors_.GetAuthors()) {
        if (author.GetId() == id) return author.GetName();
    }
    return "";
}

std::vector<BookFullInfo> UseCasesImpl::GetBooks() const {
    auto domain_books = books_.GetBooks();
    std::vector<BookFullInfo> result;
    result.reserve(domain_books.size());
    for (const auto& b : domain_books) {
        result.push_back({
            b.GetId(),
            b.GetAuthorId(),
            b.GetTitle(),
            GetAuthorNameById(b.GetAuthorId()),
            b.GetPublicationYear(),
            books_.GetBookTags(b.GetId())
        });
    }
    return result;
}

std::vector<BookFullInfo> UseCasesImpl::GetAuthorBooks(const domain::AuthorId& author_id) const {
    auto domain_books = books_.GetAuthorBooks(author_id);
    std::string author_name = GetAuthorNameById(author_id);
    std::vector<BookFullInfo> result;
    result.reserve(domain_books.size());
    for (const auto& b : domain_books) {
        result.push_back({
            b.GetId(),
            b.GetAuthorId(),
            b.GetTitle(),
            author_name,
            b.GetPublicationYear(),
            books_.GetBookTags(b.GetId())
        });
    }
    return result;
}

std::vector<BookFullInfo> UseCasesImpl::FindBooksByTitle(const std::string& title) const {
    auto domain_books = books_.FindBooksByTitle(title);
    std::vector<BookFullInfo> result;
    result.reserve(domain_books.size());
    for (const auto& b : domain_books) {
        result.push_back({
            b.GetId(),
            b.GetAuthorId(),
            b.GetTitle(),
            GetAuthorNameById(b.GetAuthorId()),
            b.GetPublicationYear(),
            books_.GetBookTags(b.GetId())
        });
    }
    return result;
}

std::optional<BookFullInfo> UseCasesImpl::GetBookDetails(const domain::BookId& id) const {
    for (const auto& b : GetBooks()) {
        if (b.id == id) return b;
    }
    return std::nullopt;
}

}  // namespace app