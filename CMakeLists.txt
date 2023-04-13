cmake_minimum_required(VERSION 3.10)

project(SrvLib)

ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

# specify the C++ standard
set(SUPPORTED_CXX_STANDARDS 14 17)
if(NOT DEFINED CMAKE_CXX_STANDARD)
    message(STATUS "Setting C++ version to '14' as none was specified.")
    set(CMAKE_CXX_STANDARD 14)
else()
    message(STATUS "CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} set by parent project.")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(targetSrc
    ${CMAKE_CURRENT_LIST_DIR}/ServMain.cpp
)

if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC") OR WIN32)
list(APPEND targetSrc
    ${CMAKE_CURRENT_LIST_DIR}/SrvCtrl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/BaseSrv.cpp
)
endif()

add_library(srvlib STATIC ${targetSrc})

add_executable(ExampleSrv  ExampleSrv.cpp)
target_link_libraries(ExampleSrv srvlib)
if (NOT MSVC)
    target_link_libraries(ExampleSrv pthread)
endif()

file(READ init.d/examplesrv FILE_CONTENTS)
string(REPLACE "~" ${CMAKE_CURRENT_BINARY_DIR} NEW_FILE_CONTENTS ${FILE_CONTENTS})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/examplesrv ${NEW_FILE_CONTENTS})

file(READ example.service FILE_CONTENTS)
string(REPLACE "~" ${CMAKE_CURRENT_BINARY_DIR} NEW_FILE_CONTENTS ${FILE_CONTENTS})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/example.service ${NEW_FILE_CONTENTS})

install(TARGETS srvlib DESTINATION lib)
