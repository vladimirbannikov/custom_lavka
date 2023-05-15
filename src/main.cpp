#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>


#include "lavka.h"

#include "couriers/CouriersHandler.h"
#include "couriers/CouriersIDHandler.h"

#include "orders/OrdersHandler.h"
#include "orders/OrdersIDHandler.h"
#include "orders/OrdersCompleteHandler.h"

#include "couriers/CouriersMetaInfoHandler.h"

int main(int argc, char* argv[]) {
  auto component_list = userver::components::MinimalServerComponentList()
                            .Append<userver::server::handlers::Ping>()
                            .Append<userver::components::TestsuiteSupport>()
                            .Append<userver::components::HttpClient>()
                            .Append<userver::server::handlers::TestsControl>();

  lavka::AppendLavka(component_list);

  lavka::AppendCouriers(component_list);
  lavka::AppendCouriersID(component_list);

  lavka::AppendOrders(component_list);
  lavka::AppendOrdersID(component_list);
  lavka::AppendOrdersComplete(component_list);

  lavka::AppendCouriersMetaInfo(component_list);

  return userver::utils::DaemonMain(argc, argv, component_list);
}
