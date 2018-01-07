#include <v8.h>
#include "util.h"

using namespace v8;

static void GetPromiseDetails(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  Local<Array> ret = Array::New(isolate, 2);
  info.GetReturnValue().Set(ret);

  if (!info[0]->IsPromise())
    return;

  Local<Promise> promise = info[0].As<Promise>();

  int state = promise->State();
  ret->Set(0, Integer::New(isolate, state));
  if (state != Promise::PromiseState::kPending)
    ret->Set(1, promise->Result());
}

Local<Object> MakeNativeUtil(Isolate* isolate) {
  Local<Context> context = isolate->GetCurrentContext();

  Local<Object> obj = Object::New(isolate);

#define V(name, fn) \
  USE(obj->Set(context, String::NewFromUtf8(isolate, name), FunctionTemplate::New(isolate, fn)->GetFunction()))
  V("getPromiseDetails", GetPromiseDetails);
#undef V

#define V(name, val) \
  USE(obj->Set(context, String::NewFromUtf8(isolate, name), Integer::New(isolate, val)))
  V("kPending", Promise::PromiseState::kPending);
  V("kFulfilled", Promise::PromiseState::kFulfilled);
  V("kRejected", Promise::PromiseState::kRejected);
#undef V

  return obj;
}
