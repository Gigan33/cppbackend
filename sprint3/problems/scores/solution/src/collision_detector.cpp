#include "collision_detector.h"

#include <algorithm>
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> detected_events;

    const size_t items_count = provider.ItemsCount();
    const size_t gatherers_count = provider.GatherersCount();

    for (size_t g = 0; g < gatherers_count; ++g) {
        Gatherer gatherer = provider.GetGatherer(g);

        if (gatherer.start_pos.x == gatherer.end_pos.x && 
            gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        for (size_t i = 0; i < items_count; ++i) {
            Item item = provider.GetItem(i);

            auto result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            if (result.IsCollected(item.width + gatherer.width)) {
                detected_events.push_back(GatheringEvent{
                    .item_id = i,
                    .gatherer_id = g,
                    .sq_distance = result.sq_distance,
                    .time = result.proj_ratio
                });
            }
        }
    }

    std::sort(detected_events.begin(), detected_events.end(),
              [](const GatheringEvent& e1, const GatheringEvent& e2) {
                  return e1.time < e2.time;
              });

    return detected_events;
}

}  // namespace collision_detector