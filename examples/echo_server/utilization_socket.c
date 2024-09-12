/*
 * Copyright 2020, Data61, CSIRO
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <string.h>
#include <microkit.h>

#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include <sddf/util/util.h>
#include <sddf/benchmark/bench.h>
#include <sddf/util/printf.h>

#include "echo.h"

#define START_PMU 4
#define STOP_PMU 5

#define MAX_PACKET_SIZE 0x1000

uintptr_t cyclecounters_vaddr;


/* This file implements a TCP based utilization measurment process that starts
 * and stops utilization measurements based on a client's requests.
 * The protocol used to communicate is as follows:
 * - Client connects
 * - Server sends: 100 IPBENCH V1.0\n
 * - Client sends: HELLO\n
 * - Server sends: 200 OK (Ready to go)\n
 * - Client sends: LOAD cpu_target_lukem\n
 * - Server sends: 200 OK\n
 * - Client sends: SETUP args::""\n
 * - Server sends: 200 OK\n
 * - Client sends: START\n
 * - Client sends: STOP\n
 * - Server sends: 220 VALID DATA (Data to follow)\n
 *                                Content-length: %d\n
 *                                ${content}\n
 * - Server closes socket.
 *
 * It is also possible for client to send QUIT\n during operation.
 *
 * The server starts recording utilization stats when it receives START and
 * finishes recording when it receives STOP.
 *
 * Only one client can be connected.
 */

static struct tcp_pcb *utiliz_socket;

#define WHOAMI "100 IPBENCH V1.0\n"
#define HELLO "HELLO\n"
#define OK_READY "200 OK (Ready to go)\n"
#define LOAD "LOAD cpu_target_lukem\n"
#define OK "200 OK\n"
#define SETUP "SETUP args::\"\"\n"
#define START "START\n"
#define STOP "STOP\n"
#define QUIT "QUIT\n"
#define RESPONSE "220 VALID DATA (Data to follow)\n"    \
    "Content-length: %d\n"                              \
    "%s\n"
#define IDLE_FORMAT ",%ld,%ld"
#define ERROR "400 ERROR\n"
#define KBD "KBD\n"

#define msg_match(msg, match) (strncmp(msg, match, strlen(match))==0)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define RES(x, y, z) "220 VALID DATA (Data to follow)\n"    \
    "Content-length: "STR(x)"\n"\
    ","STR(y)","STR(z)


struct bench *bench;

struct tcb_pcb *curr_pcb;
#define BUFLEN 100
char kbd_buf[BUFLEN];
int buf_index = 0;

uint64_t start;
uint64_t idle_ccount_start;

char data_packet_str[MAX_PACKET_SIZE];


static inline void my_reverse(char s[])
{
    unsigned int i, j;
    char c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

static inline void my_itoa(uint64_t n, char s[])
{
    unsigned int i;
    uint64_t sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    my_reverse(s);
}

static err_t utilization_sent_callback(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    return ERR_OK;
}

static err_t utilization_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    if (p == NULL) {
        tcp_close(pcb);
        return ERR_OK;
    }

    pbuf_copy_partial(p, (void *)data_packet_str, p->tot_len, 0);
    err_t error;

    if (msg_match(data_packet_str, HELLO)) {
        error = tcp_write(pcb, OK_READY, strlen(OK_READY), TCP_WRITE_FLAG_COPY);
        if (error) printf("Failed to send OK_READY message through utilization peer\n");
    } else if (msg_match(data_packet_str, LOAD)) {
        error = tcp_write(pcb, OK, strlen(OK), TCP_WRITE_FLAG_COPY);
        if (error) printf("Failed to send OK message through utilization peer\n");
    } else if (msg_match(data_packet_str, SETUP)) {
        error = tcp_write(pcb, OK, strlen(OK), TCP_WRITE_FLAG_COPY);
        if (error) printf("Failed to send OK message through utilization peer\n");
    } else if (msg_match(data_packet_str, START)) {
        printf("%s measurement starting... \n", microkit_name);
        if (!strcmp(microkit_name, "client0")) {
            start = __atomic_load_n(&bench->ts, __ATOMIC_RELAXED);
            idle_ccount_start = __atomic_load_n(&bench->ccount, __ATOMIC_RELAXED);
            microkit_notify(START_PMU);
        }
    } else if (msg_match(data_packet_str, STOP)) {
        printf("%s measurement finished \n", microkit_name);

        uint64_t total = 0, idle = 0;

        if (!strcmp(microkit_name, "client0")) {
            total = __atomic_load_n(&bench->ts, __ATOMIC_RELAXED) - start;
            idle = __atomic_load_n(&bench->ccount, __ATOMIC_RELAXED) - idle_ccount_start;
        }

        char tbuf[21];
        my_itoa(total, tbuf);

        char ibuf[21];
        my_itoa(idle, ibuf);

        /* Message format: ",total,idle\0" */
        int len = strlen(tbuf) + strlen(ibuf) + 3;
        char lbuf[16];
        my_itoa(len, lbuf);

        char buffer[120];
        strcat(strcpy(buffer, "220 VALID DATA (Data to follow)\nContent-length: "), lbuf);
        strcat(buffer, "\n,");
        strcat(buffer, ibuf);
        strcat(buffer, ",");
        strcat(buffer, tbuf);
        
        error = tcp_write(pcb, buffer, strlen(buffer) + 1, TCP_WRITE_FLAG_COPY);
        tcp_shutdown(pcb, 0, 1);

        if (!strcmp(microkit_name, "client0")) microkit_notify(STOP_PMU);
    } else if (msg_match(data_packet_str, QUIT)) {
        /* Do nothing for now */
    } else if (msg_match(data_packet_str, KBD)) {
        if (curr_pcb) {
            printf("sent '%s'\n", kbd_buf);
            char* str = "RECEIVED: '";
            error = tcp_write(curr_pcb, str, strlen(str), TCP_WRITE_FLAG_COPY);
            error = tcp_write(curr_pcb, kbd_buf, strlen(kbd_buf), TCP_WRITE_FLAG_COPY);
            str = "'\n";
            error = tcp_write(curr_pcb, str, strlen(str), TCP_WRITE_FLAG_COPY);
            for (int i = 0; i < 20; i++) {
                kbd_buf[i] = '\0';
            }
            buf_index = 0;
            if (error) {
                printf("Failed to send OK message through utilization peer");
            }
        } else {
            curr_pcb = pcb;
            printf("Keyboard primed\n");
            char* return_string = "Keyboard ready for input\n";
            error = tcp_write(pcb, return_string, strlen(return_string), TCP_WRITE_FLAG_COPY);
            if (error) {
                printf("Failed to send OK message through utilization peer");
            }
        }
    } else {
        printf("Received a message that we can't handle %s\n", data_packet_str);
        error = tcp_write(pcb, ERROR, strlen(ERROR), TCP_WRITE_FLAG_COPY);
        if (error) printf("Failed to send OK message through utilization peer\n");
    }

    return ERR_OK;
}

static err_t utilization_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    printf("Utilization connection established!\n");
    err_t error = tcp_write(newpcb, WHOAMI, strlen(WHOAMI), TCP_WRITE_FLAG_COPY);
    if (error) printf("Failed to send WHOAMI message through utilization peer\n");
    tcp_sent(newpcb, utilization_sent_callback);
    tcp_recv(newpcb, utilization_recv_callback);

    return ERR_OK;
}

int setup_utilization_socket(void)
{
    bench = (void *)cyclecounters_vaddr;
    utiliz_socket = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (utiliz_socket == NULL) {
        printf("Failed to open a socket for listening!\n");
        return -1;
    }

    err_t error = tcp_bind(utiliz_socket, IP_ANY_TYPE, UTILIZATION_PORT);
    if (error) {
        printf("Failed to bind the TCP socket");
        return -1;
    }

    utiliz_socket = tcp_listen_with_backlog_and_err(utiliz_socket, 1, &error);
    if (error != ERR_OK) {
        printf("Failed to listen on the utilization socket\n");
        return -1;
    }
    tcp_accept(utiliz_socket, utilization_accept_callback);

    return 0;
}

int send_keypress(char c) {
    if (curr_pcb) {
        if (buf_index < BUFLEN) {
            kbd_buf[buf_index] = c;
            buf_index++;
            /* kbd_buf[buf_index++] = '\0'; */
        } else {
            printf("\nbuffer full (max 7 chars)!\n");
        }
        // int error = tcp_write(curr_pcb, str, strlen(str), TCP_WRITE_FLAG_COPY);
    } else {
        printf("keyboard not ready, type KBD in terminal\n");
    }
    return 0;
}
