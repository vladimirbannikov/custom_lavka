#include "OrdersAssignHandler.h"

namespace lavka {



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
      case userver::server::http::HttpMethod::kPost:
        return PostOrdersAssign(request);
      default:
        throw userver::server::handlers::ClientError(
            userver::server::handlers::ExternalBody{
                fmt::format("Unsupported method {}", request.GetMethod())});
    }
  }

  const userver::storages::postgres::Query kSelectCurrentDate{
      "SELECT service_schema.ReturnCurrentDate() as result",
      userver::storages::postgres::Query::Name{"select-current-date"},
  };

  const userver::storages::postgres::Query kCheckAndReturnTimestamp{
      "SELECT service_schema.CastTextToTimestamp($1) as result",
      userver::storages::postgres::Query::Name{"check-timestamp-and-return"},
  };

  const userver::storages::postgres::Query kSelectCouriersIdWithType{
      "SELECT courier_id from service_schema.couriers "
      "where courier_type=$1",
      userver::storages::postgres::Query::Name{"select-couriers-id-with-type"},
  };

  const userver::storages::postgres::Query kSelectCouriersIdAndType{
      "SELECT courier_id, courier_type from service_schema.couriers",
      userver::storages::postgres::Query::Name{"select-couriers-id-and-type"},
  };

  const userver::storages::postgres::Query kSelectSpecificOrder{
      "SELECT order_id, CAST(weight as FLOAT) as weight, regions, delivery_hours, cost, "
      "CAST(complete_time as TEXT) as complete_time from service_schema.orders "
      "WHERE order_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_order"},
  };

  const userver::storages::postgres::Query kSelectOrdersIdWithRegion{
      "SELECT order_id from service_schema.orders "
      "where regions=$1",
      userver::storages::postgres::Query::Name{"select-orders-id-with-region"},
  };

  const userver::storages::postgres::Query kSelectSpecificCourier{
      "SELECT * from service_schema.couriers WHERE courier_id=$1",
      userver::storages::postgres::Query::Name{"select_specific_courier"},
  };

  const userver::storages::postgres::Query kAddSomeMinutes{
      "SELECT service_schema.AddSomeMinutes($1, $2) as result",
      userver::storages::postgres::Query::Name{"select_add_some_minutes"},
  };

  int TimestampWithoutDateToMinutes(const std::string& timestamp) {
    int result = std::stoi(timestamp.substr(11, 2)) * 60 +
                          std::stoi(timestamp.substr(14, 2)));
    return result;
  }

  std::pair<int, int> HhMmFormatIntervalToMinutes(const std::string& interval) {
    int hhLeft = std::stoi(interval.substr(0, 2));
    int hhRight = std::stoi(interval.substr(6, 2));
    int mmLeft = std::stoi(interval.substr(3, 2));
    int mmRight = std::stoi(interval.substr(9, 2));

    hhLeft *= 60;
    hhRight *= 60;

    return (std::make_pair(hhLeft + mmLeft, hhRight + mmRight));
  }

  bool CheckIfMatchesIntervals(const std::vector<std::pair<int, int>>& intervals, int time) {
    if (!std::any_of(intervals.begin(), intervals.end(),
                     [time](const std::pair<int, int>& p) {
                       return time >= p.first &&
                              time <= p.second;
                     }))
      return false;
    else
      return true;
  }

  std::vector<int64_t> CreateOrdersPool(const std::vector<int>& regions,
                                        std::map<int, std::vector<int64_t>>& regionToOrder) {
    std::vector<int64_t> res;
    for(auto region: regions) {
      res.insert(res.end(), regionToOrder[region].begin(), regionToOrder[region].end());
    }
    return res;
  }

  std::string PostOrdersAssign(
      const userver::server::http::HttpRequest& request) const {
    if (request.ArgCount() > 1) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    }

    std::string date;

    if (!request.HasArg("date") && request.ArgCount() == 1) {
      request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
      return {};
    } else {
      if (request.HasArg("date")) {
        date = request.GetArg("date");
        try {
          auto res = pg_cluster_->Execute(
              userver::storages::postgres::ClusterHostType::kSlave,
              kCheckAndReturnTimestamp, date);
        } catch (...) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        }
      } else {
        auto res = pg_cluster_->Execute(
            userver::storages::postgres::ClusterHostType::kSlave,
            kSelectCurrentDate);
        if (res.IsEmpty()) {
          request.SetResponseStatus(
              userver::server::http::HttpStatus::kBadRequest);
          return {};
        } else {
          date = res.AsSingleRow<std::string>();
        }
      }
    }

    std::vector<std::pair<std::string_view, std::vector<int64_t>>> typeAndCouriersArray;
    std::vector<int64_t> vec;
    typeAndCouriersArray.push_back(std::make_pair(courierType::_auto, vec));
    typeAndCouriersArray.push_back(std::make_pair(courierType::bike, vec));
    typeAndCouriersArray.push_back(std::make_pair(courierType::foot, vec));

    auto res = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kSlave,
        kSelectCouriersIdAndType);

    std::set<int> regions_set;

    auto couriersWithType = res.AsSetOf<CourierIdAndTypeStruct>(userver::storages::postgres::kRowTag);

    for(auto courier: couriersWithType) {
      if(courier.courier_type == courierType::_auto) {
        typeAndCouriersArray[0].second.push_back(courier.courier_id);
      }
      if(courier.courier_type == courierType::bike) {
        typeAndCouriersArray[1].second.push_back(courier.courier_id);
      }
      if(courier.courier_type == courierType::foot) {
        typeAndCouriersArray[2].second.push_back(courier.courier_id);
      }

      res = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kSlave,
          kSelectCouriersIdWithType, courier.courier_id);
      auto courierFullDto = res.AsSingleRow<CourierDto>(userver::storages::postgres::kRowTag);

      for(auto region: courierFullDto.regions) {
        if(regions_set.find(region) == regions_set.end())
          regions_set.insert(region);
      }

    }

    std::map<int, std::vector<int64_t>> regionToOrder;
    std::set<int64_t> ordersSet;

    for(int region: regions_set) {
      res = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kSlave,
          kSelectOrdersIdWithRegion, region);
      auto orders = res.AsSetOf<OrderIdStruct>(userver::storages::postgres::kRowTag);

      for (auto order: orders) {
        regionToOrder[region].push_back(order.order_id);
        if(ordersSet.find(order.order_id) == ordersSet.end())
          ordersSet.insert(order.order_id);
      }
    }

    int maxOrdersCount = 0;
    int used_couriers = 0;
    for(auto couriers_vec: couriersWithType) {
      for(auto couriers: )
    }
    for(auto courier_id: couriersWithType) {
      std::vector<int64_t> orders_pool;
      res = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kSlave,
          kSelectOrdersIdWithRegion, region);
      auto orders = res.AsSetOf<OrderIdStruct>(userver::storages::postgres::kRowTag);


    }




  }
};

}  // namespace

}  // namespace lavka