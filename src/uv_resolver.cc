#include <cstdio>
#include <uv.h>
#include "uv_resolver.h"

static void on_resolved(uv_getaddrinfo_t * resolver, int status, struct addrinfo * res) {
	uv_loop_t * loop = uv_default_loop();
	if (status == -1) {
		const char * error = uv_err_name(uv_last_error(loop));
		(void)error;
		return;
	}
	// Get IP from host
	char addr[17] = { '\n' };
	uv_ip4_name(reinterpret_cast<struct sockaddr_in*>(res->ai_addr), addr, 16);
	uv_freeaddrinfo(res);
	// Send IP back
	struct in_addr addr_ip[2];
	int addr_count = 1;
	inet_pton(AF_INET, addr, &addr_ip[0]);
	uv_resolver_data_t * data = static_cast<uv_resolver_data_t *>(resolver->data);
	int fd = data->pipes[0];
	if (send(fd, addr_ip, (addr_count + 1) * sizeof(struct in_addr), 0) != (int)((addr_count + 1) * sizeof(struct in_addr))) {
		// Unable to send data?
	}
}

int uv_resolver_start(int* fd, void** priv_data, const char* name) {
	uv_loop_t * loop = uv_default_loop();
	// Create temporary data for this query.
	uv_resolver_data_t * data = new uv_resolver_data_t;
	// Set up pipes.
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, data->pipes) == -1) {
		printf("nie\n");
		return -1;
	}
	// Spawn new UV resolver/
	uv_getaddrinfo_t resolver;
	resolver.data = data;
	int r = uv_getaddrinfo(loop, &resolver, on_resolved, name, NULL, NULL);
	if (r < 0) {
		// Unable to start DNS resolver.
		return r;
	}
	*fd = data->pipes[1];
	*priv_data = static_cast<void*>(data);
	return 0;
}

void uv_resolver_cleanup(void** priv_data, int force) {
	uv_resolver_data_t * data = static_cast<uv_resolver_data_t *>(*priv_data);
	// Close the pipes
	close(data->pipes[0]);
	close(data->pipes[1]);
	// Free up memory
	delete data;
}