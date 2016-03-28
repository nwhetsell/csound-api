# Csound API

This package is a [Node.js Addon](https://nodejs.org/api/addons.html) for using [Csound](https://en.wikipedia.org/wiki/Csound) through its C&nbsp;[API](https://csound.github.io/docs/api/index.html). The methods in this package try to match the functions in Csound’s API as closely as possible, and this package adds a `PerformAsync` method for running Csound in a background thread. If you `require` this package using

```javascript
var csound = require('csound-api');
```

then you can use Csound’s API as, for example,

```javascript
function messageCallback(attributes, string) {
  console.log(string);
}
var Csound = csound.Create();
csound.SetMessageCallback(Csound, messageCallback);
csound.Message(Csound, 'hello, world');
```

The equivalent in C would be something like

```c
void messageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  vprintf(format, argumentList);
}
CSOUND *Csound = csoundCreate(NULL);
csoundSetMessageCallback(Csound, messageCallback);
csoundMessage(Csound, "hello, world");
```

## Installing

Before you install this package, you need [Boost](http://www.boost.org) and Csound.

### On OS&nbsp;X

The easiest way to install Boost is probably through [Homebrew](http://brew.sh). To install Homebrew, follow the instructions at [http://brew.sh](http://brew.sh). Then, run `brew install boost` in a Terminal.

If you aren’t able to build Csound from its [source code](https://github.com/csound/csound), the most reliable way to install Csound is probably to run an installer in a disk image you can download from [SourceForge](http://sourceforge.net/projects/csound/files/csound6/). (While Csound has a [tap](https://github.com/csound/homebrew-csound) on Homebrew, it does not install a necessary framework; this is a [known issue](https://github.com/csound/csound/blob/develop/BUILD.md#known-issues).) When you double-click the installer in the disk image, OS&nbsp;X may block the installer from running because it’s from an unidentified developer. To run the installer after this happens, open System Preferences, choose Security & Privacy, and click Open Anyway in the bottom half of the window.

After you install Boost and Csound, you can install this package using
```sh
npm install csound-api
```

## [Examples](https://github.com/nwhetsell/csound-api/tree/master/examples)

Play a 440&nbsp;Hz sine tone.

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, [
  'nchnls = 1', 'sr = 44100', '0dbfs = 1', 'ksmps = 32',
  'giFunctionTableID ftgen 0, 0, 16384, 10, 1',
  'instr A440',
    'outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)',
  'endin'
].join('\n'));
csound.ReadScore(Csound, [
  'i "A440" 0 1',
  'e'
].join('\n'));
csound.Start(Csound);
csound.Perform(Csound);
csound.Destroy(Csound);
```

Run Csound asynchronously, and stop Csound in mid-performance.

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, [
  'nchnls = 1', 'sr = 44100', '0dbfs = 1', 'ksmps = 32',
  'instr SawtoothSweep',
    // This outputs a sawtooth wave with a fundamental frequency that starts
    // at 110 Hz, rises to 220 Hz over 1 second, and then falls back to 110 Hz
    // over 1 second. The score plays this instrument for 2 seconds, but the
    // call to setTimeout() stops Csound after 1 second, so only the rise is
    // heard.
    'outc vco2(0.5 * 0dbfs, expseg(110, 1, 220, 1, 110))',
  'endin'
].join('\n'));
csound.ReadScore(Csound, [
  'i "SawtoothSweep" 0 2',
  'e'
].join('\n'));
csound.Start(Csound);
csound.PerformAsync(Csound, function(result) {
  csound.Destroy(Csound);
});
setTimeout(function() {
  csound.Stop(Csound);
}, 1000);
```

Log a list of Csound’s opcodes.

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
var opcodes = [];
csound.NewOpcodeList(Csound, opcodes);
console.log(opcodes);
csound.DisposeOpcodeList(Csound, opcodes);
csound.Destroy(Csound);
```

Log an abstract syntax tree parsed from an orchestra.

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
var ASTRoot = csound.ParseOrc(Csound, [
  'nchnls = 1', 'sr = 44100', '0dbfs = 1', 'ksmps = 32',
  'giFunctionTableID ftgen 0, 0, 16384, 10, 1',
  'instr A440',
    'outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)',
  'endin'
].join('\n'));
console.log(ASTRoot);
csound.DeleteTree(Csound, ASTRoot);
csound.Destroy(Csound);
```

## Tests

Running the [tests](https://github.com/nwhetsell/csound-api/blob/master/spec/csound-api-spec.js) of this package requires [Jasmine](https://jasmine.github.io/edge/node.html). To install the Jasmine package on OS&nbsp;X globally, run

```sh
npm install -g jasmine
```

in a Terminal. To run the tests, `cd` to the `csound-api` folder (which should be in `node_modules` if you installed `csound-api` locally) and run `jasmine`.

## Contributing

[Open an issue](https://github.com/nwhetsell/csound-api/issues), or [fork this project and submit a pull request](https://guides.github.com/activities/forking/).

## API Coverage

Here are the properties and functions you can use assuming you `require` this package as

```javascript
var csound = require('csound-api');
```

### [Instantiation](https://csound.github.io/docs/api/group__INSTANTIATION.html)

##### <a name="Create"></a><code><i>Csound</i> = csound.Create([<i>value</i>])</code>

Create a new `Csound` object, optionally associating a `value` with it; `value` can be an object, a function, a string, a number, a Boolean, `null`, or `undefined`. You can retrieve a `value` associated with a `Csound` object using [`csound.GetHostData`](#GetHostData) and associate a new `value` using [`csound.SetHostData`](#SetHostData). You must pass the returned `Csound` object as the first argument to most other functions in this package, and you should pass `Csound` to [`csound.Destroy`](#Destroy) when you’re finished using `Csound`.

##### <a name="Destroy"></a><code>csound.Destroy(<i>Csound</i>)</code>

Free resources used by a `Csound` object.

##### <code><i>versionTimes1000</i> = csound.GetVersion()</code>

Get Csound’s version number multiplied by 1,000. For example, if you’re using Csound 6.06, then `versionTimes1000` will be 6,060.

##### <code><i>versionTimes100</i> = csound.GetAPIVersion()</code>

Get the version of Csound’s API, multiplied by 100. For example, if you’re using version&nbsp;3 of Csound’s API, then `versionTimes100` will be 300.

---

### [Performance](https://csound.github.io/docs/api/group__PERFORMANCE.html)

##### <a name="ParseOrc"></a><code><i>AST</i> = csound.ParseOrc(<i>Csound</i>, <i>orchestraString</i>)</code>

Parse a string containing a Csound orchestra into an abstract syntax tree (AST). The returned `AST` is an object representing the root node of the AST. AST nodes have a number of read-only properties:

<dl>
  <dt><code>type</code></dt><dd> is a number indicating the type of token. One way to determine how types correspond to tokens is to build Csound from its <a href="https://github.com/csound/csound">source code</a> and examine the <a href="https://en.wikipedia.org/wiki/GNU_bison">Bison</a>-generated file csound_orcparse.h.</dd>

  <dt><code>value</code></dt><dd> is an object describing the token. It has several read-only properties:<dl>
    <dt><code>type</code></dt><dd> is a number indicating the type of token. This need not be the same as the <code>type</code> of the <code>AST</code> object.</dd>

    <dt><code>lexeme</code></dt><dd> is a string. This is usually the string value of the token, but not always. For example, operator opcodes like <a href="https://csound.github.io/docs/manual/adds.html">+</a> have lexemes like <code>##add</code>.</dd>

    <dt><code>value</code></dt><dd> is a number equal to the value of the token if it is an integer, and 0 otherwise.</dd>

    <dt><code>fvalue</code></dt><dd> is a number equal to the value of the token if it is not an integer, and 0 otherwise.</dd>

    <!--<dt><code>optype</code></dt><dd> </dd>-->

    <!--<dt><code>next</code></dt><dd> </dd>-->
  </dl></dd>

  <!--<dt><code>rate</code></dt><dd> </dd>-->

  <!--<dt><code>len</code></dt><dd> </dd>-->

  <dt><code>line</code></dt><dd> is the line number where the token occurs.</dd>

  <!--<dt><code>locn</code></dt><dd> </dd>-->

  <dt><code>left</code></dt><dd> is an AST node that generally represents the first output argument of an opcode.</dd>

  <dt><code>right</code></dt><dd> is an AST node that generally represents the first input argument of an opcode.</dd>

  <dt><code>next</code></dt><dd> is an AST node that is the first node of a linked list of all other arguments of an opcode. Output arguments precede input arguments. For example, in an AST node parsed from
    <pre>kFrequency, kAmplitude <a href="https://csound.github.io/docs/manual/pvread.html">pvread</a> kTime, "file.pvx", 1</pre>
    the <code>pvread</code> node will have a <code>left</code> node for the <code>kFrequency</code> output argument, a <code>right</code> node for the <code>kTime</code> input argument, and <code>next</code> nodes for the <code>kAmplitude</code> output argument, <code>"file.pvx"</code> input argument, and <code>1</code> input argument.
    </dd>
</dl>

You can compile the `AST` using [`csound.CompileTree`](#CompileTree), and you should pass the `AST` to [`csound.DeleteTree`](#DeleteTree) when you’re finished with it.

##### <a name="CompileTree"></a><code><i>status</i> = csound.CompileTree(<i>Csound</i>, <i>AST</i>)</code>

Compile an `AST` returned from [`csound.ParseOrc`](#ParseOrc), adding instruments and other structures to `Csound`. The returned `status` is a Csound [status code](#status-codes).

##### <a name="DeleteTree"></a><code>csound.DeleteTree(<i>Csound</i>, <i>AST</i>)</code>

Free resources used by an `AST`.

##### <a name="CompileOrc"></a><code><i>status</i> = csound.CompileOrc(<i>Csound</i>, <i>orchestraString</i>)</code>

Compile a string containing a Csound orchestra, adding instruments and other structures to `Csound`. The returned `status` is a Csound [status code](#status-codes).

##### <a name="EvalCode"></a><code><i>number</i> = csound.EvalCode(<i>Csound</i>, <i>orchestraString</i>)</code>

Get a `number` passed to a global [`return`](https://csound.github.io/docs/manual/return.html) opcode in `orchestraString`. For example,

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.SetOption(Csound, '--nosound');
if (csound.Start(Csound) === csound.CSOUND_SUCCESS) {
  console.log(csound.EvalCode(Csound, [
    'i1 = 19 + 23',
    'return i1'
  ].join('\n')));
}
```

logs the number 42. Before you use this method, you must start `Csound`—that is, you must pass `Csound` to [`csound.Start`](#Start), which must return `csound.CSOUND_SUCCESS`.

##### <a name="CompileArgs"></a><code><i>status</i> = csound.CompileArgs(<i>Csound</i>, <i>commandLineArguments</i>)</code>

Compile instruments, set options, and perform other actions according to [command line arguments](https://csound.github.io/docs/manual/CommandFlags.html) in the `commandLineArguments` string array, without starting `Csound`. For example,

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.CompileArgs(Csound, ['csound', 'my.orc', 'my.sco']);
```

compiles the orchestra in my.orc and the score in my.sco, but does not start `Csound`. To start `Csound` after calling `csound.CompileArgs`, pass `Csound` to [`csound.Start`](#Start). To compile Csound files using command line arguments and also start `Csound`, use [`csound.Compile`](#Compile). The returned `status` is a Csound [status code](#status-codes).

##### <a name="Start"></a><code><i>status</i> = csound.Start(<i>Csound</i>)</code>

Prepare `Csound` for performance—that is, to be passed to [`csound.PerformAsync`](#PerformAsync), [`csound.Perform`](#Perform), or [`csound.PerformKsmps`](#PerformKsmps). The returned `status` is a Csound [status code](#status-codes).

##### <a name="Compile"></a><code><i>status</i> = csound.Compile(<i>Csound</i>, <i>commandLineArguments</i>)</code>

Compile instruments, set options, and perform other actions according to [command line arguments](https://csound.github.io/docs/manual/CommandFlags.html) in the `commandLineArguments` string array, and also start `Csound`. To compile Csound files using command line arguments without starting `Csound`, use [`csound.CompileArgs`](#CompileArgs). The returned `status` is a Csound [status code](#status-codes).

##### <code><i>status</i> = csound.CompileCsd(<i>Csound</i>, <i>filePath</i>)</code>

Compile the CSD file located at `filePath` and start `Csound`. The returned `status` is a Csound [status code](#status-codes).

##### <a name="PerformAsync"></a><code>csound.PerformAsync(<i>Csound</i>, function(<i>result</i>))</code><br>

Perform score and input events on a background thread, and call the passed function when the performance stops. The `result` passed to this function is a number that indicates the reason performance stopped:

When performance stops because    | `result` is
----------------------------------|---------------
the end of the score was reached  | greater than 0
[`csound.Stop`](#Stop) was called | equal to 0
an error occurred                 | less than 0

##### <a name="Perform"></a><code><i>result</i> = csound.Perform(<i>Csound</i>)</code>

Perform score and input events on the main thread. The returned `result` is the same as the `result` passed to the function argument of [`csound.PerformAsync`](#PerformAsync).

##### <a name="PerformKsmps"></a><code><i>isFinished</i> = csound.PerformKsmps(<i>Csound</i>)</code>

Perform one control period of samples on the main thread, returning `true` if the performance is finished and `false` otherwise.

##### <a name="Stop"></a><code>csound.Stop(<i>Csound</i>)</code>

Stop a `Csound` performance asynchronously.

##### <a name="Cleanup"></a><code><i>status</i> = csound.Cleanup(<i>Csound</i>)</code>

Free resources after the end of a `Csound` performance. The returned `status` is a Csound [status code](#status-codes).

##### <code>csound.Reset(<i>Csound</i>)</code>

Free resources after the end of a `Csound` performance (just like [`csound.Cleanup`](#Cleanup)), and prepare for a new performance.

---

### [Attributes](https://csound.github.io/docs/api/group__ATTRIBUTES.html)

##### <a name="GetSr"></a><code><i>sampleRate</i> = csound.GetSr(<i>Csound</i>)</code>

Get [`sr`](https://csound.github.io/docs/manual/sr.html), the `Csound` sample rate (also called the audio rate or a‑rate).

##### <code><i>controlRate</i> = csound.GetKr(<i>Csound</i>)</code>

Get [`kr`](https://csound.github.io/docs/manual/kr.html), the `Csound` control rate (also called the k‑rate).

##### <code><i>samplesPerControlPeriod</i> = csound.GetKsmps(<i>Csound</i>)</code>

Get [`ksmps`](https://csound.github.io/docs/manual/ksmps.html), the number of digital audio samples in one control period.

##### <code><i>outputChannelCount</i> = csound.GetNchnls(<i>Csound</i>)</code>

Get [`nchnls`](https://csound.github.io/docs/manual/nchnls.html), the number of audio output channels.

##### <code><i>inputChannelCount</i> = csound.GetNchnlsInput(<i>Csound</i>)</code>

Get [`nchnls_i`](https://csound.github.io/docs/manual/nchnls_i.html), the number of audio input channels.

##### <code><i>fullScalePeakAmplitude</i> = csound.Get0dBFS(<i>Csound</i>)</code>

Get [`0dBFS`](https://csound.github.io/docs/manual/Zerodbfs.html), the maximum value of a sample of audio.

##### <a name="GetCurrentTimeSamples"></a><code><i>performedSampleCount</i> = csound.GetCurrentTimeSamples(<i>Csound</i>)</code>

Get the number of samples performed by `Csound`. You can call this function during a performance. For the elapsed time in seconds of a performance, divide `performedSampleCount` by the [sample rate](#GetSr), or use [`csound.GetScoreTime`](#GetScoreTime).

##### <code><i>bytesPerFloat</i> = csound.GetSizeOfMYFLT()</code>

Get the number of bytes that Csound uses to represent floating-point numbers. When Csound is compiled to use double-precision samples, `bytesPerFloat` is 8. Otherwise, it’s 4.

##### <a name="GetHostData"></a><code><i>value</i> = csound.GetHostData(<i>Csound</i>)</code>

Get a `value` that was associated with a `Csound` object using [`csound.Create`](#Create) or [`csound.SetHostData`](#SetHostData).

##### <a name="SetHostData"></a><code>csound.SetHostData(<i>Csound</i>, <i>value</i>)</code>

Associate a `value` with a `Csound` object; `value` can be an object, a function, a string, a number, a Boolean, `null`, or `undefined`. You can retrieve a `value` associated with a `Csound` object using [`csound.GetHostData`](#GetHostData).

##### <code><i>status</i> = csound.SetOption(<i>Csound</i>, <i>commandLineArgumentString</i>)</code>

Set a `Csound` option as if `commandLineArgumentString` was input as a [command line argument](https://csound.github.io/docs/manual/CommandFlags.html). For example,

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
```

sets up `Csound` to output audio through your computer’s speakers. The returned `status` is a Csound [status code](#status-codes).

##### <code><i>queuesDebugMessages</i> = csound.GetDebug(<i>Csound</i>)</code>

Get a Boolean indicating whether `Csound` adds debug messages to its message queue.

##### <code>csound.SetDebug(<i>Csound</i>, <i>queuesDebugMessages</i>)</code>

Set a Boolean indicating whether `Csound` adds debug messages to its message queue.

---

### [General Input/Output](https://csound.github.io/docs/api/group__FILEIO.html)

##### <code><i>audioOutputName</i> = csound.GetOutputName(<i>Csound</i>)</code>

Get the name of the audio output—the value of the [`--output` command line flag](https://csound.github.io/docs/manual/CommandFlags.html#FlagsMinusLowerO). For example,

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
console.log(csound.GetOutputName(Csound));
```

logs `dac` to the console.

---

### [Score Handling](https://csound.github.io/docs/api/group__SCOREHANDLING.html)

##### <code><i>status</i> = csound.ReadScore(<i>Csound</i>, <i>scoreString</i>)</code>

Compile a string containing a Csound score, adding events and other structures to `Csound`. The returned `status` is a Csound [status code](#status-codes).

##### <a name="GetScoreTime"></a><code><i>elapsedTime</i> = csound.GetScoreTime(<i>Csound</i>)</code>

Get the elapsed time in seconds of a `Csound` performance. You can call this function during a performance. For the number of samples performed by `Csound`, multiply `elapsedTime` by the [sample rate](#GetSr), or use [`csound.GetCurrentTimeSamples`](#GetCurrentTimeSamples).

##### <code><i>performsScoreEvents</i> = csound.IsScorePending(<i>Csound</i>)</code>

Get a Boolean indicating whether `Csound` performs events from a score in addition to realtime events.

##### <code>csound.SetScorePending(<i>Csound</i>, <i>performsScoreEvents</i>)</code>

Set a Boolean indicating whether `Csound` performs events from a score in addition to realtime events.

##### <a name="GetScoreOffsetSeconds"></a><code><i>scoreEventStartTime</i> = csound.GetScoreOffsetSeconds(<i>Csound</i>)</code>

Get the amount of time subtracted from the start time of score events.

##### <code>csound.SetScoreOffsetSeconds(<i>Csound</i>, <i>scoreEventStartTime</i>)</code>

Set an amount of time to subtract from the start times of score events. For example,

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.SetOption(Csound, '--nosound');
csound.CompileOrc(Csound, [
  'nchnls = 1', 'sr = 44100', '0dbfs = 1', 'ksmps = 32',
  'instr 1',
    'prints "hello, world\n"',
  'endin'
].join('\n'));
var delay = 5;
csound.ReadScore(Csound, [
  'i 1 ' + delay + ' 0',
  'e'
].join('\n'));
csound.SetScoreOffsetSeconds(Csound, delay);
if (csound.Start(Csound) === csound.CSOUND_SUCCESS) {
  csound.Perform(Csound);
}
csound.Destroy(Csound);
```

prints `hello, world` immediately, not after a 5&nbsp;second delay.

##### <code>csound.RewindScore(<i>Csound</i>)</code>

Restart a compiled score at the time returned by [`csound.GetScoreOffsetSeconds`](#GetScoreOffsetSeconds).

---

### [Messages & Text](https://csound.github.io/docs/api/group__MESSAGES.html)

##### <code>csound.Message(<i>Csound</i>, <i>string</i>)</code>

Add to the `Csound` message queue a message consisting of a `string`.

##### <code>csound.MessageS(<i>Csound</i>, <i>attributes</i>, <i>string</i>)</code>

Add to the `Csound` message queue a message with `attributes` applied to a `string`. The value of `attributes` is a bit mask of

1. a type specified by one of `csound.CSOUNDMSG_DEFAULT`, `csound.CSOUNDMSG_ERROR`, `csound.CSOUNDMSG_ORCH`, `csound.CSOUNDMSG_REALTIME`, or `csound.CSOUNDMSG_WARNING`;

2. a text color specified by one of `csound.CSOUNDMSG_FG_BLACK`, `csound.CSOUNDMSG_FG_RED`, `csound.CSOUNDMSG_FG_GREEN`, `csound.CSOUNDMSG_FG_YELLOW`, `csound.CSOUNDMSG_FG_BLUE`, `csound.CSOUNDMSG_FG_MAGENTA`, `csound.CSOUNDMSG_FG_CYAN`, or `csound.CSOUNDMSG_FG_WHITE`;

3. the bold specifier `csound.CSOUNDMSG_FG_BOLD`;

4. the underline specifier `csound.CSOUNDMSG_FG_UNDERLINE`; and

5. a background color specified by one of `csound.CSOUNDMSG_BG_BLACK`, `csound.CSOUNDMSG_BG_RED`, `csound.CSOUNDMSG_BG_GREEN`, `csound.CSOUNDMSG_BG_ORANGE`, `csound.CSOUNDMSG_BG_BLUE`, `csound.CSOUNDMSG_BG_MAGENTA`, `csound.CSOUNDMSG_BG_CYAN`, or `csound.CSOUNDMSG_BG_GREY`.

You can determine the type, text color, and background color of the `attributes` by performing a [bitwise AND](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_Operators#Bitwise_AND) with `csound.CSOUNDMSG_TYPE_MASK`, `csound.CSOUNDMSG_FG_COLOR_MASK`, and `csound.CSOUNDMSG_BG_COLOR_MASK`, respectively.

##### <a name="SetMessageCallback"></a><code>csound.SetMessageCallback(<i>Csound</i>, function(<i>attributes</i>, <i>string</i>))</code>

Set a function for `Csound` to call when it dequeues a message with `attributes` applied to a `string`. It’s up to you to decide how to apply `attributes` to the `string`. For example, you might use the [ansi-styles](https://www.npmjs.com/package/ansi-styles) package to [log styled strings to the console](examples/log-styled-message.js).

##### <a name="CreateMessageBuffer"></a><code>csound.CreateMessageBuffer(<i>Csound</i>[, <i>writesToStandardStreams</i>])</code>

Prepare a message buffer for retrieving Csound messages using [`csound.GetMessageCnt`](#GetMessageCnt), [`csound.GetFirstMessage`](#GetFirstMessage), [`csound.GetFirstMessageAttr`](#GetFirstMessageAttr), and [`csound.PopFirstMessage`](#PopFirstMessage) instead of [`csound.SetMessageCallback`](#SetMessageCallback). You can retrieve messages from a buffer like this:

```javascript
var csound = require('csound-api');
var Csound = csound.Create();
csound.CreateMessageBuffer(Csound);
csound.Message(Csound, 'hello, world'); // Add a message to the buffer.
while (csound.GetMessageCnt(Csound) > 0) {
  console.log(csound.GetFirstMessage(Csound));
  csound.PopFirstMessage(Csound);
}
csound.DestroyMessageBuffer(Csound);
csound.Destroy(Csound);
```

To make `Csound` write messages to [standard streams](https://en.wikipedia.org/wiki/Standard_streams) in addition to the message buffer, you can pass `true` as the second argument. You should call [`csound.DestroyMessageBuffer`](#DestroyMessageBuffer) when you’re finished with the message buffer.

##### <a name="GetFirstMessage"></a><code><i>string</i> = csound.GetFirstMessage(<i>Csound</i>)</code>

Get the `string` of the first message on a message buffer.

##### <a name="GetFirstMessageAttr"></a><code><i>attributes</i> = csound.GetFirstMessageAttr(<i>Csound</i>)</code>

Get the `attributes` of the first message on a message buffer. The value of `attributes` is a bit mask like the one passed to the function argument of [`csound.SetMessageCallback`](#SetMessageCallback).

##### <a name="PopFirstMessage"></a><code>csound.PopFirstMessage(<i>Csound</i>)</code>

Remove the first message from a message buffer.

##### <a name="GetMessageCnt"></a><code><i>messageCount</i> = csound.GetMessageCnt(<i>Csound</i>)</code>

Get the number of messages on a message buffer.

##### <a name="DestroyMessageBuffer"></a><code>csound.DestroyMessageBuffer(<i>Csound</i>)</code>

Free resources used by a message buffer created using [`csound.CreateMessageBuffer`](#CreateMessageBuffer).

---

### [Channels, Control & Events](https://csound.github.io/docs/api/group__CONTROLEVENTS.html)

##### <code><i>status</i> = csound.ScoreEvent(<i>Csound</i>, <i>eventType</i>[, <i>parameterFieldValues</i>])</code>

Send a score event to `Csound`. The `eventType` string can be [`"a"`](https://csound.github.io/docs/manual/a.html), [`"e"`](https://csound.github.io/docs/manual/a.html), [`"f"`](https://csound.github.io/docs/manual/f.html), [`"i"`](https://csound.github.io/docs/manual/i.html), or [`"q"`](https://csound.github.io/docs/manual/q.html); and `parameterFieldValues` is an optional list of _numeric_ parameters for the corresponding score statement. Note that this means you cannot use `csound.ScoreEvent` to activate an instrument by name. The returned `status` is a Csound [status code](#status-codes).

##### <code>csound.InputMessage(<i>Csound</i>, <i>scoreStatement</i>)</code>

Send a [`scoreStatement`](https://csound.github.io/docs/manual/ScoreStatements.html) string to `Csound`.

---

### [Tables](https://csound.github.io/docs/api/group__TABLE.html)

##### <code><i>length</i> = csound.TableLength(<i>Csound</i>, <i>functionTableID</i>)</code>

Get the length of the function table with `functionTableID`. The `functionTableID` is parameter&nbsp;1 of a score [`f`&nbsp;statement](https://csound.github.io/docs/manual/f.html).

##### <code><i>numberAtIndex</i> = csound.TableGet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>)</code>

Get the value of the function table with `functionTableID` at the specified `index`. The `index` must be less than the function table’s length.

##### <code>csound.TableSet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>, <i>number</i>)</code>

Set the value at the specified `index` of the function table with `functionTableID` to `number`. The `index` must be less than the function table’s length.

---

### [Function Table Display](https://csound.github.io/docs/api/group__TABLEDISPLAY.html)

##### <code><i>wasGraphable</i> = csound.SetIsGraphable(<i>Csound</i>, <i>isGraphable</i>)</code>

Set a Boolean indicating whether [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) and [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) are called. Note that you must set callback functions using both [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) and [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) for either callback function to be called.

##### <a name="SetMakeGraphCallback"></a><code>csound.SetMakeGraphCallback(<i>Csound</i>, function(<i>data</i>, <i>name</i>))</code>

Set a function for `Csound` to call when it makes a graph of a function table or other data series. The function is passed the `data` to be graphed as an array of numbers, and the `name` of the graph as a string. Note that you must also set a callback function using [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) for this function to be called.

##### <a name="SetDrawGraphCallback"></a><code>csound.SetDrawGraphCallback(<i>Csound</i>, function(<i>data</i>))</code>

Set a function for `Csound` to call when it draws a graph of a function table or other data series. The function is passed the `data` to be graphed as an array of numbers. Note that you must also set a callback function using [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) for this function to be called.

---

### [Opcodes](https://csound.github.io/docs/api/group__OPCODES.html)

##### <a name="NewOpcodeList"></a><code><i>opcodeCount</i> = csound.NewOpcodeList(<i>Csound</i>, <i>array</i>)</code>

Set the items in the `array` to objects describing the opcodes available in `Csound`. The objects added to the `array` have these read-only properties:

<dl>
  <dt><code>opname</code></dt><dd> is a string containing the name of the opcode.</dd>

  <dt><code>outypes</code></dt><dd> is a string indicating the types of the opcode’s output arguments. Each character of the string corresponds to an output argument.</dd>

  <dt><code>intypes</code></dt><dd> is a string indicating the types of the opcode’s input arguments. Each character of the string corresponds to an input argument.</dd>
</dl>

To match the Csound API, this function returns the length of the `array` of opcodes. When you’re finished with the `array` of opcodes, you should pass it to [csound.DisposeOpcodeList](#DisposeOpcodeList).

##### <a name="DisposeOpcodeList"></a><code>csound.DisposeOpcodeList(<i>Csound</i>, <i>array</i>)</code>

Free resources associated with an `array` passed to [csound.NewOpcodeList](#NewOpcodeList).

---

### [Miscellaneous Functions](https://csound.github.io/docs/api/group__MISCELLANEOUS.html)

##### <code><i>environmentVariableValue</i> = csound.GetEnv(<i>Csound</i>, <i>environmentVariableName</i>)</code>

Get the value of a [`Csound` environment variable](https://csound.github.io/docs/manual/CommandEnvironment.html) named `environmentVariableName`.

##### <code><i>status</i> = csound.SetGlobalEnv(<i>environmentVariableName</i>, <i>value</i>)</code>

Set the value of a [`Csound` environment variable](https://csound.github.io/docs/manual/CommandEnvironment.html) named `environmentVariableName` to string `value`.

---

### <a name="status-codes"></a>Status Codes

A number of Csound API functions return `csound.CSOUND_SUCCESS` upon successful completion, or one of these error codes:

* `csound.CSOUND_ERROR`
* `csound.CSOUND_INITIALIZATION`
* `csound.CSOUND_PERFORMANCE`
* `csound.CSOUND_MEMORY`
* `csound.CSOUND_SIGNAL`
