cmake_minimum_required(VERSION 3.19)

project(ClearingSim LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Widgets
    Charts
)

qt_standard_project_setup()


# 主程序

qt_add_executable(ClearingSim
    WIN32 MACOSX_BUNDLE

    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    data/data_reader.h
    data/data_reader.cpp

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
)


if(MSVC)
    target_compile_options(
        ClearingSim
        PRIVATE
        /utf-8
    )
endif()


install(TARGETS ClearingSim
    BUNDLE  DESTINATION .
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
)

target_link_libraries(DataReaderTest
    PRIVATE
        Qt::Core
)

target_include_directories(DataReaderTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/data
)


# ScenarioManager 独立测试

qt_add_executable(ScenarioManagerTest
    data/scenario_manager_test.cpp
    data/scenario_manager.cpp
    data/scenario_manager.h
    data/data_reader.cpp
    data/data_reader.h
)

target_link_libraries(ScenarioManagerTest
    PRIVATE
        Qt::Core
)

target_include_directories(ScenarioManagerTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/data
)


# ClearingFacade 独立测试（V1.3 对拍：窄表等价性 / 渗透率 / 24 期聚合）

qt_add_executable(ClearingFacadeTest
    core/clearing_facade_test.cpp
    core/clearing_facade.h
    core/clearing_facade.cpp
    engine/clearing_engine.h
    engine/clearing_engine.cpp
    data/data_reader.h
    data/data_reader.cpp
)

target_link_libraries(ClearingFacadeTest
    PRIVATE
        Qt::Core
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
)

target_link_libraries(DataPipelineIntegrationTest
    PRIVATE
        Qt::Core
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

# 让 Windows 控制台输出也走 stderr（IDE/Test Explorer 看得到）
set_tests_properties(
    DataReaderTest
    ScenarioManagerTest
    ClearingFacadeTest
    DataPipelineIntegrationTest
    PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)