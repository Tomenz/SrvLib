cmake_minimum_required(VERSION 3.10)

project(SrvLib)

ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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

install(TARGETS srvlib DESTINATION lib)
#install(FILES SocketLib.h DESTINATION include)
