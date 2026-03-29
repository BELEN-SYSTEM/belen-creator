cmake_minimum_required(VERSION 3.21)
project(BelenCreator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6 con MSYS2 UCRT64 (sobrescribe con CMAKE_PREFIX_PATH si usas otra instalación)
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
  set(CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

add_executable(BelenCreator
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
    src/core/HistorialService.cpp
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
    src/ui/pages/HistorialPage.cpp
)
if(WIN32)
  set_target_properties(BelenCreator PROPERTIES WIN32_EXECUTABLE TRUE)
  target_sources(BelenCreator PRIVATE ${CMAKE_SOURCE_DIR}/app.rc)
endif()
if(APPLE)
  set_target_properties(BelenCreator PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Belen Creator"
    MACOSX_BUNDLE_GUI_IDENTIFIER com.betlemsystem.belencreator
    MACOSX_BUNDLE_INFO_STRING "Belen Creator"
    MACOSX_BUNDLE_LONG_VERSION_STRING "1.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.1"
  )
  set(_belen_mac_icon "${CMAKE_SOURCE_DIR}/icons/AppIcon.icns")
  if(EXISTS "${_belen_mac_icon}")
    set_target_properties(BelenCreator PROPERTIES MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    set_source_files_properties("${_belen_mac_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(BelenCreator PRIVATE "${_belen_mac_icon}")
  else()
    message(WARNING "macOS: falta icons/AppIcon.icns (icono del .app). Ejecuta: python3 scripts/generate_macos_icns.py")
  endif()
endif()
qt_add_resources(BelenCreator "resources" PREFIX "/"
  FILES resources.qrc
)
target_include_directories(BelenCreator PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(BelenCreator PRIVATE Qt6::Widgets Qt6::Network)
