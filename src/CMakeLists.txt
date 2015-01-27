# Dependencies 

## Boost
find_package(Boost 1.55 COMPONENTS system filesystem regex 
  date_time iostreams log REQUIRED) 
include_directories(AFTER ${Boost_INCLUDE_DIRS})
link_directories ( ${Boost_LIBRARY_DIRS} )

# Include our headers
include_directories(BEFORE ${CMAKE_CURRENT_LIST_DIR})

# Build a test ftp server
file(GLOB_RECURSE SERIAL_UTILS_SOURCE_FILES *.cpp)
add_executable(tftp-server ${SERIAL_UTILS_SOURCE_FILES})
target_link_libraries(tftp-server ${Boost_LIBRARIES})

INSTALL(TARGETS 
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

