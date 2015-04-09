## Boost
find_package(Boost 1.55 COMPONENTS system filesystem REQUIRED) 
include_directories(AFTER ${Boost_INCLUDE_DIRS})
link_directories ( ${Boost_LIBRARY_DIRS} )

# Include our headers
include_directories(BEFORE ${CMAKE_CURRENT_LIST_DIR})

# Build a test tftp server
file(GLOB_RECURSE SERIAL_UTILS_SOURCE_FILES *.cpp)
add_executable(tftp-server ${SERIAL_UTILS_SOURCE_FILES})
target_link_libraries(tftp-server ${Boost_LIBRARIES})

# Install test tftp server 
install(TARGETS tftp-server
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/ DESTINATION include 
  FILES_MATCHING PATTERN "*.hpp")
