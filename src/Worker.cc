#include <string.h>
#include <node.h>
#include <v8.h>
#include <uv.h>
#include "Worker.h"
#include "util.h"
#include "NativeUtil.h"

using namespace v8;

const uint64_t timeOrigin = uv_hrtime();

uv_thread_t main_thread;
bool inThread() {
  uv_thread_t this_thread = uv_thread_self();
  return !uv_thread_equal(&main_thread, &this_thread);
}

Persistent<Function> Worker::constructor;

const char* preload = "";

void SetPreload(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  String::Utf8Value preload_utf8(isolate, info[0].As<String>());
  preload = strdup(*preload_utf8);
}

void Worker::Init(Local<Object> exports) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
  tpl->SetClassName(String::NewFromUtf8(isolate, "Worker"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(tpl, "getPromise", GetPromise);
  NODE_SET_PROTOTYPE_METHOD(tpl, "getId", GetId);
  NODE_SET_PROTOTYPE_METHOD(tpl, "getState", GetState);
  NODE_SET_PROTOTYPE_METHOD(tpl, "send", Send);
  NODE_SET_PROTOTYPE_METHOD(tpl, "terminate", Terminate);
  NODE_SET_PROTOTYPE_METHOD(tpl, "checkOutgoingMessages", CheckOutgoingMessages);
  NODE_SET_PROTOTYPE_METHOD(tpl, "lock", Lock);
  NODE_SET_PROTOTYPE_METHOD(tpl, "unlock", Unlock);

  Local<Object> constants = Object::New(isolate);
#define V(name, val) \
  constants->Set(String::NewFromUtf8(isolate, name), Integer::New(isolate, val));
  V("created", Worker::State::created);
  V("running", Worker::State::running);
  V("terminated", Worker::State::terminated);
#undef V
  exports->Set(String::NewFromUtf8(isolate, "constants"), constants);

  exports->Set(String::NewFromUtf8(isolate, "setPreload"), FunctionTemplate::New(isolate, SetPreload)->GetFunction());

  Local<Function> fn = tpl->GetFunction();
  constructor.Reset(isolate, fn);
  exports->Set(String::NewFromUtf8(isolate, "Worker"), fn);
}

uint32_t WorkerId = 0;

Worker::Worker(Local<Promise::Resolver> resolver, Worker::Source source) {
  Isolate* isolate = Isolate::GetCurrent();

  this->source_ = source;
  this->persistent_.Reset(isolate, resolver);
  this->id = WorkerId++;

  this->async_.data = this;
  uv_async_init(uv_default_loop(), &this->async_, Worker::WorkCallback);

  uv_thread_t* thread = new uv_thread_t;
  uv_thread_create(thread, Worker::WorkThread, this);
}

Worker::~Worker() {
  this->persistent_.Reset();
}

void Worker::New(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  main_thread = uv_thread_self();

  Local<Context> context = isolate->GetCurrentContext();

  if (info.IsConstructCall()) {
    Local<Promise::Resolver> local = Promise::Resolver::New(isolate);

    Worker::Source source;

    String::Utf8Value code_utf8(isolate, info[0].As<String>());
    char* buffer = strdup(*code_utf8);
    source.code = buffer;

    source.arguments = serialize(isolate, info[1].As<Array>());

    Local<Object> references = info[2].As<Object>();
    Local<Array> refkeys = references->GetPropertyNames().As<Array>();
    source.references_length = refkeys->Length() > 128 ? 128 : refkeys->Length();
    for (int i = 0; i < source.references_length; i++) {
      String::Utf8Value ref_utf8(isolate, refkeys->Get(i).As<String>());
      char* copy = strdup(*ref_utf8);
      source.references[i] = copy;
    }

    Worker* worker = new Worker(local, source);
    worker->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
  } else {
    const int argc = 2;
    Local<Value> argv[argc] = { info[0], info[1] };
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    info.GetReturnValue().Set(cons->NewInstance(context, argc, argv).ToLocalChecked());
  }
}

void Worker::WorkThread(void *arg) {
  Worker* worker = (Worker*) arg; // work->data;

  worker->state = Worker::State::running;

  Worker::Source source = worker->source_;

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
    ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);

  {
    Locker locker(isolate);
    HandleScope scope(isolate);

    Local<ObjectTemplate> tpl = ObjectTemplate::New(isolate);
    tpl->SetInternalFieldCount(1);
    Local<Context> context = Context::New(isolate, nullptr, tpl);

    Context::Scope context_scope(context);
    TryCatch try_catch(isolate);
#define CHECK_ERR() \
    if (try_catch.HasCaught()) {                                        \
      worker->error_ = serialize(isolate, try_catch.Message()->Get());  \
      worker->state = Worker::State::terminated;                        \
      uv_async_send(&worker->async_);                                   \
      return;                                                           \
    }

    Local<Object> global = context->Global();
    global->SetAlignedPointerInInternalField(0, worker);
    USE(global->Set(context, String::NewFromUtf8(isolate, "global"), global));

    USE(global->Set(context, String::NewFromUtf8(isolate, "_console"), FunctionTemplate::New(isolate, ThreadConsole)->GetFunction()));

    Local<Object> perf = Object::New(isolate);
#define V(name, val) \
    USE(perf->Set(context, String::NewFromUtf8(isolate, name), val))

    V("_hrtime",FunctionTemplate::New(isolate, ThreadHrtime)->GetFunction());
    V("timeOrigin", Number::New(isolate, timeOrigin / 1e6));
#undef V
    USE(global->Set(context, String::NewFromUtf8(isolate, "performance"), perf));

    USE(global->Set(context, String::NewFromUtf8(isolate, "_util"), MakeNativeUtil(isolate)));

    CHECK_ERR();

    ScriptOrigin origin(String::NewFromUtf8(isolate, "Thread"), // file name
                        Integer::New(isolate, 0),               // line offset
                        Integer::New(isolate, 0),               // column offset
                        False(isolate),                         // is cross origin
                        Local<Integer>(),                       // script id
                        Local<Value>(),                         // source map URL
                        False(isolate),                         // is opaque (?)
                        False(isolate),                         // is WASM
                        False(isolate));                        // is ES6 module

    USE(Script::Compile(context, String::NewFromUtf8(isolate, preload)).ToLocalChecked()->Run(context));

    Local<String> code = String::NewFromUtf8(isolate, source.code);
    MaybeLocal<Script> maybe_script = Script::Compile(context, code, &origin);

    CHECK_ERR();

    if (maybe_script.IsEmpty())
      return;

    MaybeLocal<Value> maybe_result = maybe_script.ToLocalChecked()->Run(context);

    CHECK_ERR();

    if (maybe_result.IsEmpty())
      return;

    Local<Array> props = deserialize(isolate, source.arguments).As<Array>();

    Local<Value> args[props->Length() + 1];
    for (uint32_t i = 0; i < props->Length(); i++)
      args[i] = props->Get(context, i).ToLocalChecked();

    Local<Object> coms = Object::New(isolate);
#define V(name, fn) \
    USE(coms->Set(context, String::NewFromUtf8(isolate, name), FunctionTemplate::New(isolate, fn)->GetFunction()))

    V("on", ThreadOn);
    V("send", Send);
    V("terminate", Terminate);
    V("lock", Lock);
    V("unlock", Unlock);
#undef V
    USE(coms->Set(context, String::NewFromUtf8(isolate, "id"), Integer::New(isolate, worker->id)));

    /*
    Local<Object> references = Object::New(isolate);
    for (int i = 0; i < source.references_length; i++)
      USE(references->Set(context, String::NewFromUtf8(isolate, source.references[i]), FunctionTemplate::New(isolate, Noop)->GetFunction()));

    USE(coms->Set(context, String::NewFromUtf8(isolate, "references"), references));
    */

    args[props->Length()] = coms;

    Local<Function> fn = maybe_result.ToLocalChecked().As<Function>();
    maybe_result = fn->Call(context, global, props->Length() + 1, args);

    CHECK_ERR();

    if (maybe_result.IsEmpty())
      return;

    MaybeLocal<Value> maybe_onmessage =
      global->GetPrivate(context, Private::ForApi(isolate, String::NewFromUtf8(isolate, "WorkerOnMessage")));

    CHECK_ERR();

    if (!maybe_onmessage.IsEmpty()) {
      Local<Value> onmessage_ = maybe_onmessage.ToLocalChecked();

      CHECK_ERR();

      if (onmessage_->IsFunction()) {
        Local<Function> onmessage = onmessage_.As<Function>();
        while (true) {
          if (worker->state != Worker::State::running)
            break;

          if (worker->inQueue_.empty())
            continue;

          SerializedData data = worker->inQueue_.pop();

          Local<Value> value = deserialize(isolate, data);
          Local<Value> argv[1] = {value};
          USE(onmessage->Call(context, global, 1, argv));
          isolate->RunMicrotasks();
        }
      }
    }
    worker->result_ = serialize(isolate, maybe_result.ToLocalChecked());
  }

#undef CHECK_ERR

  worker->state = Worker::State::terminated;

  isolate->Dispose();

  uv_async_send(&worker->async_);
}

void Worker::WorkCallback(uv_async_t* async) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  Worker* worker = (Worker*) async->data;

  TryCatch try_catch(isolate);

  Local<Promise::Resolver> local = Local<Promise::Resolver>::New(isolate, worker->persistent_);

  if (worker->error_.first != nullptr) {
    Local<String> message = deserialize(isolate, worker->error_).As<String>();
    local->Reject(Exception::Error(message));
  } else {
    Local<Value> value = deserialize(isolate, worker->result_);
    local->Resolve(value);
  }

  if (try_catch.HasCaught())
    local->Reject(try_catch.Exception());

  isolate->RunMicrotasks();

  uv_close((uv_handle_t*) async, NULL);
}

Worker* Worker::GetWorker(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  if (!inThread())
    return ObjectWrap::Unwrap<Worker>(info.Holder());

  Local<Object> global = isolate->GetCurrentContext()->Global();
  return static_cast<Worker*>(global->GetAlignedPointerFromInternalField(0));
}

void Worker::GetPromise(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  Local<Promise::Resolver> local = Local<Promise::Resolver>::New(isolate, worker->persistent_);

  info.GetReturnValue().Set(local->GetPromise());
}

void Worker::GetId(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  info.GetReturnValue().Set(Integer::New(isolate, worker->id));
}

void Worker::GetState(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  info.GetReturnValue().Set(Integer::New(isolate, worker->state));
}

void Worker::Send(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  SerializedData data = serialize(isolate, info[0]);

  Worker* worker = GetWorker(info);
  if (inThread())
    worker->outQueue_.push(data);
  else
    worker->inQueue_.push(data);

  info.GetReturnValue().Set(Undefined(isolate));
}

// called using `process.nextTick`
void Worker::CheckOutgoingMessages(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = ObjectWrap::Unwrap<Worker>(info.Holder());

  if (!worker)
    return;

  Local<Context> context = isolate->GetCurrentContext();
 
  Local<Array> out = info[0].As<Array>();

  if (worker->outQueue_.empty())
    return;

  SerializedData data = worker->outQueue_.pop();

  Local<Value> value = deserialize(isolate, data);

  USE(out->Set(context, 0, True(isolate)));
  USE(out->Set(context, 1, value));
}

void Worker::Terminate(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  worker->state = Worker::State::terminated;

  info.GetReturnValue().Set(Undefined(isolate));
}

void Worker::Lock(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  info.GetReturnValue().Set(Boolean::New(isolate, worker->js_mutex_.try_lock()));
}

void Worker::Unlock(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  worker->js_mutex_.unlock();

  info.GetReturnValue().Set(True(isolate));
}

void Worker::ThreadOn(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

  if(!info[0].As<String>()->Equals(String::NewFromUtf8(isolate, "message")))
    return;

  Local<Function> onmessage = info[1].As<Function>();
  context->Global()->SetPrivate(context, Private::ForApi(isolate, String::NewFromUtf8(isolate, "WorkerOnMessage")), onmessage);

  info.GetReturnValue().Set(Undefined(isolate));
}

void Worker::ThreadConsole(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  String::Utf8Value method(isolate, info[0]);
  String::Utf8Value utf8(isolate, info[1]);

  Worker* worker = GetWorker(info);

  const char* fmt = "[Worker %02d] %s\n";
  FILE* out = strcmp(*method, "error") == 0 ? stderr : stdout;
  fprintf(out, fmt, worker->id, *utf8);

  info.GetReturnValue().Set(Undefined(isolate));
}

void Worker::ThreadHrtime(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  uint64_t t = uv_hrtime() - timeOrigin;

  Local<ArrayBuffer> ab = info[0].As<Uint32Array>()->Buffer();
  uint32_t* fields = static_cast<uint32_t*>(ab->GetContents().Data());

  uint32_t NANOS_PER_SEC = 1000000000;
  fields[0] = (t / NANOS_PER_SEC) >> 32;
  fields[1] = (t / NANOS_PER_SEC) & 0xffffffff;
  fields[2] = t % NANOS_PER_SEC;
}
