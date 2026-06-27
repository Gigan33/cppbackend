#include "model.h"
#include <algorithm>

namespace model {

// Вспомогательная структура для удобного описания границ дороги
struct RoadBounds {
    double min_x, max_x;
    double min_y, max_y;
};

// Функция, которая вычисляет границы конкретной дороги с учетом ширины 0.4
RoadBounds GetRoadBounds(const Road& road) {
    auto start = road.GetStart();
    auto end = road.GetEnd();
    
    double min_x = std::min(start.x, end.x) - 0.4;
    double max_x = std::max(start.x, end.x) + 0.4;
    double min_y = std::min(start.y, end.y) - 0.4;
    double max_y = std::max(start.y, end.y) + 0.4;
    
    return {min_x, max_x, min_y, max_y};
}

// Проверка: находится ли точка внутри границ данной дороги
bool IsPointOnRoad(const Point2D& p, const Road& road) {
    auto bounds = GetRoadBounds(road);
    return p.x >= bounds.min_x && p.x <= bounds.max_x &&
           p.y >= bounds.min_y && p.y <= bounds.max_y;
}

void Dog::UpdatePosition(double dt, const Map* map) {
    if (!map || (speed_.ux == 0.0 && speed_.uy == 0.0)) {
        return; // Собака стоит или карты нет — считать нечего
    }

    // 1. Вычисляем гипотетическую следующую точку
    Point2D next_pos;
    next_pos.x = position_.x + speed_.ux * dt;
    next_pos.y = position_.y + speed_.uy * dt;

    // 2. Ищем все дороги, на которых собака находится ПРЯМО СЕЙЧАС
    std::vector<const Road*> current_roads;
    for (const auto& road : map->GetRoads()) {
        if (IsPointOnRoad(position_, road)) {
            current_roads.push_back(&road);
        }
    }

    // 3. Проверяем, останется ли собака на одной из текущих дорог в следующей точке
    bool remains_on_road = false;
    for (const auto* road : current_roads) {
        if (IsPointOnRoad(next_pos, road)) {
            remains_on_road = true;
            break;
        }
    }

    // 4. Если выходит за рамки текущих дорог — ищем вообще любую дорогу на карте,
    // которая подхватит её в новой точке
    if (!remains_on_road) {
        for (const auto& road : map->GetRoads()) {
            if (IsPointOnRoad(next_pos, road)) {
                remains_on_road = true;
                break;
            }
        }
    }

    // 5. Если дорога продолжается, просто обновляем позицию
    if (remains_on_road) {
        position_ = next_pos;
        return;
    }

    // 6. Если мы упёрлись в тупик — нужно найти максимально доступную точку 
    // на текущих дорогах вдоль вектора движения и обнулить скорость.
    double max_allowed_x = next_pos.x;
    double max_allowed_y = next_pos.y;

    if (speed_.ux > 0) { // Движение направо (EAST)
        double limit = -1e9;
        for (const auto* road : current_roads) {
            limit = std::max(limit, GetRoadBounds(*road).max_x);
        }
        max_allowed_x = std::min(next_pos.x, limit);
        speed_.ux = 0.0;
    } 
    else if (speed_.ux < 0) { // Движение налево (WEST)
        double limit = 1e9;
        for (const auto* road : current_roads) {
            limit = std::min(limit, GetRoadBounds(*road).min_x);
        }
        max_allowed_x = std::max(next_pos.x, limit);
        speed_.ux = 0.0;
    } 
    else if (speed_.uy > 0) { // Движение вниз (SOUTH)
        double limit = -1e9;
        for (const auto* road : current_roads) {
            limit = std::max(limit, GetRoadBounds(*road).max_y);
        }
        max_allowed_y = std::min(next_pos.y, limit);
        speed_.uy = 0.0;
    } 
    else if (speed_.uy < 0) { // Движение вверх (NORTH)
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

// Не забудь сохранить существующую реализацию метода AddOffice, если она была:
void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse id");
    }
    const size_t index = offices_.size();
    auto& o = offices_.emplace_back(std::move(office));
    warehouse_id_to_index_[o.GetId()] = index;
}

// И реализацию AddMap для класса Game:
void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id " + *map.GetId() + " already exists");
    }
    maps_.emplace_back(std::move(map));
}

}  // namespace model