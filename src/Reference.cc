#include <v8.h>
#include "Reference.h"

using namespace v8;

Local<Object> Reference::New(Isolate* isolate) {
  Local<ObjectTemplate> t = ObjectTemplate::New(isolate);

  NamedPropertyHandlerConfiguration named_config(
      NamedPropertyGetterCallback,
      NamedPropertySetterCallback,
      NamedPropertyDescriptorCallback,
      NamedPropertyDeleterCallback,
      NamedPropertyEnumeratorCallback,
      NamedPropertyDefinerCallback);

  IndexedPropertyHandlerConfiguration indexed_config(
      IndexedPropertyGetterCallback,
      IndexedPropertySetterCallback,
      IndexedPropertyDescriptorCallback,
      IndexedPropertyDeleterCallback,
      IndexedPropertyEnumeratorCallback,
      IndexedPropertyDefinerCallback);

  t->SetHandler(named_config);
  t->SetHandler(indexed_config);

  Local<Object> obj = t->NewInstance(isolate->GetCurrentContext()).ToLocalChecked().As<Object>();

  return obj;
}

void Reference::NamedPropertyGetterCallback(Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);
  
  info.GetReturnValue().Set(Number::New(isolate, 1337));
}

void Reference::NamedPropertySetterCallback(
    Local<Name> property, Local<Value> value, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(true);
}

void Reference::NamedPropertyDescriptorCallback(Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  info.GetReturnValue().Set(Undefined(isolate));
}

void Reference::NamedPropertyDeleterCallback(Local<Name> property, const PropertyCallbackInfo<Boolean>& info) {
  info.GetReturnValue().Set(false);
}

void Reference::NamedPropertyEnumeratorCallback(const PropertyCallbackInfo<Array>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  info.GetReturnValue().Set(Array::New(isolate, 0));
}


void Reference::NamedPropertyDefinerCallback(
    Local<Name> property, const PropertyDescriptor& desc, const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  HandleScope scope(isolate);

  info.GetReturnValue().Set(Undefined(isolate));
}

static Local<Name> Uint32ToName(Isolate* isolate, uint32_t index) {
  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

  return Uint32::New(isolate, index)->ToString(context).ToLocalChecked();
}

void Reference::IndexedPropertyGetterCallback(uint32_t index, const PropertyCallbackInfo<Value>& info) {
  NamedPropertyGetterCallback(Uint32ToName(info.GetIsolate(), index), info);
}

void Reference::IndexedPropertySetterCallback(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info) {
  NamedPropertySetterCallback(Uint32ToName(info.GetIsolate(), index), value, info);
}

void Reference::IndexedPropertyDescriptorCallback(uint32_t index, const PropertyCallbackInfo<Value>& info) {
  NamedPropertyDescriptorCallback(Uint32ToName(info.GetIsolate(), index), info);
}

void Reference::IndexedPropertyDeleterCallback(uint32_t index, const PropertyCallbackInfo<Boolean>& info) {
  NamedPropertyDeleterCallback(Uint32ToName(info.GetIsolate(), index), info);
}

void Reference::IndexedPropertyEnumeratorCallback(const PropertyCallbackInfo<Array>& info) {
  NamedPropertyEnumeratorCallback(info);
}

void Reference::IndexedPropertyDefinerCallback(
    uint32_t index, const PropertyDescriptor& desc, const PropertyCallbackInfo<Value>& info) {
  NamedPropertyDefinerCallback(Uint32ToName(info.GetIsolate(), index), desc, info);
}
