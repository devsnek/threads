#include <string.h>
#include <node.h>
#include <v8.h>
#include <uv.h>
#include "Worker.h"
#include "util.h"

using namespace v8;

uv_thread_t main_thread;

Persistent<Function> Worker::constructor;

void Worker::Init(Local<Object> exports) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
  tpl->SetClassName(String::NewFromUtf8(isolate, "Worker"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(tpl, "getPromise", GetPromise);
  NODE_SET_PROTOTYPE_METHOD(tpl, "isRunning", IsRunning);
  NODE_SET_PROTOTYPE_METHOD(tpl, "send", Send);
  NODE_SET_PROTOTYPE_METHOD(tpl, "terminate", Terminate);
  NODE_SET_PROTOTYPE_METHOD(tpl, "checkOutgoingMessages", CheckOutgoingMessages);
  NODE_SET_PROTOTYPE_METHOD(tpl, "lock", Lock);
  NODE_SET_PROTOTYPE_METHOD(tpl, "unlock", Unlock);

  constructor.Reset(isolate, tpl->GetFunction());
  exports->Set(String::NewFromUtf8(isolate, "Worker"), tpl->GetFunction());
}

int WorkerId = 0;

Worker::Worker(Local<Promise::Resolver> resolver, Worker::Source source) {
  Isolate* isolate = Isolate::GetCurrent();

  this->source = source;
  this->persistent.Reset(isolate, resolver);
  this->id = WorkerId++;

  uv_work_t *work = new uv_work_t;
  work->data = this;

  uv_queue_work(uv_default_loop(), work, Worker::WorkThread, Worker::WorkCallback);
}

Worker::~Worker() {
  this->persistent.Reset();
}

void Worker::New(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope handle(isolate);

  main_thread = uv_thread_self();

  Local<Context> context = isolate->GetCurrentContext();

  if (info.IsConstructCall()) {
    Local<Promise::Resolver> local = Promise::Resolver::New(isolate);

    Worker::Source source;

    String::Utf8Value code_utf8(isolate, info[0].As<String>());
    char* buffer = strdup(*code_utf8);
    source.code = buffer;

    String::Utf8Value preload_utf8(isolate, info[2].As<String>());
    buffer = strdup(*preload_utf8);
    source.preload = buffer;

    source.arguments = serialize(isolate, info[1].As<Array>());

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

bool MaybeHandleException(Isolate* isolate, TryCatch* try_catch, Worker* worker) {
  HandleScope scope(isolate);

  if (!try_catch->HasCaught())
    return false;

  worker->error = serialize(isolate, try_catch->Message()->Get());
  worker->running = false;

  return true;
}

void Worker::WorkThread(uv_work_t* work) {
  Worker* worker = (Worker*) work->data;

  worker->running = true;

  Worker::Source source = worker->source;

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

    Local<Object> global = context->Global();
    global->SetAlignedPointerInInternalField(0, worker);
    USE(global->Set(context, String::NewFromUtf8(isolate, "global"), global));

    USE(global->Set(context, String::NewFromUtf8(isolate, "console_"), FunctionTemplate::New(isolate, ThreadConsole)->GetFunction()));

    if (MaybeHandleException(isolate, &try_catch, worker))
      return;

    ScriptOrigin origin(String::NewFromUtf8(isolate, "Thread"), // file name
                        Integer::New(isolate, 0),               // line offset
                        Integer::New(isolate, 0),               // column offset
                        False(isolate),                         // is cross origin
                        Local<Integer>(),                       // script id
                        Local<Value>(),                         // source map URL
                        False(isolate),                         // is opaque (?)
                        False(isolate),                         // is WASM
                        False(isolate));                        // is ES6 module

    USE(Script::Compile(context, String::NewFromUtf8(isolate, source.preload)).ToLocalChecked()->Run(context));

    Local<String> code = String::NewFromUtf8(isolate, source.code);
    Local<Script> script = Script::Compile(context, code, &origin).ToLocalChecked();
    MaybeLocal<Value> maybe_result = script->Run(context);

    if (MaybeHandleException(isolate, &try_catch, worker))
      return;

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
    args[props->Length()] = coms;

    Local<Function> fn = maybe_result.ToLocalChecked().As<Function>();
    maybe_result = fn->Call(context, global, props->Length() + 1, args);

    if (MaybeHandleException(isolate, &try_catch, worker))
      return;

    if (maybe_result.IsEmpty())
      return;

    MaybeLocal<Value> maybe_onmessage =
      global->GetPrivate(context, Private::ForApi(isolate, String::NewFromUtf8(isolate, "WorkerOnMessage")));

    if (!maybe_onmessage.IsEmpty()) {
      Local<Value> onmessage_ = maybe_onmessage.ToLocalChecked();
      if (onmessage_->IsFunction()) {
        Local<Function> onmessage = onmessage_.As<Function>();
        while (true) {
          if (!worker->running)
            break;

          if (worker->inQueue_.empty())
            continue;

          SerializedData data = worker->inQueue_.pop();

          Local<Value> value = deserialize(isolate, data);
          Local<Value> argv[1] = {value};
          USE(onmessage->Call(context, global, 1, argv));
        }
      }
    }

    worker->result = serialize(isolate, maybe_result.ToLocalChecked());
  }

  worker->running = false;

  isolate->Dispose();
}

void Worker::WorkCallback(uv_work_t *work, int status) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle(isolate);

  Worker* worker = (Worker*) work->data;

  TryCatch try_catch(isolate);

  Local<Promise::Resolver> local = Local<Promise::Resolver>::New(isolate, worker->persistent);

  if (worker->error.first != nullptr) {
    Local<String> message = deserialize(isolate, worker->error).As<String>();
    local->Reject(Exception::Error(message));
  } else {
    Local<Value> value = deserialize(isolate, worker->result);
    local->Resolve(value);
 }

  if (try_catch.HasCaught()) {
    local->Reject(try_catch.Exception());
  }
}

bool inThread() {
  uv_thread_t this_thread = uv_thread_self();
  return !uv_thread_equal(&main_thread, &this_thread);
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

  Local<Promise::Resolver> local = Local<Promise::Resolver>::New(isolate, worker->persistent);

  info.GetReturnValue().Set(local->GetPromise());
}


void Worker::IsRunning(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  // is your worker running?
  // yes? well you had better go catch it!
  info.GetReturnValue().Set(Boolean::New(isolate, worker->running));
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

// called once per tick using `process.nextTick`, a better method is definitely needed
// for receiving messages
void Worker::CheckOutgoingMessages(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = ObjectWrap::Unwrap<Worker>(info.Holder());

  if (!worker)
    return;

  Local<Context> context = isolate->GetCurrentContext();
 
  Local<Array> out = Array::New(isolate, 2);
  info.GetReturnValue().Set(out);

  if (worker->outQueue_.empty()) {
    USE(out->Set(context, 0, False(isolate)));
  } else {
    SerializedData data = worker->outQueue_.pop();

    Local<Value> value = deserialize(isolate, data);

    USE(out->Set(context, 0, True(isolate)));
    USE(out->Set(context, 1, value));
  }
}

void Worker::Terminate(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorker(info);

  worker->running = false;

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
