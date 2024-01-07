/** Connection handle for a UDP Server session */

#include "udp_perf_server.h"
#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t udp_control;

extern struct netif server_netif;
/* Report interval in ms */

#define REPORT_INTERVAL_TIME (INTERIM_REPORT_INTERVAL * 1000)

void print_app_header(void)
{
	xil_printf("UDP server listening on port %d\r\n",
			UDP_CONN_PORT);
	xil_printf("On Host: Run $iperf -c %s -i %d -t 300 -u -b <bandwidth>\r\n",
			inet_ntoa(server_netif.ip_addr),
			INTERIM_REPORT_INTERVAL);
}

/** Receive data on a udp session */
static void udp_recv_perf_traffic(int sock)  //*******************************************************************************//
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
       // recv_buf[count]= '\0';
        //xil_printf("%s",recv_buf);

        // Ensure buffer is null-terminated to safely print
        if (count < UDP_RECV_BUFSIZE) {
            recv_buf[count] = '\0';
        } else {
            recv_buf[UDP_RECV_BUFSIZE - 1] = '\0';
        }

        // Send data to the queue
        if(udp_control != NULL) {
            xQueueSend(udp_control, recv_buf, portMAX_DELAY);
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