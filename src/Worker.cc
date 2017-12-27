#include <string.h>
#include <node.h>
#include <v8.h>
#include <uv.h>
#include "Worker.h"
#include "util.h"

using namespace v8;

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

  constructor.Reset(isolate, tpl->GetFunction());
  exports->Set(String::NewFromUtf8(isolate, "Worker"), tpl->GetFunction());
}

Worker::Worker(Local<Promise::Resolver> resolver, Worker::Source source) {
  Isolate* isolate = Isolate::GetCurrent();

  this->source = source;
  this->persistent.Reset(isolate, resolver);

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
    Local<Object> global = context->Global();
    global->SetAlignedPointerInInternalField(0, worker);
    USE(global->Set(context, String::NewFromUtf8(isolate, "global"), global));

    Context::Scope context_scope(context);
    TryCatch try_catch(isolate);

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
    USE(coms->Set(context, String::NewFromUtf8(isolate, "on"), FunctionTemplate::New(isolate, ThreadOn)->GetFunction()));
    USE(coms->Set(context, String::NewFromUtf8(isolate, "send"), FunctionTemplate::New(isolate, ThreadSend)->GetFunction()));
    USE(coms->Set(context, String::NewFromUtf8(isolate, "terminate"), FunctionTemplate::New(isolate, ThreadTerminate)->GetFunction()));
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

void Worker::GetPromise(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = ObjectWrap::Unwrap<Worker>(info.Holder());

  Local<Promise::Resolver> local = Local<Promise::Resolver>::New(isolate, worker->persistent);

  info.GetReturnValue().Set(local->GetPromise());
}

void Worker::IsRunning(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = ObjectWrap::Unwrap<Worker>(info.Holder());

  // is your worker running?
  // yes? well you had better go catch it!
  info.GetReturnValue().Set(Boolean::New(isolate, worker->running));
}

void Worker::Send(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = ObjectWrap::Unwrap<Worker>(info.Holder());
  SerializedData data = serialize(isolate, info[0]);
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

  Worker* worker = ObjectWrap::Unwrap<Worker>(info.Holder());
  worker->running = false;

  info.GetReturnValue().Set(Undefined(isolate));
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

Worker* GetWorkerFromInternalField(Isolate* isolate) {
  HandleScope scope(isolate);

  Local<Object> object = isolate->GetCurrentContext()->Global();

  if (object->InternalFieldCount() != 1)
    return nullptr;

  Worker* worker = static_cast<Worker*>(object->GetAlignedPointerFromInternalField(0));
  if (worker == nullptr)
    return nullptr;

  return worker;
}

void Worker::ThreadSend(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorkerFromInternalField(isolate);
  SerializedData data = serialize(isolate, info[0]);
  worker->outQueue_.push(data);

  info.GetReturnValue().Set(Undefined(isolate));
}


void Worker::ThreadTerminate(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Worker* worker = GetWorkerFromInternalField(isolate);
  worker->running = false;

  info.GetReturnValue().Set(Undefined(isolate));
}
