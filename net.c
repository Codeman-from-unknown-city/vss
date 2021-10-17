#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <syslog.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "utils.h"

#define SAPC(ptr) (struct sockaddr*) ptr 
#define PORT 0x1001
#define NOBACKLOG 0
#define BLUETOOTH_MAGIC 0x28
#define FLUSH_TIMEOUT 1
#define MAX_PCKT_SIZE 65535
#define PCKT_SIZE_FIELD_TYPE uint16_t
#define MAX_DATA_SIZE MAX_PCKT_SIZE - sizeof(PCKT_SIZE_FIELD_TYPE)

int listenfd;
int connfd;

struct sockaddr_l2 loc_addr;
struct sockaddr_l2 rem_addr;
socklen_t addrlen = sizeof(struct sockaddr_l2);

static void init_loc_addr()
{
	memclr(&loc_addr, addrlen);
	loc_addr.l2_family = AF_BLUETOOTH;
	loc_addr.l2_bdaddr = *BDADDR_ANY;
	loc_addr.l2_psm = htobs(PORT);
}

static void setup_l2cap()
{
	EXIT_IF_SYSCALL_FAILS(listenfd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP));
	init_loc_addr();
	EXIT_IF_SYSCALL_FAILS(bind(listenfd, SAPC(&loc_addr), addrlen));
	EXIT_IF_SYSCALL_FAILS(listen(listenfd, NOBACKLOG));
} 

static void change_mtu()
{
	struct l2cap_options opts;
	socklen_t optlen = sizeof(opts);
	memclr(&opts, optlen);
	EXIT_IF_SYSCALL_FAILS(getsockopt(listenfd, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen));
	opts.omtu = opts.imtu = MAX_PCKT_SIZE;
	EXIT_IF_SYSCALL_FAILS(setsockopt(listenfd, SOL_L2CAP, L2CAP_OPTIONS, &opts, optlen));
}

static void set_flush_timeout(bdaddr_t* ba)
{
	struct hci_request rq;
	memclr(&rq, sizeof(rq));
	struct {
		uint16_t handle;
		uint16_t flush_timeout;
	} cmd_param;
	struct {
		uint16_t handle;
		uint16_t status;
	} cmd_resp;
	struct hci_conn_info_req *cr = malloc(sizeof(struct hci_conn_info_req) + 
					sizeof(struct hci_conn_info));
	bacpy(&cr->bdaddr, ba);
	cr->type = ACL_LINK;
	int dd;
	EXIT_IF_SYSCALL_FAILS(dd = hci_open_dev(hci_get_route(&cr->bdaddr)));
	EXIT_IF_SYSCALL_FAILS(ioctl(dd, HCIGETCONNINFO, cr));
	cmd_param.handle = cr->conn_info->handle;
	cmd_param.flush_timeout = htobs(FLUSH_TIMEOUT);
	rq.ogf = OGF_HOST_CTL;
	rq.ocf = BLUETOOTH_MAGIC; 
	rq.cparam = &cmd_param;
	rq.clen = sizeof(cmd_param);
	rq.rparam = &cmd_resp;
	rq.rlen = sizeof(cmd_resp);
	rq.event = EVT_CMD_COMPLETE;
	EXIT_IF_SYSCALL_FAILS(hci_send_req(dd, &rq, 0));
	if (cmd_resp.status) {
		errno = bt_error(cmd_resp.status);
		die("hci_send_req");
	}
	close(dd);
	free(cr);
}


void setup_server()
{
	setup_l2cap();
	change_mtu();
}

bool conn_reset = true;

void wait_connection()
{
	addrlen = sizeof(rem_addr);
	EXIT_IF_SYSCALL_FAILS(connfd = accept(listenfd, SAPC(&rem_addr), &addrlen));
	set_flush_timeout(&rem_addr.l2_bdaddr);
	conn_reset = false;
}

bool client_connected()
{
	return !conn_reset;
}

struct __attribute__((__packed__)) pckt {
	PCKT_SIZE_FIELD_TYPE size;
	void* data[MAX_DATA_SIZE];
};

void send_msg(void* data, size_t size)
{
	if (size > MAX_DATA_SIZE) {
		syslog(LOG_WARNING, "send_msg: msg too big (%u)", size);
		return;
	}
	static struct pckt pckt;
	pckt.size = size;
	memcpy(pckt.data, data, size);
	ssize_t ns = 0;
	ssize_t prev  = 0;
	size += sizeof(PCKT_SIZE_FIELD_TYPE);
	while (ns < size) {
		ns += send(connfd, &pckt + ns, size - ns, MSG_NOSIGNAL);
		if (ns < prev) { 
			if (errno == EPIPE || errno == ECONNRESET) {
				conn_reset = true;
				return;
			}
			die("send");
		}	
		prev = ns;
	}
}
