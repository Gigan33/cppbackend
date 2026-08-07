#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "../app/use_cases.h"

namespace menu {
class Menu;
}

namespace ui {

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output);

private:
    bool AddAuthor(std::istream& cmd_input) const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool ShowAuthors() const;

    bool AddBook(std::istream& cmd_input) const;
    bool DeleteBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;
    bool ShowBooks() const;
    bool ShowBook(std::istream& cmd_input) const;
    bool ShowAuthorBooks(std::istream& cmd_input) const;

    std::optional<domain::AuthorId> SelectAuthor() const;
    std::optional<app::BookFullInfo> SelectBook(std::istream& cmd_input) const;

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui