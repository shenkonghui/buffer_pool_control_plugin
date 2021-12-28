# Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/client
  ${CMAKE_SOURCE_DIR}/include/mysql/psi
  ${CMAKE_SOURCE_DIR}/libmysql
   ${CMAKE_SOURCE_DIR}/sql
   ${CMAKE_SOURCE_DIR}/storage/perfschema
)

MYSQL_ADD_PLUGIN(buffer_pool_control buffer_pool_control.cc memory.cc MODULE_ONLY TEST_ONLY MODULE_OUTPUT_NAME "buffer_pool_control")

TARGET_LINK_LIBRARIES(buffer_pool_control libmysql)
