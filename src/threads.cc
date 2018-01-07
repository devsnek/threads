#include <v8.h>
#include "Worker.h"
#include "util.h"

using namespace v8;

void init(Local<Object> target) {
  int argc = 2;
  const char* flags[] = {
    "--max-semi-space-size", "0",
    "--max-old-space-size", "0"
  };
  V8::SetFlagsFromCommandLine(&argc, (char**)flags, false);

  Worker::Init(target);
}

NODE_MODULE(threads, init)
