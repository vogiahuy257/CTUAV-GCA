qt_add_library(DroneDataLoaderModule STATIC
    DroneDataLoader.cc
    DroneDataLoader.h
)

qt_add_qml_module(DroneDataLoaderModule
    URI CTUAV.DroneList
    VERSION 1.0
    NO_PLUGIN
    SOURCES
        DroneDataLoader.cc
        DroneDataLoader.h
)

target_include_directories(DroneDataLoaderModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(DroneDataLoaderModule
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/QmlControls
        ${CMAKE_SOURCE_DIR}/src/FactSystem
        ${CMAKE_SOURCE_DIR}/src/FactSystem/FactControls
)

target_link_libraries(DroneDataLoaderModule
    PRIVATE
        QGroundControlControlsModule
)
