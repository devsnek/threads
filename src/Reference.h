#ifndef _WORKER_SRC_REFERENCE_H
#define _WORKER_SRC_REFERENCE_H

#include <v8.h>

class Reference {
 public:
  static v8::Local<v8::Object> New(v8::Isolate* isolate);

 private:
  static void NamedPropertyGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
  static void NamedPropertySetterCallback(v8::Local<v8::Name>, v8::Local<v8::Value>, const v8::PropertyCallbackInfo<v8::Value>&);
  static void NamedPropertyDescriptorCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
  static void NamedPropertyDeleterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Boolean>&);
  static void NamedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>&);
  static void NamedPropertyDefinerCallback(v8::Local<v8::Name>, const v8::PropertyDescriptor&, const v8::PropertyCallbackInfo<v8::Value>&);

  static void IndexedPropertyGetterCallback(uint32_t, const v8::PropertyCallbackInfo<v8::Value>&);
  static void IndexedPropertySetterCallback(uint32_t, v8::Local<v8::Value>, const v8::PropertyCallbackInfo<v8::Value>&);
  static void IndexedPropertyDescriptorCallback(uint32_t, const v8::PropertyCallbackInfo<v8::Value>&);
  static void IndexedPropertyDeleterCallback(uint32_t, const v8::PropertyCallbackInfo<v8::Boolean>&);
  static void IndexedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>&);
  static void IndexedPropertyDefinerCallback(uint32_t, const v8::PropertyDescriptor&, const v8::PropertyCallbackInfo<v8::Value>&);
};

#endif // _WORKER_SRC_REFERENCE_H
