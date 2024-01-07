/*
 * Copyright (C) 2017 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

/** Connection handle for a UDP Server session */

#include "udp_perf_server.h"
#include "FreeRTOS.h"
#include "queue.h"

extern struct netif server_netif;
extern QueueHandle_t xQueue;

/* Report interval in ms */
#define REPORT_INTERVAL_TIME (INTERIM_REPORT_INTERVAL 1000)

void print_app_header(void)
{
    xil_printf("UDP server listening on port %d\r\n",
            UDP_CONN_PORT);
}

static void udp_recv_perf_traffic(int sock)
{
    int count;
    char recv_buf[UDP_RECV_BUFSIZE];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        if((count = lwip_recvfrom(sock, recv_buf, UDP_RECV_BUFSIZE, 0,
                (struct sockaddr *)&from, &fromlen)) <= 0) {
            continue;
        }

        // Ensure buffer is null-terminated to safely print
        if (count < UDP_RECV_BUFSIZE) {
            recv_buf[count] = '\0';
        } else {
            recv_buf[UDP_RECV_BUFSIZE - 1] = '\0';
        }

        // Send data to the queue
        if(xQueue != NULL) {
            xQueueSend(xQueue, recv_buf, portMAX_DELAY);
        }
    }
}

void start_application(void)
{
    err_t err;
    int sock;
    struct sockaddr_in addr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        xil_printf("UDP server: Error creating Socket\r\n");
        return;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_CONN_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (err != ERR_OK) {
        xil_printf("UDP server: Error on bind: %d\r\n", err);
        close(sock);
        return;
    }

    udp_recv_perf_traffic(sock);
}
