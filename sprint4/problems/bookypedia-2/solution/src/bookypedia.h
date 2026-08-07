#pragma once

#include <pqxx/pqxx>

#include "app/use_cases_impl.h"
#include "postgres/postgres.h"
#include "ui/view.h"

namespace bookypedia {

class Application {
public:
    explicit Application(const std::string& db_url)
        : db_{postgres::Database{pqxx::connection{db_url}}}
        , use_cases_{db_.GetAuthors(), db_.GetBooks()}
        , view_{use_cases_} {}

    void Run() {
        view_.Run();
    }

private:
    postgres::Database db_;
    app::UseCasesImpl use_cases_;
    ui::View view_;
};

}  // namespace bookypedia