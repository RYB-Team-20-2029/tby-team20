# Purpose: top-level build. Each board builds its own module: cmake -B build -DRYB_MODULE=<name>.
# Owning module: management (mgmt/).

cmake_minimum_required(VERSION 3.19)
project(tby_team20 LANGUAGES C)

enable_testing()
add_subdirectory(include/ryb)
