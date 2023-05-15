cmake_minimum_required(VERSION 3.12)
project(lavka CXX)

include(third_party/userver/cmake/SetupEnvironment.cmake)
include(GNUInstallDirs)

add_subdirectory(third_party/userver)

set(COURIERS_SOURCE
        src/couriers/CouriersHandler.h src/couriers/CouriersHandler.cpp
        src/couriers/CouriersIDHandler.h src/couriers/CouriersIDHandler.cpp
        src/couriers/CouriersMetaInfoHandler.h src/couriers/CouriersMetaInfoHandler.cpp
        )

set(ORDERS_SOURCE
        src/orders/OrdersHandler.h src/orders/OrdersHandler.cpp
        src/orders/OrdersIDHandler.h src/orders/OrdersIDHandler.cpp
        src/orders/OrdersCompleteHandler.h src/orders/OrdersCompleteHandler.cpp
        )

# Common sources
add_library(${PROJECT_NAME}_objs OBJECT
        ${COURIERS_SOURCE}
        ${ORDERS_SOURCE}
        src/lavka.h
        src/lavka.cpp
        )
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver-core userver-postgresql)


# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)


# Functional Tests
#add_subdirectory(tests)

if (DEFINED ENV{PREFIX})
    message(STATUS "Set install prefix: $ENV{PREFIX}")
    file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
    set(CMAKE_INSTALL_PREFIX ${PREFIX_PATH})
endif ()

set(CONFIG_VAR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/configs/config_vars.yaml)
set(CONFIG_FALLBACK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/configs/dynamic_config_fallback.json)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config.yaml.in ${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config.yaml)

FILE(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PROJECT_NAME})
install(FILES ${CONFIGS_FILES} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME} COMPONENT ${PROJECT_NAME})
