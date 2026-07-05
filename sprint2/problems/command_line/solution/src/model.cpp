#include "model.h"
#include <algorithm>
#include <random>

namespace model {

struct RoadBounds {
    double min_x, max_x;
    double min_y, max_y;
};

RoadBounds GetRoadBounds(const Road& road) {
    auto start = road.GetStart();
    auto end = road.GetEnd();
    
    double min_x = std::min(start.x, end.x) - 0.4;
    double max_x = std::max(start.x, end.x) + 0.4;
    double min_y = std::min(start.y, end.y) - 0.4;
    double max_y = std::max(start.y, end.y) + 0.4;
    
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

    if (speed_.ux > 0) { // Движение направо
        double limit = -1e9;
        for (const auto* road : current_roads) {
            limit = std::max(limit, GetRoadBounds(*road).max_x);
        }
        max_allowed_x = std::min(next_pos.x, limit);
        speed_.ux = 0.0;
    } 
    else if (speed_.ux < 0) { // Движение налево
        double limit = 1e9;
        for (const auto* road : current_roads) {
            limit = std::min(limit, GetRoadBounds(*road).min_x);
        }
        max_allowed_x = std::max(next_pos.x, limit);
        speed_.ux = 0.0;
    } 
    else if (speed_.uy > 0) { // Движение вниз
        double limit = -1e9;
        for (const auto* road : current_roads) {
            limit = std::max(limit, GetRoadBounds(*road).max_y);
        }
        max_allowed_y = std::min(next_pos.y, limit);
        speed_.uy = 0.0;
    } 
    else if (speed_.uy < 0) { // Движение вверх
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

}  // namespace model