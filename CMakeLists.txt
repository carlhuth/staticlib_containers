# Copyright 2015, alex at staticlibs.net
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required ( VERSION 2.8.12 )

# toolchain setup
set ( STATICLIB_TOOLCHAIN linux_amd64_gcc CACHE STRING "toolchain triplet" )
set ( CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/toolchains/${STATICLIB_TOOLCHAIN}.cmake CACHE INTERNAL "" )

# project setup
project ( staticlib_containers CXX )
set ( ${PROJECT_NAME}_STATICLIB_VERSION 1.2.0 )
# common include
include ( ${CMAKE_CURRENT_LIST_DIR}/cmake/staticlibs_common.cmake )

# tests and docs
set ( ${PROJECT_NAME}_TEST_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/include )
set ( ${PROJECT_NAME}_TEST_LIBS "" )
set ( ${PROJECT_NAME}_TEST_OPTS "" )
if ( STATICLIB_TOOLCHAIN MATCHES "linux_[^_]+_(clang|gcc)" )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} pthread )
endif ( )
staticlib_enable_testing ( ${PROJECT_NAME}_TEST_INCLUDES ${PROJECT_NAME}_TEST_LIBS ${PROJECT_NAME}_TEST_OPTS )
staticlib_enable_docs ( )

# export headers
set ( ${PROJECT_NAME}_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/include CACHE INTERNAL "" )