#include "view.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <set>
#include <sstream>

#include "../menu/menu.h"

using namespace std::literals;

namespace ui {

namespace {

std::vector<std::string> NormalizeTags(const std::string& raw_tags) {
    std::stringstream ss(raw_tags);
    std::string token;
    std::set<std::string> unique_tags;

    while (std::getline(ss, token, ',')) {
        boost::algorithm::trim(token);
        if (token.empty()) continue;

        std::string normalized;
        bool last_was_space = false;
        for (char ch : token) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                if (!last_was_space) {
                    normalized += ' ';
                    last_was_space = true;
                }
            } else {
                normalized += ch;
                last_was_space = false;
            }
        }

        if (!normalized.empty() && normalized.length() <= 30) {
            unique_tags.insert(normalized);
        }
    }

    return {unique_tags.begin(), unique_tags.end()};
}

}  // namespace

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {

    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, [this](auto& in) { return AddAuthor(in); });
    menu_.AddAction("DeleteAuthor"s, "name_or_empty"s, "Deletes author"s, [this](auto& in) { return DeleteAuthor(in); });
    menu_.AddAction("EditAuthor"s, "name_or_empty"s, "Edits author"s, [this](auto& in) { return EditAuthor(in); });
    menu_.AddAction("ShowAuthors"s, {}, "Shows authors"s, [this](auto&) { return ShowAuthors(); });

    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, [this](auto& in) { return AddBook(in); });
    menu_.AddAction("DeleteBook"s, "title_or_empty"s, "Deletes book"s, [this](auto& in) { return DeleteBook(in); });
    menu_.AddAction("EditBook"s, "title_or_empty"s, "Edits book"s, [this](auto& in) { return EditBook(in); });
    menu_.AddAction("ShowBooks"s, {}, "Shows books"s, [this](auto&) { return ShowBooks(); });
    menu_.AddAction("ShowBook"s, "title_or_empty"s, "Shows detailed book info"s, [this](auto& in) { return ShowBook(in); });
    menu_.AddAction("ShowAuthorBooks"s, {}, "Shows author books"s, [this](auto& in) { return ShowAuthorBooks(in); });
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        use_cases_.AddAuthor(name);
    } catch (...) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<domain::AuthorId> author_id;
        if (!name.empty()) {
            auto author = use_cases_.FindAuthorByName(name);
            if (author) author_id = author->GetId();
        } else {
            author_id = SelectAuthor();
        }

        if (!author_id || !use_cases_.DeleteAuthor(*author_id)) {
            output_ << "Failed to delete author"sv << std::endl;
        }
    } catch (...) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<domain::AuthorId> author_id;
        if (!name.empty()) {
            auto author = use_cases_.FindAuthorByName(name);
            if (author) author_id = author->GetId();
        } else {
            author_id = SelectAuthor();
        }

        if (!author_id) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);

        if (new_name.empty() || !use_cases_.EditAuthor(*author_id, new_name)) {
            output_ << "Failed to edit author"sv << std::endl;
        }
    } catch (...) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    int idx = 1;
    for (const auto& author : use_cases_.GetAuthors()) {
        output_ << idx++ << " " << author.GetName() << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int pub_year = 0;
        if (!(cmd_input >> pub_year)) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        if (title.empty()) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_name;
        std::getline(input_, author_name);
        boost::algorithm::trim(author_name);

        std::optional<domain::AuthorId> author_id;
        if (!author_name.empty()) {
            auto author = use_cases_.FindAuthorByName(author_name);
            if (!author) {
                output_ << "No author found. Do you want to add " << author_name << " (y/n)?" << std::endl;
                std::string answer;
                std::getline(input_, answer);
                boost::algorithm::trim(answer);
                if (answer != "y" && answer != "Y") {
                    output_ << "Failed to add book"sv << std::endl;
                    return true;
                }
                author_id = use_cases_.AddAuthor(author_name);
            } else {
                author_id = author->GetId();
            }
        } else {
            author_id = SelectAuthor();
        }

        if (!author_id) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        output_ << "Enter tags (comma separated):" << std::endl;
        std::string raw_tags;
        std::getline(input_, raw_tags);
        auto tags = NormalizeTags(raw_tags);

        use_cases_.AddBook(*author_id, title, pub_year, tags);
    } catch (...) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        auto book = SelectBook(cmd_input);
        if (!book || !use_cases_.DeleteBook(book->id)) {
            output_ << "Failed to delete book"sv << std::endl;
        }
    } catch (...) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        auto book = SelectBook(cmd_input);
        if (!book) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        output_ << "Enter new title or empty line to use the current one (" << book->title << "):" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) new_title = book->title;

        output_ << "Enter publication year or empty line to use the current one (" << book->publication_year << "):" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        int new_year = year_str.empty() ? book->publication_year : std::stoi(year_str);

        std::string current_tags = boost::algorithm::join(book->tags, ", ");
        output_ << "Enter tags (current tags: " << current_tags << "):" << std::endl;
        std::string raw_tags;
        std::getline(input_, raw_tags);
        auto new_tags = NormalizeTags(raw_tags);

        if (!use_cases_.EditBook(book->id, new_title, new_year, new_tags)) {
            output_ << "Book not found"sv << std::endl;
        }
    } catch (...) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

bool View::ShowBooks() const {
    int idx = 1;
    for (const auto& b : use_cases_.GetBooks()) {
        output_ << idx++ << " " << b.title << " by " << b.author_name << ", " << b.publication_year << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        auto book = SelectBook(cmd_input);
        if (!book) return true;

        output_ << "Title: " << book->title << std::endl;
        output_ << "Author: " << book->author_name << std::endl;
        output_ << "Publication year: " << book->publication_year << std::endl;
        if (!book->tags.empty()) {
            output_ << "Tags: " << boost::algorithm::join(book->tags, ", ") << std::endl;
        }
    } catch (...) {}
    return true;
}

bool View::ShowAuthorBooks(std::istream&) const {
    try {
        auto author_id = SelectAuthor();
        if (!author_id) return true;

        int idx = 1;
        for (const auto& b : use_cases_.GetAuthorBooks(*author_id)) {
            output_ << idx++ << " " << b.title << ", " << b.publication_year << std::endl;
        }
    } catch (...) {}
    return true;
}

std::optional<domain::AuthorId> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = use_cases_.GetAuthors();
    int idx = 1;
    for (const auto& a : authors) {
        output_ << idx++ << " " << a.GetName() << std::endl;
    }
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) return std::nullopt;

    int author_idx = std::stoi(str) - 1;
    if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) return std::nullopt;

    return authors[author_idx].GetId();
}

std::optional<app::BookFullInfo> View::SelectBook(std::istream& cmd_input) const {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    std::vector<app::BookFullInfo> candidates;
    if (!title.empty()) {
        candidates = use_cases_.FindBooksByTitle(title);
        if (candidates.empty()) return std::nullopt;
        if (candidates.size() == 1) return candidates[0];
    } else {
        candidates = use_cases_.GetBooks();
        if (candidates.empty()) return std::nullopt;
    }

    int idx = 1;
    for (const auto& b : candidates) {
        output_ << idx++ << " " << b.title << " by " << b.author_name << ", " << b.publication_year << std::endl;
    }
    output_ << "Enter the book # or empty line to cancel:" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) return std::nullopt;

    int book_idx = std::stoi(str) - 1;
    if (book_idx < 0 || book_idx >= static_cast<int>(candidates.size())) return std::nullopt;

    return candidates[book_idx];
}

}  // namespace ui