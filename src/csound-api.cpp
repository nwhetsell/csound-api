#include <boost/lockfree/queue.hpp>
#include <csound/cwindow.h>
#include <nan.h>

// CsoundCallback is a subclass of Nan::Callback
// <https://github.com/nodejs/nan/blob/master/doc/callback.md> to make it easier
// to store JavaScript functions as Csound callback functions. CsoundCallback
// instances maintain an async handle <http://docs.libuv.org/en/v1.x/async.html>
// and a lock-free queue of callback arguments of type T. Argument classes must
// have a static member T::argc storing the number of arguments of the callback,
// a method T::getArgv that puts Csound callback arguments into an array of V8
// values, and a method T::wereSent that can clean up after arguments are sent.
// When the static method CsoundCallback::asyncCallback runs after a call to
// uv_async_send, the CsoundCallback instance pops arguments off its queue into
// arrays of V8 values, which are then passed to a JavaScript function by
// Nan::Callback::Call(). Using a lock-free queue is from csound.node by Michael
// Gogins, which is available as part of Csound
// <https://github.com/csound/csound/tree/develop/frontends/nwjs>.
template <typename T>
struct CsoundCallback : public Nan::Callback {
  uv_async_t handle;
  boost::lockfree::queue<T> argumentsQueue;

  static void asyncCallback(uv_async_t *handle) {
    ((CsoundCallback *)handle->data)->executeCalls();
  }

  CsoundCallback(const v8::Local<v8::Function> &function) : Nan::Callback(function), argumentsQueue(0) {
    assert(uv_async_init(uv_default_loop(), &handle, asyncCallback) == 0);
    handle.data = this;
  }

  void executeCalls() {
    Nan::HandleScope scope;
    T arguments;
    while (argumentsQueue.pop(arguments)) {
      v8::Local<v8::Value> argv[T::argc];
      arguments.getArgv(argv);
      Call(T::argc, argv);
      arguments.wereSent();
    }
  }
};

// These structs store arguments from Csound callbacks.
struct CsoundMessageCallbackArguments {
  static const int argc = 2;
  int attributes;
  char *message;

  static CsoundMessageCallbackArguments create(int attributes, const char *format, va_list argumentList) {
    CsoundMessageCallbackArguments arguments;
    arguments.attributes = attributes;

    // When determining the length of the string in the first call to vsnprintf,
    // pass a copy of the argument list so that the original argument list is
    // preserved when writing the string in the second call to vsnprintf.
    va_list argumentListCopy;
    va_copy(argumentListCopy, argumentList);
    int length = vsnprintf(NULL, 0, format, argumentListCopy) + 1;
    va_end(argumentListCopy);
    arguments.message = (char *)malloc(sizeof(char) * length);
    vsnprintf(arguments.message, length, format, argumentList);

    return arguments;
  }

  void getArgv(v8::Local<v8::Value> *argv) const {
    argv[0] = Nan::New(attributes);
    argv[1] = Nan::New(message).ToLocalChecked();
  }

  void wereSent() {
    free(message);
  }
};

static Nan::Persistent<v8::FunctionTemplate> WINDATProxyConstructor;

struct WINDATWrapper : public Nan::ObjectWrap {
  WINDAT *data;
  MYFLT *fdataCopy;

  static NAN_METHOD(New) {
    (new WINDATWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static WINDAT *dataFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<WINDATWrapper>(info.This())->data;
  }
  static NAN_GETTER(windid) { info.GetReturnValue().Set(Nan::New((unsigned int)dataFromPropertyCallbackInfo(info)->windid)); }
  static NAN_GETTER(caption) { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->caption).ToLocalChecked()); }
  static NAN_GETTER(polarity) { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->polarity)); }
  static NAN_GETTER(max) { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->max)); }
  static NAN_GETTER(min) { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->min)); }
  static NAN_GETTER(oabsmax) { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->oabsmax)); }

  static NAN_GETTER(fdata) {
    WINDATWrapper *wrapper = WINDATWrapper::Unwrap<WINDATWrapper>(info.This());
    v8::Local<v8::Array> fdata = Nan::New<v8::Array>(wrapper->data->npts);
    for (int32 i = 0; i < wrapper->data->npts; i++) {
      fdata->Set(i, Nan::New(wrapper->fdataCopy[i]));
    }
    info.GetReturnValue().Set(fdata);
  }
};

struct CsoundGraphCallbackArguments {
  WINDAT *data;
  MYFLT *fdata;

  void getArgv(v8::Local<v8::Value> *argv) const {
    v8::Local<v8::Object> proxy = Nan::New(WINDATProxyConstructor)->GetFunction()->NewInstance();
    WINDATWrapper *wrapper = WINDATWrapper::Unwrap<WINDATWrapper>(proxy);
    wrapper->data = data;
    wrapper->fdataCopy = fdata;
    argv[0] = proxy;
  }

  void setData(WINDAT *data) {
    this->data = data;
    fdata = (MYFLT *)malloc(sizeof(MYFLT) * data->npts);
    for (int32 i = 0; i < data->npts; i++) {
      fdata[i] = data->fdata[i];
    }
  }

  void wereSent() {
    free(fdata);
  }
};

struct CsoundDrawGraphCallbackArguments : public CsoundGraphCallbackArguments {
  static const int argc = 1;

  static CsoundDrawGraphCallbackArguments create(WINDAT *data) {
    CsoundDrawGraphCallbackArguments arguments;
    arguments.setData(data);
    return arguments;
  }
};

struct CsoundMakeGraphCallbackArguments : public CsoundGraphCallbackArguments {
  static const int argc = 2;
  const char *name;

  static CsoundMakeGraphCallbackArguments create(WINDAT *data, const char *name) {
    CsoundMakeGraphCallbackArguments arguments;
    arguments.setData(data);
    arguments.name = name;
    return arguments;
  }

  void getArgv(v8::Local<v8::Value> *argv) const {
    CsoundGraphCallbackArguments::getArgv(argv);
    argv[1] = Nan::New(name).ToLocalChecked();
  }
};

static Nan::Persistent<v8::FunctionTemplate> CSOUNDProxyConstructor;

// CSOUNDWrapper instances perform tasks related to callbacks. They also store
// V8 values passed as host data from JavaScript.
struct CSOUNDWrapper : public Nan::ObjectWrap {
  CSOUND *Csound;
  Nan::Persistent<v8::Value, v8::CopyablePersistentTraits<v8::Value> > hostData;

  CsoundCallback<CsoundMessageCallbackArguments> *CsoundMessageCallbackObject;
  CsoundCallback<CsoundMakeGraphCallbackArguments> *CsoundMakeGraphCallbackObject;
  CsoundCallback<CsoundDrawGraphCallbackArguments> *CsoundDrawGraphCallbackObject;

  // Require Csound 6.04 or later to use debugger functions.
#define CSOUND_6_04_OR_LATER CS_VERSION >= 6 && CS_SUBVER >= 4
#if CSOUND_6_04_OR_LATER
  Nan::Callback *CsoundBreakpointCallbackObject;
#endif // CSOUND_6_04_OR_LATER

  static NAN_METHOD(New) {
    (new CSOUNDWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  void queueMessage(int attributes, const char *format, va_list argumentList) {
    CsoundMessageCallbackObject->argumentsQueue.push(CsoundMessageCallbackArguments::create(attributes, format, argumentList));
    uv_async_send(&(CsoundMessageCallbackObject->handle));
  }

  void queueMakeGraph(WINDAT *windowData, const char *name) {
    CsoundMakeGraphCallbackObject->argumentsQueue.push(CsoundMakeGraphCallbackArguments::create(windowData, name));
    uv_async_send(&(CsoundMakeGraphCallbackObject->handle));
  }

  void queueDrawGraph(WINDAT *windowData) {
    CsoundDrawGraphCallbackObject->argumentsQueue.push(CsoundDrawGraphCallbackArguments::create(windowData));
    uv_async_send(&(CsoundDrawGraphCallbackObject->handle));
  }
};

// The CSOUND_CALLBACK_METHOD macro associates a Csound callback function with a
// CsoundCallback instance using an appropriate argument type. For example,
// CSOUND_CALLBACK_METHOD(Message) associates csoundSetMessageCallback with
// CsoundCallback<CsoundMessageCallbackArguments>;
// CSOUND_CALLBACK_METHOD(MakeGraph) associates csoundSetMakeGraphCallback with
// CsoundCallback<CsoundMakeGraphCallbackArguments>; and so on.
#define CSOUND_CALLBACK_METHOD(name) \
NAN_METHOD(Set ## name ## Callback) { \
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>()); \
  v8::Local<v8::Value> value = info[1]; \
  if (value->IsFunction()) { \
    wrapper->Csound ## name ## CallbackObject = new CsoundCallback<Csound ## name ## CallbackArguments>(value.As<v8::Function>()); \
    csoundSet ## name ## Callback(wrapper->Csound, Csound ## name ## Callback); \
  } else if (wrapper->Csound ## name ## CallbackObject) { \
    delete wrapper->Csound ## name ## CallbackObject; \
    wrapper->Csound ## name ## CallbackObject = NULL; \
    csoundSet ## name ## Callback(wrapper->Csound, NULL); \
  } \
}

// Helper function to set return values to either a string or null.
static void setReturnValueWithCString(Nan::ReturnValue<v8::Value> returnValue, const char *string) {
  if (string)
    returnValue.Set(Nan::New(string).ToLocalChecked());
  else
    returnValue.SetNull();
}

static NAN_METHOD(Create) {
  v8::Local<v8::Object> proxy = Nan::New<v8::FunctionTemplate>(CSOUNDProxyConstructor)->GetFunction()->NewInstance();
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(proxy);
  CSOUND *Csound = csoundCreate(wrapper);
  if (Csound) {
    wrapper->Csound = Csound;
    wrapper->hostData.Reset(info[0]);
    info.GetReturnValue().Set(proxy);
  } else {
    info.GetReturnValue().SetNull();
  }
}

static CSOUND *CsoundFromFunctionCallbackInfo(Nan::NAN_METHOD_ARGS_TYPE info) {
  return Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>())->Csound;
}

static NAN_METHOD(Destroy) {
  csoundDestroy(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(GetVersion) {
  info.GetReturnValue().Set(Nan::New(csoundGetVersion()));
}

static NAN_METHOD(GetAPIVersion) {
  info.GetReturnValue().Set(Nan::New(csoundGetAPIVersion()));
}

static Nan::Persistent<v8::FunctionTemplate> ORCTOKENProxyConstructor;

struct ORCTOKENWrapper : public Nan::ObjectWrap {
  ORCTOKEN *token;

  static NAN_METHOD(New) {
    (new ORCTOKENWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static ORCTOKEN *tokenFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<ORCTOKENWrapper>(info.This())->token;
  }
  static NAN_GETTER(type) { info.GetReturnValue().Set(Nan::New(tokenFromPropertyCallbackInfo(info)->type)); }
  static NAN_GETTER(value) { info.GetReturnValue().Set(Nan::New(tokenFromPropertyCallbackInfo(info)->value)); }
  static NAN_GETTER(fvalue) { info.GetReturnValue().Set(Nan::New(tokenFromPropertyCallbackInfo(info)->fvalue)); }

  static NAN_GETTER(lexeme) { setReturnValueWithCString(info.GetReturnValue(), tokenFromPropertyCallbackInfo(info)->lexeme); }
  static NAN_GETTER(optype) { setReturnValueWithCString(info.GetReturnValue(), tokenFromPropertyCallbackInfo(info)->optype); }

  static NAN_GETTER(next) {
    ORCTOKEN *token = tokenFromPropertyCallbackInfo(info)->next;
    if (token) {
      v8::Local<v8::Object> proxy = Nan::New(ORCTOKENProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<ORCTOKENWrapper>(proxy)->token = token;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static Nan::Persistent<v8::FunctionTemplate> TREEProxyConstructor;

struct TREEWrapper : public Nan::ObjectWrap {
  TREE *tree;

  static NAN_METHOD(New) {
    (new TREEWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static TREE *treeFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<TREEWrapper>(info.This())->tree;
  }

  static NAN_GETTER(value) {
    ORCTOKEN *token = treeFromPropertyCallbackInfo(info)->value;
    if (token) {
      v8::Local<v8::Object> proxy = Nan::New(ORCTOKENProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<ORCTOKENWrapper>(proxy)->token = token;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
  static NAN_GETTER(type) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->type)); }
  static NAN_GETTER(rate) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->rate)); }
  static NAN_GETTER(len) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->len)); }
  static NAN_GETTER(line) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->line)); }
  static NAN_GETTER(locn) { info.GetReturnValue().Set(Nan::New(static_cast<double>(treeFromPropertyCallbackInfo(info)->locn))); }

  static void setPropertyCallbackInfoReturnValueWithTree(Nan::NAN_GETTER_ARGS_TYPE info, TREE *tree) {
    if (tree) {
      v8::Local<v8::Object> proxy = Nan::New(TREEProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<TREEWrapper>(proxy)->tree = tree;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
  static NAN_GETTER(left) { setPropertyCallbackInfoReturnValueWithTree(info, treeFromPropertyCallbackInfo(info)->left); }
  static NAN_GETTER(right) { setPropertyCallbackInfoReturnValueWithTree(info, treeFromPropertyCallbackInfo(info)->right); }
  static NAN_GETTER(next) { setPropertyCallbackInfoReturnValueWithTree(info, treeFromPropertyCallbackInfo(info)->next); }
};

static NAN_METHOD(ParseOrc) {
  TREE *tree = csoundParseOrc(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]));
  if (tree) {
    v8::Local<v8::Object> proxy = Nan::New(TREEProxyConstructor)->GetFunction()->NewInstance();
    Nan::ObjectWrap::Unwrap<TREEWrapper>(proxy)->tree = tree;
    info.GetReturnValue().Set(proxy);
  } else {
    info.GetReturnValue().SetNull();
  }
}

static NAN_METHOD(CompileTree) {
  info.GetReturnValue().Set(Nan::New(csoundCompileTree(CsoundFromFunctionCallbackInfo(info), Nan::ObjectWrap::Unwrap<TREEWrapper>(info[1].As<v8::Object>())->tree)));
}

static NAN_METHOD(DeleteTree) {
  csoundDeleteTree(CsoundFromFunctionCallbackInfo(info), Nan::ObjectWrap::Unwrap<TREEWrapper>(info[1].As<v8::Object>())->tree);
}

static NAN_METHOD(CompileOrc) {
  info.GetReturnValue().Set(Nan::New(csoundCompileOrc(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
}

static NAN_METHOD(EvalCode) {
  info.GetReturnValue().Set(Nan::New(csoundEvalCode(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
}

// On Windows, csoundInitializeCscore appears to be broken.
#ifndef _MSC_VER
static NAN_METHOD(InitializeCscore) {
  FILE *inputFile = fopen(*Nan::Utf8String(info[1]), "r");
  FILE *outputFile = fopen(*Nan::Utf8String(info[2]), "w");
  info.GetReturnValue().Set(Nan::New(csoundInitializeCscore(CsoundFromFunctionCallbackInfo(info), inputFile, outputFile)));
  fclose(inputFile);
  fclose(outputFile);
}
#endif

// Helper function to pass V8 values to csoundCompileArgs and csoundCompile.
static Nan::NAN_METHOD_RETURN_TYPE performCsoundCompileFunction(Nan::NAN_METHOD_ARGS_TYPE info, int (*compileFunction)(CSOUND *, int, char **)) {
  v8::Local<v8::Array> array = info[1].As<v8::Array>();
  uint32_t argumentCount = array->Length();
  if (argumentCount > 0) {
    char **arguments = (char **)malloc(sizeof(char *) * argumentCount);
    for (uint32_t i = 0; i < argumentCount; i++) {
      arguments[i] = strdup(*Nan::Utf8String(array->Get(i)));
    }
    info.GetReturnValue().Set(Nan::New(compileFunction(CsoundFromFunctionCallbackInfo(info), argumentCount, arguments)));
    for (uint32_t i = 0; i < argumentCount; i++) {
      free(arguments[i]);
    }
    free(arguments);
  }
}

static NAN_METHOD(CompileArgs) {
  performCsoundCompileFunction(info, csoundCompileArgs);
}

static NAN_METHOD(Start) {
  info.GetReturnValue().Set(csoundStart(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(Compile) {
  performCsoundCompileFunction(info, csoundCompile);
}

static NAN_METHOD(CompileCsd) {
  info.GetReturnValue().Set(Nan::New(csoundCompileCsd(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
}

static NAN_METHOD(Perform) {
  info.GetReturnValue().Set(csoundPerform(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(PerformKsmps) {
  info.GetReturnValue().Set((bool)csoundPerformKsmps(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(PerformBuffer) {
  info.GetReturnValue().Set(csoundPerformBuffer(CsoundFromFunctionCallbackInfo(info)));
}

struct CsoundPerformanceWorker : public Nan::AsyncWorker {
  CSOUND *Csound;
  int result;

  CsoundPerformanceWorker(Nan::Callback *callback, CSOUND *Csound) : Nan::AsyncWorker(callback), Csound(Csound) {}
  ~CsoundPerformanceWorker() {};

  void Execute() {
    result = csoundPerform(Csound);
  }

  void HandleOKCallback() {
    const int argc = 1;
    v8::Local<v8::Value> argv[argc];
    argv[0] = Nan::New(result);
    callback->Call(argc, argv);
  }
};

static NAN_METHOD(PerformAsync) {
  Nan::AsyncQueueWorker(new CsoundPerformanceWorker(new Nan::Callback(info[1].As<v8::Function>()), CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(Stop) {
  csoundStop(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(Cleanup) {
  info.GetReturnValue().Set(csoundCleanup(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(Reset) {
  csoundReset(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(GetSr) {
  info.GetReturnValue().Set(csoundGetSr(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(GetKr) {
  info.GetReturnValue().Set(csoundGetKr(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(GetKsmps) {
  info.GetReturnValue().Set(csoundGetKsmps(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(GetNchnls) {
  info.GetReturnValue().Set(csoundGetNchnls(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(GetNchnlsInput) {
  info.GetReturnValue().Set(csoundGetNchnlsInput(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(Get0dBFS) {
  info.GetReturnValue().Set(csoundGet0dBFS(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(GetCurrentTimeSamples) {
  info.GetReturnValue().Set(static_cast<double>(csoundGetCurrentTimeSamples(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(GetSizeOfMYFLT) {
  info.GetReturnValue().Set(csoundGetSizeOfMYFLT());
}

static NAN_METHOD(GetHostData) {
  info.GetReturnValue().Set(Nan::New(Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>())->hostData));
}

static NAN_METHOD(SetHostData) {
  Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>())->hostData.Reset(info[1]);
}

static NAN_METHOD(SetOption) {
  info.GetReturnValue().Set(Nan::New(csoundSetOption(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
}

static NAN_METHOD(GetDebug) {
  info.GetReturnValue().Set((bool)csoundGetDebug(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(SetDebug) {
  csoundSetDebug(CsoundFromFunctionCallbackInfo(info), info[1]->BooleanValue());
}

static NAN_METHOD(GetOutputName) {
  setReturnValueWithCString(info.GetReturnValue(), csoundGetOutputName(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(ReadScore) {
  info.GetReturnValue().Set(Nan::New(csoundReadScore(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
}

static NAN_METHOD(GetScoreTime) {
  info.GetReturnValue().Set(Nan::New(csoundGetScoreTime(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(IsScorePending) {
  info.GetReturnValue().Set(Nan::New(csoundIsScorePending(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(SetScorePending) {
  csoundSetScorePending(CsoundFromFunctionCallbackInfo(info), info[1]->BooleanValue());
}

static NAN_METHOD(GetScoreOffsetSeconds) {
  info.GetReturnValue().Set(Nan::New(csoundGetScoreOffsetSeconds(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(SetScoreOffsetSeconds) {
  csoundSetScoreOffsetSeconds(CsoundFromFunctionCallbackInfo(info), info[1]->NumberValue());
}

static NAN_METHOD(RewindScore) {
  csoundRewindScore(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(Message) {
  csoundMessage(CsoundFromFunctionCallbackInfo(info), "%s", *Nan::Utf8String(info[1]));
}

static NAN_METHOD(MessageS) {
  csoundMessageS(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value(), "%s", *Nan::Utf8String(info[2]));
}

struct CsoundMessageType {
  static NAN_GETTER(Default) { info.GetReturnValue().Set(CSOUNDMSG_DEFAULT); }
  static NAN_GETTER(Error) { info.GetReturnValue().Set(CSOUNDMSG_ERROR); }
  static NAN_GETTER(OrchestraOpcode) { info.GetReturnValue().Set(CSOUNDMSG_ORCH); }
  static NAN_GETTER(RealTime) { info.GetReturnValue().Set(CSOUNDMSG_REALTIME); }
  static NAN_GETTER(Warning) { info.GetReturnValue().Set(CSOUNDMSG_WARNING); }
  static NAN_GETTER(Mask) { info.GetReturnValue().Set(CSOUNDMSG_TYPE_MASK); }
};

struct CsoundMessageForegroundColor {
  static NAN_GETTER(Black) { info.GetReturnValue().Set(CSOUNDMSG_FG_BLACK); }
  static NAN_GETTER(Red) { info.GetReturnValue().Set(CSOUNDMSG_FG_RED); }
  static NAN_GETTER(Green) { info.GetReturnValue().Set(CSOUNDMSG_FG_GREEN); }
  static NAN_GETTER(Yellow) { info.GetReturnValue().Set(CSOUNDMSG_FG_YELLOW); }
  static NAN_GETTER(Blue) { info.GetReturnValue().Set(CSOUNDMSG_FG_BLUE); }
  static NAN_GETTER(Magenta) { info.GetReturnValue().Set(CSOUNDMSG_FG_MAGENTA); }
  static NAN_GETTER(Cyan) { info.GetReturnValue().Set(CSOUNDMSG_FG_CYAN); }
  static NAN_GETTER(White) { info.GetReturnValue().Set(CSOUNDMSG_FG_WHITE); }
  static NAN_GETTER(Mask) { info.GetReturnValue().Set(CSOUNDMSG_FG_COLOR_MASK); }
};

struct CsoundMessageAttribute {
  static NAN_GETTER(Bold) { info.GetReturnValue().Set(CSOUNDMSG_FG_BOLD); }
  static NAN_GETTER(Underline) { info.GetReturnValue().Set(CSOUNDMSG_FG_UNDERLINE); }
  static NAN_GETTER(Mask) { info.GetReturnValue().Set(CSOUNDMSG_FG_ATTR_MASK); }
};

struct CsoundMessageBackgroundColor {
  static NAN_GETTER(Black) { info.GetReturnValue().Set(CSOUNDMSG_BG_BLACK); }
  static NAN_GETTER(Red) { info.GetReturnValue().Set(CSOUNDMSG_BG_RED); }
  static NAN_GETTER(Green) { info.GetReturnValue().Set(CSOUNDMSG_BG_GREEN); }
  static NAN_GETTER(Orange) { info.GetReturnValue().Set(CSOUNDMSG_BG_ORANGE); }
  static NAN_GETTER(Blue) { info.GetReturnValue().Set(CSOUNDMSG_BG_BLUE); }
  static NAN_GETTER(Magenta) { info.GetReturnValue().Set(CSOUNDMSG_BG_MAGENTA); }
  static NAN_GETTER(Cyan) { info.GetReturnValue().Set(CSOUNDMSG_BG_CYAN); }
  static NAN_GETTER(Grey) { info.GetReturnValue().Set(CSOUNDMSG_BG_GREY); }
  static NAN_GETTER(Mask) { info.GetReturnValue().Set(CSOUNDMSG_BG_COLOR_MASK); }
};

static void CsoundMessageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  ((CSOUNDWrapper *)csoundGetHostData(Csound))->queueMessage(attributes, format, argumentList);
}
static CSOUND_CALLBACK_METHOD(Message)

static NAN_METHOD(GetMessageLevel) {
  info.GetReturnValue().Set(Nan::New(csoundGetMessageLevel(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(SetMessageLevel) {
  csoundSetMessageLevel(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value());
}

static NAN_METHOD(CreateMessageBuffer) {
  csoundCreateMessageBuffer(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value());
}

static NAN_METHOD(GetFirstMessage ) {
  setReturnValueWithCString(info.GetReturnValue(), csoundGetFirstMessage(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(GetFirstMessageAttr) {
  info.GetReturnValue().Set(Nan::New(csoundGetFirstMessageAttr(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(PopFirstMessage) {
  csoundPopFirstMessage(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(GetMessageCnt) {
  info.GetReturnValue().Set(Nan::New(csoundGetMessageCnt(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(DestroyMessageBuffer) {
  csoundDestroyMessageBuffer(CsoundFromFunctionCallbackInfo(info));
}

template <typename ItemType, typename WrapperType>
static void performCsoundListCreationFunction(Nan::NAN_METHOD_ARGS_TYPE info, int (*listCreationFunction)(CSOUND *, ItemType **), Nan::Persistent<v8::FunctionTemplate> *ListProxyConstructorRef, Nan::Persistent<v8::FunctionTemplate> *ItemProxyConstructorRef) {
  ItemType *list = NULL;
  int length = listCreationFunction(CsoundFromFunctionCallbackInfo(info), &list);
  if (list && length >= 0) {
    v8::Local<v8::Object> listProxy = Nan::New<v8::FunctionTemplate>(*ListProxyConstructorRef)->GetFunction()->NewInstance();
    listProxy->SetAlignedPointerInInternalField(0, list);
    v8::Local<v8::Array> array = info[1].As<v8::Array>();
    array->SetHiddenValue(Nan::New("Csound::listProxy").ToLocalChecked(), listProxy);
    for (int i = 0; i < length; i++) {
      v8::Local<v8::Object> itemProxy = Nan::New<v8::FunctionTemplate>(*ItemProxyConstructorRef)->GetFunction()->NewInstance();
      Nan::ObjectWrap::Unwrap<WrapperType>(itemProxy)->setItem(list[i]);
      array->Set(i, itemProxy);
    }
  }
  info.GetReturnValue().Set(Nan::New(length));
}

template <typename ItemType>
static void performCsoundListDestructionFunction(Nan::NAN_METHOD_ARGS_TYPE info, void (*listDestructionFunction)(CSOUND *, ItemType *)) {
  v8::Local<v8::Array> array = info[1].As<v8::Array>();
  array->Set(Nan::New("length").ToLocalChecked(), Nan::New(0));
  v8::Local<v8::String> key = Nan::New("Csound::listProxy").ToLocalChecked();
  v8::Local<v8::Object> listProxy = array->GetHiddenValue(key)->ToObject();
  ItemType *list = (ItemType *)listProxy->GetAlignedPointerFromInternalField(0);
  listProxy->SetAlignedPointerInInternalField(0, NULL);
  array->DeleteHiddenValue(key);
  listDestructionFunction(CsoundFromFunctionCallbackInfo(info), list);
}

template <typename T>
struct CsoundListItemWrapper : public Nan::ObjectWrap {
  T item;

  void setItem(T item) {
    this->item = item;
  }
};

static Nan::Persistent<v8::FunctionTemplate> ControlChannelListProxyConstructor;
static Nan::Persistent<v8::FunctionTemplate> ControlChannelInfoProxyConstructor;
struct ControlChannelInfoWrapper : CsoundListItemWrapper<controlChannelInfo_t> {
  static NAN_METHOD(New) {
    (new ControlChannelInfoWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static controlChannelInfo_t itemFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<ControlChannelInfoWrapper>(info.This())->item;
  }
  static NAN_GETTER(name) { setReturnValueWithCString(info.GetReturnValue(), itemFromPropertyCallbackInfo(info).name); }
  static NAN_GETTER(type) { info.GetReturnValue().Set(Nan::New(itemFromPropertyCallbackInfo(info).type)); }
};

static NAN_METHOD(ListChannels) {
  performCsoundListCreationFunction<controlChannelInfo_t, ControlChannelInfoWrapper>(info, csoundListChannels, &ControlChannelListProxyConstructor, &ControlChannelInfoProxyConstructor);
}

static NAN_METHOD(DeleteChannelList) {
  performCsoundListDestructionFunction<controlChannelInfo_t>(info, csoundDeleteChannelList);
}

static NAN_METHOD(GetControlChannel) {
  int status;
  info.GetReturnValue().Set(Nan::New(csoundGetControlChannel(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]), &status)));
  v8::Local<v8::Value> value = info[2];
  if (value->IsObject())
    value.As<v8::Object>()->Set(Nan::New("status").ToLocalChecked(), Nan::New(status));
}

static NAN_METHOD(SetControlChannel) {
  csoundSetControlChannel(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]), info[2]->NumberValue());
}

static NAN_METHOD(ScoreEvent) {
  int status;
  Nan::Utf8String *eventTypeString = new Nan::Utf8String(info[1]);
  if (eventTypeString->length() != 1) {
    status = CSOUND_ERROR;
  } else {
    char eventType = (**eventTypeString)[0];
    v8::Local<v8::Value> value = info[2];
    long parameterFieldCount = value->IsObject() ? value.As<v8::Object>()->Get(Nan::New("length").ToLocalChecked())->Int32Value() : 0;
    if (parameterFieldCount > 0) {
      v8::Local<v8::Object> object = value.As<v8::Object>();
      MYFLT *parameterFieldValues = (MYFLT *)malloc(sizeof(MYFLT) * parameterFieldCount);
      for (long i = 0; i < parameterFieldCount; i++) {
        parameterFieldValues[i] = object->Get(i)->NumberValue();
      }
      status = csoundScoreEvent(CsoundFromFunctionCallbackInfo(info), eventType, parameterFieldValues, parameterFieldCount);
      free(parameterFieldValues);
    } else {
      status = csoundScoreEvent(CsoundFromFunctionCallbackInfo(info), eventType, NULL, 0);
    }
  }
  delete eventTypeString;
  info.GetReturnValue().Set(Nan::New(status));
}

static NAN_METHOD(InputMessage) {
  csoundInputMessage(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]));
}

static NAN_METHOD(TableLength) {
  info.GetReturnValue().Set(Nan::New(csoundTableLength(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value())));
}

static NAN_METHOD(TableGet) {
  info.GetReturnValue().Set(Nan::New(csoundTableGet(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value(), info[2]->Int32Value())));
}

static NAN_METHOD(TableSet) {
  csoundTableSet(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value(), info[2]->Int32Value(), info[3]->NumberValue());
}

static NAN_METHOD(SetIsGraphable) {
  info.GetReturnValue().Set(Nan::New(csoundSetIsGraphable(CsoundFromFunctionCallbackInfo(info), info[1]->BooleanValue())));
}

static void CsoundMakeGraphCallback(CSOUND *Csound, WINDAT *windowData, const char *name) {
  ((CSOUNDWrapper *)csoundGetHostData(Csound))->queueMakeGraph(windowData, name);
}
static CSOUND_CALLBACK_METHOD(MakeGraph)

static void CsoundDrawGraphCallback(CSOUND *Csound, WINDAT *windowData) {
  ((CSOUNDWrapper *)csoundGetHostData(Csound))->queueDrawGraph(windowData);
}
static CSOUND_CALLBACK_METHOD(DrawGraph)

static Nan::Persistent<v8::FunctionTemplate> OpcodeListProxyConstructor;
static Nan::Persistent<v8::FunctionTemplate> OpcodeListEntryProxyConstructor;
struct OpcodeListEntryWrapper : public CsoundListItemWrapper<opcodeListEntry> {
  static NAN_METHOD(New) {
    (new OpcodeListEntryWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static opcodeListEntry itemFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<OpcodeListEntryWrapper>(info.This())->item;
  }
  static NAN_GETTER(opname) { setReturnValueWithCString(info.GetReturnValue(), itemFromPropertyCallbackInfo(info).opname); }
  static NAN_GETTER(outypes) { setReturnValueWithCString(info.GetReturnValue(), itemFromPropertyCallbackInfo(info).outypes); }
  static NAN_GETTER(intypes) { setReturnValueWithCString(info.GetReturnValue(), itemFromPropertyCallbackInfo(info).intypes); }
};

static NAN_METHOD(NewOpcodeList) {
  performCsoundListCreationFunction<opcodeListEntry, OpcodeListEntryWrapper>(info, csoundNewOpcodeList, &OpcodeListProxyConstructor, &OpcodeListEntryProxyConstructor);
}

static NAN_METHOD(DisposeOpcodeList) {
  performCsoundListDestructionFunction<opcodeListEntry>(info, csoundDisposeOpcodeList);
}

static NAN_METHOD(GetEnv) {
  setReturnValueWithCString(info.GetReturnValue(), csoundGetEnv(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1])));
}

static NAN_METHOD(SetGlobalEnv) {
  info.GetReturnValue().Set(csoundSetGlobalEnv(*Nan::Utf8String(info[0]), *Nan::Utf8String(info[1])));
}

static Nan::Persistent<v8::FunctionTemplate> UtilityNameListProxyConstructor;

static NAN_METHOD(ListUtilities) {
  char **list = csoundListUtilities(CsoundFromFunctionCallbackInfo(info));
  if (list) {
    v8::Local<v8::Object> listProxy = Nan::New<v8::FunctionTemplate>(UtilityNameListProxyConstructor)->GetFunction()->NewInstance();
    listProxy->SetAlignedPointerInInternalField(0, list);
    v8::Local<v8::Array> array = Nan::New<v8::Array>();
    array->SetHiddenValue(Nan::New("Csound::listProxy").ToLocalChecked(), listProxy);
    for (uint32_t i = 0; char *utilityName = list[i]; i++) {
      array->Set(i, Nan::New(utilityName).ToLocalChecked());
    }
    info.GetReturnValue().Set(array);
  } else {
    info.GetReturnValue().SetNull();
  }
}

static NAN_METHOD(DeleteUtilityList) {
  performCsoundListDestructionFunction<char *>(info, csoundDeleteUtilityList);
}

static NAN_METHOD(GetUtilityDescription) {
  setReturnValueWithCString(info.GetReturnValue(), csoundGetUtilityDescription(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1])));
}

struct CsoundStatus {
  static NAN_GETTER(Success) { info.GetReturnValue().Set(CSOUND_SUCCESS); }
  static NAN_GETTER(Error) { info.GetReturnValue().Set(CSOUND_ERROR); }
  static NAN_GETTER(Initialization) { info.GetReturnValue().Set(CSOUND_INITIALIZATION); }
  static NAN_GETTER(Performance) { info.GetReturnValue().Set(CSOUND_PERFORMANCE); }
  static NAN_GETTER(Memory) { info.GetReturnValue().Set(CSOUND_MEMORY); }
  static NAN_GETTER(Signal) { info.GetReturnValue().Set(CSOUND_SIGNAL); }
};

#if CSOUND_6_04_OR_LATER
#include <csound/csdebug.h>

static NAN_METHOD(DebuggerInit) {
  csoundDebuggerInit(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(DebuggerClean) {
  csoundDebuggerClean(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(SetInstrumentBreakpoint) {
  csoundSetInstrumentBreakpoint(CsoundFromFunctionCallbackInfo(info), info[1]->NumberValue(), info[2]->Int32Value());
}

static NAN_METHOD(RemoveInstrumentBreakpoint) {
  csoundRemoveInstrumentBreakpoint(CsoundFromFunctionCallbackInfo(info), info[1]->NumberValue());
}

static NAN_METHOD(ClearBreakpoints) {
  csoundClearBreakpoints(CsoundFromFunctionCallbackInfo(info));
}

static Nan::Persistent<v8::FunctionTemplate> DebuggerInstrumentProxyConstructor;

struct DebuggerInstrumentWrapper : public Nan::ObjectWrap {
  debug_instr_t *instrument;

  static NAN_METHOD(New) {
    (new DebuggerInstrumentWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static debug_instr_t *instrumentFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<DebuggerInstrumentWrapper>(info.This())->instrument;
  }

  static NAN_GETTER(p1) { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->p1)); }
  static NAN_GETTER(p2) { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->p2)); }
  static NAN_GETTER(p3) { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->p3)); }
  static NAN_GETTER(kcounter) { info.GetReturnValue().Set(Nan::New((uint32_t)instrumentFromPropertyCallbackInfo(info)->kcounter)); }
  static NAN_GETTER(line) { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->line)); }

  static NAN_GETTER(next) {
    debug_instr_t *next = instrumentFromPropertyCallbackInfo(info)->next;
    if (next) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerInstrumentProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<DebuggerInstrumentWrapper>(proxy)->instrument = next;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static Nan::Persistent<v8::FunctionTemplate> DebuggerOpcodeProxyConstructor;

struct DebuggerOpcodeWrapper : public Nan::ObjectWrap {
  debug_opcode_t *opcode;

  static NAN_METHOD(New) {
    (new DebuggerOpcodeWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static debug_opcode_t *opcodeFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<DebuggerOpcodeWrapper>(info.This())->opcode;
  }

  static NAN_GETTER(opname) { info.GetReturnValue().Set(Nan::New(opcodeFromPropertyCallbackInfo(info)->opname).ToLocalChecked()); }
  static NAN_GETTER(line) { info.GetReturnValue().Set(Nan::New(opcodeFromPropertyCallbackInfo(info)->line)); }

  static void setReturnValueWithDebuggerOpcode(Nan::ReturnValue<v8::Value> returnValue, debug_opcode_t *opcode) {
    if (opcode) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerOpcodeProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<DebuggerOpcodeWrapper>(proxy)->opcode = opcode;
      returnValue.Set(proxy);
    } else {
      returnValue.SetNull();
    }
  }
  static NAN_GETTER(next) { setReturnValueWithDebuggerOpcode(info.GetReturnValue(), opcodeFromPropertyCallbackInfo(info)->next); }
  static NAN_GETTER(prev) { setReturnValueWithDebuggerOpcode(info.GetReturnValue(), opcodeFromPropertyCallbackInfo(info)->prev); }
};

static Nan::Persistent<v8::FunctionTemplate> DebuggerVariableProxyConstructor;

struct DebuggerVariableWrapper : public Nan::ObjectWrap {
  debug_variable_t *variable;

  static NAN_METHOD(New) {
    (new DebuggerVariableWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static debug_variable_t *variableFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<DebuggerVariableWrapper>(info.This())->variable;
  }

  static NAN_GETTER(name) { info.GetReturnValue().Set(Nan::New(variableFromPropertyCallbackInfo(info)->name).ToLocalChecked()); }
  static NAN_GETTER(typeName) { info.GetReturnValue().Set(Nan::New(variableFromPropertyCallbackInfo(info)->typeName).ToLocalChecked()); }

  static NAN_GETTER(data) {
    debug_variable_t *variable = variableFromPropertyCallbackInfo(info);
    const char *typeName = variable->typeName;
    if (strcmp("S", typeName) == 0)
      info.GetReturnValue().Set(Nan::New((char *)variable->data).ToLocalChecked());
    else
      info.GetReturnValue().Set(Nan::New(*((MYFLT *)variable->data)));
  }

  static NAN_GETTER(next) {
    debug_variable_t *next = variableFromPropertyCallbackInfo(info)->next;
    if (next) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerVariableProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<DebuggerVariableWrapper>(proxy)->variable = next;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static Nan::Persistent<v8::FunctionTemplate> DebuggerBreakpointInfoProxyConstructor;

struct DebuggerBreakpointInfoWrapper : public Nan::ObjectWrap {
  debug_bkpt_info_t *breakpointInfo;

  static NAN_METHOD(New) {
    (new DebuggerBreakpointInfoWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static debug_bkpt_info_t *breakpointInfoFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<DebuggerBreakpointInfoWrapper>(info.This())->breakpointInfo;
  }

  static void setReturnValueWithDebuggerInstrument(Nan::ReturnValue<v8::Value> returnValue, debug_instr_t *instrument) {
    if (instrument) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerInstrumentProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<DebuggerInstrumentWrapper>(proxy)->instrument = instrument;
      returnValue.Set(proxy);
    } else {
      returnValue.SetNull();
    }
  }

  static NAN_GETTER(breakpointInstr) {
    setReturnValueWithDebuggerInstrument(info.GetReturnValue(), breakpointInfoFromPropertyCallbackInfo(info)->breakpointInstr);
  }

  static NAN_GETTER(instrVarList) {
    debug_variable_t *instrVarList = breakpointInfoFromPropertyCallbackInfo(info)->instrVarList;
    if (instrVarList) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerVariableProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<DebuggerVariableWrapper>(proxy)->variable = instrVarList;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }

  static NAN_GETTER(instrListHead) {
    setReturnValueWithDebuggerInstrument(info.GetReturnValue(), breakpointInfoFromPropertyCallbackInfo(info)->instrListHead);
  }

  static NAN_GETTER(currentOpcode) {
    debug_opcode_t *opcode = breakpointInfoFromPropertyCallbackInfo(info)->currentOpcode;
    if (opcode) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerOpcodeProxyConstructor)->GetFunction()->NewInstance();
      Unwrap<DebuggerOpcodeWrapper>(proxy)->opcode = opcode;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static void CsoundBreakpointCallback(CSOUND *Csound, debug_bkpt_info_t *breakpointInfo, void *userData) {
  const int argc = 1;
  v8::Local<v8::Value> argv[argc];
  v8::Local<v8::Object> proxy = Nan::New(DebuggerBreakpointInfoProxyConstructor)->GetFunction()->NewInstance();
  DebuggerBreakpointInfoWrapper::Unwrap<DebuggerBreakpointInfoWrapper>(proxy)->breakpointInfo = breakpointInfo;
  argv[0] = proxy;
  ((CSOUNDWrapper *)csoundGetHostData(Csound))->CsoundBreakpointCallbackObject->Call(argc, argv);
}
static NAN_METHOD(SetBreakpointCallback) {
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  v8::Local<v8::Value> value = info[1];
  if (value->IsFunction()) {
    wrapper->CsoundBreakpointCallbackObject = new Nan::Callback(value.As<v8::Function>());
    csoundSetBreakpointCallback(wrapper->Csound, CsoundBreakpointCallback, NULL);
  } else {
    wrapper->CsoundBreakpointCallbackObject = NULL;
    csoundSetBreakpointCallback(wrapper->Csound, NULL, NULL);
  }
}

static NAN_METHOD(DebugContinue) {
  csoundDebugContinue(CsoundFromFunctionCallbackInfo(info));
}

static NAN_METHOD(DebugStop) {
  csoundDebugStop(CsoundFromFunctionCallbackInfo(info));
}

#endif // CSOUND_6_04_OR_LATER

static NAN_MODULE_INIT(init) {
  Nan::SetMethod(target, "Create", Create);
  Nan::SetMethod(target, "Destroy", Destroy);
  Nan::SetMethod(target, "GetVersion", GetVersion);
  Nan::SetMethod(target, "GetAPIVersion", GetAPIVersion);

  Nan::SetMethod(target, "ParseOrc", ParseOrc);
  Nan::SetMethod(target, "CompileTree", CompileTree);
  Nan::SetMethod(target, "DeleteTree", DeleteTree);
  Nan::SetMethod(target, "CompileOrc", CompileOrc);
  Nan::SetMethod(target, "EvalCode", EvalCode);
#ifndef _MSC_VER
  Nan::SetMethod(target, "InitializeCscore", InitializeCscore);
#endif
  Nan::SetMethod(target, "CompileArgs", CompileArgs);
  Nan::SetMethod(target, "Start", Start);
  Nan::SetMethod(target, "Compile", Compile);
  Nan::SetMethod(target, "CompileCsd", CompileCsd);
  Nan::SetMethod(target, "Perform", Perform);
  Nan::SetMethod(target, "PerformKsmps", PerformKsmps);
  Nan::SetMethod(target, "PerformBuffer", PerformBuffer);
  Nan::SetMethod(target, "PerformAsync", PerformAsync);
  Nan::SetMethod(target, "Stop", Stop);
  Nan::SetMethod(target, "Cleanup", Cleanup);
  Nan::SetMethod(target, "Reset", Reset);

  Nan::SetMethod(target, "GetSr", GetSr);
  Nan::SetMethod(target, "GetKr", GetKr);
  Nan::SetMethod(target, "GetKsmps", GetKsmps);
  Nan::SetMethod(target, "GetNchnls", GetNchnls);
  Nan::SetMethod(target, "GetNchnlsInput", GetNchnlsInput);
  Nan::SetMethod(target, "Get0dBFS", Get0dBFS);
  Nan::SetMethod(target, "GetCurrentTimeSamples", GetCurrentTimeSamples);
  Nan::SetMethod(target, "GetSizeOfMYFLT", GetSizeOfMYFLT);
  Nan::SetMethod(target, "GetHostData", GetHostData);
  Nan::SetMethod(target, "SetHostData", SetHostData);
  Nan::SetMethod(target, "SetOption", SetOption);
  Nan::SetMethod(target, "GetDebug", GetDebug);
  Nan::SetMethod(target, "SetDebug", SetDebug);

  Nan::SetMethod(target, "GetOutputName", GetOutputName);

  Nan::SetMethod(target, "ReadScore", ReadScore);
  Nan::SetMethod(target, "GetScoreTime", GetScoreTime);
  Nan::SetMethod(target, "IsScorePending", IsScorePending);
  Nan::SetMethod(target, "SetScorePending", SetScorePending);
  Nan::SetMethod(target, "GetScoreOffsetSeconds", GetScoreOffsetSeconds);
  Nan::SetMethod(target, "SetScoreOffsetSeconds", SetScoreOffsetSeconds);
  Nan::SetMethod(target, "RewindScore", RewindScore);

  Nan::SetMethod(target, "Message", Message);
  Nan::SetMethod(target, "MessageS", MessageS);
  Nan::SetMethod(target, "SetMessageCallback", SetMessageCallback);
  Nan::SetMethod(target, "GetMessageLevel", GetMessageLevel);
  Nan::SetMethod(target, "SetMessageLevel", SetMessageLevel);
  Nan::SetMethod(target, "CreateMessageBuffer", CreateMessageBuffer);
  Nan::SetMethod(target, "GetFirstMessage", GetFirstMessage);
  Nan::SetMethod(target, "GetFirstMessageAttr", GetFirstMessageAttr);
  Nan::SetMethod(target, "PopFirstMessage", PopFirstMessage);
  Nan::SetMethod(target, "GetMessageCnt", GetMessageCnt);
  Nan::SetMethod(target, "DestroyMessageBuffer", DestroyMessageBuffer);

  Nan::SetAccessor(target, Nan::New("MSG_DEFAULT").ToLocalChecked(), CsoundMessageType::Default);
  Nan::SetAccessor(target, Nan::New("MSG_ERROR").ToLocalChecked(), CsoundMessageType::Error);
  Nan::SetAccessor(target, Nan::New("MSG_ORCH").ToLocalChecked(), CsoundMessageType::OrchestraOpcode);
  Nan::SetAccessor(target, Nan::New("MSG_REALTIME").ToLocalChecked(), CsoundMessageType::RealTime);
  Nan::SetAccessor(target, Nan::New("MSG_WARNING").ToLocalChecked(), CsoundMessageType::Warning);
  Nan::SetAccessor(target, Nan::New("MSG_TYPE_MASK").ToLocalChecked(), CsoundMessageType::Mask);

  Nan::SetAccessor(target, Nan::New("MSG_FG_BLACK").ToLocalChecked(), CsoundMessageForegroundColor::Black);
  Nan::SetAccessor(target, Nan::New("MSG_FG_RED").ToLocalChecked(), CsoundMessageForegroundColor::Red);
  Nan::SetAccessor(target, Nan::New("MSG_FG_GREEN").ToLocalChecked(), CsoundMessageForegroundColor::Green);
  Nan::SetAccessor(target, Nan::New("MSG_FG_YELLOW").ToLocalChecked(), CsoundMessageForegroundColor::Yellow);
  Nan::SetAccessor(target, Nan::New("MSG_FG_BLUE").ToLocalChecked(), CsoundMessageForegroundColor::Blue);
  Nan::SetAccessor(target, Nan::New("MSG_FG_MAGENTA").ToLocalChecked(), CsoundMessageForegroundColor::Magenta);
  Nan::SetAccessor(target, Nan::New("MSG_FG_CYAN").ToLocalChecked(), CsoundMessageForegroundColor::Cyan);
  Nan::SetAccessor(target, Nan::New("MSG_FG_WHITE").ToLocalChecked(), CsoundMessageForegroundColor::White);
  Nan::SetAccessor(target, Nan::New("MSG_FG_COLOR_MASK").ToLocalChecked(), CsoundMessageForegroundColor::Mask);

  Nan::SetAccessor(target, Nan::New("MSG_FG_BOLD").ToLocalChecked(), CsoundMessageAttribute::Bold);
  Nan::SetAccessor(target, Nan::New("MSG_FG_UNDERLINE").ToLocalChecked(), CsoundMessageAttribute::Underline);
  Nan::SetAccessor(target, Nan::New("MSG_FG_ATTR_MASK").ToLocalChecked(), CsoundMessageAttribute::Mask);

  Nan::SetAccessor(target, Nan::New("MSG_BG_BLACK").ToLocalChecked(), CsoundMessageBackgroundColor::Black);
  Nan::SetAccessor(target, Nan::New("MSG_BG_RED").ToLocalChecked(), CsoundMessageBackgroundColor::Red);
  Nan::SetAccessor(target, Nan::New("MSG_BG_GREEN").ToLocalChecked(), CsoundMessageBackgroundColor::Green);
  Nan::SetAccessor(target, Nan::New("MSG_BG_ORANGE").ToLocalChecked(), CsoundMessageBackgroundColor::Orange);
  Nan::SetAccessor(target, Nan::New("MSG_BG_BLUE").ToLocalChecked(), CsoundMessageBackgroundColor::Blue);
  Nan::SetAccessor(target, Nan::New("MSG_BG_MAGENTA").ToLocalChecked(), CsoundMessageBackgroundColor::Magenta);
  Nan::SetAccessor(target, Nan::New("MSG_BG_CYAN").ToLocalChecked(), CsoundMessageBackgroundColor::Cyan);
  Nan::SetAccessor(target, Nan::New("MSG_BG_GREY").ToLocalChecked(), CsoundMessageBackgroundColor::Grey);
  Nan::SetAccessor(target, Nan::New("MSG_BG_COLOR_MASK").ToLocalChecked(), CsoundMessageBackgroundColor::Mask);

  Nan::SetMethod(target, "ListChannels", ListChannels);
  Nan::SetMethod(target, "DeleteChannelList", DeleteChannelList);
  Nan::SetMethod(target, "GetControlChannel", GetControlChannel);
  Nan::SetMethod(target, "SetControlChannel", SetControlChannel);
  Nan::SetMethod(target, "ScoreEvent", ScoreEvent);
  Nan::SetMethod(target, "InputMessage", InputMessage);

  Nan::SetMethod(target, "TableLength", TableLength);
  Nan::SetMethod(target, "TableGet", TableGet);
  Nan::SetMethod(target, "TableSet", TableSet);

  Nan::SetMethod(target, "SetIsGraphable", SetIsGraphable);
  Nan::SetMethod(target, "SetMakeGraphCallback", SetMakeGraphCallback);
  Nan::SetMethod(target, "SetDrawGraphCallback", SetDrawGraphCallback);

  Nan::SetMethod(target, "NewOpcodeList", NewOpcodeList);
  Nan::SetMethod(target, "DisposeOpcodeList", DisposeOpcodeList);

  Nan::SetMethod(target, "GetEnv", GetEnv);
  Nan::SetMethod(target, "SetGlobalEnv", SetGlobalEnv);
  Nan::SetMethod(target, "ListUtilities", ListUtilities);
  Nan::SetMethod(target, "DeleteUtilityList", DeleteUtilityList);
  Nan::SetMethod(target, "GetUtilityDescription", GetUtilityDescription);

  Nan::SetAccessor(target, Nan::New("SUCCESS").ToLocalChecked(), CsoundStatus::Success);
  Nan::SetAccessor(target, Nan::New("ERROR").ToLocalChecked(), CsoundStatus::Error);
  Nan::SetAccessor(target, Nan::New("INITIALIZATION").ToLocalChecked(), CsoundStatus::Initialization);
  Nan::SetAccessor(target, Nan::New("PERFORMANCE").ToLocalChecked(), CsoundStatus::Performance);
  Nan::SetAccessor(target, Nan::New("MEMORY").ToLocalChecked(), CsoundStatus::Memory);
  Nan::SetAccessor(target, Nan::New("SIGNAL").ToLocalChecked(), CsoundStatus::Signal);

  v8::Local<v8::FunctionTemplate> classTemplate = Nan::New<v8::FunctionTemplate>(WINDATWrapper::New);
  WINDATProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("WINDAT").ToLocalChecked());
  v8::Local<v8::ObjectTemplate> instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("windid").ToLocalChecked(), WINDATWrapper::windid);
  Nan::SetAccessor(instanceTemplate, Nan::New("fdata").ToLocalChecked(), WINDATWrapper::fdata);
  Nan::SetAccessor(instanceTemplate, Nan::New("caption").ToLocalChecked(), WINDATWrapper::caption);
  Nan::SetAccessor(instanceTemplate, Nan::New("polarity").ToLocalChecked(), WINDATWrapper::polarity);
  Nan::SetAccessor(instanceTemplate, Nan::New("max").ToLocalChecked(), WINDATWrapper::max);
  Nan::SetAccessor(instanceTemplate, Nan::New("min").ToLocalChecked(), WINDATWrapper::min);
  Nan::SetAccessor(instanceTemplate, Nan::New("oabsmax").ToLocalChecked(), WINDATWrapper::oabsmax);

  classTemplate = Nan::New<v8::FunctionTemplate>(CSOUNDWrapper::New);
  CSOUNDProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("CSOUND").ToLocalChecked());
  classTemplate->InstanceTemplate()->SetInternalFieldCount(1);

  classTemplate = Nan::New<v8::FunctionTemplate>(ORCTOKENWrapper::New);
  ORCTOKENProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("ORCTOKEN").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("type").ToLocalChecked(), ORCTOKENWrapper::type);
  Nan::SetAccessor(instanceTemplate, Nan::New("lexeme").ToLocalChecked(), ORCTOKENWrapper::lexeme);
  Nan::SetAccessor(instanceTemplate, Nan::New("value").ToLocalChecked(), ORCTOKENWrapper::value);
  Nan::SetAccessor(instanceTemplate, Nan::New("fvalue").ToLocalChecked(), ORCTOKENWrapper::fvalue);
  Nan::SetAccessor(instanceTemplate, Nan::New("optype").ToLocalChecked(), ORCTOKENWrapper::optype);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), ORCTOKENWrapper::next);

  classTemplate = Nan::New<v8::FunctionTemplate>(TREEWrapper::New);
  TREEProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("TREE").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("type").ToLocalChecked(), TREEWrapper::type);
  Nan::SetAccessor(instanceTemplate, Nan::New("value").ToLocalChecked(), TREEWrapper::value);
  Nan::SetAccessor(instanceTemplate, Nan::New("rate").ToLocalChecked(), TREEWrapper::rate);
  Nan::SetAccessor(instanceTemplate, Nan::New("len").ToLocalChecked(), TREEWrapper::len);
  Nan::SetAccessor(instanceTemplate, Nan::New("line").ToLocalChecked(), TREEWrapper::line);
  Nan::SetAccessor(instanceTemplate, Nan::New("locn").ToLocalChecked(), TREEWrapper::locn);
  Nan::SetAccessor(instanceTemplate, Nan::New("left").ToLocalChecked(), TREEWrapper::left);
  Nan::SetAccessor(instanceTemplate, Nan::New("right").ToLocalChecked(), TREEWrapper::right);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), TREEWrapper::next);

  classTemplate = Nan::New<v8::FunctionTemplate>();
  OpcodeListProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("OpcodeList").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);

  classTemplate = Nan::New<v8::FunctionTemplate>(OpcodeListEntryWrapper::New);
  OpcodeListEntryProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("opcodeListEntry").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("opname").ToLocalChecked(), OpcodeListEntryWrapper::opname);
  Nan::SetAccessor(instanceTemplate, Nan::New("outypes").ToLocalChecked(), OpcodeListEntryWrapper::outypes);
  Nan::SetAccessor(instanceTemplate, Nan::New("intypes").ToLocalChecked(), OpcodeListEntryWrapper::intypes);

  classTemplate = Nan::New<v8::FunctionTemplate>();
  UtilityNameListProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("UtilityNameList").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);

#if CSOUND_6_04_OR_LATER
  Nan::SetMethod(target, "DebuggerInit", DebuggerInit);
  Nan::SetMethod(target, "DebuggerClean", DebuggerClean);
  Nan::SetMethod(target, "SetInstrumentBreakpoint", SetInstrumentBreakpoint);
  Nan::SetMethod(target, "RemoveInstrumentBreakpoint", RemoveInstrumentBreakpoint);
  Nan::SetMethod(target, "ClearBreakpoints", ClearBreakpoints);
  Nan::SetMethod(target, "SetBreakpointCallback", SetBreakpointCallback);
  Nan::SetMethod(target, "DebugContinue", DebugContinue);
  Nan::SetMethod(target, "DebugStop", DebugStop);

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerInstrumentWrapper::New);
  DebuggerInstrumentProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("debug_instr_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("p1").ToLocalChecked(), DebuggerInstrumentWrapper::p1);
  Nan::SetAccessor(instanceTemplate, Nan::New("p2").ToLocalChecked(), DebuggerInstrumentWrapper::p2);
  Nan::SetAccessor(instanceTemplate, Nan::New("p3").ToLocalChecked(), DebuggerInstrumentWrapper::p3);
  Nan::SetAccessor(instanceTemplate, Nan::New("kcounter").ToLocalChecked(), DebuggerInstrumentWrapper::kcounter);
  Nan::SetAccessor(instanceTemplate, Nan::New("line").ToLocalChecked(), DebuggerInstrumentWrapper::line);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), DebuggerInstrumentWrapper::next);

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerOpcodeWrapper::New);
  DebuggerOpcodeProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("debug_opcode_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("opname").ToLocalChecked(), DebuggerOpcodeWrapper::opname);
  Nan::SetAccessor(instanceTemplate, Nan::New("line").ToLocalChecked(), DebuggerOpcodeWrapper::line);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), DebuggerOpcodeWrapper::next);
  Nan::SetAccessor(instanceTemplate, Nan::New("prev").ToLocalChecked(), DebuggerOpcodeWrapper::prev);

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerVariableWrapper::New);
  DebuggerVariableProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("debug_variable_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("name").ToLocalChecked(), DebuggerVariableWrapper::name);
  Nan::SetAccessor(instanceTemplate, Nan::New("typeName").ToLocalChecked(), DebuggerVariableWrapper::typeName);
  Nan::SetAccessor(instanceTemplate, Nan::New("data").ToLocalChecked(), DebuggerVariableWrapper::data);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), DebuggerVariableWrapper::next);

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerBreakpointInfoWrapper::New);
  DebuggerBreakpointInfoProxyConstructor.Reset(classTemplate);
  classTemplate->SetClassName(Nan::New("debug_bkpt_info_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("breakpointInstr").ToLocalChecked(), DebuggerBreakpointInfoWrapper::breakpointInstr);
  Nan::SetAccessor(instanceTemplate, Nan::New("instrVarList").ToLocalChecked(), DebuggerBreakpointInfoWrapper::instrVarList);
  Nan::SetAccessor(instanceTemplate, Nan::New("instrListHead").ToLocalChecked(), DebuggerBreakpointInfoWrapper::instrListHead);
  Nan::SetAccessor(instanceTemplate, Nan::New("currentOpcode").ToLocalChecked(), DebuggerBreakpointInfoWrapper::currentOpcode);
#endif // CSOUND_6_04_OR_LATER
}

NODE_MODULE(CsoundAPI, init)
