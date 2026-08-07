#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h" // 1. Подключаем книги

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    // 2. Реализуем недостающий чистый виртуальный метод
    std::vector<domain::Author> GetAuthors() const override {
        return saved_authors;
    }
};

// 3. Добавляем мок для репозитория книг
struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }

    std::vector<domain::Book> GetBooks() const override {
        return saved_books;
    }

    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books; // 4. Добавляем репозиторий книг в фикстуру
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books}; // 5. Передаем оба репозитория

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}