#ifndef LAVKA_COURIERSHANDLER_H
#define LAVKA_COURIERSHANDLER_H

#include "../lavka.h"

namespace lavka {

struct CourierDto {
  int64_t courier_id;
  std::string courier_type;
  std::vector<int> regions;
  std::vector<std::string> working_hours;
  std::optional<std::vector<int64_t>> completed_orders;
};

namespace courierType {
inline constexpr std::string_view foot{"FOOT"};
inline constexpr std::string_view bike{"BIKE"};
inline constexpr std::string_view _auto{"AUTO"};
}  // namespace courierType

class CourierIdManager {
  static int64_t last_id_;
  static userver::engine::Mutex mutex_;

 public:
  static int64_t GetNewId();
};

userver::formats::json::Value Serialize(const CourierDto& data,
    userver::formats::serialize::To<userver::formats::json::Value>);


void AppendCouriers(userver::components::ComponentList& component_list);

}  // namespace lavka

#endif
