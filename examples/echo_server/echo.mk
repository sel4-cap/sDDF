#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

QEMU := qemu-system-aarch64

MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit
ECHO_SERVER:=${SDDF}/examples/echo_server
LWIPDIR:=network/ipstacks/lwip/src
BENCHMARK:=$(SDDF)/benchmark
UTIL:=$(SDDF)/util
ETHERNET_DRIVER:=$(SDDF)/drivers/network/$(DRIV_DIR)
ETHERNET_CONFIG_INCLUDE:=${ECHO_SERVER}/include/ethernet_config
SERIAL_COMPONENTS := $(SDDF)/serial/components
UART_DRIVER := $(SDDF)/drivers/serial/$(UART_DRIV_DIR)
SERIAL_CONFIG_INCLUDE:=${ECHO_SERVER}/include/serial_config
TIMER_DRIVER:=$(SDDF)/drivers/timer/$(TIMER_DRV_DIR)
NETWORK_COMPONENTS:=$(SDDF)/network/components
BITTY:=${ECHO_SERVER}/BittyHTTP/src
PICOLIBC_DIR := ${ECHO_SERVER}/picolibc_build/picolibc/aarch64-linux-gnu

BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)
SYSTEM_FILE := ${ECHO_SERVER}/board/$(MICROKIT_BOARD)/echo_server.system
IMAGE_FILE := loader.img
REPORT_FILE := report.txt

vpath %.c ${SDDF} ${ECHO_SERVER}

IMAGES := eth_driver.elf lwip.elf benchmark.elf idle.elf network_virt_rx.elf\
	  network_virt_tx.elf copy.elf timer_driver.elf uart_driver.elf serial_virt_tx.elf web_server.elf

CFLAGS := -mcpu=$(CPU) \
	  -mstrict-align \
	  -ffreestanding \
	  -g3 -O3 -Wall \
	  -Wno-unused-function \
	  -DMICROKIT_CONFIG_$(MICROKIT_CONFIG) \
	  -I$(BOARD_DIR)/include \
	  -I$(SDDF)/include \
	  -I${ECHO_INCLUDE}/lwip \
	  -I${ETHERNET_CONFIG_INCLUDE} \
	  -I$(SERIAL_CONFIG_INCLUDE) \
	  -I${SDDF}/$(LWIPDIR)/include \
	  -I${SDDF}/$(LWIPDIR)/include/ipv4 \
	  -MD \
	  -MP

CFLAGS_PICO :=-mcpu=$(CPU) \
	  -mstrict-align \
	  -ffreestanding \
	  -g3 -O3 -Wall \
	  -Wno-unused-function \
	  -DMICROKIT_CONFIG_$(MICROKIT_CONFIG) \
	  -I$(BOARD_DIR)/include \
	  -I$(SDDF)/include \
	  -I${BITTY} \
	  -I${PICOLIBC_DIR}/include \
	  -MD \
	  -MP

LDFLAGS := -L$(BOARD_DIR)/lib -L${LIBC}
PICO_LDFLAGS := -L$(BOARD_DIR)/lib
PICO_LIBS := --start-group -lmicrokit -Tmicrokit.ld -L${PICOLIBC_DIR}/lib -lgcc -lc -lm -lgcc libsddf_util_debug.a --end-group
LIBS := --start-group -lmicrokit -Tmicrokit.ld -lc libsddf_util_debug.a --end-group

CHECK_FLAGS_BOARD_MD5:=.board_cflags-$(shell echo -- ${CFLAGS} ${BOARD} ${MICROKIT_CONFIG} | shasum | sed 's/ *-//')

${CHECK_FLAGS_BOARD_MD5}:
	-rm -f .board_cflags-*
	touch $@

%.elf: %.o
	$(LD) $(LDFLAGS) $< $(LIBS) -o $@

include ${SDDF}/${LWIPDIR}/Filelists.mk

# NETIFFILES: Files implementing various generic network interface functions
# Override version in Filelists.mk
NETIFFILES:=$(LWIPDIR)/netif/ethernet.c

# LWIPFILES: All the above.
LWIPFILES=lwip.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)
LWIP_OBJS := $(LWIPFILES:.c=.o) lwip.o utilization_socket.o \
	     udp_echo_socket.o tcp_echo_socket.o

OBJS := $(LWIP_OBJS)
DEPS := $(filter %.d,$(OBJS:.o=.d))

BITTY_SOURCES := ${BITTY}/WebServer.c ${BITTY}/SocketsConMaaxboard.c ${BITTY}/main_bitty.c 
BITTY_OBJS := $(patsubst %.c, %.o, $(notdir $(BITTY_SOURCES)))

all: loader.img

$(info The value of BUILD_DIR is $(BITTY_OBJS))

%.o: ${BITTY}/%.c
	$(CC) -c $(CFLAGS_PICO) $< -o $@


# ${BUILD_DIR}/main.o: $(BITTY)/main.c 
# 	$(CC) -c $(CFLAGS) $< -o $@

# ${BUILD_DIR}/SocketsCon.o: $(BITTY)/SocketsCon.c 
# 	$(CC) -c $(CFLAGS) $< -o $@

# ${BUILD_DIR}/SocketsCon.o: $(BITTY)/WebServer.c 
# 	$(CC) -c $(CFLAGS) $< -o $@

FileServer.o: $(BITTY)/Examples/HelloWorld/FileServer.c 
	$(CC) -c $(CFLAGS_PICO) $< -o $@

${LWIP_OBJS}: ${CHECK_FLAGS_BOARD_MD5}
lwip.elf: $(LWIP_OBJS) libsddf_util.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

web_server.elf: FileServer.o ${BITTY_OBJS} libsddf_util.a
	$(LD) $(PICO_LDFLAGS) $^ $(PICO_LIBS) -o $@

LWIPDIRS := $(addprefix ${LWIPDIR}/, core/ipv4 netif api)
${LWIP_OBJS}: |${BUILD_DIR}/${LWIPDIRS}
${BUILD_DIR}/${LWIPDIRS}:
	mkdir -p $@

# Need to build libsddf_util_debug.a because it's included in LIBS
# for the unimplemented libc dependencies
${IMAGES}: libsddf_util_debug.a

${IMAGE_FILE} $(REPORT_FILE): $(IMAGES) $(SYSTEM_FILE)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)


include ${SDDF}/util/util.mk
include ${SDDF}/network/components/network_components.mk
include ${ETHERNET_DRIVER}/eth_driver.mk
include ${BENCHMARK}/benchmark.mk
include ${TIMER_DRIVER}/timer_driver.mk
include ${UART_DRIVER}/uart_driver.mk
include ${SERIAL_COMPONENTS}/serial_components.mk

qemu: $(IMAGE_FILE)
	$(QEMU) -machine virt,virtualization=on \
			-cpu cortex-a53 \
			-serial mon:stdio \
			-device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 \
			-m size=2G \
			-nographic \
			-device virtio-net-device,netdev=netdev0 \
			-netdev user,id=netdev0,hostfwd=tcp::1236-:1236,hostfwd=tcp::1237-:1237,hostfwd=udp::1235-:1235 \
			-global virtio-mmio.force-legacy=false \
			-d guest_errors

clean::
	${RM} -f *.elf .depend* $
	find . -name \*.[do] |xargs --no-run-if-empty rm

clobber:: clean
	rm -f *.a
	rm -f ${IMAGE_FILE} ${REPORT_FILE}

-include $(DEPS)
