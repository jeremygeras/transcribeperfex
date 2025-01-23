cmake_minimum_required(VERSION 3.5.0)

project(AceMediaServicesCli)

set(CMAKE_CXX_STANDARD 20)

set(SERVICE_COMPONENTS transcribestreaming)
find_package(AWSSDK COMPONENTS ${SERVICE_COMPONENTS})

add_executable(ace-media-services-cli main.cpp)

target_include_directories(ace-media-services-cli PRIVATE "../third_party/asio/include")
target_compile_definitions(ace-media-services-cli PRIVATE ASIO_STANDALONE)
target_link_libraries(ace-media-services-cli PRIVATE ${AWSSDK_LINK_LIBRARIES})

set_target_properties(ace-media-services-cli PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" BUILD_WITH_INSTALL_RPATH TRUE)

install(TARGETS ace-media-services-cli RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR})
