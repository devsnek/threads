#include <v8.h>
#include <uv.h>
#include <node.h>
#include "util.h"

using namespace v8;

static node::node_module* modpending;
static node::node_module* modlist_addon;

static void GetPromiseDetails(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  Local<Array> ret = Array::New(isolate, 2);
  info.GetReturnValue().Set(ret);

  if (!info[0]->IsPromise())
    return;

  Local<Promise> promise = info[0].As<Promise>();

  int state = promise->State();
  ret->Set(0, Integer::New(isolate, state));
  if (state != Promise::PromiseState::kPending)
    ret->Set(1, promise->Result());
}

struct DLib {
  std::string filename_;
  std::string errmsg_;
  void* handle_;
  int flags_;

#ifdef __POSIX__
  static const int kDefaultFlags = RTLD_LAZY;

  bool Open() {
    handle_ = dlopen(filename_.c_str(), flags_);
    if (handle_ != nullptr)
      return true;
    errmsg_ = dlerror();
    return false;
  }

  void Close() {
    if (handle_ != nullptr)
      dlclose(handle_);
  }
#else  // !__POSIX__
  static const int kDefaultFlags = 0;
  uv_lib_t lib_;

  bool Open() {
    int ret = uv_dlopen(filename_.c_str(), &lib_);
    if (ret == 0) {
      handle_ = static_cast<void*>(lib_.handle);
      return true;
    }
    errmsg_ = uv_dlerror(&lib_);
    uv_dlclose(&lib_);
    return false;
  }

  void Close() {
    uv_dlclose(&lib_);
  }
#endif  // !__POSIX__
};

enum {
  NM_F_BUILTIN  = 1 << 0,
  NM_F_LINKED   = 1 << 1,
  NM_F_INTERNAL = 1 << 2,
};

static void DLOpen(const FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  if (modpending != nullptr)
    return;

  if (info.Length() < 2) {
    // env->ThrowError("process.dlopen needs at least 2 arguments.");
    return;
  }

  int32_t flags = DLib::kDefaultFlags;
  if (info.Length() > 2 && !info[2]->Int32Value(context).To(&flags)) {
    return;
    // return env->ThrowTypeError("flag argument must be an integer.");
  }

  Local<Object> module = info[0]->ToObject(context).ToLocalChecked();  // Cast
  String::Utf8Value filename(isolate, info[1]);  // Cast
  DLib dlib;
  dlib.filename_ = *filename;
  dlib.flags_ = flags;
  bool is_opened = dlib.Open();

  // Objects containing v14 or later modules will have registered themselves
  // on the pending list.  Activate all of them now.  At present, only one
  // module per object is supported.
  node::node_module* const mp = modpending;
  modpending = nullptr;

  if (!is_opened) {
    Local<String> errmsg = String::NewFromUtf8(isolate, dlib.errmsg_.c_str());
    dlib.Close();
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(errmsg, info[1]->ToString(context).ToLocalChecked());
#endif  // _WIN32
    // env->ThrowException(Exception::Error(errmsg));
    return;
  }

  if (mp == nullptr) {
    dlib.Close();
    // env->ThrowError("Module did not self-register.");
    return;
  }
  if (mp->nm_version == -1) {
    // return if n-api emitted, meh
  } else if (mp->nm_version != NODE_MODULE_VERSION) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "The module '%s'"
             "\nwas compiled against a different Node.js version using"
             "\nNODE_MODULE_VERSION %d. This version of Node.js requires"
             "\nNODE_MODULE_VERSION %d. Please try re-compiling or "
             "re-installing\nthe module (for instance, using `npm rebuild` "
             "or `npm install`).",
             *filename, mp->nm_version, NODE_MODULE_VERSION);

    dlib.Close();
    // env->ThrowError(errmsg);
    return;
  }
  if (mp->nm_flags & NM_F_BUILTIN) {
    dlib.Close();
    // env->ThrowError("Built-in module self-registered.");
    return;
  }

  mp->nm_dso_handle = dlib.handle_;
  mp->nm_link = modlist_addon;
  modlist_addon = mp;

  Local<String> exports_string = String::NewFromUtf8(isolate, "exports");
  MaybeLocal<Value> maybe_exports = module->Get(context, exports_string);

  if (maybe_exports.IsEmpty() || maybe_exports.ToLocalChecked()->ToObject(context).IsEmpty()) {
    dlib.Close();
    return;
  }

  Local<Object> exports = maybe_exports.ToLocalChecked()->ToObject(context).FromMaybe(Local<Object>());

  if (mp->nm_context_register_func != nullptr) {
    mp->nm_context_register_func(exports, module, context, mp->nm_priv);
  } else if (mp->nm_register_func != nullptr) {
    mp->nm_register_func(exports, module, mp->nm_priv);
  } else {
    dlib.Close();
    // env->ThrowError("Module has no declared entry point.");
    return;
  }
}

Local<Object> MakeNativeUtil(Isolate* isolate) {
  Local<Context> context = isolate->GetCurrentContext();

  Local<Object> obj = Object::New(isolate);

#define V(name, fn) \
  USE(obj->Set(context, String::NewFromUtf8(isolate, name), FunctionTemplate::New(isolate, fn)->GetFunction()))
  V("getPromiseDetails", GetPromiseDetails);
  V("dlopen", DLOpen);
#undef V

#define V(name, val) \
  USE(obj->Set(context, String::NewFromUtf8(isolate, name), Integer::New(isolate, val)))
  V("kPending", Promise::PromiseState::kPending);
  V("kFulfilled", Promise::PromiseState::kFulfilled);
  V("kRejected", Promise::PromiseState::kRejected);
#undef V

  return obj;
}
