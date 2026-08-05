#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_map.hpp>

#include "model.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar & vec.x;
    ar & vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, FoundObject& obj, [[maybe_unused]] const unsigned version) {
    ar & obj.id;
    ar & obj.type;
}

}  // namespace model

namespace serialization {

class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog)
        : id_(*dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , bag_capacity_(dog.GetBagCapacity())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , score_(dog.GetScore())
        , bag_content_(dog.GetBagContent()) {
    }

    [[nodiscard]] model::Dog Restore() const {
        model::Dog dog{model::Dog::Id{id_}, name_, pos_, bag_capacity_};
        dog.SetSpeed(speed_);
        dog.SetDirection(direction_);
        dog.AddScore(score_);
        for (const auto& item : bag_content_) {
            if (!dog.PutToBag(item)) {
                throw std::runtime_error("Failed to put bag content");
            }
        }
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & name_;
        ar & pos_;
        ar & bag_capacity_;
        ar & speed_;
        ar & direction_;
        ar & score_;
        ar & bag_content_;
    }

private:
    uint32_t id_ = 0;
    std::string name_;
    geom::Point2D pos_;
    size_t bag_capacity_ = 0;
    geom::Vec2D speed_;
    model::Direction direction_ = model::Direction::NORTH;
    uint32_t score_ = 0;
    model::Dog::BagContent bag_content_;
};

class LostObjectRepr {
public:
    LostObjectRepr() = default;

    explicit LostObjectRepr(const model::LostObject& obj)
        : id_(obj.id)
        , type_(obj.type)
        , pos_(obj.pos) {
    }

    [[nodiscard]] model::LostObject Restore() const {
        return model::LostObject{id_, type_, pos_};
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & type_;
        ar & pos_;
    }

    uint32_t GetId() const { return id_; }
    unsigned int GetType() const { return type_; }
    const geom::Point2D& GetPos() const { return pos_; }

private:
    uint32_t id_ = 0;
    unsigned int type_ = 0;
    geom::Point2D pos_;
};

class SessionRepr {
public:
    SessionRepr() = default;

    explicit SessionRepr(const model::GameSession& session)
        : map_id_(*session.GetMap()->GetId()) {
        for (const auto& dog : session.GetDogs()) {
            dogs_.emplace_back(*dog);
        }
        for (const auto& [id, obj] : session.GetLostObjects()) {
            lost_objects_.emplace_back(obj);
        }
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & map_id_;
        ar & dogs_;
        ar & lost_objects_;
    }

    const std::string& GetMapId() const { return map_id_; }
    const std::vector<DogRepr>& GetDogs() const { return dogs_; }
    const std::vector<LostObjectRepr>& GetLostObjects() const { return lost_objects_; }

private:
    std::string map_id_;
    std::vector<DogRepr> dogs_;
    std::vector<LostObjectRepr> lost_objects_;
};

class PlayerRepr {
public:
    PlayerRepr() = default;

    PlayerRepr(uint32_t id, std::string name, std::string token, uint32_t dog_id, std::string map_id)
        : id_(id)
        , name_(std::move(name))
        , token_(std::move(token))
        , dog_id_(dog_id)
        , map_id_(std::move(map_id)) {}

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & id_;
        ar & name_;
        ar & token_;
        ar & dog_id_;
        ar & map_id_;
    }

    uint32_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    const std::string& GetToken() const { return token_; }
    uint32_t GetDogId() const { return dog_id_; }
    const std::string& GetMapId() const { return map_id_; }

private:
    uint32_t id_ = 0;
    std::string name_;
    std::string token_;
    uint32_t dog_id_ = 0;
    std::string map_id_;
};

class SavedState {
public:
    SavedState() = default;

    void AddSession(SessionRepr session) {
        sessions_.push_back(std::move(session));
    }

    void AddPlayer(PlayerRepr player) {
        players_.push_back(std::move(player));
    }

    const std::vector<SessionRepr>& GetSessions() const { return sessions_; }
    const std::vector<PlayerRepr>& GetPlayers() const { return players_; }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & sessions_;
        ar & players_;
    }

private:
    std::vector<SessionRepr> sessions_;
    std::vector<PlayerRepr> players_;
};

}  // namespace serialization