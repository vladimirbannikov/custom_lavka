#include "lavka.h"
#include "fstream"

namespace lavka {

  userver::engine::Mutex DatabaseAccessManager::couriers_mutex_;
  userver::engine::Mutex DatabaseAccessManager::orders_mutex_;
  userver::engine::Mutex& DatabaseAccessManager::GetCouriersMutex() {
    return couriers_mutex_;
  }
  userver::engine::Mutex& DatabaseAccessManager::GetOrdersMutex() {
    return orders_mutex_;
  }

  bool IsValidHours(const std::string& working_hours) {
    if (working_hours.size() != 11) return false;

    if (working_hours[5] != '-') return false;

    if (working_hours[2] != ':' || working_hours[8] != ':') return false;

    int hours1 = std::stoi(working_hours.substr(0, 2));
    int hours2 = std::stoi(working_hours.substr(6, 2));

    if (hours1 < 0 || hours1 > 23 || hours2 < 0 || hours2 > 23) return false;

    int minutes1 = std::stoi(working_hours.substr(3, 2));
    int minutes2 = std::stoi(working_hours.substr(9, 2));

    if (minutes1 < 0 || minutes1 > 59 || minutes2 < 0 || minutes2 > 59)
      return false;

    if (hours1 > hours2) return false;

    if (hours1 == hours2) {
      if (minutes1 > minutes2) return false;
    }

    return true;
  }

  void AppendLavka(userver::components::ComponentList& component_list) {
    component_list.Append<userver::components::Postgres>("postgres-db-1");
    component_list.Append<userver::clients::dns::Component>();
  }

}
