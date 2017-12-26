#include <v8.h>
#include "Job.h"
#include "util.h"

using namespace v8;

void Run(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  Local<Promise::Resolver> local =
    Job::New(isolate, info[0].As<String>(), info[1].As<Object>());

  info.GetReturnValue().Set(local->GetPromise());
}

void init(Local<Object> target) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  target->Set(String::NewFromUtf8(isolate, "run"), FunctionTemplate::New(isolate, Run)->GetFunction());
}

NODE_MODULE(isolated_vm, init)
