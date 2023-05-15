#include "CouriersHandler.h"
#include <fstream>

namespace lavka {

userver::engine::Mutex CourierIdManager::mutex_;

int64_t CourierIdManager::last_id_ = 1;

int64_t CourierIdManager::GetNewId() {
  std::lock_guard<userver::engine::Mutex> lock(mutex_);
  if (last_id_ < INT64_MAX) {
    ++last_id_;
    return last_id_ - 1;
  } else
    return -1;  // exc_max_couriers + response 400 bad request
}

userver::formats::json::Value Serialize(
    const CourierDto& data,
    userver::formats::serialize::To<userver::formats::json::Value>) {
  userver::formats::json::ValueBuilder jsonCourier;
  jsonCourier["courier_id"] = data.courier_id;
  jsonCourier["courier_type"] = data.courier_type;
  jsonCourier["regions"] = data.regions;
  jsonCourier["working_hours"] = data.working_hours;
  if (data.completed_orders.has_value()) {
    jsonCourier["completed_orders"] = data.completed_orders.value();
  }
  return jsonCourier.ExtractValue();
}

namespace {

class CouriersHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-couriers";
  userver::storages::postgres::ClusterPtr pg_cluster_;

  CouriersHandler(
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
        return GetCouriers(request);
      case userver::server::http::HttpMethod::kPost:
        return PostCouriers(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  bool IsCourierJsonValid(
      const userver::formats::json::Value& courier_json) const {
    try {
      if (courier_json.GetSize() != 3) return false;

      if (!(courier_json.HasMember("courier_type") &&
            courier_json.HasMember("regions") &&
            courier_json.HasMember("working_hours")))
        return false;

      if (courier_json["courier_type"].As<std::string>() != courierType::foot &&
          courier_json["courier_type"].As<std::string>() != courierType::bike &&
          courier_json["courier_type"].As<std::string>() != courierType::_auto)
        return false;

      if (courier_json["regions"].IsEmpty()) return false;

      for (const auto& region : courier_json["regions"]) {
        region.As<int>();
      }

      if (courier_json["working_hours"].IsEmpty()) return false;

      for (const auto& working_hour : courier_json["working_hours"]) {
        if (!IsValidHours(working_hour.As<std::string>())) return false;
      }

    } catch (...) {
      return false;
    }

    return true;
  }

  const userver::storages::postgres::Query kSelectCouriers{
      "SELECT * from service_schema.couriers LIMIT $2 OFFSET $1",
      userver::storages::postgres::Query::Name{"select_couriers"},
  };

  std::string GetCouriers(
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
        userver::storages::postgres::ClusterHostType::kSlave, kSelectCouriers,
        offset, limit);

    auto resVec = res.AsSetOf<CourierDto>(userver::storages::postgres::kRowTag);

    userver::formats::json::ValueBuilder couriersBuilder{resVec};
    for (auto& courier : couriersBuilder) {
      if (courier.HasMember("completed_orders"))
        courier.Remove("completed_orders");
    }
    auto couriersJson = couriersBuilder.ExtractValue();

    userver::formats::json::ValueBuilder responseJson;
    responseJson["couriers"] = couriersJson;
    responseJson["offset"] = offset;
    responseJson["limit"] = limit;

    return userver::formats::json::ToStableString(responseJson.ExtractValue());
  }

  const userver::storages::postgres::Query kInsertCouriers{
      "INSERT INTO service_schema.couriers "
      "VALUES ($1, $2, $3, $4) "
      "ON CONFLICT DO NOTHING",
      userver::storages::postgres::Query::Name{"insert_couriers"},
  };

  std::string PostCouriers(
      const userver::server::http::HttpRequest& request) const {
    userver::formats::json::Value couriers_arr;

    if(request.ArgCount() > 0) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    try {
      couriers_arr = userver::formats::json::FromString(request.RequestBody());

      if (couriers_arr.IsEmpty() || !couriers_arr.IsArray()) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return {};
      }

    } catch (const userver::formats::json::Exception& exc) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    for (const auto& single_courier : couriers_arr) {
      if (!IsCourierJsonValid(single_courier)) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return {};
      }
    }

    std::lock_guard<userver::engine::Mutex> lock(
        DatabaseAccessManager::GetCouriersMutex());

    userver::formats::json::ValueBuilder responseBuilder;

    for (const auto& single_courier : couriers_arr) {
      int64_t courier_id = CourierIdManager::GetNewId();
      auto type = single_courier["courier_type"].As<std::string>();
      auto regions_arr = single_courier["regions"].As<std::vector<int>>();
      auto hours_arr =
          single_courier["working_hours"].As<std::vector<std::string>>();

      userver::storages::postgres::Transaction transaction = pg_cluster_->Begin(
          "transaction_insert_courier_value",
          userver::storages::postgres::ClusterHostType::kMaster, {});

      auto res = transaction.Execute(kInsertCouriers, courier_id, type,
                                     regions_arr, hours_arr);

      userver::formats::json::ValueBuilder courierDO_builder{single_courier};
      courierDO_builder["courier_id"] = courier_id;

      if (res.RowsAffected()) {
        transaction.Commit();
        responseBuilder.PushBack(courierDO_builder.ExtractValue());
      }
    }

    return userver::formats::json::ToStableString(
        responseBuilder.ExtractValue());
  }
};

}  // namespace

void AppendCouriers(userver::components::ComponentList& component_list) {
  component_list.Append<CouriersHandler>();
}

}  // namespace lavka
