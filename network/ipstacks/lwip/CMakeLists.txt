cmake_minimum_required(VERSION 3.7)

project(lwIP)

set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# The minimum set of files needed for lwIP.
set(lwipcore_SRCS
    ${LWIP_DIR}/src/core/init.c
    ${LWIP_DIR}/src/core/def.c
    ${LWIP_DIR}/src/core/dns.c
    ${LWIP_DIR}/src/core/inet_chksum.c
    ${LWIP_DIR}/src/core/ip.c
    ${LWIP_DIR}/src/core/mem.c
    ${LWIP_DIR}/src/core/memp.c
    ${LWIP_DIR}/src/core/netif.c
    ${LWIP_DIR}/src/core/pbuf.c
    ${LWIP_DIR}/src/core/raw.c
    ${LWIP_DIR}/src/core/stats.c
    ${LWIP_DIR}/src/core/sys.c
    ${LWIP_DIR}/src/core/altcp.c
    ${LWIP_DIR}/src/core/altcp_alloc.c
    ${LWIP_DIR}/src/core/altcp_tcp.c
    ${LWIP_DIR}/src/core/tcp.c
    ${LWIP_DIR}/src/core/tcp_in.c
    ${LWIP_DIR}/src/core/tcp_out.c
    ${LWIP_DIR}/src/core/timeouts.c
    ${LWIP_DIR}/src/core/udp.c
)
set(lwipcore4_SRCS
    ${LWIP_DIR}/src/core/ipv4/autoip.c
    ${LWIP_DIR}/src/core/ipv4/dhcp.c
    ${LWIP_DIR}/src/core/ipv4/etharp.c
    ${LWIP_DIR}/src/core/ipv4/icmp.c
    ${LWIP_DIR}/src/core/ipv4/igmp.c
    ${LWIP_DIR}/src/core/ipv4/ip4_frag.c
    ${LWIP_DIR}/src/core/ipv4/ip4.c
    ${LWIP_DIR}/src/core/ipv4/ip4_addr.c
)
set(lwipcore6_SRCS
    ${LWIP_DIR}/src/core/ipv6/dhcp6.c
    ${LWIP_DIR}/src/core/ipv6/ethip6.c
    ${LWIP_DIR}/src/core/ipv6/icmp6.c
    ${LWIP_DIR}/src/core/ipv6/inet6.c
    ${LWIP_DIR}/src/core/ipv6/ip6.c
    ${LWIP_DIR}/src/core/ipv6/ip6_addr.c
    ${LWIP_DIR}/src/core/ipv6/ip6_frag.c
    ${LWIP_DIR}/src/core/ipv6/mld6.c
    ${LWIP_DIR}/src/core/ipv6/nd6.c
)

# Files implementing various generic network interface functions
set(lwipnetif_SRCS
    ${LWIP_DIR}/src/netif/ethernet.c
    ${LWIP_DIR}/src/netif/bridgeif.c
    ${LWIP_DIR}/src/netif/bridgeif_fdb.c
    ${LWIP_DIR}/src/netif/slipif.c
)

# APIFILES: The files which implement the sequential and socket APIs.
set(lwipapi_SRCS
    ${LWIP_DIR}/src/api/api_lib.c
    ${LWIP_DIR}/src/api/api_msg.c
    ${LWIP_DIR}/src/api/err.c
    ${LWIP_DIR}/src/api/if_api.c
    ${LWIP_DIR}/src/api/netbuf.c
    ${LWIP_DIR}/src/api/netdb.c
    ${LWIP_DIR}/src/api/netifapi.c
    ${LWIP_DIR}/src/api/sockets.c
    ${LWIP_DIR}/src/api/tcpip.c
)

# Define the lwIP library target
add_library(lwIP STATIC ${lwipcore_SRCS} ${lwipcore4_SRCS} ${lwipcore6_SRCS} ${lwipnetif_SRCS} ${lwipapi_SRCS})

# Include directories for the lwIP library
target_include_directories(lwIP PUBLIC
    ${LWIP_DIR}/src/include
    ${SDDF}/include
    ${BOARD_DIR}/include
    ${PICOLIBC_DIR}/include
    ${ECHO_SERVER}/include/lwip
)

# Package generation
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_PACKAGE_DESCRIPTION_SUMMARY "lwIP lightweight IP stack")
set(CPACK_PACKAGE_VERSION_MAJOR "${LWIP_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${LWIP_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${LWIP_VERSION_REVISION}")
set(CPACK_SOURCE_IGNORE_FILES "/build/;${CPACK_SOURCE_IGNORE_FILES};.git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "lwip-${LWIP_VERSION_MAJOR}.${LWIP_VERSION_MINOR}.${LWIP_VERSION_REVISION}")
include(CPack)

# Target for package generation
add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
# add_dependencies(dist lwipdocs)
