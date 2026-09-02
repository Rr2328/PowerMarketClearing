cmake_minimum_required(VERSION 3.19)
project(ClearingSim LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts)

qt_standard_project_setup()

qt_add_executable(ClearingSim
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    clearing_engine.h
    clearing_engine.cpp
    market_runner.h
    market_runner.cpp
    data_reader.h
    data_reader.cpp



)

target_link_libraries(ClearingSim
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Charts
)

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
