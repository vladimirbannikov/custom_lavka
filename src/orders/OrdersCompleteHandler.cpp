#include "OrdersCompleteHandler.h"

namespace lavka {

namespace {
class OrdersCompleteHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-orders-complete";
  userver::storages::postgres::ClusterPtr pg_cluster_;

  OrdersCompleteHandler(
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
      case userver::server::http::HttpMethod::kPost:
        return PostOrdersComplete(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  bool IsComplete(const std::vector<std::string>& courierWorkTime,
                  const std::vector<std::string>& orderDeliveryHours,
                  const std::string& completeTime) const {
    try {
      std::vector<std::pair<int, int>> workTime;
      std::vector<std::pair<int, int>> deliveryHours;

      for (const auto& timeStr : courierWorkTime) {
        int hhLeft = std::stoi(timeStr.substr(0, 2));
        int hhRight = std::stoi(timeStr.substr(6, 2));
        int mmLeft = std::stoi(timeStr.substr(3, 2));
        int mmRight = std::stoi(timeStr.substr(9, 2));

        hhLeft *= 60;
        hhRight *= 60;

        workTime.push_back(std::make_pair(hhLeft + mmLeft, hhRight + mmRight));
      }

      for (const auto& timeStr : orderDeliveryHours) {
        int hhLeft = std::stoi(timeStr.substr(0, 2));
        int hhRight = std::stoi(timeStr.substr(6, 2));
        int mmLeft = std::stoi(timeStr.substr(3, 2));
        int mmRight = std::stoi(timeStr.substr(9, 2));

        hhLeft *= 60;
        hhRight *= 60;

        deliveryHours.push_back(
            std::make_pair(hhLeft + mmLeft, hhRight + mmRight));
      }

      int completeTimeInt = std::stoi(completeTime.substr(11, 2)) * 60 +
                            std::stoi(completeTime.substr(14, 2));

      if (!std::any_of(workTime.begin(), workTime.end(),
                       [completeTimeInt](const std::pair<int, int>& p) {
                         return completeTimeInt >= p.first &&
                                completeTimeInt <= p.second;
                       }))
        return false;

      if (!std::any_of(deliveryHours.begin(), deliveryHours.end(),
                       [completeTimeInt](const std::pair<int, int>& p) {
                         return completeTimeInt >= p.first &&
                                completeTimeInt <= p.second;
                       }))
        return false;

      return true;

    } catch (...) {
      return false;
    }
  }

  const userver::storages::postgres::Query kSelectSpecificCourier{
      "SELECT * from service_schema.couriers WHERE courier_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_courier"},
  };

  const userver::storages::postgres::Query kSelectSpecificOrder{
      "SELECT order_id, CAST(weight as FLOAT) as weight, regions, delivery_hours, cost, "
      "CAST(complete_time as TEXT) as complete_time from service_schema.orders "
      "WHERE order_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_order"},
  };

  const userver::storages::postgres::Query kUpdateOrderCompleteTime{
      "UPDATE service_schema.orders set complete_time=CAST($2 as TIMESTAMP) "
      "where order_id=$1",
      userver::storages::postgres::Query::Name{"update-order-complete-time"},
  };

  const userver::storages::postgres::Query kUpdateCourierCompletedOrders{
      "UPDATE service_schema.couriers set completed_orders=$2 where "
      "courier_id=$1",
      userver::storages::postgres::Query::Name{"update-courier-completed-orders"},
  };

  const userver::storages::postgres::Query kCheckAndReturnTimestamp{
      "SELECT service_schema.CastTextToTimestamp($1) as result",
      userver::storages::postgres::Query::Name{"check-timestamp-and-return"},
  };

  std::string PostOrdersComplete(
      const userver::server::http::HttpRequest& request) const {
    if(request.ArgCount() > 0) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    try {
      auto orders_full_json =
          userver::formats::json::FromString(request.RequestBody());
      auto orders_arr = orders_full_json["complete_info"];
      if (!orders_arr.IsArray()) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return {};
      }

      std::lock_guard<userver::engine::Mutex> lock_orders(
          DatabaseAccessManager::GetOrdersMutex());

      std::lock_guard<userver::engine::Mutex> lock_couriers(
          DatabaseAccessManager::GetCouriersMutex());

      for (const auto& complete_order : orders_full_json["complete_info"]) {
        userver::storages::postgres::ResultSet res = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            kSelectSpecificCourier, complete_order["courier_id"].As<int64_t>());
        if (res.IsEmpty()) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        }
        auto courierValue =
            res.AsSingleRow<CourierDto>(userver::storages::postgres::kRowTag);
        std::vector<int> courier_regions = courierValue.regions;
        std::vector<std::string> courier_work_time = courierValue.working_hours;

        res = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            kSelectSpecificOrder, complete_order["order_id"].As<int64_t>());
        if (res.IsEmpty()) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        }
        auto orderValue =
            res.AsSingleRow<OrderDto>(userver::storages::postgres::kRowTag);
        std::vector<std::string> order_delivery_hours =
            orderValue.delivery_hours;
        int order_region = orderValue.regions;
        if (orderValue.complete_time.has_value()) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        }

        if (!std::any_of(courier_regions.begin(), courier_regions.end(),
                         [order_region](int courier_region) {
                           return order_region == courier_region;
                         })) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        }

        res = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            kCheckAndReturnTimestamp,
            complete_order["complete_time"].As<std::string>());
        std::string complete_time = res.AsSingleRow<std::string>();

        if (!IsComplete(courier_work_time, order_delivery_hours,
                        complete_time)) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        }
      }

      userver::formats::json::ValueBuilder responseJson;

      for (const auto& complete_order : orders_full_json["complete_info"]) {
        auto res = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            kSelectSpecificCourier, complete_order["courier_id"].As<int64_t>());
        auto courierValue =
            res.AsSingleRow<CourierDto>(userver::storages::postgres::kRowTag);
        std::vector<int64_t> completed_orders;
        if (courierValue.completed_orders.has_value())
          completed_orders = courierValue.completed_orders.value();
        completed_orders.push_back(complete_order["order_id"].As<int64_t>());

        userver::storages::postgres::Transaction transaction_courier =
            pg_cluster_->Begin(
                "transaction_update_courier_completed_orders",
                userver::storages::postgres::ClusterHostType::kMaster, {});
        auto res_couriers = transaction_courier.Execute(
            kUpdateCourierCompletedOrders,
            complete_order["courier_id"].As<int64_t>(), completed_orders);

        userver::storages::postgres::Transaction transaction_order =
            pg_cluster_->Begin(
                "transaction_update_order_complete_time",
                userver::storages::postgres::ClusterHostType::kMaster, {});
        auto res_order = transaction_order.Execute(
            kUpdateOrderCompleteTime, complete_order["order_id"].As<int64_t>(),
            complete_order["complete_time"].As<std::string>());

        if (res_order.RowsAffected() && res_couriers.RowsAffected()) {
          transaction_order.Commit();
          transaction_courier.Commit();

          res = pg_cluster_->Execute(
              userver::storages::postgres::ClusterHostType::kSlave,
              kSelectSpecificOrder, complete_order["order_id"].As<int64_t>());
          userver::formats::json::ValueBuilder orderBuilder{
              res.AsSingleRow<OrderDto>(userver::storages::postgres::kRowTag)};
          responseJson.PushBack(orderBuilder.ExtractValue());
        }
      }

      return userver::formats::json::ToStableString(
          responseJson.ExtractValue());

    } catch (...) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }
  }
};
}

void AppendOrdersComplete(userver::components::ComponentList& component_list) {
  component_list.Append<OrdersCompleteHandler>();
}

}  // namespace lavka