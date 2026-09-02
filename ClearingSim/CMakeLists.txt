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

    clearing_engine.h
    clearing_engine.cpp

    core/perspective.h
    core/app_session.h
    core/fake_engine.h
    core/fake_engine.cpp
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