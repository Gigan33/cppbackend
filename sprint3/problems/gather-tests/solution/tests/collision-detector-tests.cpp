#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <sstream>
#include <vector>

namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << "," 
            << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

namespace {

using namespace collision_detector;

// Тестовая реализация провайдера предметов и собирателей
class VectorItemGathererProvider : public ItemGathererProvider {
public:
    VectorItemGathererProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

// Вспомогательный оператор/компаратор для Catch2 матчеров с учётом погрешности 10^-10
constexpr double EPSILON = 1e-10;

bool IsEventEqual(const GatheringEvent& a, const GatheringEvent& b) {
    return a.item_id == b.item_id &&
           a.gatherer_id == b.gatherer_id &&
           std::abs(a.sq_distance - b.sq_distance) < EPSILON &&
           std::abs(a.time - b.time) < EPSILON;
}

// Кастомный кастом-матчер для сравнения векторов событий с плавающей точкой
class GatheringEventsMatcher : public Catch::Matchers::MatcherBase<std::vector<GatheringEvent>> {
public:
    explicit GatheringEventsMatcher(std::vector<GatheringEvent> expected)
        : expected_(std::move(expected)) {}

    bool match(const std::vector<GatheringEvent>& actual) const override {
        if (actual.size() != expected_.size()) {
            return false;
        }
        for (size_t i = 0; i < actual.size(); ++i) {
            if (!IsEventEqual(actual[i], expected_[i])) {
                return false;
            }
        }
        return true;
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "Equals vector of GatheringEvent with precision " << EPSILON << ":\n  { ";
        for (const auto& e : expected_) {
            ss << Catch::Detail::stringify(e) << " ";
        }
        ss << "}";
        return ss.str();
    }

private:
    std::vector<GatheringEvent> expected_;
};

inline GatheringEventsMatcher MatchesEvents(std::vector<GatheringEvent> expected) {
    return GatheringEventsMatcher(std::move(expected));
}

}  // namespace

SCENARIO("Collision detector tests", "[collision_detector]") {
    GIVEN("No items and no gatherers") {
        VectorItemGathererProvider provider({}, {});
        WHEN("FindGatherEvents is called") {
            auto events = FindGatherEvents(provider);
            THEN("No events are detected") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("Gatherer that does not move (start_pos == end_pos)") {
        std::vector<Item> items{
            { {0.0, 0.0}, 0.6 }
        };
        std::vector<Gatherer> gatherers{
            { {0.0, 0.0}, {0.0, 0.0}, 0.6 }
        };
        VectorItemGathererProvider provider(items, gatherers);

        WHEN("FindGatherEvents is called") {
            auto events = FindGatherEvents(provider);
            THEN("Zero movement means no collision") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("Gatherer moving along X axis and multiple items along the path") {
        std::vector<Item> items{
            { { 5.0, 0.0 }, 0.5 },  // item 0: ровно на пути, time = 0.5
            { { 2.0, 0.0 }, 0.5 },  // item 1: ровно на пути, time = 0.2
            { { 8.0, 0.0 }, 0.5 },  // item 2: ровно на пути, time = 0.8
            { { 5.0, 2.0 }, 0.5 }   // item 3: слишком далеко по оси Y (2.0 > 0.5 + 0.5)
        };
        std::vector<Gatherer> gatherers{
            { { 0.0, 0.0 }, { 10.0, 0.0 }, 0.5 }
        };
        VectorItemGathererProvider provider(items, gatherers);

        WHEN("FindGatherEvents is called") {
            auto events = FindGatherEvents(provider);

            THEN("Events are sorted chronologically and contain exact collision data") {
                std::vector<GatheringEvent> expected{
                    { /*item_id*/ 1, /*gatherer_id*/ 0, /*sq_distance*/ 0.0, /*time*/ 0.2 },
                    { /*item_id*/ 0, /*gatherer_id*/ 0, /*sq_distance*/ 0.0, /*time*/ 0.5 },
                    { /*item_id*/ 2, /*gatherer_id*/ 0, /*sq_distance*/ 0.0, /*time*/ 0.8 }
                };
                REQUIRE_THAT(events, MatchesEvents(expected));
            }
        }
    }

    GIVEN("Item on the edge of reach (distance == width1 + width2)") {
        std::vector<Item> items{
            { { 5.0, 1.0 }, 0.5 },   // Радиус предмета 0.5 + Радиус собирателя 0.5 = 1.0 (касание)
            { { 5.0, 1.01 }, 0.5 }   // Чуть дальше касания — промах
        };
        std::vector<Gatherer> gatherers{
            { { 0.0, 0.0 }, { 10.0, 0.0 }, 0.5 }
        };
        VectorItemGathererProvider provider(items, gatherers);

        WHEN("FindGatherEvents is called") {
            auto events = FindGatherEvents(provider);

            THEN("Only the item within or exactly on the edge is gathered") {
                std::vector<GatheringEvent> expected{
                    { /*item_id*/ 0, /*gatherer_id*/ 0, /*sq_distance*/ 1.0, /*time*/ 0.5 }
                };
                REQUIRE_THAT(events, MatchesEvents(expected));
            }
        }
    }

    GIVEN("Items behind start position or after end position") {
        std::vector<Item> items{
            { { -1.0, 0.0 }, 0.5 },  // До начала движения
            { { 11.0, 0.0 }, 0.5 }   // После окончания движения
        };
        std::vector<Gatherer> gatherers{
            { { 0.0, 0.0 }, { 10.0, 0.0 }, 0.5 }
        };
        VectorItemGathererProvider provider(items, gatherers);

        WHEN("FindGatherEvents is called") {
            auto events = FindGatherEvents(provider);

            THEN("No events are detected") {
                CHECK(events.empty());
            }
        }
    }

    GIVEN("Multiple gatherers crossing the same item at different times") {
        std::vector<Item> items{
            { { 5.0, 0.0 }, 0.5 }
        };
        std::vector<Gatherer> gatherers{
            { { 0.0, 0.0 }, { 10.0, 0.0 }, 0.5 },  // gatherer 0: время 0.5
            { { 0.0, 0.0 }, { 20.0, 0.0 }, 0.5 }   // gatherer 1: время 0.25
        };
        VectorItemGathererProvider provider(items, gatherers);

        WHEN("FindGatherEvents is called") {
            auto events = FindGatherEvents(provider);

            THEN("Both gatherers generate events sorted strictly by time") {
                std::vector<GatheringEvent> expected{
                    { /*item_id*/ 0, /*gatherer_id*/ 1, /*sq_distance*/ 0.0, /*time*/ 0.25 },
                    { /*item_id*/ 0, /*gatherer_id*/ 0, /*sq_distance*/ 0.0, /*time*/ 0.5 }
                };
                REQUIRE_THAT(events, MatchesEvents(expected));
            }
        }
    }
}