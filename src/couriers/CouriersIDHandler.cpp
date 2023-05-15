
#include "CouriersIDHandler.h"

namespace lavka {

namespace {
class CouriersIdHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-couriers-id";
  userver::storages::postgres::ClusterPtr pg_cluster_;

  CouriersIdHandler(
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
        return GetSpecificCourier(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  const userver::storages::postgres::Query kSelectSpecificCourier{
      "SELECT * from service_schema.couriers WHERE courier_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_courier"},
  };

  std::string GetSpecificCourier(
      const userver::server::http::HttpRequest& request) const {
    if(request.ArgCount() > 0) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    auto id_str = request.GetPathArg("courier_id");

    int64_t courier_id;

    try {
      courier_id = std::stoi(id_str);
    } catch (...) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    userver::storages::postgres::ResultSet res = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        kSelectSpecificCourier, courier_id);

    if (res.IsEmpty()) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
      return {};
    }

    auto resValue =
        res.AsSingleRow<CourierDto>(userver::storages::postgres::kRowTag);

    userver::formats::json::ValueBuilder courierBuilder{resValue};
    if (courierBuilder.HasMember("completed_orders"))
      courierBuilder.Remove("completed_orders");
    auto courierJson = courierBuilder.ExtractValue();

    return userver::formats::json::ToStableString(courierJson);
  }
};
}  // namespace

void AppendCouriersID(userver::components::ComponentList& component_list){
  component_list.Append<CouriersIdHandler>();
}

}  // namespace lavka