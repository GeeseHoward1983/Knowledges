cmake_minimum_required(VERSION 3.10)
project(ECDSASign)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Miracl库
find_path(MIRACL_INCLUDE_DIR 
    NAMES miracl.h
    PATHS /usr/include /usr/local/include /opt/local/include
    ENV MIRACL_PATH
    PATH_SUFFIXES miracl
)

find_library(MIRACL_LIBRARIES 
    NAMES miracl libmiracl
    PATHS /usr/lib /usr/local/lib /opt/local/lib
    ENV MIRACL_PATH
)

if(NOT MIRACL_LIBRARIES OR NOT MIRACL_INCLUDE_DIR)
    # 如果找不到预编译的库，则添加源码编译
    message(WARNING "未找到预编译的Miracl库，添加源码编译...")
    
    # 添加Miracl源码文件
    set(MIRACL_SOURCES 
        ${CMAKE_SOURCE_DIR}/miracl/miracl_osmt.c
        ${CMAKE_SOURCE_DIR}/miracl/big.c
        ${CMAKE_SOURCE_DIR}/miracl/ecurve.c
    )
    
    add_library(miracl_static ${MIRACL_SOURCES})
    target_include_directories(miracl_static PUBLIC 
        ${CMAKE_SOURCE_DIR}/miracl
    )
    
    set(MIRACL_LIBRARIES miracl_static)
    set(MIRACL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/miracl)
endif()

add_executable(ecdsa_sign ecdsa_miracl.cpp)
add_executable(ecdsa_verify ecdsa_verify.cpp)
add_executable(recover_k recover_k.cpp)

target_include_directories(ecdsa_sign PRIVATE ${MIRACL_INCLUDE_DIR})
target_link_libraries(ecdsa_sign ${MIRACL_LIBRARIES})

target_include_directories(ecdsa_verify PRIVATE ${MIRACL_INCLUDE_DIR})
target_link_libraries(ecdsa_verify ${MIRACL_LIBRARIES})

target_include_directories(recover_k PRIVATE ${MIRACL_INCLUDE_DIR})
target_link_libraries(recover_k ${MIRACL_LIBRARIES})

# 如果使用的是Windows平台，可能需要额外的库
if(WIN32)
    target_link_libraries(ecdsa_sign ws2_32)
    target_link_libraries(ecdsa_verify ws2_32)
    target_link_libraries(recover_k ws2_32)
endif()