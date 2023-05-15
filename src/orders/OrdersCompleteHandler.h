#ifndef LAVKA_ORDERSCOMPLETEHANDLER_H
#define LAVKA_ORDERSCOMPLETEHANDLER_H

#include "OrdersHandler.h"
#include "../couriers/CouriersHandler.h"


namespace lavka {

struct OrderCompleteDto {
  int64_t courier_id;
  int64_t order_id;
  std::string complete_time;
};

void AppendOrdersComplete(userver::components::ComponentList& component_list);

}  // namespace lavka

#endif  // LAVKA_ORDERSCOMPLETEHANDLER_H
