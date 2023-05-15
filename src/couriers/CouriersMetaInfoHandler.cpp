#include "CouriersMetaInfoHandler.h"

namespace lavka {

namespace {

class CouriersMetaInfoHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-couriers-meta-info";
  userver::storages::postgres::ClusterPtr pg_cluster_;

  CouriersMetaInfoHandler(
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
        return GetCouriersMetaInfo(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  const userver::storages::postgres::Query SelectIsInDateInterval{
      "SELECT service_schema.IsInDateInterval"
      "(CAST($1 as TIMESTAMP),CAST($2 as TIMESTAMP),CAST($3 as TIMESTAMP)) as result",
      userver::storages::postgres::Query::Name{"select_is_in_date_interval"},
  };

  const userver::storages::postgres::Query kSelectSpecificCourier{
      "SELECT * from service_schema.couriers WHERE courier_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_courier"},
  };

  const userver::storages::postgres::Query kSelectSpecificOrder{
      "SELECT order_id, CAST(weight as FLOAT) as weight, regions, "
      "delivery_hours, cost, "
      "CAST(complete_time as TEXT) as complete_time from service_schema.orders "
      "WHERE order_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_order"},
  };

  const userver::storages::postgres::Query kSelectTimeDiffInSeconds{
      "SELECT service_schema.CalculateTimestampDiffInSeconds"
      "(CAST($1 as TIMESTAMP),CAST($2 as TIMESTAMP)) as result",
      userver::storages::postgres::Query::Name{"select_time_diff_in_seconds"},
  };

  std::string GetCouriersMetaInfo(
      const userver::server::http::HttpRequest& request) const {
    if (request.ArgCount() != 2) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    std::string startDate, endDate;
    int realArgCount = 0;
    try {
      if (request.HasArg("startDate")) {
        startDate = request.GetArg("startDate");
        ++realArgCount;
      }
      if (request.HasArg("endDate")) {
        ++realArgCount;
        endDate = request.GetArg("endDate");
      }
    } catch (...) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }
    if (request.ArgCount() > realArgCount) {
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
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }
    auto courierValue =
        res.AsSingleRow<CourierDto>(userver::storages::postgres::kRowTag);

    int earnings = 0;
    int rating = 0;
    int completedOrdersCount = 0;

    res = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        kSelectTimeDiffInSeconds, startDate, endDate);

    int timeDiffInSeconds = res.AsSingleRow<int>();

    if (timeDiffInSeconds > 0 && courierValue.completed_orders.has_value()) {
      std::vector<int64_t> completeOrders =
          courierValue.completed_orders.value();

      for (auto order_id : completeOrders) {
        userver::storages::postgres::ResultSet orderRes = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            kSelectSpecificOrder, order_id);

        auto orderValue = orderRes.AsSingleRow<OrderDto>(
            userver::storages::postgres::kRowTag);

        res = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            SelectIsInDateInterval, startDate, endDate, orderValue.complete_time.value());

        bool completeTimeIsInInterval = res.AsSingleRow<bool>();

        if (completeTimeIsInInterval) {
          ++completedOrdersCount;
          earnings += orderValue.cost;
        }
      }
    }

    userver::formats::json::ValueBuilder responseJson{courierValue};
    if (responseJson.HasMember("completed_orders")) {
      responseJson.Remove("completed_orders");
    }

    if (completedOrdersCount != 0) {
      int C = 0;
      if (courierValue.courier_type == courierType::foot) C = 2;
      if (courierValue.courier_type == courierType::bike) C = 3;
      if (courierValue.courier_type == courierType::_auto) C = 4;
      earnings *= C;
      responseJson["earnings"] = earnings;

      C = 0;
      if (courierValue.courier_type == courierType::foot) C = 3;
      if (courierValue.courier_type == courierType::bike) C = 2;
      if (courierValue.courier_type == courierType::_auto) C = 1;
      rating = (completedOrdersCount / (timeDiffInSeconds / 3600)) * C;
      responseJson["rating"] = rating;
    }

    return userver::formats::json::ToStableString(responseJson.ExtractValue());
  }
};

}  // namespace

void AppendCouriersMetaInfo(userver::components::ComponentList& component_list){
  component_list.Append<CouriersMetaInfoHandler>();
}

}  // namespace lavka