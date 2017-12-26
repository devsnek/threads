#ifndef _WORKER_SRC_UTIL_H
#define _WORKER_SRC_UTIL_H

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

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
 
template <typename T>
class ThreadQueue {
 public:
  T pop() {
    std::unique_lock<std::mutex> mlock(this->mutex_);
    /*
    while (this->queue_.empty())
      cond_.wait(mlock);
    */
    if (this->queue_.empty())
      return nullptr;

    T item = this->queue_.front();
    this->queue_.pop();
    return item;
  }
 
  bool pop(T& item) {
    std::unique_lock<std::mutex> mlock(this->mutex_);
    if (this->queue_.empty())
      return false;
    /*
    while (this->queue_.empty())
      this->cond_.wait(mlock);
    */
    item = this->queue_.front();
    this->queue_.pop();
    return true;
  }
 
  void push(const T& item) {
    std::unique_lock<std::mutex> mlock(this->mutex_);
    this->queue_.push(item);
    mlock.unlock();
    this->cond_.notify_one();
  }
 
  void push(T&& item) {
    std::unique_lock<std::mutex> mlock(this->mutex_);
    this->queue_.push(std::move(item));
    mlock.unlock();
    this->cond_.notify_one();
  }
 
 private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

#endif // _WORKER_SRC_UTIL_H
