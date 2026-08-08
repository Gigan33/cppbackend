#pragma once

#include "connection_pool.h"
#include "../records.h"

namespace postgres {

class RecordsRepositoryImpl : public records::Repository {
public:
    explicit RecordsRepositoryImpl(std::shared_ptr<ConnectionPool> pool)
        : pool_{std::move(pool)} {
        EnsureSchema();
    }

    void Save(const model::RetiredPlayerRecord& record) override;
    std::vector<model::RetiredPlayerRecord> GetRecords(int start, int max_items) override;

private:
    void EnsureSchema();
    std::shared_ptr<ConnectionPool> pool_;
};

}  // namespace postgres