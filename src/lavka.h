#ifndef LAVKA_LAVKA_H
#define LAVKA_LAVKA_H

#include <cstdint>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/serialize_container.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/datetime.hpp>

namespace lavka {

class DatabaseAccessManager {
  static userver::engine::Mutex couriers_mutex_;
  static userver::engine::Mutex orders_mutex_;

 public:
  static userver::engine::Mutex& GetCouriersMutex();
  static userver::engine::Mutex& GetOrdersMutex();
};

bool IsValidHours(const std::string& working_hours);

void AppendLavka(userver::components::ComponentList& component_list);

}
#endif
