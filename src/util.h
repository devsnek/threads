#ifndef _WORKER_SRC_UTIL_H
#define _WORKER_SRC_UTIL_H

#include <queue>
#include <thread>
#include <mutex>
#include <v8.h>

typedef std::pair<uint8_t*, size_t> SerializedData;

template <typename T> inline void USE(T&&) {};

inline SerializedData serialize(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::ValueSerializer* serializer = new v8::ValueSerializer(isolate);
  serializer->WriteHeader();
  USE(serializer->WriteValue(context, value));
  return serializer->Release();
}

inline v8::Local<v8::Value> deserialize(v8::Isolate* isolate, SerializedData data) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::ValueDeserializer* deserializer = new v8::ValueDeserializer(isolate, data.first, data.second);
  USE(deserializer->ReadHeader(context));
  return deserializer->ReadValue(context).ToLocalChecked();
}

template <typename T>
class ThreadQueue {
 public:
  void push(T item) {
    std::lock_guard<std::mutex> mlock(this->mutex_);

    this->queue_.push(item);
  }

  bool empty() {
    std::lock_guard<std::mutex> mlock(this->mutex_);

    return this->queue_.empty();
  }

  T pop() {
    std::lock_guard<std::mutex> mlock(this->mutex_);

    T item = this->queue_.front();
    this->queue_.pop();
    return item;
  }
 
 private:
  std::queue<T> queue_;
  std::mutex mutex_;
};

#endif // _WORKER_SRC_UTIL_H
