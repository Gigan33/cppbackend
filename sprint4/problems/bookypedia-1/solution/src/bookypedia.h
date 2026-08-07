#pragma once

#include <iosfwd>

#include "app/use_cases_impl.h"
#include "postgres/postgres.h"

namespace bookypedia {

class Application {
public:
    explicit Application(pqxx::connection conn)
        : db_{std::move(conn)} {
    }

    void Run(std::istream& input, std::ostream& output);

private:
    postgres::Database db_;
    app::UseCasesImpl use_cases_{db_.GetAuthors(), db_.GetBooks()};
};

}  // namespace bookypedia