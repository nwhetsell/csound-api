# Csound API

[![Actions](https://github.com/nwhetsell/csound-api/workflows/CI/badge.svg)](https://github.com/nwhetsell/csound-api/actions?workflow=CI)
[![npm](https://img.shields.io/npm/v/csound-api.svg)](https://www.npmjs.com/package/csound-api)
[![npm](https://img.shields.io/npm/dt/csound-api.svg)](https://www.npmjs.com/package/csound-api)

This package is a [Node.js Addon](https://nodejs.org/api/addons.html) for using
[Csound](https://csound.com) through its C [API](https://csound.com/docs/api/).
The functions in this package try to match the functions in Csound’s API as
closely as possible, and this package adds [`PerformAsync`](#PerformAsync) and
[`PerformKsmpsAsync`](#PerformKsmpsAsync) functions that run Csound in a
background thread. If you `require` this package using:

```javascript
const csound = require('csound-api');
```

you can use Csound’s API as, for example:

```javascript
function messageCallback(attributes, string) {
  console.log(string);
}
const Csound = csound.Create();
csound.SetMessageCallback(Csound, messageCallback);
csound.Message(Csound, 'hello, world');
```

The equivalent in C would be something like:

```c
void messageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  vprintf(format, argumentList);
}
CSOUND *Csound = csoundCreate(NULL);
csoundSetMessageCallback(Csound, messageCallback);
csoundMessage(Csound, "hello, world");
```

## Contents

* [Installing](#installing)
  * [On macOS](#on-macos)
  * [On Linux](#on-linux)
  * [On Windows](#on-windows)
* [Examples](#examples)
* [Contributing](#contributing)
* [API Coverage](#api-coverage)
  * [Instantiation](#instantiation)
  * [Performance](#performance)
  * [Attributes](#attributes)
  * [General Input/Output](#general-inputoutput)
  * [Score Handling](#score-handling)
  * [Messages & Text](#messages--text)
  * [Channels, Control & Events](#channels-control--events)
  * [Tables](#tables)
  * [Function Table Display](#function-table-display)
  * [Opcodes](#opcodes)
  * [Miscellaneous Functions](#miscellaneous-functions)
  * [Status Codes](#status-codes)
* [Tests](#tests)
  * [On macOS](#on-macos-1)
  * [On Windows](#on-windows-1)

## Installing

Before you install this package, you need [Boost](https://www.boost.org) 1.53.0
or later and Csound.

### On macOS

The easiest way to install Boost and Csound is probably through
[Homebrew](https://brew.sh). To install Homebrew, follow the instructions at
https://brew.sh. Then, enter in Terminal:

```sh
brew install boost csound
```

When installing Csound, Homebrew may output this error message:

```
Error: The `brew link` step did not complete successfully
The formula built, but is not symlinked into /usr/local
Could not symlink bin/atsa
Target /usr/local/bin/atsa
already exists. You may want to remove it:
  rm '/usr/local/bin/atsa'

To force the link and overwrite all conflicting files:
  brew link --overwrite csound

To list all files that would be deleted:
  brew link --overwrite --dry-run csound
```

This error occurs when Csound is already installed from a disk image available
at https://github.com/csound/csound/releases. To resolve this error, follow the
instructions in the error message.

After you install Boost and Csound, you can install this package by entering in
Terminal:

```sh
npm install csound-api
```

### On Linux

On many Linux distributions, you can install Boost and Csound by entering:

```sh
sudo apt-get --assume-yes install libboost-dev libcsound64-dev
```

You can then install this package by entering:

```sh
npm install csound-api
```

On Linux, this package depends on a Csound shared object (.so) file. This file
is named `libcsound64.so` when Csound is compiled to use double-precision
samples, and `libcsound.so` when Csound is compiled to use single-precision
samples. (The `64` in `libcsound64` refers to the number of bits in a
double-precision sample, not computer architecture.) This package depends on
`libcsound64.so`, not `libcsound.so`. If an error about a missing
`libcsound64.so` file occurs when installing this package, it probably means
your version of Csound uses single-precision samples.

### On Windows

To install Boost, you can download and run an installer of a prebuilt binary
from https://sourceforge.net/projects/boost/files/boost-binaries/.

To install Csound, you can download and run an installer from
https://github.com/csound/csound/releases/latest.

You must also follow the steps at https://github.com/nodejs/node-gyp#on-windows.

You can then install this package by entering in PowerShell:

```powershell
$Env:CL = '/I"C:\path\to\boost" /I"C:\path\to\csound\include"'
$Env:LINK = '"C:\path\to\csound\lib\csound64.lib"'
npm install csound-api
```

or in Command Prompt:

```batch
set CL=/I"C:\path\to\boost" /I"C:\path\to\csound\include"
set LINK="C:\path\to\csound\lib\csound64.lib"
npm install csound-api
```

where `C:\path\to\boost` is the path to Boost and `C:\path\to\csound` is the
path to Csound (usually `C:\Program Files\Csound6_x64`).

## [Examples](https://github.com/nwhetsell/csound-api/tree/master/examples)

Play a 440 Hz sine tone:

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, `
  0dbfs = 1
  giFunctionTableID ftgen 0, 0, 16384, 10, 1
  instr A440
    outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)
  endin
`);
csound.ReadScore(Csound, `
  i "A440" 0 1
  e
`);
if (csound.Start(Csound) === csound.SUCCESS)
  csound.Perform(Csound);
csound.Destroy(Csound);
```

Run Csound asynchronously, and stop Csound in mid-performance:

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, `
  0dbfs = 1
  instr SawtoothSweep
    // This outputs a sawtooth wave with a fundamental frequency that starts at
    // 110 Hz, rises to 220 Hz over 1 second, and then falls back to 110 Hz over
    // 1 second. The score plays this instrument for 2 seconds, but the call to
    // setTimeout() stops Csound after 1 second, so only the rise is heard.
    outc vco2(0.5 * 0dbfs, expseg(110, 1, 220, 1, 110))
  endin
`);
csound.ReadScore(Csound, `
  i "SawtoothSweep" 0 2
  e
`);
if (csound.Start(Csound) === csound.SUCCESS) {
  csound.PerformAsync(Csound, () => csound.Destroy(Csound));
  setTimeout(() => csound.Stop(Csound), 1000);
}
```

Log a list of Csound’s opcodes:

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
const opcodes = [];
csound.NewOpcodeList(Csound, opcodes);
console.log(opcodes);
csound.DisposeOpcodeList(Csound, opcodes);
csound.Destroy(Csound);
```

Log an abstract syntax tree parsed from an orchestra:

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
const ASTRoot = csound.ParseOrc(Csound, `
  0dbfs = 1
  giFunctionTableID ftgen 0, 0, 16384, 10, 1
  instr A440
    outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)
  endin
`);
console.log(ASTRoot);
csound.DeleteTree(Csound, ASTRoot);
csound.Destroy(Csound);
```

## Contributing

[Open an issue](https://github.com/nwhetsell/csound-api/issues), or
[fork this project and make a pull request](https://guides.github.com/activities/forking/).

## API Coverage

Here are the properties and functions you can use assuming you `require` this
package as

```javascript
const csound = require('csound-api');
```

---

### [Instantiation](https://csound.com/docs/api/group___i_n_s_t_a_n_t_i_a_t_i_o_n.html)

<a name="Create"></a>
**<code><i>Csound</i> = csound.Create([<i>value</i>])</code>**
creates a new `Csound` object and optionally associates a `value` with it;
`value` can be an object, a function, a string, a number, a Boolean, `null`, or
`undefined`. You can retrieve a `value` associated with a `Csound` object using
[`csound.GetHostData`](#GetHostData) and associate a new `value` using
[`csound.SetHostData`](#SetHostData). You must pass the returned `Csound` object
as the first argument to most other functions in this package, and you should
pass `Csound` to [`csound.Destroy`](#Destroy) when you’re finished using
`Csound`.

<a name="Destroy"></a>
**<code>csound.Destroy(<i>Csound</i>)</code>**
frees resources used by a `Csound` object.

<a name="GetVersion"></a>
**<code><i>versionTimes1000</i> = csound.GetVersion()</code>**
gets Csound’s version number multiplied by 1000. For example, if you’re using
Csound 6.13, `versionTimes1000` will be 6130.

<a name="GetAPIVersion"></a>
**<code><i>versionTimes100</i> = csound.GetAPIVersion()</code>**
gets the version of Csound’s API, multiplied by 100. For example, if you’re
using version 4.0 of Csound’s API, then `versionTimes100` will be 400.

<a name="Initialize"></a>
**<code><i>result</i> = csound.Initialize([<i>options</i>])</code>**
is called by [`csound.Create`](#Create), but you can call it before any calls to
`csound.Create` to prevent initialization of exit and signal handling functions.
Pass `csound.INIT_NO_ATEXIT` to prevent initialization of exit functions,
`csound.INIT_NO_SIGNAL_HANDLER` to prevent initialization of signal handling
functions, and a bitmask of both to prevent both. This can be useful when
debugging segmentation faults using a package like
[segfault-handler](https://www.npmjs.com/package/segfault-handler). The returned
`result` indicates the state of initialization:

When `result` is | Initialization
-----------------|-----------------------------------
greater than 0   | was already performed successfully
equal to 0       | is successful
less than 0      | failed because of an error

---

### [Performance](https://csound.com/docs/api/group___p_e_r_f_o_r_m_a_n_c_e.html)

<a name="ParseOrc"></a>
**<code><i>AST</i> = csound.ParseOrc(<i>Csound</i>, <i>orchestraString</i>)</code>**
parses a string containing a Csound orchestra into an abstract syntax tree
(AST). The returned `AST` is an object representing the root node of the AST.
AST nodes have these read-only properties:

<!--
Don’t indent inline HTML. NPM’s Markdown parser may think indented HTML is a
code block (https://github.com/npm/marky-markdown/issues/169).
-->
<table>
<thead><tr><th>Property</th><th>Description</th></tr></thead>
<tbody>

<tr>
<td><code>type</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
indicating the type of token. One way to determine how types correspond to
tokens is to build Csound from its
<a href="https://github.com/csound/csound">source code</a> and examine the
<a href="https://en.wikipedia.org/wiki/GNU_bison">Bison</a>-generated file
csound_orcparse.h.
</td>
</tr>

<tr>
<td><code>value</code></td>
<td>
<p>An
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object"><code>Object</code></a>
describing the token with several read-only properties:</p>
<table>
<thead><tr><th>Property</th><th>Description</th></tr></thead>
<tbody>
<tr>
<td><code>type</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
indicating the type of token. This may not be the same as the <code>type</code>
of the <code>AST</code> object.
</td>
</tr>
<tr>
<td><code>lexeme</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>,
usually the string value of the token, but not always. For example, operator
opcodes like
<a href="https://csound.com/docs/manual/adds.html"><code>+</code></a> have
lexemes like <code>##add</code>.
</td>
</tr>
<tr>
<td><code>value</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
equal to the value of the token if it’s an integer, and 0 otherwise.
</td>
</tr>
<tr>
<td><code>fvalue</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
equal to the value of the token if it’s a floating-point number, and 0
otherwise.
</td>
</tr>
<!--
<tr>
<td><code>optype</code></td>
<td></td>
</tr>
-->
<!--
<tr>
<td><code>next</code></td>
<td></td>
</tr>
-->
</tbody>
</table>
</td>
</tr>

<!--
<tr>
<td><code>rate</code></td>
<td></td>
</tr>
-->

<!--
<tr>
<td><code>len</code></td>
<td></td>
</tr>
-->

<tr>
<td><code>line</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
indicating the line where the token occurs.
</td>
</tr>

<!--
<tr>
<td><code>locn</code></td>
<td></td>
</tr>
-->

<tr>
<td><code>left</code></td>
<td>
An AST node that generally represents the first output argument of an opcode.
</td>
</tr>

<tr>
<td><code>right</code></td>
<td>
An AST node that generally represents the first input argument of an opcode.
</td>
</tr>

<tr>
<td><code>next</code></td>
<td>
<p>An AST node that is the first node of a linked list of all other arguments of
an opcode. Output arguments precede input arguments. For example, in an AST node
parsed from</p>
<pre>kFrequency, kAmplitude <a href="https://csound.com/docs/manual/pvread.html">pvread</a> kTime, "file.pvx", 1</pre>
<p>the <code>pvread</code> node will have a <code>left</code> node for the
<code>kFrequency</code> output argument; a <code>right</code> node for the
<code>kTime</code> input argument; and <code>next</code> nodes for the
<code>kAmplitude</code> output argument, <code>"file.pvx"</code> input argument,
and <code>1</code> input argument.</p>
</td>
</tr>

</tbody>
</table>

You can compile the `AST` using [`csound.CompileTree`](#CompileTree), and you
should pass the `AST` to [`csound.DeleteTree`](#DeleteTree) when you’re finished
with it.

<a name="CompileTree"></a>
**<code><i>status</i> = csound.CompileTree(<i>Csound</i>, <i>AST</i>)</code>**
compiles an `AST` returned from [`csound.ParseOrc`](#ParseOrc), adding
instruments and other structures to `Csound`. The returned `status` is a Csound
[status code](#status-codes).

<a name="DeleteTree"></a>
**<code>csound.DeleteTree(<i>Csound</i>, <i>AST</i>)</code>**
frees resources used by an `AST`.

<a name="CompileOrc"></a>
**<code><i>status</i> = csound.CompileOrc(<i>Csound</i>, <i>orchestraString</i>)</code>**
compiles a string containing a Csound orchestra, adding instruments and other
structures to `Csound`. The returned `status` is a Csound [status
code](#status-codes).

<a name="EvalCode"></a>
**<code><i>number</i> = csound.EvalCode(<i>Csound</i>, <i>orchestraString</i>)</code>**
gets a `number` passed to a global
[`return`](https://csound.com/docs/manual/return.html) opcode in
`orchestraString`. For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--nosound');
if (csound.Start(Csound) === csound.SUCCESS) {
  console.log(csound.EvalCode(Csound, `
    iResult = 19 + 23
    return iResult
  `));
}
```

logs the number 42. Before using this function, you must start `Csound`—that is,
you must pass `Csound` to [`csound.Start`](#Start), which must return the
`csound.SUCCESS` [status code](#status-codes).

<a name="CompileArgs"></a>
**<code><i>status</i> = csound.CompileArgs(<i>Csound</i>, <i>commandLineArguments</i>)</code>**
compiles instruments, sets options, and performs other actions according to
[command line arguments](https://csound.com/docs/manual/CommandFlags.html) in
the `commandLineArguments` string array, without starting `Csound`. For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.CompileArgs(Csound, ['csound', 'my.orc', 'my.sco']);
```

compiles the orchestra in my.orc and the score in my.sco, but does not start
`Csound`. To start `Csound` after calling `csound.CompileArgs`, pass `Csound` to
[`csound.Start`](#Start). To compile Csound files using command line arguments
and also start `Csound`, use [`csound.Compile`](#Compile). The returned `status`
is a Csound [status code](#status-codes).

<a name="Start"></a>
**<code><i>status</i> = csound.Start(<i>Csound</i>)</code>**
prepares `Csound` for performance—that is, to be passed to
[`csound.PerformAsync`](#PerformAsync), [`csound.Perform`](#Perform), or
[`csound.PerformKsmps`](#PerformKsmps). The returned `status` is a Csound
[status code](#status-codes).

<a name="Compile"></a>
**<code><i>status</i> = csound.Compile(<i>Csound</i>, <i>commandLineArguments</i>)</code>**
compiles instruments, sets options, and performs other actions according to
[command line arguments](https://csound.com/docs/manual/CommandFlags.html) in
the `commandLineArguments` string array, and also starts `Csound`. To compile
Csound files using command line arguments without starting `Csound`, use
[`csound.CompileArgs`](#CompileArgs). The returned `status` is a Csound [status
code](#status-codes).

<a name="CompileCsd"></a>
**<code><i>status</i> = csound.CompileCsd(<i>Csound</i>, <i>filePath</i>)</code>**
compiles the CSD file located at `filePath` and starts `Csound`. The returned
`status` is a Csound [status code](#status-codes).

<a name="PerformAsync"></a>
**<code>csound.PerformAsync(<i>Csound</i>, function(<i>result</i>))</code>**
performs score and input events on a background thread, and calls the passed
function when the performance stops. The `result` passed to this function is a
number that indicates the reason performance stopped:

When `result` is | Performance stopped because
-----------------|----------------------------------
greater than 0   | the end of the score was reached
equal to 0       | [`csound.Stop`](#Stop) was called
less than 0      | an error occurred

<a name="Perform"></a>
**<code><i>result</i> = csound.Perform(<i>Csound</i>)</code>**
performs score and input events on the main thread. The returned `result` is the
same as the `result` passed to the function argument of
[`csound.PerformAsync`](#PerformAsync).

<a name="PerformKsmpsAsync"></a>
**<code>csound.PerformKsmpsAsync(<i>Csound</i>, <i>controlPeriodFunction</i>, <i>performanceFinishedFunction</i>)</code>**
performs score and input events on a background thread, calling
`controlPeriodFunction` after a control period, and
`performanceFinishedFunction` when the performance is finished.

<a name="PerformKsmps"></a>
**<code><i>performanceFinished</i> = csound.PerformKsmps(<i>Csound</i>)</code>**
performs [one control period of samples](#GetKsmps) on the main thread,
returning `true` if the performance is finished and `false` otherwise.

<a name="Stop"></a>
**<code>csound.Stop(<i>Csound</i>)</code>**
stops a `Csound` performance asynchronously.

<a name="Cleanup"></a>
**<code><i>status</i> = csound.Cleanup(<i>Csound</i>)</code>**
frees resources after the end of a `Csound` performance. The returned `status`
is a Csound [status code](#status-codes).

<a name="Reset"></a>
**<code>csound.Reset(<i>Csound</i>)</code>**
frees resources after the end of a `Csound` performance (just like
[`csound.Cleanup`](#Cleanup)) and prepares for a new performance.

---

### [Attributes](https://csound.com/docs/api/group___a_t_t_r_i_b_u_t_e_s.html)

<a name="GetSr"></a>
**<code><i>sampleRate</i> = csound.GetSr(<i>Csound</i>)</code>**
gets [`sr`](https://csound.com/docs/manual/sr.html), the `Csound` sample rate
(also called the audio rate or a‑rate).

<a name="GetKr"></a>
**<code><i>controlRate</i> = csound.GetKr(<i>Csound</i>)</code>**
gets [`kr`](https://csound.com/docs/manual/kr.html), the `Csound` control rate
(also called the k‑rate).

<a name="GetKsmps"></a>
**<code><i>samplesPerControlPeriod</i> = csound.GetKsmps(<i>Csound</i>)</code>**
gets [`ksmps`](https://csound.com/docs/manual/ksmps.html), the number of audio
samples in one control period.

<a name="GetNchnls"></a>
**<code><i>outputChannelCount</i> = csound.GetNchnls(<i>Csound</i>)</code>**
gets [`nchnls`](https://csound.com/docs/manual/nchnls.html), the number of audio
output channels.

<a name="GetNchnlsInput"></a>
**<code><i>inputChannelCount</i> = csound.GetNchnlsInput(<i>Csound</i>)</code>**
gets [`nchnls_i`](https://csound.com/docs/manual/nchnls_i.html), the number of
audio input channels.

<a name="Get0dBFS"></a>
**<code><i>fullScalePeakAmplitude</i> = csound.Get0dBFS(<i>Csound</i>)</code>**
gets [`0dBFS`](https://csound.com/docs/manual/Zerodbfs.html), the maximum value
of a sample of audio.

<a name="GetCurrentTimeSamples"></a>
**<code><i>performedSampleCount</i> = csound.GetCurrentTimeSamples(<i>Csound</i>)</code>**
gets the number of samples performed by `Csound`. You can call this function
during a performance. For the elapsed time in seconds of a performance, divide
`performedSampleCount` by the [sample rate](#GetSr), or use
[`csound.GetScoreTime`](#GetScoreTime).

<a name="GetSizeOfMYFLT"></a>
**<code><i>bytesPerFloat</i> = csound.GetSizeOfMYFLT()</code>**
gets the number of bytes that Csound uses to represent floating-point numbers.
When Csound is compiled to use double-precision samples, `bytesPerFloat` is 8.
Otherwise, it’s 4.

<a name="GetHostData"></a>
**<code><i>value</i> = csound.GetHostData(<i>Csound</i>)</code>**
gets a `value` associated with a `Csound` object using
[`csound.Create`](#Create) or [`csound.SetHostData`](#SetHostData).

<a name="SetHostData"></a>
**<code>csound.SetHostData(<i>Csound</i>, <i>value</i>)</code>**
associates a `value` with a `Csound` object; `value` can be an object, a
function, a string, a number, a Boolean, `null`, or `undefined`. You can
retrieve a `value` associated with a `Csound` object using
[`csound.GetHostData`](#GetHostData).

<a name="SetOption"></a>
**<code><i>status</i> = csound.SetOption(<i>Csound</i>, <i>commandLineArgumentString</i>)</code>**
sets a `Csound` option as if `commandLineArgumentString` was input as a
[command line argument](https://csound.com/docs/manual/CommandFlags.html). For
example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
```

sets up `Csound` to output audio through your computer’s speakers. The returned
`status` is a Csound [status code](#status-codes).

<a name="GetDebug"></a>
**<code><i>queuesDebugMessages</i> = csound.GetDebug(<i>Csound</i>)</code>**
gets a Boolean indicating whether `Csound` adds debug messages to its message
queue. Use [`csound.SetDebug`](#SetDebug) to set this value.

<a name="SetDebug"></a>
**<code>csound.SetDebug(<i>Csound</i>, <i>queuesDebugMessages</i>)</code>**
sets a Boolean indicating whether `Csound` adds debug messages to its message
queue. Use [`csound.GetDebug`](#GetDebug) to get this value.

---

### [General Input/Output](https://csound.com/docs/api/group___f_i_l_e_i_o.html)

<a name="GetOutputName"></a>
**<code><i>audioOutputName</i> = csound.GetOutputName(<i>Csound</i>)</code>**
gets the name of the audio output—the value of the [`--output` command line
flag](https://csound.com/docs/manual/CommandFlags.html#FlagsMinusLowerO). For
example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
console.log(csound.GetOutputName(Csound));
```

logs `dac`.

<a name="SetOutput"></a>
**<code>csound.SetOutput(<i>Csound</i>, <i>name</i>[, <i>type</i>[, <i>format</i>]])</code>**
sets the `name`, file `type`, and encoding `format` of `Csound` output. If
`name` is `'dac'`, then `Csound` will output audio through your computer’s
speakers. Otherwise, `name` is the name of the file to which Csound will write
your performance. The optional `type` is a string indicating one of
[libsndfile](https://github.com/erikd/libsndfile)’s supported file types:

String    | File type
----------|----------
`'wav'`   | [Microsoft WAV](https://en.wikipedia.org/wiki/WAV)
`'aiff'`  | [Apple AIFF/AIFC](https://en.wikipedia.org/wiki/Audio_Interchange_File_Format)
`'au'`    | [Sun Au](https://en.wikipedia.org/wiki/Au_file_format)
`'raw'`   | Audio in any format
`'paf'`   | [Ensoniq PARIS](https://en.wikipedia.org/wiki/Ensoniq_PARIS) Audio Format
`'svx'`   | [Amiga 8SVX](https://en.wikipedia.org/wiki/8SVX)
`'nist'`  | [NIST Speech File Manipulation Software (SPHERE)](https://www.nist.gov/itl/iad/mig/tools)
`'voc'`   | [Creative Labs](https://en.wikipedia.org/wiki/Creative_Technology_Limited) Voice
`'ircam'` | [Berkeley](https://en.wikipedia.org/wiki/Berkeley_Software_Distribution)/[IRCAM](https://en.wikipedia.org/wiki/IRCAM)/[CARL](https://music-cms.ucsd.edu) Sound Format
`'w64'`   | [Sound Forge](https://en.wikipedia.org/wiki/Sound_Forge) Wave 64
`'mat4'`  | [MATLAB](https://en.wikipedia.org/wiki/MATLAB) MAT-File Level 4
`'mat5'`  | [MATLAB](https://en.wikipedia.org/wiki/MATLAB) MAT-File Level 5
`'pvf'`   | [Nullsoft](https://en.wikipedia.org/wiki/Nullsoft) Portable Voice Format
`'htk'`   | [Hidden Markov Model Toolkit](https://htk.eng.cam.ac.uk)
`'sds'`   | MIDI Sample Dump Standard
`'avr'`   | Audio Visual Research
`'wavex'` | Microsoft WAV Extensible
`'sd2'`   | [Sound Designer II](https://en.wikipedia.org/wiki/Avid_Audio#Sound_Designer_file_formats)
`'flac'`  | [Free Lossless Audio Codec](https://en.wikipedia.org/wiki/FLAC)
`'caf'`   | [Apple Core Audio Format](https://en.wikipedia.org/wiki/Core_Audio_Format)
`'wve'`   | [Psion](https://en.wikipedia.org/wiki/Psion_(company)) waveform
`'ogg'`   | [Ogg](https://en.wikipedia.org/wiki/Ogg) container
`'mpc2k'` | [Akai MPC2000](https://en.wikipedia.org/wiki/Music_Production_Controller#MPC2000)
`'rf64'`  | [European Broadcasting Union RF64](https://en.wikipedia.org/wiki/RF64)

The optional `format` is a string indicating one of libsndfile’s encoding
formats:

String     | Encoding format
-----------|----------------
`'schar'`  | Signed 8‑bit integer
`'short'`  | Signed 16‑bit integer
`'24bit'`  | Signed 24‑bit integer
`'long'`   | Signed 32‑bit integer
`'uchar'`  | Unsigned 8‑bit integer
`'float'`  | 32‑bit floating-point number
`'double'` | 64‑bit floating-point number
`'ulaw'`   | [_µ_‑law](https://en.wikipedia.org/wiki/Μ-law_algorithm)
`'alaw'`   | [A‑law](https://en.wikipedia.org/wiki/A-law_algorithm)
`'vorbis'` | [Vorbis](https://en.wikipedia.org/wiki/Vorbis)

To learn about the encoding formats you can use with each file type, see the
table at http://www.mega-nerd.com/libsndfile/.

---

### [Score Handling](https://csound.com/docs/api/group___s_c_o_r_e_h_a_n_d_l_i_n_g.html)

<a name="ReadScore"></a>
**<code><i>status</i> = csound.ReadScore(<i>Csound</i>, <i>scoreString</i>)</code>**
compiles a string containing a Csound score, adding events and other structures
to `Csound`. The returned `status` is a Csound [status code](#status-codes).

<a name="GetScoreTime"></a>
**<code><i>elapsedTime</i> = csound.GetScoreTime(<i>Csound</i>)</code>**
gets the elapsed time in seconds of a `Csound` performance. You can call this
function during a performance. For the number of samples performed by `Csound`,
multiply `elapsedTime` by the [sample rate](#GetSr), or use
[`csound.GetCurrentTimeSamples`](#GetCurrentTimeSamples).

<a name="IsScorePending"></a>
**<code><i>performsScoreEvents</i> = csound.IsScorePending(<i>Csound</i>)</code>**
gets a Boolean indicating whether `Csound` performs events from a score in
addition to realtime events. Use [`csound.SetScorePending`](#SetScorePending) to
set this value.

<a name="SetScorePending"></a>
**<code>csound.SetScorePending(<i>Csound</i>, <i>performsScoreEvents</i>)</code>**
sets a Boolean indicating whether `Csound` performs events from a score in
addition to realtime events. Use [`csound.IsScorePending`](#IsScorePending) to
get this value.

<a name="GetScoreOffsetSeconds"></a>
**<code><i>scoreEventStartTime</i> = csound.GetScoreOffsetSeconds(<i>Csound</i>)</code>**
gets the amount of time subtracted from the start time of score events. Use
[`csound.SetScoreOffsetSeconds`](#SetScoreOffsetSeconds) to set this time.

<a name="SetScoreOffsetSeconds"></a>
**<code>csound.SetScoreOffsetSeconds(<i>Csound</i>, <i>scoreEventStartTime</i>)</code>**
sets an amount of time to subtract from the start times of score events. For
example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--nosound');
csound.CompileOrc(Csound, `
  instr 1
    prints "hello, world\n"
  endin
`);
const delay = 5;
csound.ReadScore(Csound, `
  i 1 ${delay} 0
  e
`);
csound.SetScoreOffsetSeconds(Csound, delay);
if (csound.Start(Csound) === csound.SUCCESS)
  csound.Perform(Csound);
csound.Destroy(Csound);
```

prints `hello, world` immediately, not after a 5 second delay. Use
[`csound.GetScoreOffsetSeconds`](#GetScoreOffsetSeconds) to get this time.

<a name="RewindScore"></a>
**<code>csound.RewindScore(<i>Csound</i>)</code>**
restarts a compiled score at the time returned by
[`csound.GetScoreOffsetSeconds`](#GetScoreOffsetSeconds).

---

### [Messages & Text](https://csound.com/docs/api/group___m_e_s_s_a_g_e_s.html)

<a name="Message"></a>
**<code>csound.Message(<i>Csound</i>, <i>string</i>)</code>**
adds to the `Csound` message queue a message consisting of a `string`.

<a name="MessageS"></a>
**<code>csound.MessageS(<i>Csound</i>, <i>attributes</i>, <i>string</i>)</code>**
adds to the `Csound` message queue a message with `attributes` applied to a
`string`. The value of `attributes` is a bit mask of:

* a type specified by one of `csound.MSG_DEFAULT`, `csound.MSG_ERROR`,
`csound.MSG_ORCH`, `csound.MSG_REALTIME`, or `csound.MSG_WARNING`

* a text color specified by one of `csound.MSG_FG_BLACK`, `csound.MSG_FG_RED`,
`csound.MSG_FG_GREEN`, `csound.MSG_FG_YELLOW`, `csound.MSG_FG_BLUE`,
`csound.MSG_FG_MAGENTA`, `csound.MSG_FG_CYAN`, or `csound.MSG_FG_WHITE`

* the bold specifier `csound.MSG_FG_BOLD`

* the underline specifier `csound.MSG_FG_UNDERLINE`

* a background color specified by one of `csound.MSG_BG_BLACK`,
`csound.MSG_BG_RED`, `csound.MSG_BG_GREEN`, `csound.MSG_BG_ORANGE`,
`csound.MSG_BG_BLUE`, `csound.MSG_BG_MAGENTA`, `csound.MSG_BG_CYAN`, or
`csound.MSG_BG_GREY`

<a name="SetDefaultMessageCallback"></a>
**<code>csound.SetDefaultMessageCallback(function(<i>attributes</i>, <i>string</i>))</code>**
sets a function to call when Csound dequeues a default message—a message not
associated with a particular instance of Csound—with `attributes` applied to a
`string`. You can determine the type, text color, and background color of the
`attributes` by performing a
[bitwise AND](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_Operators#Bitwise_AND)
with `csound.MSG_TYPE_MASK`, `csound.MSG_FG_COLOR_MASK`, and
`csound.MSG_BG_COLOR_MASK` respectively. It’s up to you to decide how to apply
`attributes` to the `string`. For example, you might use the
[ansi-styles](https://www.npmjs.com/package/ansi-styles) package to
[log styled strings to the console](examples/log-styled-message.js).

<a name="SetMessageCallback"></a>
**<code>csound.SetMessageCallback(<i>Csound</i>, function(<i>attributes</i>, <i>string</i>))</code>**
sets a function to call when a particular instance of `Csound` dequeues a
message with `attributes` applied to a `string`. This function is called _in
addition_ to a function you pass to
[`csound.SetDefaultMessageCallback`](#SetDefaultMessageCallback).

<a name="CreateMessageBuffer"></a>
**<code>csound.CreateMessageBuffer(<i>Csound</i>[, <i>writesToStandardStreams</i>])</code>**
prepares a message buffer for retrieving Csound messages using
[`csound.GetMessageCnt`](#GetMessageCnt),
[`csound.GetFirstMessage`](#GetFirstMessage),
[`csound.GetFirstMessageAttr`](#GetFirstMessageAttr), and
[`csound.PopFirstMessage`](#PopFirstMessage) instead of
[`csound.SetMessageCallback`](#SetMessageCallback). You can retrieve messages
from a buffer like this:

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.CreateMessageBuffer(Csound);
csound.Message(Csound, 'hello, world'); // Add a message to the buffer.
while (csound.GetMessageCnt(Csound) > 0) {
  console.log(csound.GetFirstMessage(Csound));
  csound.PopFirstMessage(Csound);
}
csound.DestroyMessageBuffer(Csound);
csound.Destroy(Csound);
```

You can write `Csound` messages to
[standard streams](https://en.wikipedia.org/wiki/Standard_streams) in addition
to the message buffer by passing `true` as the second argument. You should call
[`csound.DestroyMessageBuffer`](#DestroyMessageBuffer) when you’re finished with
the message buffer.

<a name="GetFirstMessage"></a>
**<code><i>string</i> = csound.GetFirstMessage(<i>Csound</i>)</code>**
gets the `string` of the first message on a message buffer.

<a name="GetFirstMessageAttr"></a>
**<code><i>attributes</i> = csound.GetFirstMessageAttr(<i>Csound</i>)</code>**
gets the `attributes` of the first message on a message buffer. The value of
`attributes` is a bit mask like the one passed to the function argument of
[`csound.SetDefaultMessageCallback`](#SetDefaultMessageCallback).

<a name="PopFirstMessage"></a>
**<code>csound.PopFirstMessage(<i>Csound</i>)</code>**
removes the first message from a message buffer.

<a name="GetMessageCnt"></a>
**<code><i>messageCount</i> = csound.GetMessageCnt(<i>Csound</i>)</code>**
gets the number of messages on a message buffer.

<a name="DestroyMessageBuffer"></a>
**<code>csound.DestroyMessageBuffer(<i>Csound</i>)</code>**
frees resources used by a message buffer created using
[`csound.CreateMessageBuffer`](#CreateMessageBuffer).

---

### [Channels, Control & Events](https://csound.com/docs/api/group___c_o_n_t_r_o_l_e_v_e_n_t_s.html)

<a name="ListChannels"></a>
**<code><i>channelCount</i> = csound.ListChannels(<i>Csound</i>, <i>array</i>)</code>**
sets the contents of the `array` to objects describing communication channels
available in `Csound`, returning the new length of the `array` or a negative
[error code](#status-codes). When you’re finished with the `array`, you should
pass it to [`csound.DeleteChannelList`](#DeleteChannelList). The objects added
to the `array` have these read-only properties:

<table>
<thead><tr><th>Property</th><th>Description</th></tr></thead>
<tbody>

<tr>
<td><code>name</code></td>
<td>
The name of the channel as a
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>.
You can use this name with
<a href="#GetControlChannel"><code>csound.GetControlChannel</code></a> and
<a href="#SetControlChannel"><code>csound.SetControlChannel</code></a>; and the
<a href="https://csound.com/docs/manual/chn.html"><code>chn_*</code></a>,
<a href="https://csound.com/docs/manual/chnexport.html"><code>chnexport</code></a>,
<a href="https://csound.com/docs/manual/chnget.html"><code>chnget</code></a>,
<a href="https://csound.com/docs/manual/chnparams.html"><code>chnparams</code></a>,
and <a href="https://csound.com/docs/manual/chnset.html"><code>chnset</code></a>
opcodes.
</td>
</tr>

<tr>
<td><code>type</code></td>
<td>
<p>A bit mask of:</p>
<ul>
<li>a channel type specified by one of <code>csound.CONTROL_CHANNEL</code>,
<code>csound.AUDIO_CHANNEL</code>, <code>csound.STRING_CHANNEL</code>, or
<code>csound.PVS_CHANNEL</code></li>
<li>the input specifier <code>csound.INPUT_CHANNEL</code></li>
<li>the output specifier <code>csound.OUTPUT_CHANNEL</code></li>
</ul>
<p>You can determine the channel type by performing a
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_Operators#Bitwise_AND">bitwise AND</a>
with <code>csound.CHANNEL_TYPE_MASK</code>.</p>
</td>
</tr>

<tr>
<td><code>hints</code></td>
<td>
An
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object"><code>Object</code></a>
with the same properties as the object you obtain from
<a href="#GetControlChannelHints"><code>csound.GetControlChannelHints</code></a>.
</td>
</tr>

</tbody>
</table>


<a name="DeleteChannelList"></a>
**<code>csound.DeleteChannelList(<i>Csound</i>, <i>array</i>)</code>**
frees resources associated with an `array` passed to
[`csound.ListChannels`](#ListChannels).

<a name="GetControlChannelHints"></a>
**<code><i>status</i> = csound.GetControlChannelHints(<i>Csound</i>, <i>name</i>, <i>hints</i>)</code>**
gets the `hints` of a control channel named `name`. When this function returns,
the `hints` object will have properties you can use in a user interface:

<table>
<thead><tr><th>Property</th><th>Description</th></tr></thead>
<tbody>

<tr>
<td><code>behav</code></td>
<td>
<p>A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
indicating how the channel should behave:</p>
<table>
<thead>
<tr><th>When <code>behav</code> equals</th><th>The channel uses</th></tr>
</thead>
<tbody>
<tr>
<td><code>csound.CONTROL_CHANNEL_INT</code></td>
<td>integer values</td>
</tr>
<tr>
<td><code>csound.CONTROL_CHANNEL_LIN</code></td>
<td>real numbers on a linear scale</td>
</tr>
<tr>
<td><code>csound.CONTROL_CHANNEL_EXP</code></td>
<td>real numbers on an exponential scale</td>
</tr>
</tbody>
</table>
</td>
</tr>

<tr>
<td><code>dflt</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the channel’s default value.
</td>
</tr>

<tr>
<td><code>min</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the channel’s minimum value.
</td>
</tr>

<tr>
<td><code>max</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the channel’s maximum value.
</td>
</tr>

<tr>
<td><code>x</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the preferred <i>x</i>-coordinate for the channel’s user interface.
</td>
</tr>

<tr>
<td><code>y</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the preferred <i>y</i>-coordinate for the channel’s user interface.
</td>
</tr>

<tr>
<td><code>width</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the preferred width for the channel’s user interface.
</td>
</tr>

<tr>
<td><code>height</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the preferred height for the channel’s user interface.
</td>
</tr>

<tr>
<td><code>attributes</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>
of attributes for the channel.
</td>
</tr>

</tbody>
</table>

You must set the `hints` of a control channel using
[`csound.SetControlChannelHints`](#SetControlChannelHints) before using this
function. The returned `status` is a Csound [status code](#status-codes).

<a name="SetControlChannelHints"></a>
**<code><i>status</i> = csound.SetControlChannelHints(<i>Csound</i>, <i>name</i>, <i>hints</i>)</code>**
sets the `hints` of the control channel named `name`. For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--nosound');
const name = 'Channel';
csound.CompileOrc(Csound, `chn_k "${name}", 1`);
if (csound.Start(Csound) === csound.SUCCESS) {
  csound.SetControlChannelHints(Csound, name, {
    behav: csound.CONTROL_CHANNEL_INT,
    attributes: '==> attributes'
  });
  const hints = {};
  csound.GetControlChannelHints(Csound, name, hints);
  console.log(hints.attributes);
}
```

logs attributes of the control channel named Channel. Note that the `hints`
object you pass to this function _must_ have a `behav` property set to
`csound.CONTROL_CHANNEL_INT`, `csound.CONTROL_CHANNEL_LIN`, or
`csound.CONTROL_CHANNEL_EXP`. The returned `status` is a Csound [status
code](#status-codes).

<a name="GetControlChannel"></a>
**<code><i>number</i> = csound.GetControlChannel(<i>Csound</i>, <i>name</i>[, <i>info</i>])</code>**
gets the value of the control channel named `name`. If you pass an `info` object
to this function, when this function returns the object will have a `status`
property set to a Csound [status code](#status-codes).

<a name="SetControlChannel"></a>
**<code>csound.SetControlChannel(<i>Csound</i>, <i>name</i>, <i>number</i>)</code>**
sets the value of the control channel named `name` to a `number`.

<a name="ScoreEvent"></a>
**<code><i>status</i> = csound.ScoreEvent(<i>Csound</i>, <i>eventType</i>[, <i>parameterFieldValues</i>])</code>**
sends a score event to `Csound`. The `eventType` string can be
[`'a'`](https://csound.com/docs/manual/a.html),
[`'e'`](https://csound.com/docs/manual/a.html),
[`'f'`](https://csound.com/docs/manual/f.html),
[`'i'`](https://csound.com/docs/manual/i.html), or
[`'q'`](https://csound.com/docs/manual/q.html); and `parameterFieldValues` is an
optional array of _numeric_ parameters for the score event. (This means you
cannot use `csound.ScoreEvent` to activate an instrument by name.) The returned
`status` is a Csound [status code](#status-codes).

<a name="InputMessage"></a>
**<code>csound.InputMessage(<i>Csound</i>, <i>scoreStatement</i>)</code>**
sends a [score statement](https://csound.com/docs/manual/ScoreStatements.html)
string to `Csound`.

---

### [Tables](https://csound.com/docs/api/group___t_a_b_l_e.html)

<a name="TableLength"></a>
**<code><i>length</i> = csound.TableLength(<i>Csound</i>, <i>functionTableID</i>)</code>**
gets the length of the function table with `functionTableID`. The
`functionTableID` is parameter 1 of a score
[`f` statement](https://csound.com/docs/manual/f.html).

<a name="TableGet"></a>
**<code><i>numberAtIndex</i> = csound.TableGet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>)</code>**
gets the value of the function table with `functionTableID` at the specified
`index`. The `index` must be less than the function table’s length.

<a name="TableSet"></a>
**<code>csound.TableSet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>, <i>number</i>)</code>**
sets the value at the specified `index` of the function table with
`functionTableID` to `number`. The `index` must be less than the function
table’s length.

---

### [Function Table Display](https://csound.com/docs/api/group___t_a_b_l_e_d_i_s_p_l_a_y.html)

<a name="SetIsGraphable"></a>
**<code><i>wasGraphable</i> = csound.SetIsGraphable(<i>Csound</i>, <i>isGraphable</i>)</code>**
sets a Boolean indicating whether
[`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) and
[`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) are called, and returns
the previous value. Note that you must set callback functions using both
`csound.SetMakeGraphCallback` and `csound.SetDrawGraphCallback` for either
callback function to be called.

<a name="SetMakeGraphCallback"></a>
**<code>csound.SetMakeGraphCallback(<i>Csound</i>, function(<i>data</i>, <i>name</i>))</code>**
sets a function for `Csound` to call when it first makes a graph of a function
table or other data series. The function is passed a `data` object and the
`name` of the graph as a string. Note that you must pass `true` to
[`csound.SetIsGraphable`](#SetIsGraphable) and also set a callback function
using [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) for this function
to be called. The `data` object passed to the function has these read-only
properties:

<table>
<thead><tr><th>Property</th><th>Description</th></tr></thead>
<tbody>

<tr>
<td><code>windid</code></td>
<td>
An arbitrary
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
identifying the graph.
</td>
</tr>

<tr>
<td><code>caption</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>
describing the graph.
</td>
</tr>

<!--
<tr>
<td><code>polarity</code></td>
<td></td>
</tr>
-->

<tr>
<td><code>max</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the maximum of the <code>fdata</code> property.
</td>
</tr>

<tr>
<td><code>min</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving the minimum of the <code>fdata</code> property.
</td>
</tr>

<tr>
<td><code>oabsmax</code></td>
<td>
A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>
giving a scale factor for the vertical axis.
</td>
</tr>

<tr>
<td><code>fdata</code></td>
<td>
The data to be graphed as an
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array"><code>Array</code></a>
of <a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number"><code>Number</code></a>s.
</td>
</tr>

</tbody>
</table>

<a name="SetDrawGraphCallback"></a>
**<code>csound.SetDrawGraphCallback(<i>Csound</i>, function(<i>data</i>))</code>**
sets a function for `Csound` to call when it draws a graph of a function table
or other data series. The function is passed a `data` object with the same
properties as the one passed to the function argument of
[`csound.SetMakeGraphCallback`](#SetMakeGraphCallback). Note that you must pass
`true` to [`csound.SetIsGraphable`](#SetIsGraphable) and also set a callback
function using `csound.SetMakeGraphCallback` for this function to be called.

---

### [Opcodes](https://csound.com/docs/api/group___o_p_c_o_d_e_s.html)

<a name="NewOpcodeList"></a>
**<code><i>opcodeCount</i> = csound.NewOpcodeList(<i>Csound</i>, <i>array</i>)</code>**
sets the contents of the `array` to objects describing opcodes available in
`Csound`, returning the new length of the `array` or a negative [error
code](#status-codes). When you’re finished with the `array`, you should pass it
to [`csound.DisposeOpcodeList`](#DisposeOpcodeList). The objects added to the
`array` have these read-only properties:

<table>
<thead><tr><th>Property</th><th>Description</th></tr></thead>
<tbody>

<tr>
<td><code>opname</code></td>
<td>
The opcode’s name as a
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>.
</td>
</tr>

<tr>
<td><code>outypes</code></td>
<td>
<p>A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>
of characters that describe output arguments:</p>
<table>
<tbody>
<tr><td><code>a</code></td><td>a‑rate vector</td></tr>
<tr><td><code>F</code></td><td>comma-separated list of frequency-domain variables, used by <a href="https://csound.com/docs/manual/SpectralTop.html">phase vocoder opcodes</a></td></tr>
<tr><td><code>m</code></td><td>comma-separated list of a‑rate vectors</td></tr>
<tr><td><code>N</code></td><td>comma-separated list of i‑time scalars, k‑rate scalars, a‑rate vectors, and strings</td></tr>
<tr><td><code>s</code></td><td>k‑rate scalar or a‑rate vector</td></tr>
<tr><td><code>X</code></td><td>comma-separated list of i‑time scalars, k‑rate scalars, and a‑rate vectors</td></tr>
<tr><td><code>z</code></td><td>comma-separated list of k‑rate scalars</td></tr>
<tr><td><code>*</code></td><td>comma-separated list of arguments of any type</td></tr>
</tbody>
</table>
</td>
</tr>

<tr>
<td><code>intypes</code></td>
<td>
<p>A
<a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String"><code>String</code></a>
of characters that describe input arguments:</p>
<table>
<tbody>
<tr><td><code>a</code></td><td>a‑rate vector</td></tr>
<tr><td><code>B</code></td><td>Boolean</td></tr>
<tr><td><code>f</code></td><td>frequency-domain variable, used by <a href="https://csound.com/docs/manual/SpectralTop.html">phase vocoder opcodes</a></td></tr>
<tr><td><code>h</code></td><td>optional i‑time scalar defaulting to 127</td></tr>
<tr><td><code>i</code></td><td>i‑time scalar</td></tr>
<tr><td><code>j</code></td><td>optional i‑time scalar defaulting to –1</td></tr>
<tr><td><code>J</code></td><td>optional k‑rate scalar defaulting to –1</td></tr>
<tr><td><code>k</code></td><td>k‑rate scalar</td></tr>
<tr><td><code>l</code></td><td>label, used by <code>goto</code> opcodes</td></tr>
<tr><td><code>m</code></td><td>comma-separated list of any number of i‑time scalars</td></tr>
<tr><td><code>M</code></td><td>comma-separated list of i‑time scalars, k‑rate scalars, and a‑rate vectors</td></tr>
<tr><td><code>n</code></td><td>comma-separated list of an odd number of i‑time scalars</td></tr>
<tr><td><code>N</code></td><td>comma-separated list of i‑time scalars, k‑rate scalars, a‑rate vectors, and strings</td></tr>
<tr><td><code>o</code></td><td>optional i‑time scalar defaulting to 0</td></tr>
<tr><td><code>O</code></td><td>optional k‑rate scalar defaulting to 0</td></tr>
<tr><td><code>p</code></td><td>optional i‑time scalar defaulting to 1</td></tr>
<tr><td><code>P</code></td><td>optional k‑rate scalar defaulting to 1</td></tr>
<tr><td><code>q</code></td><td>optional i‑time scalar defaulting to 10</td></tr>
<tr><td><code>S</code></td><td>string</td></tr>
<tr><td><code>T</code></td><td>i‑time scalar or string</td></tr>
<tr><td><code>U</code></td><td>i‑time scalar, k‑rate scalar, or string</td></tr>
<tr><td><code>v</code></td><td>optional i‑time scalar defaulting to 0.5</td></tr>
<tr><td><code>V</code></td><td>optional k‑rate scalar defaulting to 0.5</td></tr>
<tr><td><code>w</code></td><td>frequency-domain variable, used by <a href="https://csound.com/docs/manual/SpectralNonstand.html"><code>spectrum</code> and related opcodes</a></td></tr>
<tr><td><code>W</code></td><td>comma-separated list of strings</td></tr>
<tr><td><code>x</code></td><td>k‑rate scalar or a‑rate vector</td></tr>
<tr><td><code>y</code></td><td>comma-separated list of a‑rate vectors</td></tr>
<tr><td><code>z</code></td><td>comma-separated list of k‑rate scalars</td></tr>
<tr><td><code>Z</code></td><td>comma-separated list of alternating k‑rate scalars and a‑rate vectors</td></tr>
<tr><td><code>.</code></td><td>required argument of any type</td></tr>
<tr><td><code>?</code></td><td>optional argument of any type</td></tr>
<tr><td><code>*</code></td><td>comma-separated list of arguments of any type</td></tr>
</tbody>
</table>
</td>
</tr>

</tbody>
</table>

<a name="DisposeOpcodeList"></a>
**<code>csound.DisposeOpcodeList(<i>Csound</i>, <i>array</i>)</code>**
frees resources associated with an `array` passed to
[`csound.NewOpcodeList`](#NewOpcodeList).

---

### [Miscellaneous Functions](https://csound.com/docs/api/group___m_i_s_c_e_l_l_a_n_e_o_u_s.html)

<a name="GetEnv"></a>
**<code><i>environmentVariableValue</i> = csound.GetEnv(<i>Csound</i>, <i>environmentVariableName</i>)</code>**
gets the string value of a [`Csound` environment
variable](https://csound.com/docs/manual/CommandEnvironment.html) named
`environmentVariableName`.

<a name="SetGlobalEnv"></a>
**<code><i>status</i> = csound.SetGlobalEnv(<i>environmentVariableName</i>, <i>value</i>)</code>**
sets the value of a [`Csound` environment
variable](https://csound.com/docs/manual/CommandEnvironment.html) named
`environmentVariableName` to string `value`.

---

### Status Codes

A number of csound-api functions return `csound.SUCCESS` upon successful
completion, or one of these error codes:

* `csound.ERROR`
* `csound.INITIALIZATION`
* `csound.PERFORMANCE`
* `csound.MEMORY`
* `csound.SIGNAL`

---

## Tests

The [tests](spec/csound-api-spec.js) of this package require
[Jasmine](https://jasmine.github.io/edge/node.html). To install the Jasmine
package globally, run `npm install --global jasmine`. To run the tests, `cd` to
the csound-api folder (which should be in node_modules if you installed
csound-api locally) and run `jasmine`.

### On macOS

To run the Jasmine tests in Xcode:

1. `cd` to the csound-api folder and run:
    ```sh
    node-gyp rebuild --debug && node-gyp configure -- -f xcode
    ```
    to create a debug version of csound-api and an Xcode project at
csound-api/build/binding.xcodeproj.

2. Open the Xcode project, choose Product > Scheme > Edit Scheme or press
<kbd>Command</kbd>-<kbd>&lt;</kbd> to open the scheme editor, and select Run in
the list on the left.

3. In the Info tab, select Other from the Executable pop-up menu, press
<kbd>Command</kbd>-<kbd>Shift</kbd>-<kbd>G</kbd>, enter the path to the Node.js
executable in the dialog that appears, click Go, and then click Choose. The
Node.js executable is usually at /usr/local/bin/node, and you can determine the
path to the Node.js executable by running `which node` in Terminal.

4. In the Arguments tab, add to the Arguments Passed On Launch:
    1. Jasmine’s path (if you installed Jasmine globally, you can determine
    Jasmine’s path by running `which jasmine` in Terminal)
    2. `--config=/path/to/csound-api-spec.js`, where
    `/path/to/csound-api-spec.js` is the path to
    [csound-api-spec.js](spec/csound-api-spec.js).
    3. `--no-color`
    4. `--random=false`
    5. `--reporter=/path/to/reporter.js`, where `/path/to/reporter.js` is the
    path to [reporter.js](spec/reporter.js).

5. Close the scheme editor, and then choose Product > Run or press
<kbd>Command</kbd>-<kbd>R</kbd> to run csound-api’s tests in Xcode.

### On Windows

To run the Jasmine tests in Visual Studio:

1. `cd` to the csound-api folder and run
    ```batch
    node-gyp rebuild --debug && node-gyp configure -- -f msvs
    ```
    to create a debug version of csound-api and a Visual Studio solution at
csound-api/build/binding.sln.

2. Open the Visual Studio solution, select the csound-api project in the
Solution Explorer, press <kbd>Alt</kbd>-<kbd>Enter</kbd> to open the csound-api
Property Pages, select C/C++ in the list on the left, and add the path to Boost
to the semicolon-separated list of Additional Include Directories.

3. Choose File > Add > Existing Project, select the Node.js executable in the
dialog that appears, and then click Open. The Node.js executable is usually at
C:\Program Files\nodejs\node.exe, and you can determine the path to the Node.js
executable by running `where node` in PowerShell or Command Prompt.

4. Right-click the Node.js executable in the Solution Explorer and select Set as
StartUp Project in the menu that appears. Then, press
<kbd>Alt</kbd>-<kbd>Enter</kbd> to view the Node.js executable’s properties.

5. Set the Arguments to Jasmine’s path, enclosed in quotes. If you installed
Jasmine globally, this is usually the output of running in PowerShell:
    ```powershell
    "$Env:APPDATA\npm\node_modules\jasmine\bin\jasmine.js"
    ```
    or in Command Prompt:
    ```batch
    echo "%APPDATA%\npm\node_modules\jasmine\bin\jasmine.js"
    ```

6. Add an environment variable named JASMINE_CONFIG_PATH with a value of the
relative path from Node.js to the csound-api test script. To quickly determine
this path, `cd` to the csound-api folder and run:
    ```batch
    python -c "import os, subprocess; print(os.path.relpath(os.path.join(os.getcwd(), 'spec', 'csound-api-spec.js'), subprocess.check_output(['where', 'node'])))"
    ```

7. Choose Debug > Start Debugging or press <kbd>F5</kbd> to run csound-api’s
tests in Visual Studio.
