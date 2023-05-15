#ifndef LAVKA_ORDERSASSIGNHANDLER_H
#define LAVKA_ORDERSASSIGNHANDLER_H
#include "OrdersHandler.h"
#include "../couriers/CouriersHandler.h"

struct CouriersDtoWithoutType{
  int64_t courier_id;
  std::vector<int> regions;
  std::vector<std::string> working_hours;
};

struct CourierIdAndTypeStruct{
  int64_t courier_id;
  std::string courier_type;
};

struct CourierIdStruct{
  int64_t courier_id;
};

struct OrderIdStruct{
  int64_t order_id;
};

void AppendOrdersAssign(userver::components::ComponentList& component_list);

#endif
