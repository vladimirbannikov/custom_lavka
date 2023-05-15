#include "OrdersHandler.h"

namespace lavka {

userver::engine::Mutex OrderIdManager::mutex_;

int64_t OrderIdManager::last_id_ = 1;

int64_t OrderIdManager::GetNewId() {
  std::lock_guard<userver::engine::Mutex> lock(mutex_);
  if (last_id_ < INT64_MAX) {
    ++last_id_;
    return last_id_ - 1;
  } else
    return -1;  // exc_max_orders + response 400 bad request
}

userver::formats::json::Value Serialize(
    const OrderDto& data,
    userver::formats::serialize::To<userver::formats::json::Value>) {
  userver::formats::json::ValueBuilder jsonOrder;
  jsonOrder["order_id"] = data.order_id;
  jsonOrder["weight"] = data.weight;
  jsonOrder["regions"] = data.regions;
  jsonOrder["delivery_hours"] = data.delivery_hours;
  jsonOrder["cost"] = data.cost;
  if (data.complete_time.has_value())
    jsonOrder["complete_time"] = data.complete_time.value();

  return jsonOrder.ExtractValue();
}

namespace {

class OrdersHandler final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-orders";
  userver::storages::postgres::ClusterPtr pg_cluster_;

  OrdersHandler(const userver::components::ComponentConfig& config,
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
        return GetOrders(request);
      case userver::server::http::HttpMethod::kPost:
        return PostOrders(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  bool IsOrderJsonValid(const userver::formats::json::Value& order_json) const {
    try {
      if (order_json.GetSize() != 4) return false;

      if (!(order_json.HasMember("weight") && order_json.HasMember("regions") &&
            order_json.HasMember("delivery_hours") &&
            order_json.HasMember("cost")))
        return false;

      order_json["weight"].As<float>();

      if (order_json["weight"].As<float>() < 0) return false;

      order_json["regions"].As<int>();

      if (order_json["delivery_hours"].IsEmpty()) return false;

      for (const auto& delivery_hour : order_json["delivery_hours"]) {
        if (!IsValidHours(delivery_hour.As<std::string>())) return false;
      }

      order_json["cost"].As<int>();

    } catch (...) {
      return false;
    }

    return true;
  }

  const userver::storages::postgres::Query kSelectOrders{
      "SELECT order_id, CAST(weight as FLOAT) as weight, regions, delivery_hours, cost, "
      "CAST(complete_time as TEXT) as complete_time from service_schema.orders "
      "LIMIT $2 OFFSET $1",
      userver::storages::postgres::Query::Name{"select_orders"},
  };

  std::string GetOrders(
      const userver::server::http::HttpRequest& request) const {
    if (request.ArgCount() > 2) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    int offset = 0, limit = 1, realArgCount = 0;

    try {
      if (request.HasArg("offset")) {
        offset = std::stoi(request.GetArg("offset"));
        ++realArgCount;
      }
      if (request.HasArg("limit")) {
        ++realArgCount;
        limit = std::stoi(request.GetArg("limit"));
      }
    }
    catch (...) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    if(request.ArgCount() > realArgCount) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    userver::storages::postgres::ResultSet res = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave, kSelectOrders,
        offset, limit);

    auto resVec = res.AsSetOf<OrderDto>(userver::storages::postgres::kRowTag);

    userver::formats::json::ValueBuilder ordersBuilder{resVec};

    return userver::formats::json::ToStableString(ordersBuilder.ExtractValue());
  }

  const userver::storages::postgres::Query kInsertOrders{
      "INSERT INTO service_schema.orders "
      "VALUES ($1, CAST($2 as NUMERIC), $3, $4, $5) "
      "ON CONFLICT DO NOTHING",
      userver::storages::postgres::Query::Name{"insert_orders"},
  };

  std::string PostOrders(
      const userver::server::http::HttpRequest& request) const {

    if(request.ArgCount() > 0) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    std::string test_res;

    userver::formats::json::Value orders_arr;
    try {
      orders_arr = userver::formats::json::FromString(request.RequestBody());

      if (orders_arr.IsEmpty() || !orders_arr.IsArray()) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return {};
      }

    } catch (const userver::formats::json::Exception& exc) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    for (const auto& single_order : orders_arr) {
      if (!IsOrderJsonValid(single_order)) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return {};
      }
    }

    std::lock_guard<userver::engine::Mutex> lock(
        DatabaseAccessManager::GetOrdersMutex());

    userver::formats::json::ValueBuilder responseBuilder;

    for (const auto& single_order : orders_arr) {
      int64_t order_id = OrderIdManager::GetNewId();
      auto weight = single_order["weight"].As<float>();
      auto regions = single_order["regions"].As<int>();
      auto delivery_hours =
          single_order["delivery_hours"].As<std::vector<std::string>>();
      auto cost = single_order["cost"].As<int>();

      userver::storages::postgres::Transaction transaction = pg_cluster_->Begin(
          "transaction_insert_order_value",
          userver::storages::postgres::ClusterHostType::kMaster, {});

      auto res = transaction.Execute(kInsertOrders, order_id, weight, regions,
                                     delivery_hours, cost);

      userver::formats::json::ValueBuilder orderDO_builder{single_order};
      orderDO_builder["order_id"] = order_id;

      if (res.RowsAffected()) {
        transaction.Commit();
        responseBuilder.PushBack(orderDO_builder.ExtractValue());
      }
    }

    return userver::formats::json::ToStableString(
        responseBuilder.ExtractValue());
  }
};

}  // namespace

void AppendOrders(userver::components::ComponentList& component_list) {
  component_list.Append<OrdersHandler>();
}

}  // namespace lavka
