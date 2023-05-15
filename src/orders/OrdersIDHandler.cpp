#include "OrdersIDHandler.h"

namespace lavka {

namespace {
class OrdersIdHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-orders-id";
  userver::storages::postgres::ClusterPtr pg_cluster_;

  OrdersIdHandler(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("postgres-db-1")
                .GetCluster()){};

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext&) const override {
    switch (request.GetMethod()) {
      case userver::server::http::HttpMethod::kGet:
        return GetSpecificOrder(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  const userver::storages::postgres::Query kSelectSpecificOrder{
      "SELECT order_id, CAST(weight as FLOAT) as weight, regions, delivery_hours, cost, "
      "CAST(complete_time as TEXT) as complete_time from service_schema.orders "
      "WHERE order_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_order"},
  };

  std::string GetSpecificOrder(
      const userver::server::http::HttpRequest& request) const {
    auto id_str = request.GetPathArg("order_id");

    if(request.ArgCount() > 0) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    int64_t order_id;

    try {
      order_id = std::stoi(id_str);
    } catch (...) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    userver::storages::postgres::ResultSet res = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        kSelectSpecificOrder, order_id);

    if (res.IsEmpty()) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
      return {};
    }

    auto resValue =
        res.AsSingleRow<OrderDto>(userver::storages::postgres::kRowTag);

    userver::formats::json::ValueBuilder orderBuilder{resValue};
    auto orderJson = orderBuilder.ExtractValue();

    return userver::formats::json::ToStableString(orderJson);
  }
};
}

void AppendOrdersID(userver::components::ComponentList& component_list) {
  component_list.Append<OrdersIdHandler>();
}

}  // namespace lavka