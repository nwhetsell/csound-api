#include <boost/lockfree/queue.hpp>
#include <cwindow.h>
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
    WINDAT *data = dataFromPropertyCallbackInfo(info);
    v8::Local<v8::Array> fdata = Nan::New<v8::Array>(data->npts);
    for (int32 i = 0; i < data->npts; i++) {
      fdata->Set(i, Nan::New(data->fdata[i]));
    }
    info.GetReturnValue().Set(fdata);
  }
};

struct CsoundGraphCallbackArguments {
  WINDAT *data;

  void getArgv(v8::Local<v8::Value> *argv) const {
    v8::Local<v8::Object> proxy = Nan::New(WINDATProxyConstructor)->GetFunction()->NewInstance();
    WINDATWrapper::Unwrap<WINDATWrapper>(proxy)->data = data;
    argv[0] = proxy;
  }

  void wereSent() {}
};

struct CsoundDrawGraphCallbackArguments : public CsoundGraphCallbackArguments {
  static const int argc = 1;

  static CsoundDrawGraphCallbackArguments create(WINDAT *data) {
    CsoundDrawGraphCallbackArguments arguments;
    arguments.data = data;
    return arguments;
  }
};

struct CsoundMakeGraphCallbackArguments : public CsoundGraphCallbackArguments {
  static const int argc = 2;
  const char *name;

  static CsoundMakeGraphCallbackArguments create(WINDAT *data, const char *name) {
    CsoundMakeGraphCallbackArguments arguments;
    arguments.data = data;
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

// The CSOUND_CALLBACK_METHOD macro associates a Csound callback function with
// a CsoundCallback instance using an appropriate argument type. For example,
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
  } else { \
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

static NAN_METHOD(InitializeCscore) {
  FILE *inputFile = fopen(*Nan::Utf8String(info[1]), "r");
  FILE *outputFile = fopen(*Nan::Utf8String(info[2]), "w");
  info.GetReturnValue().Set(Nan::New(csoundInitializeCscore(CsoundFromFunctionCallbackInfo(info), inputFile, outputFile)));
  fclose(inputFile);
  fclose(outputFile);
}

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
  info.GetReturnValue().Set(csoundGetDebug(CsoundFromFunctionCallbackInfo(info)));
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

static NAN_METHOD(GetMessageLevel) {
  info.GetReturnValue().Set(Nan::New(csoundGetMessageLevel(CsoundFromFunctionCallbackInfo(info))));
}

static NAN_METHOD(SetMessageLevel) {
  csoundSetMessageLevel(CsoundFromFunctionCallbackInfo(info), info[1]->Int32Value());
}

static void CsoundMessageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  ((CSOUNDWrapper *)csoundGetHostData(Csound))->queueMessage(attributes, format, argumentList);
}
static CSOUND_CALLBACK_METHOD(Message)

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

struct OpcodeListEntryWrapper : public Nan::ObjectWrap {
  opcodeListEntry entry;

  static NAN_METHOD(New) {
    (new OpcodeListEntryWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static opcodeListEntry entryFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<OpcodeListEntryWrapper>(info.This())->entry;
  }
  static NAN_GETTER(opname) { setReturnValueWithCString(info.GetReturnValue(), entryFromPropertyCallbackInfo(info).opname); }
  static NAN_GETTER(outypes) { setReturnValueWithCString(info.GetReturnValue(), entryFromPropertyCallbackInfo(info).outypes); }
  static NAN_GETTER(intypes) { setReturnValueWithCString(info.GetReturnValue(), entryFromPropertyCallbackInfo(info).intypes); }
};

static NAN_METHOD(NewOpcodeList) {
  opcodeListEntry *opcodeList = NULL;
  int opcodeCount = csoundNewOpcodeList(CsoundFromFunctionCallbackInfo(info), &opcodeList);
  if (opcodeList && opcodeCount >= 0) {
    v8::Local<v8::Object> listProxy = Nan::New<v8::FunctionTemplate>(OpcodeListProxyConstructor)->GetFunction()->NewInstance();
    listProxy->SetAlignedPointerInInternalField(0, opcodeList);
    v8::Local<v8::Array> opcodeArray = info[1].As<v8::Array>();
    opcodeArray->SetHiddenValue(Nan::New("Csound::opcodeListProxy").ToLocalChecked(), listProxy);
    for (int i = 0; i < opcodeCount; i++) {
      v8::Local<v8::Object> entryProxy = Nan::New<v8::FunctionTemplate>(OpcodeListEntryProxyConstructor)->GetFunction()->NewInstance();
      Nan::ObjectWrap::Unwrap<OpcodeListEntryWrapper>(entryProxy)->entry = opcodeList[i];
      opcodeArray->Set(i, entryProxy);
    }
  }
  info.GetReturnValue().Set(Nan::New(opcodeCount));
}

static NAN_METHOD(DisposeOpcodeList) {
  v8::Local<v8::Array> opcodeArray = info[1].As<v8::Array>();
  opcodeArray->Set(Nan::New("length").ToLocalChecked(), Nan::New(0));
  v8::Local<v8::String> key = Nan::New("Csound::opcodeListProxy").ToLocalChecked();
  v8::Local<v8::Object> listProxy = opcodeArray->GetHiddenValue(key)->ToObject();
  opcodeListEntry *opcodeList = (opcodeListEntry *)listProxy->GetAlignedPointerFromInternalField(0);
  listProxy->SetAlignedPointerInInternalField(0, NULL);
  opcodeArray->DeleteHiddenValue(key);
  csoundDisposeOpcodeList(CsoundFromFunctionCallbackInfo(info), opcodeList);
}

static NAN_METHOD(GetEnv) {
  setReturnValueWithCString(info.GetReturnValue(), csoundGetEnv(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1])));
}

static NAN_METHOD(SetGlobalEnv) {
  info.GetReturnValue().Set(csoundSetGlobalEnv(*Nan::Utf8String(info[0]), *Nan::Utf8String(info[1])));
}

static Nan::Persistent<v8::FunctionTemplate> UtilityNameListProxyConstructor;

static NAN_METHOD(ListUtilities) {
  char **utilityNameList = csoundListUtilities(CsoundFromFunctionCallbackInfo(info));
  if (utilityNameList) {
    v8::Local<v8::Object> listProxy = Nan::New<v8::FunctionTemplate>(UtilityNameListProxyConstructor)->GetFunction()->NewInstance();
    listProxy->SetAlignedPointerInInternalField(0, utilityNameList);
    v8::Local<v8::Array> utilityNameArray = Nan::New<v8::Array>();
    utilityNameArray->SetHiddenValue(Nan::New("Csound::utilityNameListProxy").ToLocalChecked(), listProxy);
    for (uint32_t i = 0; char *utilityName = utilityNameList[i]; i++) {
      utilityNameArray->Set(i, Nan::New(utilityName).ToLocalChecked());
    }
    info.GetReturnValue().Set(utilityNameArray);
  } else {
    info.GetReturnValue().SetNull();
  }
}

static NAN_METHOD(DeleteUtilityList) {
  v8::Local<v8::Array> utilityNameArray = info[1].As<v8::Array>();
  utilityNameArray->Set(Nan::New("length").ToLocalChecked(), Nan::New(0));
  v8::Local<v8::String> key = Nan::New("Csound::utilityNameListProxy").ToLocalChecked();
  v8::Local<v8::Object> listProxy = utilityNameArray->GetHiddenValue(key)->ToObject();
  char **utilityNameList = (char **)listProxy->GetAlignedPointerFromInternalField(0);
  listProxy->SetAlignedPointerInInternalField(0, NULL);
  utilityNameArray->DeleteHiddenValue(key);
  csoundDeleteUtilityList(CsoundFromFunctionCallbackInfo(info), utilityNameList);
}

static NAN_METHOD(GetUtilityDescription) {
  setReturnValueWithCString(info.GetReturnValue(), csoundGetUtilityDescription(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1])));
}

struct CsoundStatus {
  static NAN_GETTER(success) { info.GetReturnValue().Set(CSOUND_SUCCESS); }
  static NAN_GETTER(error) { info.GetReturnValue().Set(CSOUND_ERROR); }
  static NAN_GETTER(initialization) { info.GetReturnValue().Set(CSOUND_INITIALIZATION); }
  static NAN_GETTER(performance) { info.GetReturnValue().Set(CSOUND_PERFORMANCE); }
  static NAN_GETTER(memory) { info.GetReturnValue().Set(CSOUND_MEMORY); }
  static NAN_GETTER(signal) { info.GetReturnValue().Set(CSOUND_SIGNAL); }
};

static NAN_MODULE_INIT(init) {
  Nan::SetAccessor((v8::Local<v8::Object>)target, Nan::New("CSOUND_SUCCESS").ToLocalChecked(), CsoundStatus::success);
  Nan::SetAccessor((v8::Local<v8::Object>)target, Nan::New("CSOUND_ERROR").ToLocalChecked(), CsoundStatus::error);
  Nan::SetAccessor((v8::Local<v8::Object>)target, Nan::New("CSOUND_INITIALIZATION").ToLocalChecked(), CsoundStatus::initialization);
  Nan::SetAccessor((v8::Local<v8::Object>)target, Nan::New("CSOUND_PERFORMANCE").ToLocalChecked(), CsoundStatus::performance);
  Nan::SetAccessor((v8::Local<v8::Object>)target, Nan::New("CSOUND_MEMORY").ToLocalChecked(), CsoundStatus::memory);
  Nan::SetAccessor((v8::Local<v8::Object>)target, Nan::New("CSOUND_SIGNAL").ToLocalChecked(), CsoundStatus::signal);

  Nan::SetMethod(target, "Create", Create);
  Nan::SetMethod(target, "Destroy", Destroy);

  Nan::SetMethod(target, "GetVersion", GetVersion);
  Nan::SetMethod(target, "GetAPIVersion", GetAPIVersion);

  Nan::SetMethod(target, "ParseOrc", ParseOrc);
  Nan::SetMethod(target, "CompileTree", CompileTree);
  Nan::SetMethod(target, "DeleteTree", DeleteTree);
  Nan::SetMethod(target, "CompileOrc", CompileOrc);
  Nan::SetMethod(target, "EvalCode", EvalCode);
  Nan::SetMethod(target, "InitializeCscore", InitializeCscore);
  Nan::SetMethod(target, "CompileArgs", CompileArgs);
  Nan::SetMethod(target, "Start", Start);
  Nan::SetMethod(target, "Compile", Compile);
  Nan::SetMethod(target, "CompileCsd", CompileCsd);
  Nan::SetMethod(target, "Perform", Perform);
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
  Nan::SetMethod(target, "SetMessageCallback", SetMessageCallback);
  Nan::SetMethod(target, "GetMessageLevel", GetMessageLevel);
  Nan::SetMethod(target, "SetMessageLevel", SetMessageLevel);

  Nan::SetMethod(target, "GetControlChannel", GetControlChannel);
  Nan::SetMethod(target, "SetControlChannel", SetControlChannel);
  Nan::SetMethod(target, "ScoreEvent", ScoreEvent);

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
}

NODE_MODULE(CsoundAPI, init)
