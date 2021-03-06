#include <node.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <v8.h>
#include <uv.h>
#include <nan.h>
#include "session.h"
#include "helpers.h"
#include "uv_resolver.h"

using namespace v8;

Session::Session()
	: session_(0)
	, poll_fd_(0)
	, timer_poll_(0) {
}

Session::~Session() {
}


Persistent<Function> Session::constructor;

void Session::Init(Local<Object> exports) {
	Isolate* isolate = exports->GetIsolate();
	
	// Prepare constructor template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
	tpl->SetClassName(String::NewFromUtf8(isolate, "Session"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	// Prototype
	NODE_SET_PROTOTYPE_METHOD(tpl, "login", Login);
	NODE_SET_PROTOTYPE_METHOD(tpl, "logoff", Logoff);
	NODE_SET_PROTOTYPE_METHOD(tpl, "send", SendMessage);
	NODE_SET_PROTOTYPE_METHOD(tpl, "notify", Notify);
	NODE_SET_PROTOTYPE_METHOD(tpl, "changeStatus", ChangeStatus);

	constructor.Reset(isolate, tpl->GetFunction());
  	exports->Set(String::NewFromUtf8(isolate, "Session"), tpl->GetFunction());
}


void Session::New(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();
	Session* obj = new Session();
	obj->Wrap(args.This());

	// Install global DNS resolver
//	if (gg_global_set_custom_resolver(uv_resolver_start, uv_resolver_cleanup) < 0) {
//		const char* error = strerror(errno);
//		args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, error)));
//	}

	args.GetReturnValue().Set(args.This());
}

void Session::Login(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();
	Session* obj = ObjectWrap::Unwrap<Session>(args.This());
	struct gg_login_params p;
	memset(&p, 0, sizeof(struct gg_login_params));
	p.uin = args[0]->NumberValue();

	String::Utf8Value passwordArg(args[1]->ToString());
	p.password = *passwordArg;
	p.async = 1;
	p.protocol_features = GG_FEATURE_IMAGE_DESCR;
	p.encoding = GG_ENCODING_UTF8;
	
	// Save persistent callback
	obj->login_callback_.Reset(isolate, Local<Function>::Cast(args[2]));
	
	// Do login
	struct ::gg_session* sess = ::gg_login(&p);
	
	if (!sess) {
		const char* error = strerror(errno);
		args.GetReturnValue().Set(isolate->ThrowException(String::NewFromUtf8(isolate, error)));
	}
	
	obj->session_ = sess;
	obj->login_params_ = p;
	
	// Start polling
	obj->poll_fd_ = static_cast<uv_poll_t*>(malloc(sizeof(uv_poll_t)));
	uv_poll_init(uv_default_loop(), obj->poll_fd_, sess->fd);
	obj->poll_fd_->data = obj;
	
	// Setup ping timer.
	obj->timer_poll_ = new uv_timer_t();
	uv_timer_init(uv_default_loop(), obj->timer_poll_);
	obj->timer_poll_->data = obj;
	uv_timer_start(obj->timer_poll_, (uv_timer_cb) &Session::ping_callback, 0, 60000);

	// Watch for R/W
	if ((sess->check & GG_CHECK_READ)) {
		uv_poll_start(obj->poll_fd_, UV_READABLE, gadu_perform);
	}
	
	if ((sess->check & GG_CHECK_WRITE)) {
		uv_poll_start(obj->poll_fd_, UV_WRITABLE, gadu_perform);
	}
	
	args.GetReturnValue().Set(args.This());
}

void Session::SendMessage(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();
	Session* obj = ObjectWrap::Unwrap<Session>(args.This());
	unsigned long uin = args[0]->NumberValue();
	String::Utf8Value messageTextArg(args[1]->ToString());
	unsigned char* text = reinterpret_cast<unsigned char*>(*messageTextArg);
	int seq = gg_send_message(obj->session_, GG_CLASS_MSG, uin, text);
	
	if (seq < 0) {
		obj->disconnect();
	}
	
	args.GetReturnValue().Set(Number::New(isolate, seq));
}

void Session::Notify(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();
	Session* obj = ObjectWrap::Unwrap<Session>(args.This());
	struct gg_session* sess = obj->session_;

	// Convert v8::Array of Numbers to std::vector.
	std::vector<uin_t> contacts;
	
	if ((args.Length() == 1) && args[0]->IsArray()) {
		Handle<Array> values = Handle<Array>::Cast(args[0]);
		std::vector<uin_t> vec(values->Length());
		
		for (unsigned int i = 0; i < values->Length(); i++) {
			Local<Value> index = Number::New(isolate, i);
			Local<Value> value = values->Get(index);
			
			if (!value->IsNumber()) {
				args.GetReturnValue().Set(isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid uin"))));
			}
			
			uin_t uin = value->ToObject()->Uint32Value();
			vec[i] = uin;
		}
		
		contacts.swap(vec);
	}
	
	// Notify server with contact list.
	if (gg_notify(sess, contacts.data(), contacts.size()) < 0) {
		obj->disconnect();
	}
	
	args.GetReturnValue().Set(args.This());
}

void Session::Logoff(const FunctionCallbackInfo<Value>& args) {
	Session* obj = ObjectWrap::Unwrap<Session>(args.This());
	struct gg_session* sess = obj->session_;
	gg_logoff(sess);
	uv_poll_stop(obj->poll_fd_);
	uv_close((uv_handle_t*)obj->poll_fd_, (uv_close_cb)free);
	uv_timer_stop(obj->timer_poll_);
	uv_close((uv_handle_t*)obj->timer_poll_, (uv_close_cb)free);
	gg_free_session(sess);
	args.GetReturnValue().Set(args.This());
}

void Session::ChangeStatus(const FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();
	Session* obj = ObjectWrap::Unwrap<Session>(args.This());
	struct gg_session* sess = obj->session_;
	int result = 0;
	
	if (args.Length() <= 2) {
		// Status
		int status = args[0]->NumberValue();
		
		if (args.Length() == 2) {
			// Description is optional
			if (!args[1]->IsString()) {
				args.GetReturnValue().Set(isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "String required"))));
			}

			String::Utf8Value statusArg(args[1]->ToString());
			const char* message = *statusArg;
			result = gg_change_status_descr(sess, status, message);
		} else {
			result = gg_change_status(sess, status);
		}
		
		if (result < 0) {
			obj->disconnect();
		}
		
		args.GetReturnValue().Set(args.This());

		return;
	}
    
    args.GetReturnValue().Set(isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Invalid arguments"))));
}

void Session::gadu_perform(uv_poll_t* req, int status, int events) {
	Isolate* isolate = Isolate::GetCurrent();
	Session* obj = static_cast<Session*>(req->data);
	struct gg_session* sess = obj->session_;
	Nan::HandleScope scope;
	
	if (sess && ((events & UV_READABLE) || (events & UV_WRITABLE) || (sess->timeout == 0 && sess->soft_timeout))) {	
		struct gg_event* e = 0;
		raii_destructor<struct gg_event> destructor(e, &gg_free_event);
		
		if (!(e = gg_watch_fd(sess))) {
			// In case of error, event value passed is Undefined
			const unsigned argc = 1;
			Local<Value> argv[argc] = { Nan::Undefined() };
			Nan::Callback callback;
 			callback.SetFunction(Local<Function>::New(isolate, obj->login_callback_));
			callback.Call(argc, argv);
			obj->disconnect();
			return;
		}
		
		// Construct a new object with the events data.
		Local<Object> event = Object::New(isolate);
		NODE_SET_ATTRIBUTE(isolate, event, "type", Number::New(isolate, e->type));
		Local<Object> target = Object::New(isolate);
		
		switch (e->type) {
			case GG_EVENT_CONN_FAILED:
				gg_logoff(sess);
				obj->disconnect();
				return;
			case GG_EVENT_MSG: {
				// Received message.
				NODE_SET_ATTRIBUTE(isolate, target, "sender", Number::New(isolate, e->event.msg.sender));
				NODE_SET_ATTRIBUTE(isolate, target, "msgclass", Number::New(isolate, e->event.msg.msgclass));
				NODE_SET_ATTRIBUTE(isolate, target, "time", Number::New(isolate, e->event.msg.time));
				Local<Array> recipients = Array::New(isolate, e->event.msg.recipients_count);
            
				for (int i = 0; i < e->event.msg.recipients_count; i++) {
					recipients->Set(Number::New(isolate, i), Number::New(isolate, *(e->event.msg.recipients + i)));
				}
            
				NODE_SET_ATTRIBUTE(isolate, target, "recipients", recipients);
            
				// TODO:
				// formats_length
				// formats
				NODE_SET_ATTRIBUTE(isolate, target, "seq", Number::New(isolate, e->event.msg.seq));
				
                char* xhtml_message = reinterpret_cast<char*>(e->event.msg.xhtml_message);
            
                if (!xhtml_message) {
                    NODE_SET_ATTRIBUTE(isolate, target, "xhtml_message", Nan::Null());
                } else {
                    NODE_SET_ATTRIBUTE(isolate, target, "xhtml_message", String::NewFromUtf8(isolate, xhtml_message));
                }
			
				char* message = reinterpret_cast<char*>(e->event.msg.message);
            
                if (!message) {
                    NODE_SET_ATTRIBUTE(isolate, target, "message", Nan::Null());
                } else {
                    NODE_SET_ATTRIBUTE(isolate, target, "message", String::NewFromUtf8(isolate, message));
                }
            
				break;
			}
			case GG_EVENT_ACK: {
				// Message is acknowledged.
				NODE_SET_ATTRIBUTE(isolate, target, "recipient", Number::New(isolate, e->event.ack.recipient));
				NODE_SET_ATTRIBUTE(isolate, target, "status", Number::New(isolate, e->event.ack.status));
				NODE_SET_ATTRIBUTE(isolate, target, "seq", Number::New(isolate, e->event.ack.seq));
				break;
			}
			default:
				break;
		}
		
		// Add target event details to the event object.
		event->Set(String::NewFromUtf8(isolate, "target"), target);
		
		// Call the callback with newly created object.
		const unsigned argc = 1;
		Local<Value> argv[argc] = { Local<Value>::New(isolate, event) };
		Nan::Callback callback;
		callback.SetFunction(Local<Function>::New(isolate, obj->login_callback_));
		callback.Call(argc, argv);
	}
    
	// Watch for R/W again
	if ((sess->check & GG_CHECK_READ)) {
		uv_poll_start(obj->poll_fd_, UV_READABLE, gadu_perform);
    }
    
	if ((sess->check & GG_CHECK_WRITE)) {
		uv_poll_start(obj->poll_fd_, UV_WRITABLE, gadu_perform);
    }
}

void Session::ping_callback(uv_timer_t* timer, int status) {
	Session* obj = static_cast<Session*>(timer->data);
    
	if (gg_ping(obj->session_) < 0) {
		return;
	}	
}

void Session::disconnect() {
	if (poll_fd_) {
		uv_poll_stop(poll_fd_);
		uv_close((uv_handle_t*)poll_fd_, (uv_close_cb)free);
	}
    
	if (timer_poll_) {
		uv_timer_stop(timer_poll_);
		uv_close((uv_handle_t*)timer_poll_, (uv_close_cb)free);
	}
}
