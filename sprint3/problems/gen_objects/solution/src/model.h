#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string_view>

#include "loot_generator.h"
#include "tagged.h"
#include <map>

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Speed2D {
    double ux = 0.0;
    double uy = 0.0;
};

struct LostObject {
    unsigned int id = 0;
    unsigned int type = 0;
    Point2D position;
};

struct LootGeneratorConfig {
    double period = 0.0;
    double probability = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void SetLootTypesCount(size_t count) noexcept {
        loot_types_count_ = count;
    }

    size_t GetLootTypesCount() const noexcept {
        return loot_types_count_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double dog_speed_ = 0.0;
    size_t loot_types_count_ = 0;
};

class Dog {
public:
    Dog(std::string name, uint32_t id, Point2D position)
        : name_(std::move(name))
        , id_(id)
        , position_(position)
        , speed_({0.0, 0.0})
        , direction_(Direction::NORTH) {}

    uint32_t GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Point2D& GetPosition() const noexcept { return position_; }
    const Speed2D& GetSpeed() const noexcept { return speed_; }
    
    std::string GetDirectionString() const noexcept {
        switch (direction_) {
            case Direction::NORTH: return "U";
            case Direction::SOUTH: return "D";
            case Direction::WEST:  return "L";
            case Direction::EAST:  return "R";
        }
        return "U";
    }

    void Move(std::string_view action, double speed) {
        if (action == "L") {
            speed_ = {-speed, 0.0};
            direction_ = Direction::WEST;
        } else if (action == "R") {
            speed_ = {speed, 0.0};
            direction_ = Direction::EAST;
        } else if (action == "U") {
            speed_ = {0.0, -speed};
            direction_ = Direction::NORTH;
        } else if (action == "D") {
            speed_ = {0.0, speed};
            direction_ = Direction::SOUTH;
        } else if (action == "") {
            speed_ = {0.0, 0.0};
        }
    }

    void UpdatePosition(double dt, const Map* map);

private:
    std::string name_;
    uint32_t id_;
    Point2D position_;
    Speed2D speed_;
    Direction direction_;
};

class GameSession {
public:
    explicit GameSession(const Map* map) : map_(map) {}
    const Map::Id& GetMapId() const noexcept { return map_->GetId(); }
    const std::vector<std::shared_ptr<Dog>>& GetDogs() const noexcept { return dogs_; }
    const std::map<unsigned int, LostObject>& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    void GenerateLoot(double dt, loot_gen::LootGenerator& generator);

    std::shared_ptr<Dog> CreateDog(const std::string& dog_name, bool randomize_spawn) {
        uint32_t dog_id = next_dog_id_++;
        
        Point2D spawn_pos{0.0, 0.0};
        if (!map_->GetRoads().empty()) {
            if (randomize_spawn) {
                spawn_pos = GetRandomPosition();
            } else {
                const auto& first_road = map_->GetRoads().front();
                spawn_pos.x = static_cast<double>(first_road.GetStart().x);
                spawn_pos.y = static_cast<double>(first_road.GetStart().y);
            }
        }
        
        auto dog = std::make_shared<Dog>(dog_name, dog_id, spawn_pos);
        dogs_.push_back(dog);
        return dog;
    }

    void Tick(double dt);
}

private:
    Point2D GetRandomPosition();

    const Map* map_;
    std::vector<std::shared_ptr<Dog>> dogs_;
    uint32_t next_dog_id_ = 0;
    std::map<unsigned int, LostObject> lost_objects_;
    unsigned int next_loot_id_ = 0;
};

class Player {
public:
    Player(uint32_t id, std::shared_ptr<GameSession> session, std::shared_ptr<Dog> dog)
        : id_(id), session_(std::move(session)), dog_(std::move(dog)) {}

    uint32_t GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return dog_->GetName(); }
    std::shared_ptr<GameSession> GetSession() const noexcept { return session_; }
    Dog& GetDog() const noexcept { return *dog_; }

private:
    uint32_t id_;
    std::shared_ptr<GameSession> session_;
    std::shared_ptr<Dog> dog_;
};

namespace detail {
    struct TokenTag {};
} // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    PlayerTokens() : random_device_(std::make_unique<std::random_device>()) {}
    
    PlayerTokens(PlayerTokens&& other) noexcept = default;
    PlayerTokens& operator=(PlayerTokens&& other) noexcept = default;

    PlayerTokens(const PlayerTokens&) = delete;
    PlayerTokens& operator=(const PlayerTokens&) = delete;

    Token AddPlayer(std::shared_ptr<Player> player) {
        std::string token_str = GenerateToken();
        Token token{token_str};
        token_to_player_[token] = std::move(player);
        return token;
    }

    std::shared_ptr<Player> FindPlayerByToken(const Token& token) const {
        auto it = token_to_player_.find(token);
        return (it != token_to_player_.end()) ? it->second : nullptr;
    }

private:
    std::string GenerateToken() {
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << part1 << std::setw(16) << part2;
        return ss.str();
    }

    std::unique_ptr<std::random_device> random_device_;
    
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(*random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(*random_device_);
    }()};

    std::unordered_map<Token, std::shared_ptr<Player>, util::TaggedHasher<Token>> token_to_player_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    Game() = default;
    Game(Game&& other) noexcept = default;
    Game& operator=(Game&& other) noexcept = default;

    void SetDefaultDogSpeed(double speed) noexcept {
        default_dog_speed_ = speed;
    }

    double GetDefaultDogSpeed() const noexcept {
        return default_dog_speed_;
    }

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        auto it = map_id_to_index_.find(id);
        return (it != map_id_to_index_.end()) ? &maps_.at(it->second) : nullptr;
    }

    void SetRandomizeSpawnPoints(bool randomize) noexcept {
        randomize_spawn_points_ = randomize;
    }

    bool IsRandomizeSpawnPoints() const noexcept {
        return randomize_spawn_points_;
    }

    std::pair<Token, uint32_t> JoinGame(const std::string& map_id, const std::string& user_name) {
        auto map_ptr = FindMap(Map::Id{map_id});
        if (!map_ptr) {
            throw std::invalid_argument("mapNotFound");
        }
        if (user_name.empty()) {
            throw std::invalid_argument("emptyName");
        }

        auto session = FindOrCreateSession(map_ptr);

        auto dog = session->CreateDog(user_name, randomize_spawn_points_);

        uint32_t player_id = next_player_id_++;
        auto player = std::make_shared<Player>(player_id, session, dog);
        players_.push_back(player);

        Token token = tokens_.AddPlayer(player);
        return {token, player_id};
    }

    const PlayerTokens& GetTokens() const noexcept { return tokens_; }

    std::shared_ptr<Player> FindPlayerByToken(const Token& token) const {
        return tokens_.FindPlayerByToken(token);
    }

    const std::vector<std::shared_ptr<Player>>& GetPlayers() const noexcept { return players_; }

    void Tick(double dt) {
        for (auto& session : sessions_) {
            session->Tick(dt);
        }
    }

    void SetLootGeneratorConfig(LootGeneratorConfig config) {
        loot_config_ = config;
        
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

    void SetMapLootJson(const Map::Id& id, std::string json_str) {
        map_loot_json_[id] = std::move(json_str);
    }

    const std::string& GetMapLootJson(const Map::Id& id) const {
        static const std::string empty_str = "[]";
        auto it = map_loot_json_.find(id);
        return (it != map_loot_json_.end()) ? it->second : empty_str;
    }

private:
    std::shared_ptr<GameSession> FindOrCreateSession(const Map* map) {
        if (auto it = map_id_to_session_.find(map->GetId()); it != map_id_to_session_.end()) {
            return it->second;
        }
        auto session = std::make_shared<GameSession>(map);
        sessions_.push_back(session);
        map_id_to_session_[map->GetId()] = session;
        return session;
    }

    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    uint32_t next_player_id_ = 0;
    std::vector<std::shared_ptr<GameSession>> sessions_;
    std::vector<std::shared_ptr<Player>> players_;
    std::unordered_map<Map::Id, std::shared_ptr<GameSession>, util::TaggedHasher<Map::Id>> map_id_to_session_;
    PlayerTokens tokens_;

    double default_dog_speed_ = 1.0;
    bool randomize_spawn_points_ = false;

    LootGeneratorConfig loot_config_;
    std::unique_ptr<loot_gen::LootGenerator> loot_generator_;
    std::unordered_map<Map::Id, std::string, util::TaggedHasher<Map::Id>> map_loot_json_;
};

}  // namespace model