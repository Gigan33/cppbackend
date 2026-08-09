#pragma once

#include <vector>
#include "model.h"

namespace records {

class Repository {
public:
    virtual ~Repository() = default;
    virtual void Save(const model::RetiredPlayerRecord& record) = 0;
    virtual std::vector<model::RetiredPlayerRecord> GetRecords(int start, int max_items) = 0;
};

}  // namespace records