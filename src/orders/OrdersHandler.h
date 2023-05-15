#ifndef LAVKA_ORDERSHANDLER_H
#define LAVKA_ORDERSHANDLER_H

#include "../lavka.h"

namespace lavka {

struct OrderDto {
  int64_t order_id;
  double weight;
  int regions;
  std::vector<std::string> delivery_hours;
  int cost;
  std::optional<std::string> complete_time;
};


class OrderIdManager {
  static int64_t last_id_;
  static userver::engine::Mutex mutex_;

 public:
  static int64_t GetNewId();
};

userver::formats::json::Value Serialize(const OrderDto& data,
    userver::formats::serialize::To<userver::formats::json::Value>);

void AppendOrders(userver::components::ComponentList& component_list);

}  // namespace lavka

#endif
