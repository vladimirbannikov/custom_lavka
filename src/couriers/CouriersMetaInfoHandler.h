#ifndef LAVKA_COURIERSMETAINFOHANDLER_H
#define LAVKA_COURIERSMETAINFOHANDLER_H

#include "CouriersHandler.h"
#include "../orders/OrdersHandler.h"

namespace lavka {

void AppendCouriersMetaInfo(userver::components::ComponentList& component_list);

}  // namespace lavka

#endif  // LAVKA_COURIERSMETAINFOHANDLER_H
