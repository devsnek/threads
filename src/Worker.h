#ifndef _WORKER_SRC_WORKER_H
#define _WORKER_SRC_WORKER_H

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <uv.h>
#include "util.h"

class Worker : public node::ObjectWrap {
 public:
  struct Source {
    const char* code;
    const char* preload;
    std::pair<uint8_t*, size_t> arguments;
  };

  enum State { created, running, terminated };

  Worker(v8::Local<v8::Promise::Resolver>, Worker::Source);
  ~Worker();
  static void Init(v8::Local<v8::Object>);
  static void New(const v8::FunctionCallbackInfo<v8::Value>&);

  static void GetPromise(const v8::FunctionCallbackInfo<v8::Value>&);
  static void GetId(const v8::FunctionCallbackInfo<v8::Value>&);
  static void GetState(const v8::FunctionCallbackInfo<v8::Value>&);
  static void Send(const v8::FunctionCallbackInfo<v8::Value>&);
  static void CheckOutgoingMessages(const v8::FunctionCallbackInfo<v8::Value>&);
  static void Terminate(const v8::FunctionCallbackInfo<v8::Value>&);
  static void Lock(const v8::FunctionCallbackInfo<v8::Value>&);
  static void Unlock(const v8::FunctionCallbackInfo<v8::Value>&);

  static void ThreadOn(const v8::FunctionCallbackInfo<v8::Value>&);
  static void ThreadConsole(const v8::FunctionCallbackInfo<v8::Value>&);
  static void ThreadHrtime(const v8::FunctionCallbackInfo<v8::Value>&);

  static Worker* GetWorker(const v8::FunctionCallbackInfo<v8::Value>&);

  Worker::Source source;
  v8::Persistent<v8::Promise::Resolver> persistent;
  SerializedData result;
  SerializedData error;

  State state = State::created;
  uint32_t id;

 private:
  static void WorkThread(uv_work_t*);
  static void WorkCallback(uv_work_t*, int);

  ThreadQueue<SerializedData> inQueue_;
  ThreadQueue<SerializedData> outQueue_;

  std::mutex js_mutex_;

  static v8::Persistent<v8::Function> constructor;
};

#endif // _WORKER_SRC_WORKER_H
