#include <node.h>
#include <v8.h>
#include "libgadu.h"
#include "session.h"

using namespace v8;

/**
 * Return version of libgadu.
 */
Handle<Value> Version(const v8::internal::Arguments& args) {
	HandleScope scope;
	return scope.Close(String::New(::gg_libgadu_version()));
}

extern "C" {

/**
 * Module initializer
 */
void init(Handle<Object> target) {
	NODE_SET_METHOD(target, "version", Version);
	// Events
	NODE_DEFINE_CONSTANT(target, GG_EVENT_CONN_SUCCESS);
	NODE_DEFINE_CONSTANT(target, GG_EVENT_CONN_FAILED);
	NODE_DEFINE_CONSTANT(target, GG_EVENT_MSG);
	NODE_DEFINE_CONSTANT(target, GG_EVENT_ACK);
	// Ack defines
	NODE_DEFINE_CONSTANT(target, GG_ACK_BLOCKED);
	NODE_DEFINE_CONSTANT(target, GG_ACK_DELIVERED);
	NODE_DEFINE_CONSTANT(target, GG_ACK_QUEUED);
	NODE_DEFINE_CONSTANT(target, GG_ACK_MBOXFULL);
	NODE_DEFINE_CONSTANT(target, GG_ACK_NOT_DELIVERED);
	// Message classes
	NODE_DEFINE_CONSTANT(target, GG_CLASS_QUEUED);
	NODE_DEFINE_CONSTANT(target, GG_CLASS_OFFLINE);
	NODE_DEFINE_CONSTANT(target, GG_CLASS_MSG);
	NODE_DEFINE_CONSTANT(target, GG_CLASS_CHAT);
	NODE_DEFINE_CONSTANT(target, GG_CLASS_CTCP);
	NODE_DEFINE_CONSTANT(target, GG_CLASS_ACK);
	NODE_DEFINE_CONSTANT(target, GG_CLASS_EXT);
	// Descriptions
	NODE_DEFINE_CONSTANT(target, GG_STATUS_NOT_AVAIL);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_NOT_AVAIL_DESCR);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FFC);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FFC_DESCR);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_AVAIL);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_AVAIL_DESCR);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_BUSY);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_BUSY_DESCR);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_DND);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_DND_DESCR);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_INVISIBLE);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_INVISIBLE_DESCR);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_BLOCKED);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_IMAGE_MASK);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_DESCR_MASK);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FRIENDS_MASK);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FLAG_UNKNOWN);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FLAG_VIDEO);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FLAG_MOBILE);
	NODE_DEFINE_CONSTANT(target, GG_STATUS_FLAG_SPAM);
	Session::Init(target);
}

}

NODE_MODULE(addon, init);
