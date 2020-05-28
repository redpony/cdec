# Locate nanomsg.
# (This file is based on the original FindGTest.cmake file,
#  feel free to use it as it is or modify it for your own needs.)
#
#
# Defines the following variables:
#
#   NANOMSG_FOUND - Found the nanomsg library
#   NANOMSG_INCLUDE_DIR - Include directory
#
# Also defines the library variables below as normal
# variables.
#
#   NANOMSG_LIBRARIES - libnanomsg.so and libnanomsg.a
#
#-----------------------
# Example Usage:
#
#    find_package(nanomsg REQUIRED)
#    include_directories(${NANOMSG_INCLUDE_DIR})
#
#    add_executable(foo foo.cc)
#    target_link_libraries(foo ${NANOMSG_LIBRARIES})
#
#=============================================================================
# This file is released under the MIT licence:
#
# Copyright (c) 2011 Matej Svec
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#=============================================================================

find_path(NANOMSG_INCLUDE_DIR NAMES nanomsg/bus.h nanomsg/inproc.h nanomsg/ipc.h nanomsg/nn.h nanomsg/pair.h nanomsg/pipeline.h nanomsg/pubsub.h nanomsg/reqrep.h nanomsg/survey.h nanomsg/tcp.h)
mark_as_advanced(NANOMSG_INCLUDE_DIR)

find_library(NANOMSG_SHARED libnanomsg.so)
mark_as_advanced(NANOMSG_SHARED)

find_library(NANOMSG_STATIC libnanomsg.a)
mark_as_advanced(NANOMSG_STATIC)

if(NANOMSG_INCLUDE_DIR AND NANOMSG_SHARED AND NANOMSG_STATIC)
  set(NANOMSG_FOUND TRUE)
else()
  set(NANOMSG_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(nanomsg DEFAULT_MSG NANOMSG_SHARED NANOMSG_STATIC NANOMSG_INCLUDE_DIR)

if(NANOMSG_FOUND)
  set(NANOMSG_LIBRARIES ${NANOMSG_STATIC} ${NANOMSG_SHARED})
endif()
