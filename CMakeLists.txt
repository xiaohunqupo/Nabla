# Copyright (C) 2018-2025 - DevSH Graphics Programming Sp. z O.O.
# This file is part of the "Nabla Engine".
# For conditions of distribution and use, see copyright notice in nabla.h.in or nabla.h
cmake_minimum_required(VERSION 3.31)

# TODO: Yas - once we deploy 4.x we will fire `cmake_policy(VERSION <min>[...<max>])` instead of manually picking policies
# https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html#policy-version
# also we should update deps which throw warnings about < 3.10 compatibility

macro(NBL_POLICY P S)
if(POLICY ${P})
	cmake_policy(SET ${P} ${S})
	set(CMAKE_POLICY_DEFAULT_${P} ${S})
endif()
endmacro()

NBL_POLICY(CMP0003 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0003.html#cmp0003
NBL_POLICY(CMP0077 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0077.html#cmp0077
NBL_POLICY(CMP0112 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0112.html#cmp0112
NBL_POLICY(CMP0141 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0141.html#policy:CMP0141
NBL_POLICY(CMP0118 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0118.html#policy:CMP0118

set(NBL_BUILD_ANDROID OFF)

include(ExternalProject)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/android/build.cmake)

project(Nabla LANGUAGES CXX C)
enable_language(C CXX ASM ASM_NASM)

if(MSVC)
	enable_language(ASM_MASM)
	link_libraries(delayimp)
endif()

# TODO: TO BE KILLED, keep both in one tree
option(NBL_STATIC_BUILD "" OFF) # ON for static builds, OFF for shared

option(NBL_COMPILER_DYNAMIC_RUNTIME "" ON)
option(NBL_SANITIZE_ADDRESS OFF)

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT $<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>) # ignored on non xMSVC-ABI targets

if(NBL_STATIC_BUILD)
	message(STATUS "Static Nabla build enabled!")
else()
	if(NOT NBL_COMPILER_DYNAMIC_RUNTIME)
		message(FATAL_ERROR "Turn NBL_COMPILER_DYNAMIC_RUNTIME on! For dynamic Nabla builds dynamic runtime is mandatory!")
	endif()
endif()

find_package(Vulkan)
if (Vulkan_FOUND)
	message(STATUS "Found Vulkan SDK")
	cmake_path(GET Vulkan_INCLUDE_DIR PARENT_PATH VULKAN_SDK)
else()
	message(STATUS "Vulkan SDK is not found")
endif()

option(NBL_COMPILE_WITH_CUDA "Compile with CUDA interop?" OFF)

if(NBL_COMPILE_WITH_CUDA)
	find_package(CUDAToolkit REQUIRED)
	if(${CUDAToolkit_VERSION} VERSION_GREATER "9.0")
		message(STATUS "CUDA version 9.0+ found!")
	else()
		message(FATAL_ERROR "CUDA version 9.0+ needed for C++14 support!")
	endif()
endif()

get_filename_component(NBL_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(NBL_ROOT_PATH_BINARY "${CMAKE_CURRENT_BINARY_DIR}" ABSOLUTE)
get_filename_component(NBL_PYTHON_MODULE_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests" ABSOLUTE)
set(NBL_BUILTIN_RESOURCES_DIRECTORY_PATH "${NBL_ROOT_PATH}/include/nbl/builtin")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${NBL_ROOT_PATH}/cmake" CACHE PATH "")
get_property(NBL_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

include(common)

option(NBL_ENABLE_VS_CONFIG_IMPORT "Request import of .vsconfig file with VS components required to build Nabla" OFF)

if(NBL_ENABLE_VS_CONFIG_IMPORT)
	NBL_IMPORT_VS_CONFIG()
endif()

# Configure CCache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  option(USE_CCACHE "Some dependency is using it, but I just don't know which!" ON)
else(CCACHE_FOUND)
  option(USE_CCACHE "Some dependency is using it, but I just don't know which!" OFF)
endif(CCACHE_FOUND)
if(USE_CCACHE)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(USE_CCACHE)

# global IDE stuff for examples
set(CMAKE_CODELITE_USE_TARGETS ON CACHE BOOL "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Cause CUDA is a special snowflake
if (NBL_COMPILE_WITH_CUDA)
	set(CMAKE_CUDA_STANDARD 14)
	set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()
set(CMAKE_CXX_EXTENSIONS OFF) #...without compiler extensions like gnu++11, but is it really needed?

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;")
set(LLVM_USE_CRT_DEBUG MTd CACHE STRING "Nabla developer note: No idea what subfolder provides this")
set(LLVM_USE_CRT_RELEASE MT CACHE STRING "Nabla developer note: No idea what subfolder provides this")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type: Debug or Release" FORCE)
endif()

option(NBL_MEMORY_CONSUMPTION_CHECK_SKIP "Turn it ON to bypass memory consumption test given _NBL_JOBS_AMOUNT_. Be aware you are doing it on your own risk of potential build failures!" ON)
option(NBL_JOBS_OVERRIDE "Override jobs with safe bias if required to respect currently available memory" OFF)

include(ProcessorCount)

if(NOT DEFINED _NBL_JOBS_AMOUNT_)
	if(NOT "$ENV{_NBL_JOBS_AMOUNT_}" STREQUAL "")
		set(_NBL_JOBS_AMOUNT_ "$ENV{_NBL_JOBS_AMOUNT_}")
	else()
		ProcessorCount(_NBL_JOBS_AMOUNT_)
		if(_NBL_JOBS_AMOUNT_ EQUAL 0)
			set(_NBL_JOBS_AMOUNT_ 1)
		endif()
	endif()
endif()

cmake_host_system_information(RESULT _NBL_TOTAL_PHYSICAL_MEMORY_ QUERY TOTAL_PHYSICAL_MEMORY) # MB
cmake_host_system_information(RESULT _NBL_AVAILABLE_PHYSICAL_MEMORY_ QUERY AVAILABLE_PHYSICAL_MEMORY) # MB

if(NBL_JOBS_OVERRIDE)
	math(EXPR _CI_NBL_JOBS_AMOUNT_ "(${_NBL_AVAILABLE_PHYSICAL_MEMORY_} - 512)/(2*1024)") # override with safe bias, respect memory and don't take more then max processors we have
	if(_CI_NBL_JOBS_AMOUNT_ LESS _NBL_JOBS_AMOUNT_)
		message(WARNING "Overriding _NBL_JOBS_AMOUNT_: \"${_NBL_JOBS_AMOUNT_}\" with \"${_CI_NBL_JOBS_AMOUNT_}\"")
	
		set(_NBL_JOBS_AMOUNT_ "${_CI_NBL_JOBS_AMOUNT_}")
	endif()
endif()

message(STATUS "_NBL_JOBS_AMOUNT_: \"${_NBL_JOBS_AMOUNT_}\"")

math(EXPR _NBL_DEBUG_MEMORY_CONSUPTION_WITH_ALL_JOBS_ "${_NBL_JOBS_AMOUNT_}*2*1024") # MB
math(EXPR _NBL_CURRENTLY_USED_PHYSICAL_MEMORY_ "${_NBL_TOTAL_PHYSICAL_MEMORY_}-${_NBL_AVAILABLE_PHYSICAL_MEMORY_}") # MB

if(_NBL_AVAILABLE_PHYSICAL_MEMORY_ LESS_EQUAL _NBL_DEBUG_MEMORY_CONSUPTION_WITH_ALL_JOBS_) # TODO: we may also add Release and RWDI checks as well
	if(NBL_MEMORY_CONSUMPTION_CHECK_SKIP)
		set(_NBL_CMAKE_STATUS_ WARNING)
	else()
		set(_NBL_CMAKE_STATUS_ FATAL_ERROR)
	endif()
	
	message(${_NBL_CMAKE_STATUS_} "Memory consumption issue detected! To protect you from compile and linker errors, please read this message.\n\nYour total physical memory is ${_NBL_TOTAL_PHYSICAL_MEMORY_} MBs, your OS is currently using ${_NBL_CURRENTLY_USED_PHYSICAL_MEMORY_} MBs and consumption of your memory with requested ${_NBL_JOBS_AMOUNT_} jobs in Debug configuration may be around ${_NBL_DEBUG_MEMORY_CONSUPTION_WITH_ALL_JOBS_} MBs. Please override '_NBL_JOBS_AMOUNT_' variable by setting it as cache variable and lower the jobs! If you want to continue anyway, please define 'NBL_MEMORY_CONSUMPTION_CHECK_SKIP' but be aware - you are doing it on your own risk of possible build failures.")
endif()

include(adjust/flags)
include(adjust/definitions)

if(WIN32)
	set(_NBL_PLATFORM_WINDOWS_ 1)
elseif(ANDROID)
	set(_NBL_PLATFORM_ANDROID_ 1)
elseif(UNIX)
	set(_NBL_PLATFORM_LINUX_ 1)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")
endif()

if(NBL_STATIC_BUILD)
	unset(_NBL_SHARED_BUILD_ CACHE)
else()
	set(_NBL_SHARED_BUILD_ ON CACHE INTERNAL "")
endif()

option(NBL_BUILD_DPL "Enable DPL (Dynamic Parallelism Library)" OFF)
option(NBL_PCH "Enable pre-compiled header" ON)
option(NBL_FAST_MATH "Enable fast low-precision math" ON)
option(NBL_BUILD_EXAMPLES "Enable building examples" ON)
option(NBL_BUILD_MITSUBA_LOADER "Enable nbl::ext::MitsubaLoader?" OFF) # TODO: once it compies turn this ON by default!
option(NBL_BUILD_IMGUI "Enable nbl::ext::ImGui?" ON)

option(NBL_BUILD_OPTIX "Enable nbl::ext::OptiX?" OFF)
if(NBL_COMPILE_WITH_CUDA)
	find_package(OPTIX REQUIRED)
	message(STATUS "CUDA enabled and OptiX found!")
else()
	if(NBL_BUILD_OPTIX)
		message(FATAL_ERROR "You cannot build Optix without enabled CUDA! NBL_COMPILE_WITH_CUDA must be ON!")
	endif()
endif()

option(NBL_BUILD_BULLET "Enable Bullet Physics building and integration?" OFF)
option(NBL_BUILD_DOCS "Enable building documentation?" OFF) # No one has doxygen installed, plus we dont know when was the last time we generated working doxy and we'll use SphinX in the future
option(NBL_ENABLE_PROJECT_JSON_CONFIG_VALIDATION "" ON)
option(NBL_EMBED_BUILTIN_RESOURCES "Embed built-in resources?" OFF)
option(NBL_ENABLE_DOCKER_INTEGRATION "Enables docker integration, if client is not found Docker Desktop will be installed" OFF)

if (NBL_ENABLE_DOCKER_INTEGRATION)
	if (NOT CMAKE_HOST_WIN32)
		message(FATAL_ERROR "NBL_ENABLE_DOCKER_INTEGRATION works only with Windows host machines. Please disable this option")
	endif()

	find_program(DOCKER_EXECUTABLE docker REQUIRED)

	if (DOCKER_EXECUTABLE)
		message(STATUS "Found docker executable: ${DOCKER_EXECUTABLE}")
	else()
		execute_process(COMMAND powershell -Command "& {
			Invoke-WebRequest -Uri https://desktop.docker.com/win/main/amd64/Docker%20Desktop%20Installer.exe -OutFile DockerDesktopInstaller.exe;
			$exitCode = Start-Process -Wait -FilePath .\\DockerDesktopInstaller.exe -ArgumentList 'install','--quiet','--accept-license','--backend=wsl-2','--always-run-service';
			Remove-Item -Force DockerDesktopInstaller.exe;
			exit $exitCode;
		}" RESULT_VARIABLE DOCKER_INSTALLER_EXIT_CODE)

		if (DOCKER_INSTALLER_EXIT_CODE NOT EQUAL 0)
			message(FATAL_ERROR "Docker installer exited with non-zero return code")
		endif()
	endif()
	
	message(STATUS "Docker endpoint healty check..")
	set(DDT_MAX_ATTEMPTS 10)
	set(DDT_DOCKER_RESPONDING False)
	foreach(ATTEMPT RANGE 1 ${DDT_MAX_ATTEMPTS})
		message(STATUS "Attempt ${ATTEMPT} of ${DDT_MAX_ATTEMPTS}: Checking Docker Endpoint")
		NBL_DOCKER(info)

		if (DOCKER_EXIT_CODE EQUAL 0)
			set(DDT_DOCKER_RESPONDING True)
			message(STATUS "Docker endpoint detected, the engine is running.")
			message(STATUS "${DOCKER_OUTPUT_VAR}")
			break()
		endif()

		NBL_WAIT_FOR(5)
	endforeach()

	if (NOT DDT_DOCKER_RESPONDING)
		message(FATAL_ERROR "Docker Daemon is not responding. Please make sure it's running in the background!")
	endif()

	set(DNT_NETWORK_NAME "docker_default")
	NBL_DOCKER(network inspect ${DNT_NETWORK_NAME})
	if (NOT DOCKER_EXIT_CODE EQUAL 0)
		message(STATUS "Default docker network doesn't exist, creating \"${DNT_NETWORK_NAME}\" network..")
		NBL_DOCKER(network create -d nat ${DNT_NETWORK_NAME})
		
		if (NOT DOCKER_EXIT_CODE EQUAL 0)
			message(STATUS "Could not create default docker \"${DNT_NETWORK_NAME}\" network!")
		endif()
	endif()
	
	message(STATUS "Default docker network: \"${DNT_NETWORK_NAME}\"")
endif()

set(THIRD_PARTY_SOURCE_DIR "${PROJECT_SOURCE_DIR}/3rdparty")
set(THIRD_PARTY_BINARY_DIR "${PROJECT_BINARY_DIR}/3rdparty")

set(NBL_MEDIA_DIRECTORY "${PROJECT_SOURCE_DIR}/examples_tests/media")
get_filename_component(NBL_MEDIA_DIRECTORY_ABS "${NBL_MEDIA_DIRECTORY}" ABSOLUTE)

include(submodules/update)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_subdirectory(${THIRD_PARTY_SOURCE_DIR})

add_subdirectory(src/nbl)
add_subdirectory("${NBL_PYTHON_MODULE_ROOT_PATH}" tests) # Python Framework
if(NBL_BUILD_EXAMPLES)
	file(LOCK "${CMAKE_CURRENT_SOURCE_DIR}/examples_tests" DIRECTORY GUARD PROCESS RESULT_VARIABLE NBL_LOCK TIMEOUT 60)
	add_subdirectory(examples_tests EXCLUDE_FROM_ALL)
	file(LOCK "${CMAKE_CURRENT_SOURCE_DIR}/examples_tests" DIRECTORY RELEASE RESULT_VARIABLE NBL_LOCK)
endif()
add_subdirectory(tools)

if(NBL_BUILD_DOCS)
	add_subdirectory(docs)
endif()

add_subdirectory(artifacts)

option(NBL_CPACK_INCLUDE_EXAMPLES "CPack with examples and media" ON)
include(cpack/package)
include(build/info)
export(TARGETS ${_NBL_3RDPARTY_TARGETS_} Nabla NAMESPACE Nabla:: APPEND FILE ${NBL_ROOT_PATH_BINARY}/NablaExport.cmake)
NBL_ADJUST_FOLDERS(nabla)