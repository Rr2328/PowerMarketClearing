cmake_minimum_required(VERSION 3.19)

project(ClearingSim LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts)

qt_standard_project_setup()

# HiGHS 求解器（SCUC 机组组合，方案 §五）
#   MIT 开源、纯 C++、MinGW 兼容；源码在仓库 third_party/HiGHS（浅克隆），
#   add_subdirectory 编译为静态库 highs。只开核心库，关掉它自带的测试/示例。
set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
set(BUILD_CXX_EXE          OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_EXTRAS_LIB OFF CACHE BOOL "" FORCE)
set(FAST_BUILD             ON  CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/HiGHS
                 ${CMAKE_BINARY_DIR}/third_party/HiGHS)

# 主程序
qt_add_executable(ClearingSim
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h

    data/data_reader.h
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    data/scenario_manager.h
    data/scenario_manager.cpp

    engine/clearing_engine.h
    engine/clearing_engine.cpp

    core/perspective.h
    core/app_session.h
    core/clearing_facade.h
    core/clearing_facade.cpp
    core/market_view.h
    core/market_view.cpp

    engine/quadratic_clearing.h
    engine/quadratic_clearing.cpp

    uc/uc_solver.h
    uc/uc_solver.cpp
)

target_include_directories(ClearingSim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ClearingSim
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Charts
        highs
)

if(MSVC)
    target_compile_options(ClearingSim PRIVATE /utf-8)
endif()

install(TARGETS ClearingSim
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ClearingSim
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})

# DataReader 独立测试
qt_add_executable(DataReaderTest
    data/data_reader_test.cpp
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    data/data_reader.h
)

target_link_libraries(DataReaderTest PRIVATE Qt::Core)
target_include_directories(DataReaderTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/data)

if(MSVC)
    target_compile_options(DataReaderTest PRIVATE /utf-8)
endif()

# ScenarioManager 独立测试
qt_add_executable(ScenarioManagerTest
    data/scenario_manager_test.cpp
    data/scenario_manager.cpp
    data/scenario_manager.h
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    data/data_reader.h
)

target_link_libraries(ScenarioManagerTest PRIVATE Qt::Core)
target_include_directories(ScenarioManagerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/data)

if(MSVC)
    target_compile_options(ScenarioManagerTest PRIVATE /utf-8)
endif()

# MarketData 集成测试（data-model 分支：CSV → DataReader → 场景构建）
qt_add_executable(MarketDataIntegrationTest
    data/market_data_integration_test.cpp
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    data/data_reader.h
    data/scenario_manager.cpp
    data/scenario_manager.h
)

target_link_libraries(MarketDataIntegrationTest PRIVATE Qt::Core)
target_include_directories(MarketDataIntegrationTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/data)

if(MSVC)
    target_compile_options(MarketDataIntegrationTest PRIVATE /utf-8)
endif()

# ClearingFacade 独立测试（V1.3 对拍：窄表等价性 / 渗透率 / 24 期聚合）

qt_add_executable(ClearingFacadeTest
    core/clearing_facade_test.cpp
    core/clearing_facade.h
    core/clearing_facade.cpp
    engine/clearing_engine.h
    engine/clearing_engine.cpp
    data/data_reader.h
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    engine/quadratic_clearing.h
    engine/quadratic_clearing.cpp
    uc/uc_solver.h
    uc/uc_solver.cpp
)

target_link_libraries(ClearingFacadeTest
    PRIVATE
        Qt::Core
        highs
)

target_include_directories(ClearingFacadeTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)


# V1.3 数据流水线端到端集成测试（CSV → DataReader → ScenarioManager → ClearingFacade）

qt_add_executable(DataPipelineIntegrationTest
    data/data_pipeline_integration_test.cpp
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    data/data_reader.h
    data/scenario_manager.cpp
    data/scenario_manager.h
    core/clearing_facade.cpp
    core/clearing_facade.h
    engine/clearing_engine.cpp
    engine/clearing_engine.h
    engine/quadratic_clearing.h
    engine/quadratic_clearing.cpp
    uc/uc_solver.h
    uc/uc_solver.cpp
)

target_link_libraries(DataPipelineIntegrationTest
    PRIVATE
        Qt::Core
        highs
)

target_include_directories(DataPipelineIntegrationTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(
        DataPipelineIntegrationTest
        PRIVATE
        /utf-8
    )
endif()


# CTest 接入（#97）
#   此前 4 个测试 target 用 qt_add_executable 注册但未挂 CTest，
#   导致 ctest / IDE Test Explorer 拿不到；这里 enable_testing + add_test 接上
#   标准测试框架。
#
# Windows 下 Qt/MinGW 测试程序须在 PATH 含对应运行库才能独立启动。
#   下方为每个测试显式补齐路径，避免 ctest / IDE Test Explorer 报 0xc0000135。

enable_testing()

add_test(NAME DataReaderTest          COMMAND DataReaderTest)
add_test(NAME ScenarioManagerTest     COMMAND ScenarioManagerTest)
add_test(NAME ClearingFacadeTest      COMMAND ClearingFacadeTest)
add_test(NAME DataPipelineIntegrationTest COMMAND DataPipelineIntegrationTest)
add_test(NAME MarketDataIntegrationTest COMMAND MarketDataIntegrationTest)

# 二次曲线出清测试（选题 2026v2 (10) 问）

qt_add_executable(QuadraticClearingTest
    engine/quadratic_clearing_test.cpp
    engine/quadratic_clearing.h
    engine/quadratic_clearing.cpp
    data/data_reader.h
    data/data_reader.cpp
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
    core/clearing_facade.h
    core/clearing_facade.cpp
    engine/clearing_engine.h
    engine/clearing_engine.cpp
    uc/uc_solver.h
    uc/uc_solver.cpp
)

target_link_libraries(QuadraticClearingTest
    PRIVATE
        Qt::Core
        highs
)

target_include_directories(QuadraticClearingTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME QuadraticClearingTest COMMAND QuadraticClearingTest)

# HiGHS 求解器接入验证（SCUC S1）

qt_add_executable(HighsHelloTest
    uc/uc_hello_test.cpp
)

target_link_libraries(HighsHelloTest
    PRIVATE
        highs
)

add_test(NAME HighsHelloTest COMMAND HighsHelloTest)

# SCUC 求解器测试（S2：LP 建模 + 对偶价对拍）

qt_add_executable(UcSolverTest
    uc/uc_solver_test.cpp
    uc/uc_solver.h
    uc/uc_solver.cpp
)

target_link_libraries(UcSolverTest
    PRIVATE
        highs
        Qt::Core
)

add_test(NAME UcSolverTest COMMAND UcSolverTest)

# 让 Windows 控制台输出也走 stderr（IDE/Test Explorer 看得到）。
# ENVIRONMENT_MODIFICATION 从 CMake 3.22 起可用；旧版仍可配置项目，
# 但需由调用者自行把 Qt/编译器 bin 放入 PATH。
set(_clearing_test_targets
    DataReaderTest
    ScenarioManagerTest
    ClearingFacadeTest
    DataPipelineIntegrationTest
    MarketDataIntegrationTest
    QuadraticClearingTest
    HighsHelloTest
    UcSolverTest
)
set_tests_properties(
    ${_clearing_test_targets}
    PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
    get_filename_component(_qt_runtime_dir "${Qt6_DIR}/../../.." ABSOLUTE)
    set(_qt_runtime_dir "${_qt_runtime_dir}/bin")
    get_filename_component(_compiler_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(
        ${_clearing_test_targets}
        PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_qt_runtime_dir};PATH=path_list_prepend:${_compiler_runtime_dir}"
    )
endif()
