CMAKE_COMMON_FLAGS ?= -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
CMAKE_DEBUG_FLAGS ?= -DUSERVER_SANITIZE='addr ub'
CMAKE_RELEASE_FLAGS ?=
CMAKE_OS_FLAGS ?= -DUSERVER_CHECK_PACKAGE_VERSIONS=0 -USERVER_FEATURE_PATCH_LIBPQ=0 -DUSERVER_FEATURE_CRYPTOPP_BLAKE2=0 -DUSERVER_FEATURE_CRYPTOPP_BASE64_URL=0 -DUSERVER_FEATURE_GRPC=0 -DUSERVER_FEATURE_POSTGRESQL=1 -DUSERVER_FEATURE_MONGODB=0 -DUSERVER_FEATURE_CLICKHOUSE=0
NPROCS ?= $(shell nproc)
CLANG_FORMAT ?= clang-format

# NOTE: use Makefile.local for customization
-include Makefile.local

.PHONY: all
 all: test-debug test-release

# Debug cmake configuration
build_debug/Makefile:
	@git submodule update --init
	@mkdir -p build_debug
	@cd build_debug && \
      cmake -DCMAKE_BUILD_TYPE=Debug $(CMAKE_COMMON_FLAGS) $(CMAKE_DEBUG_FLAGS) $(CMAKE_OS_FLAGS) $(CMAKE_OPTIONS) ..

# Release cmake configuration
build_release/Makefile:
	@git submodule update --init
	@mkdir -p build_release
	@cd build_release && \
      cmake -DCMAKE_BUILD_TYPE=Release $(CMAKE_COMMON_FLAGS) $(CMAKE_RELEASE_FLAGS) $(CMAKE_OS_FLAGS) $(CMAKE_OPTIONS) ..

# Run cmake
.PHONY: cmake-debug cmake-release
cmake-debug cmake-release: cmake-%: build_%/Makefile

# Build using cmake
.PHONY: build-debug build-release
build-debug build-release: build-%: cmake-%
	@cmake --build build_$* -j $(NPROCS) --target lavka

# Test
# .PHONY: test-debug test-release
# test-debug test-release: test-%: build-%
# 	@cmake --build build_$* -j $(NPROCS) --target lavka_unittest
#	@cd build_$* && ((test -t 1 && GTEST_COLOR=1 PYTEST_ADDOPTS="--color=yes" ctest -V) || ctest -V)
#	@pep8 tests

# Start the service (via testsuite service runner)
.PHONY: service-start-debug service-start-release
service-start-debug service-start-release: service-start-%: build-%
	@cd ./build_$* && $(MAKE) start-lavka

# Start the service manually
.PHONY: service-start-manually-release
service-start-manually-release:
	@sed -i 's/config_vars.yaml/config_vars.docker_solution.yaml/g' ./configs/static_config.yaml
	@psql 'postgresql://postgres:password@db/postgres' -f ./postgresql/schemas/db1.sql
	@cd build_release && ./lavka -c ../configs/static_config.yaml

# Cleanup data
.PHONY: clean-debug clean-release
clean-debug clean-release: clean-%:
	cd build_$* && $(MAKE) clean

.PHONY: dist-clean
dist-clean:
	@rm -rf build_*
	@rm -f ./configs/static_config.yaml
	# @rm -rf tests/__pycache__/
	# @rm -rf tests/.pytest_cache/

# Install
.PHONY: install-debug install-release
install-debug install-release: install-%: build-%
	@cd build_$* && \
		cmake --install . -v --component lavka

.PHONY: install
install: install-release

# Format the sources
.PHONY: format
format:
	@find src -name '*pp' -type f | xargs $(CLANG_FORMAT) -i
	@find tests -name '*.py' -type f | xargs autopep8 -i

# Internal hidden targets that are used only in docker environment
--in-docker-start-debug --in-docker-start-release: --in-docker-start-%: install-%
	@sed -i 's/config_vars.yaml/config_vars.docker.yaml/g' /home/user/.local/etc/lavka/static_config.yaml
	@psql 'postgresql://postgres:password@db/postgres' -f ./postgresql/schemas/db1.sql
	@/home/user/.local/bin/lavka \
		--config /home/user/.local/etc/lavka/static_config.yaml

# Build and run service in docker environment
.PHONY: docker-start-service-debug docker-start-service-release
docker-start-service-debug docker-start-service-release: docker-start-service-%:
	@docker-compose run -p 8080:8080 --rm lavka-container $(MAKE) -- --in-docker-start-$*

# Start targets makefile in docker environment
.PHONY: docker-cmake-debug docker-build-debug docker-clean-debug docker-install-debug docker-cmake-release docker-build-release docker-clean-release docker-install-release
docker-cmake-debug docker-build-debug docker-clean-debug docker-install-debug docker-cmake-release docker-build-release docker-clean-release docker-install-release: docker-%:
	docker-compose run --rm lavka-container $(MAKE) $*

# Stop docker container and remove PG data
.PHONY: docker-clean-data
docker-clean-data:
	@docker-compose down -v
	@rm -rf ./.pgdata
