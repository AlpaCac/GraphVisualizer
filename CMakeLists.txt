cmake_minimum_required(VERSION 3.19)
project(GraphVisualizer LANGUAGES CXX)

# 设置 C++ 标准（解决之前的底层数学库冲突）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Gui)

list(APPEND CMAKE_PREFIX_PATH "F:/env/ogdf_install")
find_package(OGDF REQUIRED)

qt_standard_project_setup()

qt_add_executable(GraphVisualizer
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    graph_items.h
    graph_items.cpp
    test.h
    test.cpp
)


target_link_libraries(GraphVisualizer
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Gui
        OGDF
)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        "F:/env/ogdf_install/include"
        "F:/env/ogdf_install/include/ogdf-release"
)

install(TARGETS GraphVisualizer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET GraphVisualizer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
