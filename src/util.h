#ifndef _WORKER_SRC_UTIL_H
#define _WORKER_SRC_UTIL_H

#include <v8.h>

template <typename T> inline void USE(T&&) {};

inline std::pair<uint8_t*, size_t> serialize(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::ValueSerializer* serializer = new v8::ValueSerializer(isolate);
  serializer->WriteHeader();
  USE(serializer->WriteValue(context, value));
  return serializer->Release();
}

inline v8::Local<v8::Value> deserialize(v8::Isolate* isolate, std::pair<uint8_t*, size_t> pair) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::ValueDeserializer* deserializer = new v8::ValueDeserializer(isolate, pair.first, pair.second);
  USE(deserializer->ReadHeader(context));
  return deserializer->ReadValue(context).ToLocalChecked();
}

#endif // _WORKER_SRC_UTIL_H
