cmake_minimum_required(VERSION 3.21)
project(Editor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6 con MSYS2 UCRT64 (sobrescribe con CMAKE_PREFIX_PATH si usas otra instalación)
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
  set(CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

add_executable(Editor WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/core/SupabaseClient.cpp
    src/core/AuthService.cpp
    src/core/PropietarioService.cpp
    src/core/UbicacionService.cpp
    src/core/TipoService.cpp
    src/core/PuertoService.cpp
    src/core/UsadaService.cpp
    src/core/PiezaService.cpp
    src/ui/LoadingOverlay.cpp
    src/ui/LoginWidget.cpp
    src/ui/RegisterWidget.cpp
    src/ui/DashboardWidget.cpp
    src/ui/pages/PiezasPage.cpp
    src/ui/pages/TimelinesPage.cpp
    src/ui/pages/PropietariosPage.cpp
    src/ui/pages/UbicacionesPage.cpp
    src/ui/pages/PuertosPage.cpp
    src/ui/pages/DispositivosPage.cpp
    src/ui/pages/TiposPage.cpp
)
qt_add_resources(Editor "resources" PREFIX "/"
  FILES resources.qrc
)
if(WIN32)
  target_sources(Editor PRIVATE ${CMAKE_SOURCE_DIR}/app.rc)
endif()
target_include_directories(Editor PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(Editor PRIVATE Qt6::Widgets Qt6::Network)
