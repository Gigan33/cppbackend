#include "use_cases_impl.h"

#include <algorithm>
#include <string_view>

namespace app {
using namespace domain;

namespace {

// Вспомогательная функция для удаления пробелов в начале и конце строки
std::string Trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\r\n");
    return std::string{str.substr(start, end - start + 1)};
}

}  // namespace

bool UseCasesImpl::AddAuthor(const std::string& name) {
    std::string trimmed_name = Trim(name);
    if (trimmed_name.empty()) {
        return false;
    }
    try {
        authors_.Save({AuthorId::New(), trimmed_name});
        return true;
    } catch (...) {
        // Ошибка выполнения запроса (например, дубликат UNIQUE)
        return false;
    }
}

std::vector<Author> UseCasesImpl::GetAuthors() const {
    return authors_.GetAuthors();
}

void UseCasesImpl::AddBook(const AuthorId& author_id, const std::string& title, int pub_year) {
    books_.Save({BookId::New(), author_id, Trim(title), pub_year});
}

std::vector<Book> UseCasesImpl::GetBooks() const {
    return books_.GetBooks();
}

std::vector<Book> UseCasesImpl::GetAuthorBooks(const AuthorId& author_id) const {
    return books_.GetAuthorBooks(author_id);
}

}  // namespace app