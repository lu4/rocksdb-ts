# Copyright (c) 2019 Geode-solutions
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.5)

project(rocks_db)

find_package(genepi REQUIRED PATHS ${PROJECT_SOURCE_DIR}/node_modules/@geode/genepi/build)

include_directories(../deps/rocksdb/rocksdb/include)
link_directories(/usr/local/Cellar/rocksdb/6.4.6/lib)

add_genepi_library(rocks_db
    "${PROJECT_SOURCE_DIR}/src/rocks_db/common.cpp"
    "${PROJECT_SOURCE_DIR}/src/rocks_db/functions.cpp"
    "${PROJECT_SOURCE_DIR}/src/rocks_db/objects.cpp"
)

target_link_libraries(rocks_db PRIVATE rocksdb)

