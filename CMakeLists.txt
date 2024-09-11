# Copyright 2018, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.8.2)

project(libsddf C)

set(DRIV_DIR imx)
set(UART_DRIV_DIR imx)
set(TIMER_DRV_DIR imx)
set(CPU cortex-a53)

set(SDDF ${CMAKE_CURRENT_SOURCE_DIR})

set(ETHERNET_DRIVER ${SDDF}/drivers/network/${DRIV_DIR})

add_library(sddf STATIC EXCLUDE_FROM_ALL ${ETHERNET_DRIVER}/ethernet.c)
target_include_directories(sddf PUBLIC ${ETHERNET_DRIVER}/ethernet.h)