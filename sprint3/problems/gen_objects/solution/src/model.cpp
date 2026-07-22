#include "model.h"
#include <algorithm>
#include <random>

namespace model {

namespace {
constexpr double ROAD_HALF_WIDTH = 0.4;
} // namespace

struct RoadBounds {
    double min_x, max_x;
    double min_y, max_y;
};

RoadBounds GetRoadBounds(const Road& road) {
    auto start = road.GetStart();
    auto end = road.GetEnd();
    double min_x = std::min(start.x, end.x) - ROAD_HALF_WIDTH;
    double max_x = std::max(start.x, end.x) + ROAD_HALF_WIDTH;
    double min_y = std::min(start.y, end.y) - ROAD_HALF_WIDTH;
    double max_y = std::max(start.y, end.y) + ROAD_HALF_WIDTH;
    
    return {min_x, max_x, min_y, max_y};
}

bool IsPointOnRoad(const Point2D& p, const Road& road) {
    auto bounds = GetRoadBounds(road);
    return p.x >= bounds.min_x && p.x <= bounds.max_x &&
           p.y >= bounds.min_y && p.y <= bounds.max_y;
}

void Dog::UpdatePosition(double dt, const Map* map) {
    if (!map || (speed_.ux == 0.0 && speed_.uy == 0.0)) {
        return; 
    }

    Point2D next_pos;
    next_pos.x = position_.x + speed_.ux * dt;
    next_pos.y = position_.y + speed_.uy * dt;

    std::vector<const Road*> current_roads;
    for (const auto& road : map->GetRoads()) {
        if (IsPointOnRoad(position_, road)) {
            current_roads.push_back(&road);
        }
    }

    bool remains_on_road = false;
    for (const auto* road : current_roads) {
        if (IsPointOnRoad(next_pos, *road)) {
            remains_on_road = true;
            break;
        }
    }

    if (!remains_on_road) {
        for (const auto& road : map->GetRoads()) {
            if (IsPointOnRoad(next_pos, road)) {
                remains_on_road = true;
                break;
            }
        }
    }

    if (remains_on_road) {
        position_ = next_pos;
        return;
    }

    double max_allowed_x = next_pos.x;
    double max_allowed_y = next_pos.y;

    if (speed_.ux > 0) {
        double limit = -1e9;
        for (const auto* road : current_roads) {
            limit = std::max(limit, GetRoadBounds(*road).max_x);
        }
        max_allowed_x = std::min(next_pos.x, limit);
        speed_.ux = 0.0;
    } 
    else if (speed_.ux < 0) {
        double limit = 1e9;
        for (const auto* road : current_roads) {
            limit = std::min(limit, GetRoadBounds(*road).min_x);
        }
        max_allowed_x = std::max(next_pos.x, limit);
        speed_.ux = 0.0;
    } 
    else if (speed_.uy > 0) {
        double limit = -1e9;
        for (const auto* road : current_roads) {
            limit = std::max(limit, GetRoadBounds(*road).max_y);
        }
        max_allowed_y = std::min(next_pos.y, limit);
        speed_.uy = 0.0;
    } 
    else if (speed_.uy < 0) {
        double limit = 1e9;
        for (const auto* road : current_roads) {
            limit = std::min(limit, GetRoadBounds(*road).min_y);
        }
        max_allowed_y = std::max(next_pos.y, limit);
        speed_.uy = 0.0;
    }

    position_.x = max_allowed_x;
    position_.y = max_allowed_y;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.count(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse id");
    }
    const size_t index = offices_.size();
    auto& o = offices_.emplace_back(std::move(office));
    warehouse_id_to_index_[o.GetId()] = index;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id " + *map.GetId() + " already exists");
    }
    maps_.emplace_back(std::move(map));
}

void Game::Tick(double dt) {
    for (auto& session : sessions_) {
        session->Tick(dt);
        if (loot_generator_) {
            session->GenerateLoot(dt, *loot_generator_);
        }
    }
}

model::Point2D model::GameSession::GetRandomPosition() {
    if (map_->GetRoads().empty()) {
        return {0.0, 0.0};
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> road_dist(0, map_->GetRoads().size() - 1);
    const auto& road = map_->GetRoads().at(road_dist(gen));

    auto start = road.GetStart();
    auto end = road.GetEnd();

    double x = 0.0;
    double y = 0.0;

    if (road.IsHorizontal()) {
        auto [min_x, max_x] = std::minmax(start.x, end.x);
        std::uniform_real_distribution<double> x_dist(static_cast<double>(min_x), static_cast<double>(max_x));
        x = x_dist(gen);
        y = static_cast<double>(start.y);
    } else {
        auto [min_y, max_y] = std::minmax(start.y, end.y);
        std::uniform_real_distribution<double> y_dist(static_cast<double>(min_y), static_cast<double>(max_y));
        x = static_cast<double>(start.x);
        y = y_dist(gen);
    }

    return {x, y};
}

void GameSession::Tick(double dt) {
    for (auto& [id, dog] : dogs_) {
        if (dog) {
            dog->Move(dt, *map_);
        }
    }
}

void GameSession::GenerateLoot(double dt, loot_gen::LootGenerator& generator) {
    auto time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::duration<double>(dt)
    );

    unsigned int looter_count = dogs_.size();
    unsigned int current_loot_count = lost_objects_.size();

    unsigned int loot_to_generate = generator.Generate(time_delta, current_loot_count, looter_count);

    if (loot_to_generate == 0) {
        return;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());

    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return;
    }

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    size_t types_count = map_->GetLootTypesCount();
    std::uniform_int_distribution<unsigned int> type_dist(0, types_count > 0 ? types_count - 1 : 0);

    for (unsigned int i = 0; i < loot_to_generate; ++i) {
        const auto& road = roads[road_dist(gen)];
        unsigned int loot_type = type_dist(gen);

        double x = 0.0;
        double y = 0.0;

        Point start = road.GetStart();
        Point end = road.GetEnd();

        if (road.IsHorizontal()) {
            double min_x = std::min(start.x, end.x);
            double max_x = std::max(start.x, end.x);
            std::uniform_real_distribution<double> x_dist(min_x, max_x);
            x = x_dist(gen);
            y = static_cast<double>(start.y);
        } else {
            double min_y = std::min(start.y, end.y);
            double max_y = std::max(start.y, end.y);
            std::uniform_real_distribution<double> y_dist(min_y, max_y);
            x = static_cast<double>(start.x);
            y = y_dist(gen);
        }

        unsigned int obj_id = next_loot_id_++;
        lost_objects_[obj_id] = LostObject{obj_id, loot_type, {x, y}};
    }
}

}  // namespace model