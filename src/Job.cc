#include <node.h>
#include <v8.h>
#include <uv.h>
#include "Job.h"
#include "util.h"

using namespace v8;

Job::Job(Local<Promise::Resolver> resolver, Job::Source source) {
  Isolate* isolate = Isolate::GetCurrent();

  this->source = source;
  this->persistent.Reset(isolate, resolver);

  uv_work_t *work = new uv_work_t;
  work->data = this;

  uv_queue_work(uv_default_loop(), work, Job::WorkThread, Job::WorkCallback);
};

Job::~Job() {
  this->persistent.Reset();
};

Local<Promise::Resolver> Job::New(Isolate* isolate, Local<String> code, Local<Object> props) {
  HandleScope handle(isolate);

  Local<Promise::Resolver> local = Promise::Resolver::New(isolate);

  Job::Source source;

  String::Utf8Value utf8(isolate, code);
  char* buffer = strdup(*utf8);
  source.code = buffer;

  source.props = serialize(isolate, props);

  new Job(local, source);

  return local;
}

bool MaybeHandleException(Isolate* isolate, TryCatch* try_catch, Job* job) {
  if (!try_catch->HasCaught())
    return false;

  HandleScope scope(isolate);

  job->error = serialize(isolate, try_catch->Message()->Get());

  return true;
}

void Job::WorkThread(uv_work_t* work) {
  Job* job = (Job*) work->data;

  Job::Source source = job->source;

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
    ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);

  {
    Locker locker(isolate);
    HandleScope scope(isolate);

    Local<Context> context = Context::New(isolate);
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

    Local<String> code = String::NewFromUtf8(isolate, source.code);
    Local<Script> script = Script::Compile(context, code, &origin).ToLocalChecked();
    MaybeLocal<Value> maybe_result = script->Run(context);

    if (MaybeHandleException(isolate, &try_catch, job))
      return;

    if (maybe_result.IsEmpty())
      return;

    Local<Array> props = deserialize(isolate, source.props).As<Array>();
    Local<Value> args[props->Length()];
    for (uint32_t i = 0; i < props->Length(); i++) {
      args[i] = props->Get(context, i).ToLocalChecked();
    }

    Local<Function> fn = maybe_result.ToLocalChecked().As<Function>();
    maybe_result = fn->Call(context, context->Global(), props->Length(), args);

    if (MaybeHandleException(isolate, &try_catch, job))
      return;

    if (maybe_result.IsEmpty())
      return;

    job->result = serialize(isolate, maybe_result.ToLocalChecked());
  }

  isolate->Dispose();
}

void Job::WorkCallback(uv_work_t *work, int status) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle(isolate);

  Job* job = (Job*) work->data;

  TryCatch try_catch(isolate);

  Local<Promise::Resolver> local = Local<Promise::Resolver>::New(isolate, job->persistent);

  if (job->error.first != nullptr) {
    Local<String> message = deserialize(isolate, job->error).As<String>();
    local->Reject(Exception::Error(message));
  } else {
    Local<Value> value = deserialize(isolate, job->result);
    local->Resolve(value);
 }

  if (try_catch.HasCaught()) {
    local->Reject(try_catch.Exception());
  }

  delete job;
}
