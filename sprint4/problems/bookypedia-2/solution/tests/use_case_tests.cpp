#include <gtest/gtest.h>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> authors;

    void Save(const domain::Author& author) override {
        authors.push_back(author);
    }

    bool Delete(const domain::AuthorId& /*id*/) override {
        return true;
    }

    std::vector<domain::Author> GetAuthors() const override {
        return authors;
    }

    std::optional<domain::Author> FindByName(const std::string& /*name*/) const override {
        return std::nullopt;
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> books;

    void Save(const domain::Book& book, const std::vector<std::string>& /*tags*/ = {}) override {
        books.push_back(book);
    }

    bool Delete(const domain::BookId& /*id*/) override {
        return true;
    }

    std::vector<domain::Book> GetBooks() const override {
        return books;
    }

    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& book : books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }

    std::vector<domain::Book> FindBooksByTitle(const std::string& /*title*/) const override {
        return {};
    }

    std::vector<std::string> GetBookTags(const domain::BookId& /*book_id*/) const override {
        return {};
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
    app::UseCasesImpl use_cases{authors, books};
};

TEST(UseCasesTests, AddAuthorSavesToRepository) {
    Fixture fixture;
    auto id = fixture.use_cases.AddAuthor("Author Name");
    ASSERT_EQ(fixture.authors.authors.size(), 1u);
    EXPECT_EQ(fixture.authors.authors.at(0).GetId(), id);
    EXPECT_EQ(fixture.authors.authors.at(0).GetName(), "Author Name");
}

}  // namespace