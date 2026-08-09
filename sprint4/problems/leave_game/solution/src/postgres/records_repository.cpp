#include "records_repository.h"

namespace postgres {

using pqxx::operator"" _zv;

void RecordsRepositoryImpl::EnsureSchema() {
    auto conn = pool_->Acquire();
    pqxx::work work{*conn};

    work.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    score INTEGER NOT NULL,
    play_time DOUBLE PRECISION NOT NULL
);
)"_zv);

    work.exec(R"(
CREATE INDEX IF NOT EXISTS retired_players_sort_idx
    ON retired_players (score DESC, play_time ASC, name ASC);
)"_zv);

    work.commit();
}

void RecordsRepositoryImpl::Save(const model::RetiredPlayerRecord& record) {
    auto conn = pool_->Acquire();
    pqxx::work work{*conn};
    work.exec_params(
        "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);"_zv,
        record.name, record.score, record.play_time
    );
    work.commit();
}

std::vector<model::RetiredPlayerRecord> RecordsRepositoryImpl::GetRecords(int start, int max_items) {
    auto conn = pool_->Acquire();
    pqxx::read_transaction work{*conn};

    auto result = work.exec_params(R"(
SELECT name, score, play_time FROM retired_players
ORDER BY score DESC, play_time ASC, name ASC
LIMIT $1 OFFSET $2;
)"_zv, max_items, start);

    std::vector<model::RetiredPlayerRecord> records;
    for (const auto& row : result) {
        records.push_back(model::RetiredPlayerRecord{
            row["name"].as<std::string>(),
            row["score"].as<int>(),
            row["play_time"].as<double>()
        });
    }
    return records;
}

}  // namespace postgres