#ifndef SESSION_H
#define SESSION_H

#include <node.h>
#include <ctime>
#include "libgadu.h"

class Session : public node::ObjectWrap {
public:
	static void Init(v8::Handle<v8::Object> target);

private:
	Session();
	~Session();

	static v8::Handle<v8::Value> New(const v8::Arguments& args);
	/**
	 * Connect to a server.
	 */
	static v8::Handle<v8::Value> Login(const v8::Arguments& args);

	/**
	 * Low-level handle.
	 */
	struct gg_session * session_;
	/**
	 * Login params.
	 */
	struct gg_login_params login_params_;
	/**
	 * FD poller
	 */
	uv_poll_t * poll_fd_;

	/**
	 * Poller handler
	 */
	static void gadu_perform(uv_poll_t *req, int status, int events);

	time_t now_, last_;
};

#endif