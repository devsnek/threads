#include <node.h>
#include <v8.h>

using namespace v8;

void HelloWorld(const v8::FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);
  info.GetReturnValue().Set(String::NewFromUtf8(isolate, "Hello, World!"));
}

void Init(Handle<Object> exports) {
  Isolate* isolate = Isolate::GetCurrent();
  exports->Set(String::NewFromUtf8(isolate, "hello"), FunctionTemplate::New(isolate, HelloWorld)->GetFunction());
}

NODE_MODULE(hello, Init)
