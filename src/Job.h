#ifndef _WORKER_SRC_JOB_H
#define _WORKER_SRC_JOB_H

#include <node.h>
#include <v8.h>
#include <uv.h>

class Job {
  struct Source {
    const char* code;
    std::pair<uint8_t*, size_t> props;
  };

 public:
  Job(v8::Local<v8::Promise::Resolver>, Job::Source);
  ~Job();
  static v8::Local<v8::Promise::Resolver> New(v8::Isolate*, v8::Local<v8::String>, v8::Local<v8::Object>);

  Job::Source source;
  v8::Persistent<v8::Promise::Resolver> persistent;
  std::pair<uint8_t*, size_t> result;
  std::pair<uint8_t*, size_t> error;

 private:
  static void WorkThread(uv_work_t*);
  static void WorkCallback(uv_work_t*, int);
};

#endif // _WORKER_SRC_JOB_H
