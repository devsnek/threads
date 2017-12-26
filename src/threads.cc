#include <v8.h>
#include "Worker.h"
#include "util.h"

using namespace v8;

void init(Local<Object> target) {
  Worker::Init(target);
}

NODE_MODULE(isolated_vm, init)
