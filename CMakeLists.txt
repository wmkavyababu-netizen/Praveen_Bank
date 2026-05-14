cmake_minimum_required(VERSION 3.16)
project(praveen_bank)

# Toolchain (vcpkg)
set(CMAKE_TOOLCHAIN_FILE "C:/Lab programs/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain")

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Windows features
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601 -DNOMINMAX)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# Dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Crow setup
include_directories(${CMAKE_SOURCE_DIR}/Crow/include)

# Backend console app
add_executable(backend_console
    backend/src/backend.cpp
    backend/src/main_backend.cpp
)

# Main web server
add_executable(praveen_bank
    backend/src/backend.cpp
    backend/src/crow_backend.cpp
)

# Common include directories for both targets
target_include_directories(backend_console PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_include_directories(praveen_bank PRIVATE
    ${CMAKE_SOURCE_DIR}/Crow/include
    ${CMAKE_SOURCE_DIR}/backend/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Link libraries for both targets
foreach(target IN ITEMS backend_console praveen_bank)
    target_link_libraries(${target} PRIVATE
        ${Boost_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
    )
    
    if(WIN32)
        target_link_libraries(${target} PRIVATE
            ws2_32
            crypt32
        )
    endif()
endforeach()

# Copy frontend folder for web app
add_custom_command(TARGET praveen_bank POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:praveen_bank>/frontend
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/frontend
        $<TARGET_FILE_DIR:praveen_bank>/frontend
    COMMENT "Copying frontend files to build directory"
)

# Create accounts directory
add_custom_command(TARGET praveen_bank POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:praveen_bank>/accounts
    COMMENT "Creating accounts directory"
)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)