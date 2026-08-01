#include "model.h"
#include "collision_detector.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace model {

namespace {
constexpr double ROAD_HALF_WIDTH = 0.4;

enum class ProviderItemType { Item, Office };

struct ProviderItem {
    ProviderItemType type;
    size_t id;             
    geom::Point2D position;
    double width;
};

class GameItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    GameItemGathererProvider(
        std::vector<ProviderItem> items,
        std::vector<collision_detector::Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override { return items_.size(); }
    collision_detector::Item GetItem(size_t idx) const override {
        return {items_[idx].position, items_[idx].width};
    }

    size_t GatherersCount() const override { return gatherers_.size(); }
    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

    const ProviderItem& GetInternalItem(size_t idx) const { return items_[idx]; }

private:
    std::vector<ProviderItem> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

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
    }
}

std::pair<Token, uint32_t> Game::JoinGame(const std::string& map_id, const std::string& user_name) {
    auto map_ptr = FindMap(Map::Id{map_id});
    if (!map_ptr) {
        throw std::invalid_argument("mapNotFound");
    }
    if (user_name.empty()) {
        throw std::invalid_argument("emptyName");
    }

    auto session = FindOrCreateSession(map_ptr);
    auto dog = session->CreateDog(user_name, randomize_spawn_points_);
    
    session->GenerateLoot(0.0); 

    uint32_t player_id = next_player_id_++;
    auto player = std::make_shared<Player>(player_id, session, dog);
    players_.push_back(player);

    Token token = tokens_.AddPlayer(player);
    return {token, player_id};
}

GameSession::GameSession(const Map* map, const LootGeneratorConfig& config)
    : map_(map) {
    
    auto base_interval = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::duration<double>(config.period)
    );
    loot_generator_ = std::make_unique<loot_gen::LootGenerator>(
        base_interval, 
        config.probability,
        []() { 
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<double> dist(0.0, 1.0);
            return dist(gen); 
        }
    );
}

std::shared_ptr<Dog> GameSession::CreateDog(const std::string& name, bool randomize_spawn_points) {
    Point2D spawn_pos = randomize_spawn_points ? GetRandomPosition() : Point2D{0.0, 0.0};
    uint32_t dog_id = next_dog_id_++;
    auto dog = std::make_shared<Dog>(dog_id, name, spawn_pos);

    if (map_) {
        dog->SetBagCapacity(map_->GetBagCapacity());
    }
    
    dogs_.push_back(dog);
    return dog;
}

Point2D GameSession::GetRandomPosition() {
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

void GameSession::GenerateAndAddLootItem() {
    if (!map_) {
        return;
    }

    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return;
    }

    size_t types_count = map_->GetLootTypes().empty() 
                         ? map_->GetLootTypesCount() 
                         : map_->GetLootTypes().size();

    if (types_count == 0) {
        return;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    std::uniform_int_distribution<unsigned int> type_dist(0, static_cast<unsigned int>(types_count - 1));

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

void GameSession::Tick(double dt) {
    std::vector<collision_detector::Gatherer> gatherers;
    std::vector<Point2D> start_positions;

    for (auto& dog : dogs_) {
        if (!dog) continue;
        
        start_positions.push_back(dog->GetPosition());
        dog->UpdatePosition(dt, map_);

        gatherers.push_back({
            {start_positions.back().x, start_positions.back().y},
            {dog->GetPosition().x, dog->GetPosition().y},
            0.6
        });
    }

    std::vector<ProviderItem> provider_items;

    for (const auto& [id, lost_obj] : lost_objects_) {
        provider_items.push_back({
            ProviderItemType::Item,
            id,
            {lost_obj.position.x, lost_obj.position.y},
            0.0
        });
    }

    if (map_) {
        const auto& offices = map_->GetOffices();
        for (size_t i = 0; i < offices.size(); ++i) {
            const auto& office = offices[i];
            provider_items.push_back({
                ProviderItemType::Office,
                i,
                {static_cast<double>(office.GetPosition().x), static_cast<double>(office.GetPosition().y)},
                0.5
            });
        }
    }

    GameItemGathererProvider provider(std::move(provider_items), std::move(gatherers));
    auto events = collision_detector::FindGatherEvents(provider);

    std::unordered_set<size_t> picked_items;

    for (const auto& event : events) {
        auto& dog = dogs_[event.gatherer_id];
        const auto& item_info = provider.GetInternalItem(event.item_id);

        if (item_info.type == ProviderItemType::Item) {
            if (picked_items.count(item_info.id) == 0 && !dog->IsBagFull()) {
                if (auto it = lost_objects_.find(item_info.id); it != lost_objects_.end()) {
                    dog->PushToBag({it->second.id, it->second.type});
                    picked_items.insert(item_info.id);
                }
            }
        } else if (item_info.type == ProviderItemType::Office) {
            if (!dog->GetBag().empty()) {
                int score_to_add = 0;
                for (const auto& item : dog->GetBag()) {
                    score_to_add += map_->GetLootValue(item.type);
                }
                dog->AddScore(score_to_add);
                dog->ClearBag();
            }
        }
    }

    for (size_t id : picked_items) {
        RemoveLostObject(id);
    }

    if (loot_generator_) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(dt)
        );

        unsigned count = loot_generator_->Generate(
            duration, 
            lost_objects_.size(), 
            dogs_.size()
        );

        for (unsigned i = 0; i < count; ++i) {
            GenerateAndAddLootItem();
        }
    }
}

void GameSession::GenerateLoot(double dt) {
    if (!loot_generator_) {
        return;
    }

    auto time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::duration<double>(dt)
    );

    unsigned int looter_count = static_cast<unsigned int>(dogs_.size());
    unsigned int current_loot_count = static_cast<unsigned int>(lost_objects_.size());

    unsigned int loot_to_generate = loot_generator_->Generate(time_delta, current_loot_count, looter_count);

    for (unsigned int i = 0; i < loot_to_generate; ++i) {
        GenerateAndAddLootItem();
    }
}

}  // namespace model