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
struct CsoundFileOpenCallbackArguments {
  static const int argc = 4;
  char *path;
  int type;
  int isOpenForWriting;
  int isTemporary;

  static CsoundFileOpenCallbackArguments create(const char *path, int type, int isOpenForWriting, int isTemporary) {
    return {strdup(path), type, isOpenForWriting, isTemporary};
  }

  void getArgv(v8::Local<v8::Value> *argv) const {
    argv[0] = Nan::New(path).ToLocalChecked();
    argv[1] = Nan::New(type);
    argv[2] = Nan::New((bool)isOpenForWriting);
    argv[3] = Nan::New((bool)isTemporary);
  }

  void wereSent() {
    free(path);
  }
};

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

static Nan::Persistent<v8::Function> WINDATProxyConstructor;

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
  static NAN_GETTER(windid)   { info.GetReturnValue().Set(Nan::New((unsigned int)dataFromPropertyCallbackInfo(info)->windid)); }
  static NAN_GETTER(caption)  { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->caption).ToLocalChecked()); }
  static NAN_GETTER(polarity) { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->polarity)); }
  static NAN_GETTER(max)      { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->max)); }
  static NAN_GETTER(min)      { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->min)); }
  static NAN_GETTER(oabsmax)  { info.GetReturnValue().Set(Nan::New(dataFromPropertyCallbackInfo(info)->oabsmax)); }

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
  static const int argc = 1;
  WINDAT *data;
  MYFLT *fdata;

  static CsoundGraphCallbackArguments create(WINDAT *data) {
    CsoundGraphCallbackArguments arguments;
    arguments.setData(data);
    return arguments;
  }

  void getArgv(v8::Local<v8::Value> *argv) const {
    v8::Local<v8::Object> proxy = Nan::New(WINDATProxyConstructor)->NewInstance();
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

struct CsoundEventHandler {
  virtual ~CsoundEventHandler() {};

  virtual void handleStop(CSOUND *Csound) = 0;
  virtual void handleInputMessage(CSOUND *Csound, char *scoreStatement) = 0;
  virtual void handleCompileOrc(CSOUND *Csound, char *orcStatement) = 0;
  virtual int handleReadScore(CSOUND *Csound, char *score) = 0;
  virtual int handleScoreEvent(CSOUND *Csound, char eventType, MYFLT *parameterFieldValues, long parameterFieldCount) = 0;

  virtual bool CsoundDidPerformKsmps(CSOUND *Csound) {
    return false;
  }
};

struct CsoundSynchronousEventHandler : public CsoundEventHandler {
  void handleStop(CSOUND *Csound) {
    csoundStop(Csound);
  }
  void handleInputMessage(CSOUND *Csound, char *scoreStatement) {
    csoundInputMessage(Csound, scoreStatement);
  }
  void handleCompileOrc(CSOUND *Csound, char *orcStatement) {
    csoundCompileOrc(Csound, orcStatement);
  }
  int handleReadScore(CSOUND *Csound, char *score) {
    return csoundReadScore(Csound, score);
  }
  int handleScoreEvent(CSOUND *Csound, char eventType, MYFLT *parameterFieldValues, long parameterFieldCount) {
    int status = csoundScoreEvent(Csound, eventType, parameterFieldValues, parameterFieldCount);
    if (parameterFieldValues)
      free(parameterFieldValues);
    return status;
  }
};

// CSOUNDWrapper instances perform tasks related to callbacks. They also store
// V8 values passed as host data from JavaScript.
static Nan::Persistent<v8::Function> CSOUNDProxyConstructor;
struct CSOUNDWrapper : public Nan::ObjectWrap {
  CSOUND *Csound;
  Nan::Persistent<v8::Value, v8::CopyablePersistentTraits<v8::Value>> hostData;
  CsoundEventHandler *eventHandler;

  CsoundCallback<CsoundFileOpenCallbackArguments> *CsoundFileOpenCallbackObject;

  CsoundCallback<CsoundMessageCallbackArguments> *CsoundMessageCallbackObject;

  CsoundCallback<CsoundMakeGraphCallbackArguments> *CsoundMakeGraphCallbackObject;
  CsoundCallback<CsoundGraphCallbackArguments> *CsoundDrawGraphCallbackObject;
  CsoundCallback<CsoundGraphCallbackArguments> *CsoundKillGraphCallbackObject;

  // Require Csound 6.04 or later to use debugger functions.
#define CSOUND_6_04_OR_LATER CS_VERSION >= 6 && CS_SUBVER >= 4
#if CSOUND_6_04_OR_LATER
  Nan::Callback *CsoundBreakpointCallbackObject;
#endif

  static NAN_METHOD(New) {
    (new CSOUNDWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  CSOUNDWrapper() {
    eventHandler = new CsoundSynchronousEventHandler();
  }
  ~CSOUNDWrapper() {
    delete eventHandler;
  }
};

// The CSOUND_CALLBACK_METHOD macro associates a Csound callback function with a
// CsoundCallback instance using an appropriate argument type. For example,
// CSOUND_CALLBACK_METHOD(Message) associates csoundSetMessageCallback with
// CsoundCallback<CsoundMessageCallbackArguments>;
// CSOUND_CALLBACK_METHOD(MakeGraph) associates csoundSetMakeGraphCallback with
// CsoundCallback<CsoundMakeGraphCallbackArguments>; and so on.
#define CSOUND_CALLBACK_METHOD_1(methodNameStem) CSOUND_CALLBACK_METHOD_2(methodNameStem, methodNameStem)
#define CSOUND_CALLBACK_METHOD_2(methodNameStem, argumentsNameStem) \
NAN_METHOD(Set ## methodNameStem ## Callback) { \
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>()); \
  v8::Local<v8::Value> value = info[1]; \
  if (value->IsFunction()) { \
    wrapper->Csound ## methodNameStem ## CallbackObject = new CsoundCallback<Csound ## argumentsNameStem ## CallbackArguments>(value.As<v8::Function>()); \
    csoundSet ## methodNameStem ## Callback(wrapper->Csound, Csound ## methodNameStem ## Callback); \
  } else if (wrapper->Csound ## methodNameStem ## CallbackObject) { \
    delete wrapper->Csound ## methodNameStem ## CallbackObject; \
    wrapper->Csound ## methodNameStem ## CallbackObject = NULL; \
    csoundSet ## methodNameStem ## Callback(wrapper->Csound, NULL); \
  } \
}
#ifndef _MSC_VER
#  define CSOUND_CALLBACK_METHOD(...) BOOST_PP_OVERLOAD(CSOUND_CALLBACK_METHOD_,__VA_ARGS__)(__VA_ARGS__)
#else
#  define CSOUND_CALLBACK_METHOD(...) BOOST_PP_CAT(BOOST_PP_OVERLOAD(CSOUND_CALLBACK_METHOD_,__VA_ARGS__)(__VA_ARGS__),BOOST_PP_EMPTY())
#endif

// Helper function to set return values to either a string or null.
static void setReturnValueWithCString(Nan::ReturnValue<v8::Value> returnValue, const char *string) {
  if (string)
    returnValue.Set(Nan::New(string).ToLocalChecked());
  else
    returnValue.SetNull();
}

static NAN_METHOD(Initialize) {
  info.GetReturnValue().Set(csoundInitialize(info[0]->Int32Value()));
}

struct CsoundInitializationOption {
  static NAN_GETTER(NoSignalHandlers) { info.GetReturnValue().Set(CSOUNDINIT_NO_SIGNAL_HANDLER); }
  static NAN_GETTER(NoExitFunction)   { info.GetReturnValue().Set(CSOUNDINIT_NO_ATEXIT); }
};

static NAN_METHOD(Create) {
  v8::Local<v8::Object> proxy = Nan::New(CSOUNDProxyConstructor)->NewInstance();
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

static Nan::Persistent<v8::Function> ORCTOKENProxyConstructor;
struct ORCTOKENWrapper : public Nan::ObjectWrap {
  ORCTOKEN *token;

  static NAN_METHOD(New) {
    (new ORCTOKENWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static ORCTOKEN *tokenFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<ORCTOKENWrapper>(info.This())->token;
  }

  static NAN_GETTER(type)   { info.GetReturnValue().Set(Nan::New(tokenFromPropertyCallbackInfo(info)->type)); }
  static NAN_GETTER(value)  { info.GetReturnValue().Set(Nan::New(tokenFromPropertyCallbackInfo(info)->value)); }
  static NAN_GETTER(fvalue) { info.GetReturnValue().Set(Nan::New(tokenFromPropertyCallbackInfo(info)->fvalue)); }
  static NAN_GETTER(lexeme) { setReturnValueWithCString(info.GetReturnValue(), tokenFromPropertyCallbackInfo(info)->lexeme); }
  static NAN_GETTER(optype) { setReturnValueWithCString(info.GetReturnValue(), tokenFromPropertyCallbackInfo(info)->optype); }

  static NAN_GETTER(next) {
    ORCTOKEN *token = tokenFromPropertyCallbackInfo(info)->next;
    if (token) {
      v8::Local<v8::Object> proxy = Nan::New(ORCTOKENProxyConstructor)->NewInstance();
      Unwrap<ORCTOKENWrapper>(proxy)->token = token;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static Nan::Persistent<v8::Function> TREEProxyConstructor;
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
      v8::Local<v8::Object> proxy = Nan::New(ORCTOKENProxyConstructor)->NewInstance();
      Unwrap<ORCTOKENWrapper>(proxy)->token = token;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
  static NAN_GETTER(type) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->type)); }
  static NAN_GETTER(rate) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->rate)); }
  static NAN_GETTER(len)  { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->len)); }
  static NAN_GETTER(line) { info.GetReturnValue().Set(Nan::New(treeFromPropertyCallbackInfo(info)->line)); }
  static NAN_GETTER(locn) { info.GetReturnValue().Set(Nan::New(static_cast<double>(treeFromPropertyCallbackInfo(info)->locn))); }

  static void setPropertyCallbackInfoReturnValueWithTree(Nan::NAN_GETTER_ARGS_TYPE info, TREE *tree) {
    if (tree) {
      v8::Local<v8::Object> proxy = Nan::New(TREEProxyConstructor)->NewInstance();
      Unwrap<TREEWrapper>(proxy)->tree = tree;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
  static NAN_GETTER(left)  { setPropertyCallbackInfoReturnValueWithTree(info, treeFromPropertyCallbackInfo(info)->left); }
  static NAN_GETTER(right) { setPropertyCallbackInfoReturnValueWithTree(info, treeFromPropertyCallbackInfo(info)->right); }
  static NAN_GETTER(next)  { setPropertyCallbackInfoReturnValueWithTree(info, treeFromPropertyCallbackInfo(info)->next); }
};

static NAN_METHOD(ParseOrc) {
  TREE *tree = csoundParseOrc(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]));
  if (tree) {
    v8::Local<v8::Object> proxy = Nan::New(TREEProxyConstructor)->NewInstance();
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

// static NAN_METHOD(CompileOrc) {
//   info.GetReturnValue().Set(Nan::New(csoundCompileOrc(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
// }

static NAN_METHOD(EvalCode) {
  info.GetReturnValue().Set(Nan::New(csoundEvalCode(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]))));
}

// Helper function to pass V8 values to csoundCompileArgs and csoundCompile.
#define CSOUND_6_08_OR_LATER CS_VERSION >= 6 && CS_SUBVER >= 8
#if CSOUND_6_08_OR_LATER
#  define CSOUND_ARGUMENTS_TYPE const char **
#else
#  define CSOUND_ARGUMENTS_TYPE char **
#endif
static Nan::NAN_METHOD_RETURN_TYPE performCsoundCompileFunction(Nan::NAN_METHOD_ARGS_TYPE info, int (*compileFunction)(CSOUND *, int, CSOUND_ARGUMENTS_TYPE)) {
  v8::Local<v8::Array> array = info[1].As<v8::Array>();
  uint32_t argumentCount = array->Length();
  if (argumentCount > 0) {
    char **arguments = (char **)malloc(sizeof(char *) * argumentCount);
    for (uint32_t i = 0; i < argumentCount; i++) {
      arguments[i] = strdup(*Nan::Utf8String(array->Get(i)));
    }
    info.GetReturnValue().Set(Nan::New(compileFunction(CsoundFromFunctionCallbackInfo(info), argumentCount, (CSOUND_ARGUMENTS_TYPE)arguments)));
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

enum CsoundEventType {
  CsoundEventTypeStop,
  CsoundEventTypeReadScore,
  CsoundEventTypeScoreEvent,
  CsoundEventTypeInputMessage,
  CsoundEventTypeCompileOrc
};

struct CsoundEventCommand {
  CsoundEventType type;
  char *score;
  char scoreEventType;
  char *orc;
  MYFLT *parameterFieldValues;
  long parameterFieldCount;

  bool execute(CSOUND *Csound) {
    switch (type) {
      case CsoundEventTypeStop:
        return true;
      case CsoundEventTypeReadScore:
        csoundReadScore(Csound, score);
        free(score);
        break;
      case CsoundEventTypeScoreEvent:
        csoundScoreEvent(Csound, scoreEventType, parameterFieldValues, parameterFieldCount);
        if (parameterFieldValues)
          free(parameterFieldValues);
        break;
      case CsoundEventTypeInputMessage:
        csoundInputMessage(Csound, score);
        free(score);
        break;
    case CsoundEventTypeCompileOrc:
      csoundCompileOrc(Csound, orc);
      free(orc);
      break;
    }
    return false;
  }
};

struct CsoundAsynchronousEventHandler : public CsoundEventHandler {
  boost::lockfree::queue<CsoundEventCommand> commandQueue;

  CsoundAsynchronousEventHandler() : commandQueue(0) {}

  void handleStop(CSOUND *Csound) {
    CsoundEventCommand command;
    command.type = CsoundEventTypeStop;
    commandQueue.push(command);
  }
  void handleCompileOrc(CSOUND *Csound, char *orcStatement) {
    CsoundEventCommand command;
    command.type = CsoundEventTypeCompileOrc;
    command.orc = strdup(orcStatement);
    commandQueue.push(command);
  }
  void handleInputMessage(CSOUND *Csound, char *scoreStatement) {
    CsoundEventCommand command;
    command.type = CsoundEventTypeInputMessage;
    command.score = strdup(scoreStatement);
    commandQueue.push(command);
  }
  int handleReadScore(CSOUND *Csound, char *score) {
    CsoundEventCommand command;
    command.type = CsoundEventTypeReadScore;
    command.score = strdup(score);
    commandQueue.push(command);
    return CSOUND_SUCCESS;
  }
  int handleScoreEvent(CSOUND *Csound, char eventType, MYFLT *parameterFieldValues, long parameterFieldCount) {
    CsoundEventCommand command;
    command.type = CsoundEventTypeScoreEvent;
    command.scoreEventType = eventType;
    command.parameterFieldValues = parameterFieldValues;
    command.parameterFieldCount = parameterFieldCount;
    commandQueue.push(command);
    return CSOUND_SUCCESS;
  }

  bool CsoundDidPerformKsmps(CSOUND *Csound) {
    CsoundEventCommand command;
    while (commandQueue.pop(command)) {
      if (command.execute(Csound))
        return true;
    }
    return false;
  }
};

struct CsoundPerformWorker : public Nan::AsyncWorker {
  CSOUNDWrapper *wrapper;
  int result;

  CsoundPerformWorker(CSOUNDWrapper *wrapper, Nan::Callback *callback) : Nan::AsyncWorker(callback), wrapper(wrapper) {}
  ~CsoundPerformWorker() {};

  void Execute() {
    while (!(result = csoundPerformKsmps(wrapper->Csound))) {
      if (wrapper->eventHandler->CsoundDidPerformKsmps(wrapper->Csound)) {
        result = 0;
        break;
      }
    }
  }

  void HandleOKCallback() {
    wrapper->eventHandler->CsoundDidPerformKsmps(wrapper->Csound);
    delete wrapper->eventHandler;
    wrapper->eventHandler = new CsoundSynchronousEventHandler();

    const int argc = 1;
    v8::Local<v8::Value> argv[argc];
    argv[0] = Nan::New(result);
    callback->Call(argc, argv);
  }
};

static NAN_METHOD(PerformAsync) {
  if (!info[1]->IsFunction()) {
    Nan::ThrowTypeError("Argument 2 of PerformAsync must be a function.");
    return;
  }
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  delete wrapper->eventHandler;
  wrapper->eventHandler = new CsoundAsynchronousEventHandler();
  Nan::AsyncQueueWorker(new CsoundPerformWorker(wrapper, new Nan::Callback(info[1].As<v8::Function>())));
}

static NAN_METHOD(Perform) {
  info.GetReturnValue().Set(csoundPerform(CsoundFromFunctionCallbackInfo(info)));
}

struct CsoundPerformKsmpsWorker : public Nan::AsyncProgressWorker {
  Nan::Callback *progressCallback;
  CSOUNDWrapper *wrapper;

  CsoundPerformKsmpsWorker(CSOUNDWrapper *wrapper, Nan::Callback *progressCallback, Nan::Callback *callback) : Nan::AsyncProgressWorker(callback), progressCallback(progressCallback), wrapper(wrapper) {}
  ~CsoundPerformKsmpsWorker() {};

  void Execute(const Nan::AsyncProgressWorker::ExecutionProgress& executionProgress) {
    while (!csoundPerformKsmps(wrapper->Csound)) {
      executionProgress.Signal();
      if (wrapper->eventHandler->CsoundDidPerformKsmps(wrapper->Csound))
        break;
    }
  }

  void HandleProgressCallback(const char *data, size_t size) {
    Nan::HandleScope scope;
    progressCallback->Call(0, NULL);
  }

  void WorkComplete() {
    wrapper->eventHandler->CsoundDidPerformKsmps(wrapper->Csound);
    delete wrapper->eventHandler;
    wrapper->eventHandler = new CsoundSynchronousEventHandler();

    Nan::AsyncProgressWorker::WorkComplete();
  }
};

static NAN_METHOD(PerformKsmpsAsync) {
  if (!info[1]->IsFunction()) {
    Nan::ThrowTypeError("Argument 2 of PerformKsmpsAsync must be a function.");
    return;
  }
  if (!info[2]->IsFunction()) {
    Nan::ThrowTypeError("Argument 3 of PerformKsmpsAsync must be a function.");
    return;
  }
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  delete wrapper->eventHandler;
  wrapper->eventHandler = new CsoundAsynchronousEventHandler();
  Nan::AsyncQueueWorker(new CsoundPerformKsmpsWorker(wrapper, new Nan::Callback(info[1].As<v8::Function>()), new Nan::Callback(info[2].As<v8::Function>())));
}

static NAN_METHOD(PerformKsmps) {
  info.GetReturnValue().Set((bool)csoundPerformKsmps(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(PerformBuffer) {
  info.GetReturnValue().Set(csoundPerformBuffer(CsoundFromFunctionCallbackInfo(info)));
}

static NAN_METHOD(Stop) {
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  wrapper->eventHandler->handleStop(wrapper->Csound);
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

static NAN_METHOD(SetOutput) {
  v8::Local<v8::Value> fileNameValue = info[1];
  if (!fileNameValue->IsString())
    return;
  CSOUND *Csound = CsoundFromFunctionCallbackInfo(info);
  Nan::Utf8String fileName(fileNameValue);
  v8::Local<v8::Value> typeValue = info[2];
  v8::Local<v8::Value> formatValue = info[3];

#if CSOUND_6_08_OR_LATER
  csoundSetOutput(Csound, *fileName, typeValue->IsString() ? *Nan::Utf8String(typeValue) : NULL, formatValue->IsString() ? *Nan::Utf8String(formatValue) : NULL);
#else
  // csoundSetOutput is broken in Csound 6.07 and earlier. Use csoundSetOption
  // as a workaround (https://github.com/csound/csound/issues/700).
  std::string option("--output=");
  option.append(*fileName);
  csoundSetOption(Csound, (char *)option.c_str());
  if (typeValue->IsString()) {
    option.assign("--format=");
    option.append(*Nan::Utf8String(typeValue));
    if (formatValue->IsString()) {
      option.append(1, ':');
      option.append(*Nan::Utf8String(formatValue));
    }
    csoundSetOption(Csound, (char *)option.c_str());
  }
#endif
}

struct CsoundFileType {
  static NAN_GETTER(RawAudio)     { info.GetReturnValue().Set(CSFTYPE_RAW_AUDIO); }
  static NAN_GETTER(IRCAM)        { info.GetReturnValue().Set(CSFTYPE_IRCAM); }
  static NAN_GETTER(AIFF)         { info.GetReturnValue().Set(CSFTYPE_AIFF); }
  static NAN_GETTER(AIFC)         { info.GetReturnValue().Set(CSFTYPE_AIFC); }
  static NAN_GETTER(WAVE)         { info.GetReturnValue().Set(CSFTYPE_WAVE); }
  static NAN_GETTER(AU)           { info.GetReturnValue().Set(CSFTYPE_AU); }
  static NAN_GETTER(SD2)          { info.GetReturnValue().Set(CSFTYPE_SD2); }
  static NAN_GETTER(W64)          { info.GetReturnValue().Set(CSFTYPE_W64); }
  static NAN_GETTER(WAVEX)        { info.GetReturnValue().Set(CSFTYPE_WAVEX); }
  static NAN_GETTER(FLAC)         { info.GetReturnValue().Set(CSFTYPE_FLAC); }
  static NAN_GETTER(CAF)          { info.GetReturnValue().Set(CSFTYPE_CAF); }
  static NAN_GETTER(WVE)          { info.GetReturnValue().Set(CSFTYPE_WVE); }
  static NAN_GETTER(OGG)          { info.GetReturnValue().Set(CSFTYPE_OGG); }
  static NAN_GETTER(MPC2K)        { info.GetReturnValue().Set(CSFTYPE_MPC2K); }
  static NAN_GETTER(RF64)         { info.GetReturnValue().Set(CSFTYPE_RF64); }
  static NAN_GETTER(AVR)          { info.GetReturnValue().Set(CSFTYPE_AVR); }
  static NAN_GETTER(HTK)          { info.GetReturnValue().Set(CSFTYPE_HTK); }
  static NAN_GETTER(MAT4)         { info.GetReturnValue().Set(CSFTYPE_MAT4); }
  static NAN_GETTER(MAT5)         { info.GetReturnValue().Set(CSFTYPE_MAT5); }
  static NAN_GETTER(NIST)         { info.GetReturnValue().Set(CSFTYPE_NIST); }
  static NAN_GETTER(PAF)          { info.GetReturnValue().Set(CSFTYPE_PAF); }
  static NAN_GETTER(PVF)          { info.GetReturnValue().Set(CSFTYPE_PVF); }
  static NAN_GETTER(SDS)          { info.GetReturnValue().Set(CSFTYPE_SDS); }
  static NAN_GETTER(SVX)          { info.GetReturnValue().Set(CSFTYPE_SVX); }
  static NAN_GETTER(VOC)          { info.GetReturnValue().Set(CSFTYPE_VOC); }
  static NAN_GETTER(XI)           { info.GetReturnValue().Set(CSFTYPE_XI); }
  static NAN_GETTER(UnknownAudio) { info.GetReturnValue().Set(CSFTYPE_UNKNOWN_AUDIO); }
};

static void CsoundFileOpenCallback(CSOUND *Csound, const char *path, int type, int isOpenForWriting, int isTemporary) {
  CsoundCallback<CsoundFileOpenCallbackArguments> *CsoundFileOpenCallbackObject = ((CSOUNDWrapper *)csoundGetHostData(Csound))->CsoundFileOpenCallbackObject;
  CsoundFileOpenCallbackObject->argumentsQueue.push(CsoundFileOpenCallbackArguments::create(path, type, isOpenForWriting, isTemporary));
  uv_async_send(&(CsoundFileOpenCallbackObject->handle));
}
static CSOUND_CALLBACK_METHOD(FileOpen)

static NAN_METHOD(ReadScore) {
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  info.GetReturnValue().Set(wrapper->eventHandler->handleReadScore(wrapper->Csound, *Nan::Utf8String(info[1])));
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
  static NAN_GETTER(Default)         { info.GetReturnValue().Set(CSOUNDMSG_DEFAULT); }
  static NAN_GETTER(Error)           { info.GetReturnValue().Set(CSOUNDMSG_ERROR); }
  static NAN_GETTER(OrchestraOpcode) { info.GetReturnValue().Set(CSOUNDMSG_ORCH); }
  static NAN_GETTER(RealTime)        { info.GetReturnValue().Set(CSOUNDMSG_REALTIME); }
  static NAN_GETTER(Warning)         { info.GetReturnValue().Set(CSOUNDMSG_WARNING); }
  static NAN_GETTER(Mask)            { info.GetReturnValue().Set(CSOUNDMSG_TYPE_MASK); }
};

struct CsoundMessageForegroundColor {
  static NAN_GETTER(Black)   { info.GetReturnValue().Set(CSOUNDMSG_FG_BLACK); }
  static NAN_GETTER(Red)     { info.GetReturnValue().Set(CSOUNDMSG_FG_RED); }
  static NAN_GETTER(Green)   { info.GetReturnValue().Set(CSOUNDMSG_FG_GREEN); }
  static NAN_GETTER(Yellow)  { info.GetReturnValue().Set(CSOUNDMSG_FG_YELLOW); }
  static NAN_GETTER(Blue)    { info.GetReturnValue().Set(CSOUNDMSG_FG_BLUE); }
  static NAN_GETTER(Magenta) { info.GetReturnValue().Set(CSOUNDMSG_FG_MAGENTA); }
  static NAN_GETTER(Cyan)    { info.GetReturnValue().Set(CSOUNDMSG_FG_CYAN); }
  static NAN_GETTER(White)   { info.GetReturnValue().Set(CSOUNDMSG_FG_WHITE); }
  static NAN_GETTER(Mask)    { info.GetReturnValue().Set(CSOUNDMSG_FG_COLOR_MASK); }
};

struct CsoundMessageAttribute {
  static NAN_GETTER(Bold)      { info.GetReturnValue().Set(CSOUNDMSG_FG_BOLD); }
  static NAN_GETTER(Underline) { info.GetReturnValue().Set(CSOUNDMSG_FG_UNDERLINE); }
  static NAN_GETTER(Mask)      { info.GetReturnValue().Set(CSOUNDMSG_FG_ATTR_MASK); }
};

struct CsoundMessageBackgroundColor {
  static NAN_GETTER(Black)   { info.GetReturnValue().Set(CSOUNDMSG_BG_BLACK); }
  static NAN_GETTER(Red)     { info.GetReturnValue().Set(CSOUNDMSG_BG_RED); }
  static NAN_GETTER(Green)   { info.GetReturnValue().Set(CSOUNDMSG_BG_GREEN); }
  static NAN_GETTER(Orange)  { info.GetReturnValue().Set(CSOUNDMSG_BG_ORANGE); }
  static NAN_GETTER(Blue)    { info.GetReturnValue().Set(CSOUNDMSG_BG_BLUE); }
  static NAN_GETTER(Magenta) { info.GetReturnValue().Set(CSOUNDMSG_BG_MAGENTA); }
  static NAN_GETTER(Cyan)    { info.GetReturnValue().Set(CSOUNDMSG_BG_CYAN); }
  static NAN_GETTER(Grey)    { info.GetReturnValue().Set(CSOUNDMSG_BG_GREY); }
  static NAN_GETTER(Mask)    { info.GetReturnValue().Set(CSOUNDMSG_BG_COLOR_MASK); }
};

static CsoundCallback<CsoundMessageCallbackArguments> *CsoundDefaultMessageCallbackObject;
static void CsoundDefaultMessageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  CsoundDefaultMessageCallbackObject->argumentsQueue.push(CsoundMessageCallbackArguments::create(attributes, format, argumentList));
  uv_async_send(&(CsoundDefaultMessageCallbackObject->handle));
}
static NAN_METHOD(SetDefaultMessageCallback) {
  v8::Local<v8::Value> value = info[0];
  if (value->IsFunction()) {
    CsoundDefaultMessageCallbackObject = new CsoundCallback<CsoundMessageCallbackArguments>(value.As<v8::Function>());
    csoundSetDefaultMessageCallback(CsoundDefaultMessageCallback);
  } else if (CsoundDefaultMessageCallbackObject) {
    delete CsoundDefaultMessageCallbackObject;
    CsoundDefaultMessageCallbackObject = NULL;
    csoundSetDefaultMessageCallback(NULL);
  }
}

static void CsoundMessageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  CsoundCallback<CsoundMessageCallbackArguments> *CsoundMessageCallbackObject = ((CSOUNDWrapper *)csoundGetHostData(Csound))->CsoundMessageCallbackObject;
  CsoundMessageCallbackObject->argumentsQueue.push(CsoundMessageCallbackArguments::create(attributes, format, argumentList));
  uv_async_send(&(CsoundMessageCallbackObject->handle));
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

struct CsoundControlChannelType {
  static NAN_GETTER(Control)      { info.GetReturnValue().Set(CSOUND_CONTROL_CHANNEL); }
  static NAN_GETTER(Audio)        { info.GetReturnValue().Set(CSOUND_AUDIO_CHANNEL); }
  static NAN_GETTER(String)       { info.GetReturnValue().Set(CSOUND_STRING_CHANNEL); }
  static NAN_GETTER(PhaseVocoder) { info.GetReturnValue().Set(CSOUND_PVS_CHANNEL); }
  static NAN_GETTER(Mask)         { info.GetReturnValue().Set(CSOUND_CHANNEL_TYPE_MASK); }
};

struct CsoundControlChannelMode {
  static NAN_GETTER(Input)  { info.GetReturnValue().Set(CSOUND_INPUT_CHANNEL); }
  static NAN_GETTER(Output) { info.GetReturnValue().Set(CSOUND_OUTPUT_CHANNEL); }
};

struct CsoundControlChannelBehavior {
  static NAN_GETTER(None)        { info.GetReturnValue().Set(CSOUND_CONTROL_CHANNEL_NO_HINTS); }
  static NAN_GETTER(Integer)     { info.GetReturnValue().Set(CSOUND_CONTROL_CHANNEL_INT); }
  static NAN_GETTER(Linear)      { info.GetReturnValue().Set(CSOUND_CONTROL_CHANNEL_LIN); }
  static NAN_GETTER(Exponential) { info.GetReturnValue().Set(CSOUND_CONTROL_CHANNEL_EXP); }
};

// These Csound API functions populate an array, passed by reference, with items
// of a particular type:
//   - csoundListChannels
//   - csoundNewOpcodeList
// These functions destroy arrays:
//   - csoundDeleteChannelList
//   - csoundDisposeOpcodeList
//   - csoundDeleteUtilityList
// performCsoundListCreationFunction, performCsoundListDestructionFunction, and
// the CsoundListItemWrapper class generalize the bindings for these Csound API
// functions.
template <typename ItemType, typename WrapperType>
static void performCsoundListCreationFunction(Nan::NAN_METHOD_ARGS_TYPE info, int (*listCreationFunction)(CSOUND *, ItemType **), Nan::Persistent<v8::Function> *ListProxyConstructorRef, Nan::Persistent<v8::Function> *ItemProxyConstructorRef) {
  ItemType *list = NULL;
  int length = listCreationFunction(CsoundFromFunctionCallbackInfo(info), &list);
  if (list && length >= 0) {
    v8::Local<v8::Object> listProxy = Nan::New(*ListProxyConstructorRef)->NewInstance();
    listProxy->SetAlignedPointerInInternalField(0, list);
    v8::Local<v8::Array> array = info[1].As<v8::Array>();
    Nan::SetPrivate(array, Nan::New("Csound::listProxy").ToLocalChecked(), listProxy);
    for (int i = 0; i < length; i++) {
      v8::Local<v8::Object> itemProxy = Nan::New(*ItemProxyConstructorRef)->NewInstance();
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
  v8::Local<v8::Object> listProxy = Nan::GetPrivate(array, key).ToLocalChecked().As<v8::Object>();
  ItemType *list = (ItemType *)listProxy->GetAlignedPointerFromInternalField(0);
  listProxy->SetAlignedPointerInInternalField(0, NULL);
  Nan::DeletePrivate(array, key);
  listDestructionFunction(CsoundFromFunctionCallbackInfo(info), list);
}

template <typename ItemType>
struct CsoundListItemWrapper : public Nan::ObjectWrap {
  ItemType item;

  void setItem(ItemType item) {
    this->item = item;
  }
};

static void addControlChannelHintsToObject(controlChannelHints_t hints, v8::Local<v8::Object> object) {
  object->Set(Nan::New("behav").ToLocalChecked(),  Nan::New(hints.behav));
  object->Set(Nan::New("dflt").ToLocalChecked(),   Nan::New(hints.dflt));
  object->Set(Nan::New("min").ToLocalChecked(),    Nan::New(hints.min));
  object->Set(Nan::New("max").ToLocalChecked(),    Nan::New(hints.max));
  object->Set(Nan::New("x").ToLocalChecked(),      Nan::New(hints.x));
  object->Set(Nan::New("y").ToLocalChecked(),      Nan::New(hints.y));
  object->Set(Nan::New("width").ToLocalChecked(),  Nan::New(hints.width));
  object->Set(Nan::New("height").ToLocalChecked(), Nan::New(hints.height));
  if (hints.attributes)
    object->Set(Nan::New("attributes").ToLocalChecked(), Nan::New(hints.attributes).ToLocalChecked());
}

static Nan::Persistent<v8::Function> ChannelListProxyConstructor;
static Nan::Persistent<v8::Function> ChannelInfoProxyConstructor;
struct ChannelInfoWrapper : CsoundListItemWrapper<controlChannelInfo_t> {
  static NAN_METHOD(New) {
    (new ChannelInfoWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static controlChannelInfo_t itemFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<ChannelInfoWrapper>(info.This())->item;
  }
  static NAN_GETTER(name) { setReturnValueWithCString(info.GetReturnValue(), itemFromPropertyCallbackInfo(info).name); }
  static NAN_GETTER(type) { info.GetReturnValue().Set(Nan::New(itemFromPropertyCallbackInfo(info).type)); }
  static NAN_GETTER(hints) {
    v8::Local<v8::Object> object = Nan::New<v8::Object>();
    addControlChannelHintsToObject(itemFromPropertyCallbackInfo(info).hints, object);
    info.GetReturnValue().Set(object);
  }
};

static NAN_METHOD(ListChannels) {
  performCsoundListCreationFunction<controlChannelInfo_t, ChannelInfoWrapper>(info, csoundListChannels, &ChannelListProxyConstructor, &ChannelInfoProxyConstructor);
}

static NAN_METHOD(DeleteChannelList) {
  performCsoundListDestructionFunction<controlChannelInfo_t>(info, csoundDeleteChannelList);
}

static NAN_METHOD(GetControlChannelHints) {
  controlChannelHints_t hints;
  int status = csoundGetControlChannelHints(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]), &hints);
  info.GetReturnValue().Set(Nan::New(status));
  if (status == CSOUND_SUCCESS) {
    v8::Local<v8::Value> value = info[2];
    if (value->IsObject())
      addControlChannelHintsToObject(hints, value.As<v8::Object>());
  }
}

static NAN_METHOD(SetControlChannelHints) {
  controlChannelHints_t hints = {};
  v8::Local<v8::Value> value = info[2];
  if (value->IsObject()) {
    v8::Local<v8::Object> object = value.As<v8::Object>();
    hints.behav      = (controlChannelBehavior)object->Get(Nan::New("behav").ToLocalChecked())->Int32Value();
    hints.dflt       = object->Get(Nan::New("dflt").ToLocalChecked())->NumberValue();
    hints.min        = object->Get(Nan::New("min").ToLocalChecked())->NumberValue();
    hints.max        = object->Get(Nan::New("max").ToLocalChecked())->NumberValue();
    hints.x          = object->Get(Nan::New("x").ToLocalChecked())->Int32Value();
    hints.y          = object->Get(Nan::New("y").ToLocalChecked())->Int32Value();
    hints.width      = object->Get(Nan::New("width").ToLocalChecked())->Int32Value();
    hints.height     = object->Get(Nan::New("height").ToLocalChecked())->Int32Value();
    hints.attributes = strdup(*Nan::Utf8String(object->Get(Nan::New("attributes").ToLocalChecked())));
  }
  info.GetReturnValue().Set(csoundSetControlChannelHints(CsoundFromFunctionCallbackInfo(info), *Nan::Utf8String(info[1]), hints));
  if (hints.attributes)
    free(hints.attributes);
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
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  int status;
  Nan::Utf8String eventTypeString(info[1]);
  if (eventTypeString.length() != 1) {
    status = CSOUND_ERROR;
  } else {
    char eventType = (*eventTypeString)[0];
    v8::Local<v8::Value> value = info[2];
    long parameterFieldCount = value->IsObject() ? value.As<v8::Object>()->Get(Nan::New("length").ToLocalChecked())->Int32Value() : 0;
    if (parameterFieldCount > 0) {
      v8::Local<v8::Object> object = value.As<v8::Object>();
      MYFLT *parameterFieldValues = (MYFLT *)malloc(sizeof(MYFLT) * parameterFieldCount);
      for (long i = 0; i < parameterFieldCount; i++) {
        parameterFieldValues[i] = object->Get(i)->NumberValue();
      }
      status = wrapper->eventHandler->handleScoreEvent(wrapper->Csound, eventType, parameterFieldValues, parameterFieldCount);
    } else {
      status = wrapper->eventHandler->handleScoreEvent(wrapper->Csound, eventType, NULL, 0);
    }
  }
  info.GetReturnValue().Set(Nan::New(status));
}

static NAN_METHOD(InputMessage) {
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  wrapper->eventHandler->handleInputMessage(wrapper->Csound, *Nan::Utf8String(info[1]));
}

static NAN_METHOD(CompileOrc) {
  CSOUNDWrapper *wrapper = Nan::ObjectWrap::Unwrap<CSOUNDWrapper>(info[0].As<v8::Object>());
  wrapper->eventHandler->handleCompileOrc(wrapper->Csound, *Nan::Utf8String(info[1]));
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
  info.GetReturnValue().Set(Nan::New((bool)csoundSetIsGraphable(CsoundFromFunctionCallbackInfo(info), info[1]->BooleanValue())));
}

static void CsoundMakeGraphCallback(CSOUND *Csound, WINDAT *windowData, const char *name) {
  CsoundCallback<CsoundMakeGraphCallbackArguments> *CsoundMakeGraphCallbackObject = ((CSOUNDWrapper *)csoundGetHostData(Csound))->CsoundMakeGraphCallbackObject;
  CsoundMakeGraphCallbackObject->argumentsQueue.push(CsoundMakeGraphCallbackArguments::create(windowData, name));
  uv_async_send(&(CsoundMakeGraphCallbackObject->handle));
}
static CSOUND_CALLBACK_METHOD(MakeGraph)

static void CsoundDrawGraphCallback(CSOUND *Csound, WINDAT *windowData) {
  CsoundCallback<CsoundGraphCallbackArguments> *CsoundDrawGraphCallbackObject = ((CSOUNDWrapper *)csoundGetHostData(Csound))->CsoundDrawGraphCallbackObject;
  CsoundDrawGraphCallbackObject->argumentsQueue.push(CsoundGraphCallbackArguments::create(windowData));
  uv_async_send(&(CsoundDrawGraphCallbackObject->handle));
}
static CSOUND_CALLBACK_METHOD(DrawGraph, Graph)

static void CsoundKillGraphCallback(CSOUND *Csound, WINDAT *windowData) {
  CsoundCallback<CsoundGraphCallbackArguments> *CsoundKillGraphCallbackObject = ((CSOUNDWrapper *)csoundGetHostData(Csound))->CsoundKillGraphCallbackObject;
  CsoundKillGraphCallbackObject->argumentsQueue.push(CsoundGraphCallbackArguments::create(windowData));
  uv_async_send(&(CsoundKillGraphCallbackObject->handle));
}
static CSOUND_CALLBACK_METHOD(KillGraph, Graph)

static Nan::Persistent<v8::Function> OpcodeListProxyConstructor;
static Nan::Persistent<v8::Function> OpcodeListEntryProxyConstructor;
struct OpcodeListEntryWrapper : public CsoundListItemWrapper<opcodeListEntry> {
  static NAN_METHOD(New) {
    (new OpcodeListEntryWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static opcodeListEntry itemFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<OpcodeListEntryWrapper>(info.This())->item;
  }
  static NAN_GETTER(opname)  { setReturnValueWithCString(info.GetReturnValue(), itemFromPropertyCallbackInfo(info).opname); }
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

static Nan::Persistent<v8::Function> UtilityNameListProxyConstructor;

static NAN_METHOD(ListUtilities) {
  char **list = csoundListUtilities(CsoundFromFunctionCallbackInfo(info));
  if (list) {
    v8::Local<v8::Object> listProxy = Nan::New(UtilityNameListProxyConstructor)->NewInstance();
    listProxy->SetAlignedPointerInInternalField(0, list);
    v8::Local<v8::Array> array = Nan::New<v8::Array>();
    Nan::SetPrivate(array, Nan::New("Csound::listProxy").ToLocalChecked(), listProxy);
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
  static NAN_GETTER(Success)        { info.GetReturnValue().Set(CSOUND_SUCCESS); }
  static NAN_GETTER(Error)          { info.GetReturnValue().Set(CSOUND_ERROR); }
  static NAN_GETTER(Initialization) { info.GetReturnValue().Set(CSOUND_INITIALIZATION); }
  static NAN_GETTER(Performance)    { info.GetReturnValue().Set(CSOUND_PERFORMANCE); }
  static NAN_GETTER(Memory)         { info.GetReturnValue().Set(CSOUND_MEMORY); }
  static NAN_GETTER(Signal)         { info.GetReturnValue().Set(CSOUND_SIGNAL); }
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

static Nan::Persistent<v8::Function> DebuggerInstrumentProxyConstructor;
struct DebuggerInstrumentWrapper : public Nan::ObjectWrap {
  debug_instr_t *instrument;

  static NAN_METHOD(New) {
    (new DebuggerInstrumentWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static debug_instr_t *instrumentFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<DebuggerInstrumentWrapper>(info.This())->instrument;
  }

  static NAN_GETTER(p1)       { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->p1)); }
  static NAN_GETTER(p2)       { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->p2)); }
  static NAN_GETTER(p3)       { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->p3)); }
  static NAN_GETTER(kcounter) { info.GetReturnValue().Set(Nan::New((uint32_t)instrumentFromPropertyCallbackInfo(info)->kcounter)); }
  static NAN_GETTER(line)     { info.GetReturnValue().Set(Nan::New(instrumentFromPropertyCallbackInfo(info)->line)); }

  static NAN_GETTER(next) {
    debug_instr_t *next = instrumentFromPropertyCallbackInfo(info)->next;
    if (next) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerInstrumentProxyConstructor)->NewInstance();
      Unwrap<DebuggerInstrumentWrapper>(proxy)->instrument = next;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static Nan::Persistent<v8::Function> DebuggerOpcodeProxyConstructor;
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
  static NAN_GETTER(line)   { info.GetReturnValue().Set(Nan::New(opcodeFromPropertyCallbackInfo(info)->line)); }

  static void setReturnValueWithDebuggerOpcode(Nan::ReturnValue<v8::Value> returnValue, debug_opcode_t *opcode) {
    if (opcode) {
      v8::Local<v8::Object> proxy = Nan::New(DebuggerOpcodeProxyConstructor)->NewInstance();
      Unwrap<DebuggerOpcodeWrapper>(proxy)->opcode = opcode;
      returnValue.Set(proxy);
    } else {
      returnValue.SetNull();
    }
  }
  static NAN_GETTER(next) { setReturnValueWithDebuggerOpcode(info.GetReturnValue(), opcodeFromPropertyCallbackInfo(info)->next); }
  static NAN_GETTER(prev) { setReturnValueWithDebuggerOpcode(info.GetReturnValue(), opcodeFromPropertyCallbackInfo(info)->prev); }
};

static Nan::Persistent<v8::Function> DebuggerVariableProxyConstructor;
struct DebuggerVariableWrapper : public Nan::ObjectWrap {
  debug_variable_t *variable;

  static NAN_METHOD(New) {
    (new DebuggerVariableWrapper())->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  }

  static debug_variable_t *variableFromPropertyCallbackInfo(Nan::NAN_GETTER_ARGS_TYPE info) {
    return Unwrap<DebuggerVariableWrapper>(info.This())->variable;
  }

  static NAN_GETTER(name)     { info.GetReturnValue().Set(Nan::New(variableFromPropertyCallbackInfo(info)->name).ToLocalChecked()); }
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
      v8::Local<v8::Object> proxy = Nan::New(DebuggerVariableProxyConstructor)->NewInstance();
      Unwrap<DebuggerVariableWrapper>(proxy)->variable = next;
      info.GetReturnValue().Set(proxy);
    } else {
      info.GetReturnValue().SetNull();
    }
  }
};

static Nan::Persistent<v8::Function> DebuggerBreakpointInfoProxyConstructor;
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
      v8::Local<v8::Object> proxy = Nan::New(DebuggerInstrumentProxyConstructor)->NewInstance();
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
      v8::Local<v8::Object> proxy = Nan::New(DebuggerVariableProxyConstructor)->NewInstance();
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
      v8::Local<v8::Object> proxy = Nan::New(DebuggerOpcodeProxyConstructor)->NewInstance();
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
  v8::Local<v8::Object> proxy = Nan::New(DebuggerBreakpointInfoProxyConstructor)->NewInstance();
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
  } else if (wrapper->CsoundBreakpointCallbackObject) {
    delete wrapper->CsoundBreakpointCallbackObject;
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
  Nan::SetMethod(target, "Initialize", Initialize);

  Nan::SetAccessor(target, Nan::New("INIT_NO_SIGNAL_HANDLER").ToLocalChecked(), CsoundInitializationOption::NoSignalHandlers);
  Nan::SetAccessor(target, Nan::New("INIT_NO_ATEXIT").ToLocalChecked(), CsoundInitializationOption::NoExitFunction);

  Nan::SetMethod(target, "Create", Create);
  Nan::SetMethod(target, "Destroy", Destroy);
  Nan::SetMethod(target, "GetVersion", GetVersion);
  Nan::SetMethod(target, "GetAPIVersion", GetAPIVersion);

  Nan::SetMethod(target, "ParseOrc", ParseOrc);
  Nan::SetMethod(target, "CompileTree", CompileTree);
  Nan::SetMethod(target, "DeleteTree", DeleteTree);
  Nan::SetMethod(target, "CompileOrc", CompileOrc);
  Nan::SetMethod(target, "EvalCode", EvalCode);
  Nan::SetMethod(target, "CompileArgs", CompileArgs);
  Nan::SetMethod(target, "Start", Start);
  Nan::SetMethod(target, "Compile", Compile);
  Nan::SetMethod(target, "CompileCsd", CompileCsd);
  Nan::SetMethod(target, "PerformAsync", PerformAsync);
  Nan::SetMethod(target, "Perform", Perform);
  Nan::SetMethod(target, "PerformKsmpsAsync", PerformKsmpsAsync);
  Nan::SetMethod(target, "PerformKsmps", PerformKsmps);
  Nan::SetMethod(target, "PerformBuffer", PerformBuffer);
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
  Nan::SetMethod(target, "SetOutput", SetOutput);
  Nan::SetMethod(target, "SetFileOpenCallback", SetFileOpenCallback);

  Nan::SetAccessor(target, Nan::New("FTYPE_RAW_AUDIO").ToLocalChecked(), CsoundFileType::RawAudio);
  Nan::SetAccessor(target, Nan::New("FTYPE_IRCAM").ToLocalChecked(), CsoundFileType::IRCAM);
  Nan::SetAccessor(target, Nan::New("FTYPE_AIFF").ToLocalChecked(), CsoundFileType::AIFF);
  Nan::SetAccessor(target, Nan::New("FTYPE_AIFC").ToLocalChecked(), CsoundFileType::AIFC);
  Nan::SetAccessor(target, Nan::New("FTYPE_WAVE").ToLocalChecked(), CsoundFileType::WAVE);
  Nan::SetAccessor(target, Nan::New("FTYPE_AU").ToLocalChecked(), CsoundFileType::AU);
  Nan::SetAccessor(target, Nan::New("FTYPE_SD2").ToLocalChecked(), CsoundFileType::SD2);
  Nan::SetAccessor(target, Nan::New("FTYPE_W64").ToLocalChecked(), CsoundFileType::W64);
  Nan::SetAccessor(target, Nan::New("FTYPE_WAVEX").ToLocalChecked(), CsoundFileType::WAVEX);
  Nan::SetAccessor(target, Nan::New("FTYPE_FLAC").ToLocalChecked(), CsoundFileType::FLAC);
  Nan::SetAccessor(target, Nan::New("FTYPE_CAF").ToLocalChecked(), CsoundFileType::CAF);
  Nan::SetAccessor(target, Nan::New("FTYPE_WVE").ToLocalChecked(), CsoundFileType::WVE);
  Nan::SetAccessor(target, Nan::New("FTYPE_OGG").ToLocalChecked(), CsoundFileType::OGG);
  Nan::SetAccessor(target, Nan::New("FTYPE_MPC2K").ToLocalChecked(), CsoundFileType::MPC2K);
  Nan::SetAccessor(target, Nan::New("FTYPE_RF64").ToLocalChecked(), CsoundFileType::RF64);
  Nan::SetAccessor(target, Nan::New("FTYPE_AVR").ToLocalChecked(), CsoundFileType::AVR);
  Nan::SetAccessor(target, Nan::New("FTYPE_HTK").ToLocalChecked(), CsoundFileType::HTK);
  Nan::SetAccessor(target, Nan::New("FTYPE_MAT4").ToLocalChecked(), CsoundFileType::MAT4);
  Nan::SetAccessor(target, Nan::New("FTYPE_MAT5").ToLocalChecked(), CsoundFileType::MAT5);
  Nan::SetAccessor(target, Nan::New("FTYPE_NIST").ToLocalChecked(), CsoundFileType::NIST);
  Nan::SetAccessor(target, Nan::New("FTYPE_PAF").ToLocalChecked(), CsoundFileType::PAF);
  Nan::SetAccessor(target, Nan::New("FTYPE_PVF").ToLocalChecked(), CsoundFileType::PVF);
  Nan::SetAccessor(target, Nan::New("FTYPE_SDS").ToLocalChecked(), CsoundFileType::SDS);
  Nan::SetAccessor(target, Nan::New("FTYPE_SVX").ToLocalChecked(), CsoundFileType::SVX);
  Nan::SetAccessor(target, Nan::New("FTYPE_VOC").ToLocalChecked(), CsoundFileType::VOC);
  Nan::SetAccessor(target, Nan::New("FTYPE_XI").ToLocalChecked(), CsoundFileType::XI);
  Nan::SetAccessor(target, Nan::New("FTYPE_UNKNOWN_AUDIO").ToLocalChecked(), CsoundFileType::UnknownAudio);

  Nan::SetMethod(target, "ReadScore", ReadScore);
  Nan::SetMethod(target, "GetScoreTime", GetScoreTime);
  Nan::SetMethod(target, "IsScorePending", IsScorePending);
  Nan::SetMethod(target, "SetScorePending", SetScorePending);
  Nan::SetMethod(target, "GetScoreOffsetSeconds", GetScoreOffsetSeconds);
  Nan::SetMethod(target, "SetScoreOffsetSeconds", SetScoreOffsetSeconds);
  Nan::SetMethod(target, "RewindScore", RewindScore);

  Nan::SetMethod(target, "Message", Message);
  Nan::SetMethod(target, "MessageS", MessageS);
  Nan::SetMethod(target, "SetDefaultMessageCallback", SetDefaultMessageCallback);
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
  Nan::SetMethod(target, "GetControlChannelHints", GetControlChannelHints);
  Nan::SetMethod(target, "SetControlChannelHints", SetControlChannelHints);
  Nan::SetMethod(target, "GetControlChannel", GetControlChannel);
  Nan::SetMethod(target, "SetControlChannel", SetControlChannel);
  Nan::SetMethod(target, "ScoreEvent", ScoreEvent);
  Nan::SetMethod(target, "InputMessage", InputMessage);

  Nan::SetAccessor(target, Nan::New("CONTROL_CHANNEL").ToLocalChecked(), CsoundControlChannelType::Control);
  Nan::SetAccessor(target, Nan::New("AUDIO_CHANNEL").ToLocalChecked(), CsoundControlChannelType::Audio);
  Nan::SetAccessor(target, Nan::New("STRING_CHANNEL").ToLocalChecked(), CsoundControlChannelType::String);
  Nan::SetAccessor(target, Nan::New("PVS_CHANNEL").ToLocalChecked(), CsoundControlChannelType::PhaseVocoder);
  Nan::SetAccessor(target, Nan::New("CHANNEL_TYPE_MASK").ToLocalChecked(), CsoundControlChannelType::Mask);
  Nan::SetAccessor(target, Nan::New("INPUT_CHANNEL").ToLocalChecked(), CsoundControlChannelMode::Input);
  Nan::SetAccessor(target, Nan::New("OUTPUT_CHANNEL").ToLocalChecked(), CsoundControlChannelMode::Output);

  Nan::SetAccessor(target, Nan::New("CONTROL_CHANNEL_NO_HINTS").ToLocalChecked(), CsoundControlChannelBehavior::None);
  Nan::SetAccessor(target, Nan::New("CONTROL_CHANNEL_INT").ToLocalChecked(), CsoundControlChannelBehavior::Integer);
  Nan::SetAccessor(target, Nan::New("CONTROL_CHANNEL_LIN").ToLocalChecked(), CsoundControlChannelBehavior::Linear);
  Nan::SetAccessor(target, Nan::New("CONTROL_CHANNEL_EXP").ToLocalChecked(), CsoundControlChannelBehavior::Exponential);

  Nan::SetMethod(target, "TableLength", TableLength);
  Nan::SetMethod(target, "TableGet", TableGet);
  Nan::SetMethod(target, "TableSet", TableSet);

  Nan::SetMethod(target, "SetIsGraphable", SetIsGraphable);
  Nan::SetMethod(target, "SetMakeGraphCallback", SetMakeGraphCallback);
  Nan::SetMethod(target, "SetDrawGraphCallback", SetDrawGraphCallback);
  Nan::SetMethod(target, "SetKillGraphCallback", SetKillGraphCallback);

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

  v8::Local<v8::FunctionTemplate> classTemplate = Nan::New<v8::FunctionTemplate>(CSOUNDWrapper::New);
  classTemplate->SetClassName(Nan::New("CSOUND").ToLocalChecked());
  classTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  CSOUNDProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(ORCTOKENWrapper::New);
  classTemplate->SetClassName(Nan::New("ORCTOKEN").ToLocalChecked());
  v8::Local<v8::ObjectTemplate> instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("type").ToLocalChecked(), ORCTOKENWrapper::type);
  Nan::SetAccessor(instanceTemplate, Nan::New("lexeme").ToLocalChecked(), ORCTOKENWrapper::lexeme);
  Nan::SetAccessor(instanceTemplate, Nan::New("value").ToLocalChecked(), ORCTOKENWrapper::value);
  Nan::SetAccessor(instanceTemplate, Nan::New("fvalue").ToLocalChecked(), ORCTOKENWrapper::fvalue);
  Nan::SetAccessor(instanceTemplate, Nan::New("optype").ToLocalChecked(), ORCTOKENWrapper::optype);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), ORCTOKENWrapper::next);
  ORCTOKENProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(TREEWrapper::New);
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
  TREEProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>();
  classTemplate->SetClassName(Nan::New("ChannelList").ToLocalChecked());
  classTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  ChannelListProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(ChannelInfoWrapper::New);
  classTemplate->SetClassName(Nan::New("controlChannelInfo_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("name").ToLocalChecked(), ChannelInfoWrapper::name);
  Nan::SetAccessor(instanceTemplate, Nan::New("type").ToLocalChecked(), ChannelInfoWrapper::type);
  Nan::SetAccessor(instanceTemplate, Nan::New("hints").ToLocalChecked(), ChannelInfoWrapper::hints);
  ChannelInfoProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(WINDATWrapper::New);
  classTemplate->SetClassName(Nan::New("WINDAT").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("windid").ToLocalChecked(), WINDATWrapper::windid);
  Nan::SetAccessor(instanceTemplate, Nan::New("fdata").ToLocalChecked(), WINDATWrapper::fdata);
  Nan::SetAccessor(instanceTemplate, Nan::New("caption").ToLocalChecked(), WINDATWrapper::caption);
  Nan::SetAccessor(instanceTemplate, Nan::New("polarity").ToLocalChecked(), WINDATWrapper::polarity);
  Nan::SetAccessor(instanceTemplate, Nan::New("max").ToLocalChecked(), WINDATWrapper::max);
  Nan::SetAccessor(instanceTemplate, Nan::New("min").ToLocalChecked(), WINDATWrapper::min);
  Nan::SetAccessor(instanceTemplate, Nan::New("oabsmax").ToLocalChecked(), WINDATWrapper::oabsmax);
  WINDATProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>();
  classTemplate->SetClassName(Nan::New("OpcodeList").ToLocalChecked());
  classTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  OpcodeListProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(OpcodeListEntryWrapper::New);
  classTemplate->SetClassName(Nan::New("opcodeListEntry").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("opname").ToLocalChecked(), OpcodeListEntryWrapper::opname);
  Nan::SetAccessor(instanceTemplate, Nan::New("outypes").ToLocalChecked(), OpcodeListEntryWrapper::outypes);
  Nan::SetAccessor(instanceTemplate, Nan::New("intypes").ToLocalChecked(), OpcodeListEntryWrapper::intypes);
  OpcodeListEntryProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>();
  classTemplate->SetClassName(Nan::New("UtilityNameList").ToLocalChecked());
  classTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  UtilityNameListProxyConstructor.Reset(classTemplate->GetFunction());

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
  classTemplate->SetClassName(Nan::New("debug_instr_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("p1").ToLocalChecked(), DebuggerInstrumentWrapper::p1);
  Nan::SetAccessor(instanceTemplate, Nan::New("p2").ToLocalChecked(), DebuggerInstrumentWrapper::p2);
  Nan::SetAccessor(instanceTemplate, Nan::New("p3").ToLocalChecked(), DebuggerInstrumentWrapper::p3);
  Nan::SetAccessor(instanceTemplate, Nan::New("kcounter").ToLocalChecked(), DebuggerInstrumentWrapper::kcounter);
  Nan::SetAccessor(instanceTemplate, Nan::New("line").ToLocalChecked(), DebuggerInstrumentWrapper::line);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), DebuggerInstrumentWrapper::next);
  DebuggerInstrumentProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerOpcodeWrapper::New);
  classTemplate->SetClassName(Nan::New("debug_opcode_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("opname").ToLocalChecked(), DebuggerOpcodeWrapper::opname);
  Nan::SetAccessor(instanceTemplate, Nan::New("line").ToLocalChecked(), DebuggerOpcodeWrapper::line);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), DebuggerOpcodeWrapper::next);
  Nan::SetAccessor(instanceTemplate, Nan::New("prev").ToLocalChecked(), DebuggerOpcodeWrapper::prev);
  DebuggerOpcodeProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerVariableWrapper::New);
  classTemplate->SetClassName(Nan::New("debug_variable_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("name").ToLocalChecked(), DebuggerVariableWrapper::name);
  Nan::SetAccessor(instanceTemplate, Nan::New("typeName").ToLocalChecked(), DebuggerVariableWrapper::typeName);
  Nan::SetAccessor(instanceTemplate, Nan::New("data").ToLocalChecked(), DebuggerVariableWrapper::data);
  Nan::SetAccessor(instanceTemplate, Nan::New("next").ToLocalChecked(), DebuggerVariableWrapper::next);
  DebuggerVariableProxyConstructor.Reset(classTemplate->GetFunction());

  classTemplate = Nan::New<v8::FunctionTemplate>(DebuggerBreakpointInfoWrapper::New);
  classTemplate->SetClassName(Nan::New("debug_bkpt_info_t").ToLocalChecked());
  instanceTemplate = classTemplate->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(1);
  Nan::SetAccessor(instanceTemplate, Nan::New("breakpointInstr").ToLocalChecked(), DebuggerBreakpointInfoWrapper::breakpointInstr);
  Nan::SetAccessor(instanceTemplate, Nan::New("instrVarList").ToLocalChecked(), DebuggerBreakpointInfoWrapper::instrVarList);
  Nan::SetAccessor(instanceTemplate, Nan::New("instrListHead").ToLocalChecked(), DebuggerBreakpointInfoWrapper::instrListHead);
  Nan::SetAccessor(instanceTemplate, Nan::New("currentOpcode").ToLocalChecked(), DebuggerBreakpointInfoWrapper::currentOpcode);
  DebuggerBreakpointInfoProxyConstructor.Reset(classTemplate->GetFunction());
#endif // CSOUND_6_04_OR_LATER
}

NODE_MODULE(CsoundAPI, init)
