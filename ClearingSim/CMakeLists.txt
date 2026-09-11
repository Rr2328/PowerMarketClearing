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
    mainwindow.ui

    data/data_reader.h
    data/data_reader.cpp
    data/csv_utils.h
    data/csv_utils.cpp
    data/data_validator.h
    data/data_validator.cpp
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

    quadratic_clearing.h
    quadratic_clearing.cpp

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
    data/data_reader.h
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
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
    data/data_reader.h
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
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
    data/data_reader.h
    data/csv_utils.cpp
    data/csv_utils.h
    data/data_validator.cpp
    data/data_validator.h
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
    quadratic_clearing.h
    quadratic_clearing.cpp
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
# 借鉴 origin/feature/data-model 的端到端集成测试思路（陈美伊 2137534），
# 但适配到 V1.3 长表结构（bid 不带 period 字段、按 name+id 复合主体标识）

qt_add_executable(DataPipelineIntegrationTest
    data/data_pipeline_integration_test.cpp
    data/data_reader.cpp
    data/data_reader.h
    data/scenario_manager.cpp
    data/scenario_manager.h
    core/clearing_facade.cpp
    core/clearing_facade.h
    engine/clearing_engine.cpp
    engine/clearing_engine.h
    quadratic_clearing.h
    quadratic_clearing.cpp
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
# Windows 下 Qt 测试程序须在 PATH 含 Qt DLL 才能独立启动——CI 上需要 windeployqt
#   拷贝，或在 add_test 里设 ENVIRONMENT 把 Qt bin 加进去。这里开发期主要靠
#   PATH 前置 + QT_FORCE_STDERR_LOGGING=1 环境变量。

enable_testing()

add_test(NAME DataReaderTest          COMMAND DataReaderTest)
add_test(NAME ScenarioManagerTest     COMMAND ScenarioManagerTest)
add_test(NAME ClearingFacadeTest      COMMAND ClearingFacadeTest)
add_test(NAME DataPipelineIntegrationTest COMMAND DataPipelineIntegrationTest)
add_test(NAME MarketDataIntegrationTest COMMAND MarketDataIntegrationTest)

# 二次曲线出清测试（选题 2026v2 (10) 问）

qt_add_executable(QuadraticClearingTest
    quadratic_clearing_test.cpp
    quadratic_clearing.h
    quadratic_clearing.cpp
    data/data_reader.h
    data/data_reader.cpp
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

# 让 Windows 控制台输出也走 stderr（IDE/Test Explorer 看得到）
set_tests_properties(
    DataReaderTest
    ScenarioManagerTest
    ClearingFacadeTest
    DataPipelineIntegrationTest
    MarketDataIntegrationTest
    QuadraticClearingTest
    PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
