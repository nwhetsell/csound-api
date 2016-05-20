# Csound API

This package is a [Node.js Addon](https://nodejs.org/api/addons.html) for using [Csound](https://csound.github.io) through its C&nbsp;[API](https://csound.github.io/docs/api/index.html). The functions in this package try to match the functions in Csound’s API as closely as possible, and this package adds a [`PerformAsync`](#PerformAsync) function that runs Csound in a background thread. If you `require` this package using

```javascript
const csound = require('csound-api');
```

then you can use Csound’s API as, for example,

```javascript
function messageCallback(attributes, string) {
  console.log(string);
}
const Csound = csound.Create();
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

The easiest way to install Boost is probably through [Homebrew](http://brew.sh). To install Homebrew, follow the instructions at [brew.sh](http://brew.sh). Then, run `brew install boost` in a Terminal.

If you aren’t able to build Csound from its [source code](https://github.com/csound/csound), the most reliable way to install Csound so that you can build csound-api is to run an installer in a disk image you can download from https://github.com/csound/csound/releases (scroll until you find the Downloads section). When you double-click the installer in the disk image, OS&nbsp;X may not allow the installer to run because it’s from an unidentified developer. To run the installer after this happens, open System Preferences, choose Security & Privacy, and click Open Anyway in the bottom half of the window.

After you install Csound using the disk image, you must create a symbolic link to Csound’s headers in /usr/local/include. To do this, open a Terminal and run

```sh
ln -s /Library/Frameworks/CsoundLib64.framework/Headers /usr/local/include/csound
```

After you install Boost and Csound, you can install this package by running

```sh
npm install csound-api
```

### On Linux

To install Boost, run

```sh
sudo apt-get install -y libboost-dev
```

To install Csound so that you can build csound-api, run

```sh
sudo apt-get install -y libcsound64-dev
```

After you install Boost and Csound, you can install this package by running

```sh
npm install csound-api
```

### On Windows

In addition to Boost and Csound, you need Python&nbsp;2.7 and Visual Studio.

To install Python&nbsp;2.7, visit https://www.python.org/downloads/windows/ and download and run an installer for the latest release of Python&nbsp;2.7. Make sure you add python.exe to your Windows Path when you install Python.

To install Visual Studio, visit https://www.visualstudio.com and download and run an installer for Visual Studio. (Visual Studio Community 2015 is free.) Make sure you install the Windows&nbsp;8.1 software development kit (SDK) when you install Visual Studio. One way to do this is to perform a custom installation and, when selecting features, select Windows and Web Development&nbsp;> Windows&nbsp;8.1 and Windows Phone 8.0/8.1 Tools&nbsp;> Tools and Windows SDKs.

To install Boost, you can download and run an installer of a prebuilt binary from https://sourceforge.net/projects/boost/files/boost-binaries/.

To install Csound, you can download and run an installer from https://github.com/csound/csound/releases (scroll until you find the Downloads section).

You must also create a csound64.lib file after you install Csound. To do this, download pexports (which is part of [MinGW](http://mingw.org)) from https://sourceforge.net/projects/mingw/files/MinGW/Extension/pexports/. The name of the file you download should end with *bin.tar.xz*. You can unpack pexports.exe from the tar.xz file with [7‑Zip](http://7-zip.org). Put pexports.exe in C:\\Program Files\\Csound6_x64\\bin, open a Command Prompt in that folder, and run

```
pexports csound64.dll > csound64.def
"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\lib" /DEF:csound64.def /MACHINE:X64
```

to create csound64.lib. It’s OK to delete pexports.exe after you create csound64.lib.

After you install Python&nbsp;2.7, Visual Studio, Boost, and Csound, you can install this package by running

```
set CL=/I"C:\local\boost_1_61_0"
npm install csound-api
```

You may need to change `C:\local\boost_1_61_0` if Boost is in a different folder.

## [Examples](https://github.com/nwhetsell/csound-api/tree/master/examples)

Play a 440&nbsp;Hz sine tone.

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, `
  nchnls = 1
  sr = 44100
  0dbfs = 1
  ksmps = 32
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

Run Csound asynchronously, and stop Csound in mid-performance.

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, `
  nchnls = 1
  sr = 44100
  0dbfs = 1
  ksmps = 32
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

Log a list of Csound’s opcodes.

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
const opcodes = [];
csound.NewOpcodeList(Csound, opcodes);
console.log(opcodes);
csound.DisposeOpcodeList(Csound, opcodes);
csound.Destroy(Csound);
```

Log an abstract syntax tree parsed from an orchestra.

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
const ASTRoot = csound.ParseOrc(Csound, `
  nchnls = 1
  sr = 44100
  0dbfs = 1
  ksmps = 32
  giFunctionTableID ftgen 0, 0, 16384, 10, 1
  instr A440
    outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)
  endin
`);
console.log(ASTRoot);
csound.DeleteTree(Csound, ASTRoot);
csound.Destroy(Csound);
```

## Tests

Running the [tests](https://github.com/nwhetsell/csound-api/blob/master/spec/csound-api-spec.js) of this package requires [Jasmine](https://jasmine.github.io/edge/node.html). To install the Jasmine package globally, run

```sh
npm install -g jasmine
```

To run the tests, `cd` to the csound-api folder (which should be in node_modules if you installed csound-api locally) and run `jasmine`.

### On OS&nbsp;X

You can run the Jasmine tests in Xcode so that you can use a graphical debugger. To do this:

1. `cd` to the csound-api folder and run
    ```sh
    node-gyp rebuild --debug && node-gyp configure -- -f xcode
    ```
to create a debug version of csound-api and an Xcode project at csound-api/build/binding.xcodeproj.

2. Open the Xcode project, choose Product > Scheme > Edit Scheme or press <kbd>Command</kbd>-<kbd>&lt;</kbd> to open the scheme editor, and select Run in the list on the left.

3. In the Info tab, select Other from the Executable pop-up menu, press <kbd>Command</kbd>-<kbd>Shift</kbd>-<kbd>G</kbd>, enter the path to your Node.js executable in the dialog that appears, click Go, and then click Choose. The Node.js executable is usually at /usr/local/bin/node, and you can determine the path to your Node.js executable by running in Terminal
    ```sh
    which node
    ```

4. In the Arguments tab, add Jasmine’s path to the Arguments Passed On Launch. If you installed Jasmine globally, you can determine Jasmine’s path by running in Terminal
    ```sh
    which jasmine
    ```
You may also want to add a `--no-colors` argument so that [ANSI escape codes](https://en.wikipedia.org/wiki/ANSI_escape_code) don’t appear in Xcode’s Console.

5. Add an environment variable named JASMINE_CONFIG_PATH with a value of the relative path from Node.js to the csound-api test script. To quickly determine this path, `cd` to the csound-api folder and run
    ```sh
    python -c "import os; print(os.path.relpath('`pwd`/spec/csound-api-spec.js', os.path.realpath('`which node`')))"
    ```

6. Close the scheme editor, and then choose Product > Run or press <kbd>Command</kbd>-<kbd>R</kbd> to run csound-api’s tests in Xcode.

## Contributing

[Open an issue](https://github.com/nwhetsell/csound-api/issues), or [fork this project and submit a pull request](https://guides.github.com/activities/forking/).

## API Coverage

Here are the properties and functions you can use assuming you `require` this package as

```javascript
const csound = require('csound-api');
```

---

### [Instantiation](https://csound.github.io/docs/api/group__INSTANTIATION.html)

<a name="Create"></a>**<code><i>Csound</i> = csound.Create([<i>value</i>])</code>** creates a new `Csound` object and optionally associates a `value` with it; `value` can be an object, a function, a string, a number, a Boolean, `null`, or `undefined`. You can retrieve a `value` associated with a `Csound` object using [`csound.GetHostData`](#GetHostData) and associate a new `value` using [`csound.SetHostData`](#SetHostData). You must pass the returned `Csound` object as the first argument to most other functions in this package, and you should pass `Csound` to [`csound.Destroy`](#Destroy) when you’re finished using `Csound`.

<a name="Destroy"></a>**<code>csound.Destroy(<i>Csound</i>)</code>** frees resources used by a `Csound` object.

<a name="GetVersion"></a>**<code><i>versionTimes1000</i> = csound.GetVersion()</code>** gets Csound’s version number multiplied by 1,000. For example, if you’re using Csound 6.07, then `versionTimes1000` will be 6,070.

<a name="GetAPIVersion"></a>**<code><i>versionTimes100</i> = csound.GetAPIVersion()</code>** gets the version of Csound’s API, multiplied by 100. For example, if you’re using version&nbsp;3.1 of Csound’s API, then `versionTimes100` will be 310.

---

### [Performance](https://csound.github.io/docs/api/group__PERFORMANCE.html)

<a name="ParseOrc"></a>**<code><i>AST</i> = csound.ParseOrc(<i>Csound</i>, <i>orchestraString</i>)</code>** parses a string containing a Csound orchestra into an abstract syntax tree (AST). The returned `AST` is an object representing the root node of the AST. AST nodes have a number of read-only properties:

<!--
Do not indent inline HTML. NPM’s Markdown parser may think indented HTML is a
code block (https://github.com/npm/marky-markdown/issues/169).
-->
<dl>
<dt><code>type</code></dt><dd> is a number indicating the type of token. One way to determine how types correspond to tokens is to build Csound from its <a href="https://github.com/csound/csound">source code</a> and examine the <a href="https://en.wikipedia.org/wiki/GNU_bison">Bison</a>-generated file csound_orcparse.h.</dd>

<dt><code>value</code></dt><dd> is an object describing the token. It has several read-only properties:<dl>
<dt><code>type</code></dt><dd> is a number indicating the type of token. This need not be the same as the <code>type</code> of the <code>AST</code> object.</dd>

<dt><code>lexeme</code></dt><dd> is a string. This is usually the string value of the token, but not always. For example, operator opcodes like <a href="https://csound.github.io/docs/manual/adds.html"><code>+</code></a> have lexemes like <code>##add</code>.</dd>

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
the <code>pvread</code> node will have a <code>left</code> node for the <code>kFrequency</code> output argument; a <code>right</code> node for the <code>kTime</code> input argument; and <code>next</code> nodes for the <code>kAmplitude</code> output argument, <code>"file.pvx"</code> input argument, and <code>1</code> input argument.
</dd>
</dl>

You can compile the `AST` using [`csound.CompileTree`](#CompileTree), and you should pass the `AST` to [`csound.DeleteTree`](#DeleteTree) when you’re finished with it.

<a name="CompileTree"></a>**<code><i>status</i> = csound.CompileTree(<i>Csound</i>, <i>AST</i>)</code>** compiles an `AST` returned from [`csound.ParseOrc`](#ParseOrc), adding instruments and other structures to `Csound`. The returned `status` is a Csound [status code](#status-codes).

<a name="DeleteTree"></a>**<code>csound.DeleteTree(<i>Csound</i>, <i>AST</i>)</code>** frees resources used by an `AST`.

<a name="CompileOrc"></a>**<code><i>status</i> = csound.CompileOrc(<i>Csound</i>, <i>orchestraString</i>)</code>** compiles a string containing a Csound orchestra, adding instruments and other structures to `Csound`. The returned `status` is a Csound [status code](#status-codes).

<a name="EvalCode"></a>**<code><i>number</i> = csound.EvalCode(<i>Csound</i>, <i>orchestraString</i>)</code>** gets a `number` passed to a global [`return`](https://csound.github.io/docs/manual/return.html) opcode in `orchestraString`. For example,

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

logs the number 42. Before using this function, you must start `Csound`—that is, you must pass `Csound` to [`csound.Start`](#Start), which must return the `csound.SUCCESS` [status code](#status-codes).

<a name="CompileArgs"></a>**<code><i>status</i> = csound.CompileArgs(<i>Csound</i>, <i>commandLineArguments</i>)</code>** compiles instruments, sets options, and performs other actions according to [command line arguments](https://csound.github.io/docs/manual/CommandFlags.html) in the `commandLineArguments` string array, without starting `Csound`. For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.CompileArgs(Csound, ['csound', 'my.orc', 'my.sco']);
```

compiles the orchestra in my.orc and the score in my.sco, but does not start `Csound`. To start `Csound` after calling `csound.CompileArgs`, pass `Csound` to [`csound.Start`](#Start). To compile Csound files using command line arguments and also start `Csound`, use [`csound.Compile`](#Compile). The returned `status` is a Csound [status code](#status-codes).

<a name="Start"></a>**<code><i>status</i> = csound.Start(<i>Csound</i>)</code>** prepares `Csound` for performance—that is, to be passed to [`csound.PerformAsync`](#PerformAsync), [`csound.Perform`](#Perform), or [`csound.PerformKsmps`](#PerformKsmps). The returned `status` is a Csound [status code](#status-codes).

<a name="Compile"></a>**<code><i>status</i> = csound.Compile(<i>Csound</i>, <i>commandLineArguments</i>)</code>** compiles instruments, sets options, and performs other actions according to [command line arguments](https://csound.github.io/docs/manual/CommandFlags.html) in the `commandLineArguments` string array, and also starts `Csound`. To compile Csound files using command line arguments without starting `Csound`, use [`csound.CompileArgs`](#CompileArgs). The returned `status` is a Csound [status code](#status-codes).

**<code><i>status</i> = csound.CompileCsd(<i>Csound</i>, <i>filePath</i>)</code>** compiles the CSD file located at `filePath` and starts `Csound`. The returned `status` is a Csound [status code](#status-codes).

<a name="PerformAsync"></a>**<code>csound.PerformAsync(<i>Csound</i>, function(<i>result</i>))</code>** performs score and input events on a background thread, and calls the passed function when the performance stops. The `result` passed to this function is a number that indicates the reason performance stopped:

When `result` is | Performance stopped because
-----------------|----------------------------------
greater than 0   | the end of the score was reached
equal to 0       | [`csound.Stop`](#Stop) was called
less than 0      | an error occurred

<a name="Perform"></a>**<code><i>result</i> = csound.Perform(<i>Csound</i>)</code>** performs score and input events on the main thread. The returned `result` is the same as the `result` passed to the function argument of [`csound.PerformAsync`](#PerformAsync).

<a name="PerformKsmps"></a>**<code><i>performanceFinished</i> = csound.PerformKsmps(<i>Csound</i>)</code>** performs [one control period of samples](#GetKsmps) on the main thread, returning `true` if the performance is finished and `false` otherwise.

<a name="Stop"></a>**<code>csound.Stop(<i>Csound</i>)</code>** stops a `Csound` performance asynchronously.

<a name="Cleanup"></a>**<code><i>status</i> = csound.Cleanup(<i>Csound</i>)</code>** frees resources after the end of a `Csound` performance. The returned `status` is a Csound [status code](#status-codes).

**<code>csound.Reset(<i>Csound</i>)</code>** frees resources after the end of a `Csound` performance (just like [`csound.Cleanup`](#Cleanup)) and prepares for a new performance.

---

### [Attributes](https://csound.github.io/docs/api/group__ATTRIBUTES.html)

<a name="GetSr"></a>**<code><i>sampleRate</i> = csound.GetSr(<i>Csound</i>)</code>** gets [`sr`](https://csound.github.io/docs/manual/sr.html), the `Csound` sample rate (also called the audio rate or a‑rate).

<a name="GetKr"></a>**<code><i>controlRate</i> = csound.GetKr(<i>Csound</i>)</code>** gets [`kr`](https://csound.github.io/docs/manual/kr.html), the `Csound` control rate (also called the k‑rate).

<a name="GetKsmps"></a>**<code><i>samplesPerControlPeriod</i> = csound.GetKsmps(<i>Csound</i>)</code>** gets [`ksmps`](https://csound.github.io/docs/manual/ksmps.html), the number of digital audio samples in one control period.

<a name="GetNchnls"></a>**<code><i>outputChannelCount</i> = csound.GetNchnls(<i>Csound</i>)</code>** gets [`nchnls`](https://csound.github.io/docs/manual/nchnls.html), the number of audio output channels.

<a name="GetNchnlsInput"></a>**<code><i>inputChannelCount</i> = csound.GetNchnlsInput(<i>Csound</i>)</code>** gets [`nchnls_i`](https://csound.github.io/docs/manual/nchnls_i.html), the number of audio input channels.

<a name="Get0dBFS"></a>**<code><i>fullScalePeakAmplitude</i> = csound.Get0dBFS(<i>Csound</i>)</code>** gets [`0dBFS`](https://csound.github.io/docs/manual/Zerodbfs.html), the maximum value of a sample of audio.

<a name="GetCurrentTimeSamples"></a>**<code><i>performedSampleCount</i> = csound.GetCurrentTimeSamples(<i>Csound</i>)</code>** gets the number of samples performed by `Csound`. You can call this function during a performance. For the elapsed time in seconds of a performance, divide `performedSampleCount` by the [sample rate](#GetSr), or use [`csound.GetScoreTime`](#GetScoreTime).

<a name="GetSizeOfMYFLT"></a>**<code><i>bytesPerFloat</i> = csound.GetSizeOfMYFLT()</code>** gets the number of bytes that Csound uses to represent floating-point numbers. When Csound is compiled to use double-precision samples, `bytesPerFloat` is 8. Otherwise, it’s 4.

<a name="GetHostData"></a>**<code><i>value</i> = csound.GetHostData(<i>Csound</i>)</code>** gets a `value` associated with a `Csound` object using [`csound.Create`](#Create) or [`csound.SetHostData`](#SetHostData).

<a name="SetHostData"></a>**<code>csound.SetHostData(<i>Csound</i>, <i>value</i>)</code>** associates a `value` with a `Csound` object; `value` can be an object, a function, a string, a number, a Boolean, `null`, or `undefined`. You can retrieve a `value` associated with a `Csound` object using [`csound.GetHostData`](#GetHostData).

<a name="SetOption"></a>**<code><i>status</i> = csound.SetOption(<i>Csound</i>, <i>commandLineArgumentString</i>)</code>** sets a `Csound` option as if `commandLineArgumentString` was input as a [command line argument](https://csound.github.io/docs/manual/CommandFlags.html). For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
```

sets up `Csound` to output audio through your computer’s speakers. The returned `status` is a Csound [status code](#status-codes).

<a name="GetDebug"></a>**<code><i>queuesDebugMessages</i> = csound.GetDebug(<i>Csound</i>)</code>** gets a Boolean indicating whether `Csound` adds debug messages to its message queue. Use [`csound.SetDebug`](#SetDebug) to set this value.

<a name="SetDebug"></a>**<code>csound.SetDebug(<i>Csound</i>, <i>queuesDebugMessages</i>)</code>** sets a Boolean indicating whether `Csound` adds debug messages to its message queue. Use [`csound.GetDebug`](#GetDebug) to get this value.

---

### [General Input/Output](https://csound.github.io/docs/api/group__FILEIO.html)

<a name="GetOutputName"></a>**<code><i>audioOutputName</i> = csound.GetOutputName(<i>Csound</i>)</code>** gets the name of the audio output—the value of the [`--output` command line flag](https://csound.github.io/docs/manual/CommandFlags.html#FlagsMinusLowerO). For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
console.log(csound.GetOutputName(Csound));
```

logs `dac`.

---

### [Score Handling](https://csound.github.io/docs/api/group__SCOREHANDLING.html)

<a name="ReadScore"></a>**<code><i>status</i> = csound.ReadScore(<i>Csound</i>, <i>scoreString</i>)</code>** compiles a string containing a Csound score, adding events and other structures to `Csound`. The returned `status` is a Csound [status code](#status-codes).

<a name="GetScoreTime"></a>**<code><i>elapsedTime</i> = csound.GetScoreTime(<i>Csound</i>)</code>** gets the elapsed time in seconds of a `Csound` performance. You can call this function during a performance. For the number of samples performed by `Csound`, multiply `elapsedTime` by the [sample rate](#GetSr), or use [`csound.GetCurrentTimeSamples`](#GetCurrentTimeSamples).

<a name="IsScorePending"></a>**<code><i>performsScoreEvents</i> = csound.IsScorePending(<i>Csound</i>)</code>** gets a Boolean indicating whether `Csound` performs events from a score in addition to realtime events. Use [`csound.SetScorePending`](#SetScorePending) to set this value.

<a name="SetScorePending"></a>**<code>csound.SetScorePending(<i>Csound</i>, <i>performsScoreEvents</i>)</code>** sets a Boolean indicating whether `Csound` performs events from a score in addition to realtime events. Use [`csound.IsScorePending`](#IsScorePending) to get this value.

<a name="GetScoreOffsetSeconds"></a>**<code><i>scoreEventStartTime</i> = csound.GetScoreOffsetSeconds(<i>Csound</i>)</code>** gets the amount of time subtracted from the start time of score events. Use [`csound.SetScoreOffsetSeconds`](#SetScoreOffsetSeconds) to set this time.

<a name="SetScoreOffsetSeconds"></a>**<code>csound.SetScoreOffsetSeconds(<i>Csound</i>, <i>scoreEventStartTime</i>)</code>** sets an amount of time to subtract from the start times of score events. For example,

```javascript
const csound = require('csound-api');
const Csound = csound.Create();
csound.SetOption(Csound, '--nosound');
csound.CompileOrc(Csound, `
  nchnls = 1
  sr = 44100
  0dbfs = 1
  ksmps = 32
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

prints `hello, world` immediately, not after a 5&nbsp;second delay. Use [`csound.GetScoreOffsetSeconds`](#GetScoreOffsetSeconds) to get this time.

<a name="RewindScore"></a>**<code>csound.RewindScore(<i>Csound</i>)</code>** restarts a compiled score at the time returned by [`csound.GetScoreOffsetSeconds`](#GetScoreOffsetSeconds).

---

### [Messages & Text](https://csound.github.io/docs/api/group__MESSAGES.html)

<a name="Message"></a>**<code>csound.Message(<i>Csound</i>, <i>string</i>)</code>** adds to the `Csound` message queue a message consisting of a `string`.

<a name="MessageS"></a>**<code>csound.MessageS(<i>Csound</i>, <i>attributes</i>, <i>string</i>)</code>** adds to the `Csound` message queue a message with `attributes` applied to a `string`. The value of `attributes` is a bit mask of

* a type specified by one of `csound.MSG_DEFAULT`, `csound.MSG_ERROR`, `csound.MSG_ORCH`, `csound.MSG_REALTIME`, or `csound.MSG_WARNING`;

* a text color specified by one of `csound.MSG_FG_BLACK`, `csound.MSG_FG_RED`, `csound.MSG_FG_GREEN`, `csound.MSG_FG_YELLOW`, `csound.MSG_FG_BLUE`, `csound.MSG_FG_MAGENTA`, `csound.MSG_FG_CYAN`, or `csound.MSG_FG_WHITE`;

* the bold specifier `csound.MSG_FG_BOLD`;

* the underline specifier `csound.MSG_FG_UNDERLINE`; and

* a background color specified by one of `csound.MSG_BG_BLACK`, `csound.MSG_BG_RED`, `csound.MSG_BG_GREEN`, `csound.MSG_BG_ORANGE`, `csound.MSG_BG_BLUE`, `csound.MSG_BG_MAGENTA`, `csound.MSG_BG_CYAN`, or `csound.MSG_BG_GREY`.

<a name="SetMessageCallback"></a>**<code>csound.SetMessageCallback(<i>Csound</i>, function(<i>attributes</i>, <i>string</i>))</code>** sets a function for `Csound` to call when it dequeues a message with `attributes` applied to a `string`. You can determine the type, text color, and background color of the `attributes` by performing a [bitwise AND](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_Operators#Bitwise_AND) with `csound.MSG_TYPE_MASK`, `csound.MSG_FG_COLOR_MASK`, and `csound.MSG_BG_COLOR_MASK` respectively. It’s up to you to decide how to apply `attributes` to the `string`. For example, you might use the [ansi-styles](https://www.npmjs.com/package/ansi-styles) package to [log styled strings to the console](examples/log-styled-message.js).

<a name="CreateMessageBuffer"></a>**<code>csound.CreateMessageBuffer(<i>Csound</i>[, <i>writesToStandardStreams</i>])</code>** prepares a message buffer for retrieving Csound messages using [`csound.GetMessageCnt`](#GetMessageCnt), [`csound.GetFirstMessage`](#GetFirstMessage), [`csound.GetFirstMessageAttr`](#GetFirstMessageAttr), and [`csound.PopFirstMessage`](#PopFirstMessage) instead of [`csound.SetMessageCallback`](#SetMessageCallback). You can retrieve messages from a buffer like this:

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

You can write `Csound` messages to [standard streams](https://en.wikipedia.org/wiki/Standard_streams) in addition to the message buffer by passing `true` as the second argument. You should call [`csound.DestroyMessageBuffer`](#DestroyMessageBuffer) when you’re finished with the message buffer.

<a name="GetFirstMessage"></a>**<code><i>string</i> = csound.GetFirstMessage(<i>Csound</i>)</code>** gets the `string` of the first message on a message buffer.

<a name="GetFirstMessageAttr"></a>**<code><i>attributes</i> = csound.GetFirstMessageAttr(<i>Csound</i>)</code>** gets the `attributes` of the first message on a message buffer. The value of `attributes` is a bit mask like the one passed to the function argument of [`csound.SetMessageCallback`](#SetMessageCallback).

<a name="PopFirstMessage"></a>**<code>csound.PopFirstMessage(<i>Csound</i>)</code>** removes the first message from a message buffer.

<a name="GetMessageCnt"></a>**<code><i>messageCount</i> = csound.GetMessageCnt(<i>Csound</i>)</code>** gets the number of messages on a message buffer.

<a name="DestroyMessageBuffer"></a>**<code>csound.DestroyMessageBuffer(<i>Csound</i>)</code>** frees resources used by a message buffer created using [`csound.CreateMessageBuffer`](#CreateMessageBuffer).

---

### [Channels, Control & Events](https://csound.github.io/docs/api/group__CONTROLEVENTS.html)

<a name="ListChannels"></a>**<code><i>channelCount</i> = csound.ListChannels(<i>Csound</i>, <i>array</i>)</code>** sets the contents of the `array` to objects describing communication channels available in `Csound`, returning the new length of the `array` or a negative [error code](#status-codes). When you’re finished with the `array`, you should pass it to [`csound.DeleteChannelList`](#DeleteChannelList). The objects added to the `array` have these read-only properties:

<dl>
<dt><code>name</code></dt><dd> is a string name of the channel. You can use this name with <a href="#GetControlChannel"><code>csound.GetControlChannel</code></a> and <a href="#SetControlChannel"><code>csound.SetControlChannel</code></a>, and the <a href="https://csound.github.io/docs/manual/chn.html"><code>chn_*</code></a>, <a href="https://csound.github.io/docs/manual/chnexport.html"><code>chnexport</code></a>, <a href="https://csound.github.io/docs/manual/chnget.html"><code>chnget</code></a>, and <a href="https://csound.github.io/docs/manual/chnset.html"><code>chnset</code></a> opcodes.</dd>

<dt><code>type</code></dt><dd> is a bit mask of
<ul>
<li>a channel type specified by one of <code>csound.CONTROL_CHANNEL</code>, <code>csound.AUDIO_CHANNEL</code>, <code>csound.STRING_CHANNEL</code>, or <code>csound.PVS_CHANNEL</code>;</li>
<li>the input specifier <code>csound.INPUT_CHANNEL</code>; and</li>
<li>the output specifier <code>csound.OUTPUT_CHANNEL</code>.</li>
</ul>
You can determine the channel type by performing a <a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Bitwise_Operators#Bitwise_AND">bitwise AND</a> with <code>csound.CHANNEL_TYPE_MASK</code>.</dd>
</dl>

<a name="DeleteChannelList"></a>**<code>csound.DeleteChannelList(<i>Csound</i>, <i>array</i>)</code>** frees resources associated with an `array` passed to [`csound.ListChannels`](#ListChannels).

<a name="GetControlChannel"></a>**<code><i>number</i> = csound.GetControlChannel(<i>Csound</i>, <i>name</i>[, <i>info</i>])</code>** gets the value of the control channel named `name`. If you pass an `info` object to this function, when this function returns the object will have a `status` property set to a Csound [status code](#status-codes).

<a name="SetControlChannel"></a>**<code>csound.SetControlChannel(<i>Csound</i>, <i>name</i>, <i>number</i>)</code>** sets the value of the control channel named `name` to a `number`.

<a name="ScoreEvent"></a>**<code><i>status</i> = csound.ScoreEvent(<i>Csound</i>, <i>eventType</i>[, <i>parameterFieldValues</i>])</code>** sends a score event to `Csound`. The `eventType` string can be [`'a'`](https://csound.github.io/docs/manual/a.html), [`'e'`](https://csound.github.io/docs/manual/a.html), [`'f'`](https://csound.github.io/docs/manual/f.html), [`'i'`](https://csound.github.io/docs/manual/i.html), or [`'q'`](https://csound.github.io/docs/manual/q.html); and `parameterFieldValues` is an optional array of _numeric_ parameters for the score event. Note that this means you cannot use `csound.ScoreEvent` to activate an instrument by name. The returned `status` is a Csound [status code](#status-codes).

<a name="InputMessage"></a>**<code>csound.InputMessage(<i>Csound</i>, <i>scoreStatement</i>)</code>** sends a [score statement](https://csound.github.io/docs/manual/ScoreStatements.html) string to `Csound`.

---

### [Tables](https://csound.github.io/docs/api/group__TABLE.html)

<a name="TableLength"></a>**<code><i>length</i> = csound.TableLength(<i>Csound</i>, <i>functionTableID</i>)</code>** gets the length of the function table with `functionTableID`. The `functionTableID` is parameter&nbsp;1 of a score [`f`&nbsp;statement](https://csound.github.io/docs/manual/f.html).

<a name="TableGet"></a>**<code><i>numberAtIndex</i> = csound.TableGet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>)</code>** gets the value of the function table with `functionTableID` at the specified `index`. The `index` must be less than the function table’s length.

<a name="TableSet"></a>**<code>csound.TableSet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>, <i>number</i>)</code>** sets the value at the specified `index` of the function table with `functionTableID` to `number`. The `index` must be less than the function table’s length.

---

### [Function Table Display](https://csound.github.io/docs/api/group__TABLEDISPLAY.html)

<a name="SetIsGraphable"></a>**<code><i>wasGraphable</i> = csound.SetIsGraphable(<i>Csound</i>, <i>isGraphable</i>)</code>** sets a Boolean indicating whether [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) and [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) are called, and returns the previous value. Note that you must set callback functions using both [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) and [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) for either callback function to be called.

<a name="SetMakeGraphCallback"></a>**<code>csound.SetMakeGraphCallback(<i>Csound</i>, function(<i>data</i>, <i>name</i>))</code>** sets a function for `Csound` to call when it first makes a graph of a function table or other data series. The function is passed a `data` object and the `name` of the graph as a string. Note that you must pass `true` to [`csound.SetIsGraphable`](#SetIsGraphable) and also set a callback function using [`csound.SetDrawGraphCallback`](#SetDrawGraphCallback) for this function to be called. The `data` object passed to the function has these read-only properties:

<dl>
<dt><code>windid</code></dt><dd> is an arbitrary numeric identifier for the graph.</dd>

<dt><code>caption</code></dt><dd> is a string describing the graph.</dd>

<!--<dt><code>polarity</code></dt><dd> is a string indicating the types of the opcode’s input arguments. Each character of the string corresponds to an input argument.</dd>-->

<dt><code>max</code></dt><dd> is the maximum of the <code>fdata</code> property.</dd>

<dt><code>min</code></dt><dd> is the minumum of the <code>fdata</code> property.</dd>

<dt><code>oabsmax</code></dt><dd> is a scale factor for the vertical axis.</dd>

<dt><code>fdata</code></dt><dd> is the data to be graphed as an array of numbers.</dd>
</dl>

<a name="SetDrawGraphCallback"></a>**<code>csound.SetDrawGraphCallback(<i>Csound</i>, function(<i>data</i>))</code>** sets a function for `Csound` to call when it draws a graph of a function table or other data series. The function is passed a `data` object with the same properties as the one passed to the function argument of [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback). Note that you must pass `true` to [`csound.SetIsGraphable`](#SetIsGraphable) and also set a callback function using [`csound.SetMakeGraphCallback`](#SetMakeGraphCallback) for this function to be called.

---

### [Opcodes](https://csound.github.io/docs/api/group__OPCODES.html)

<a name="NewOpcodeList"></a>**<code><i>opcodeCount</i> = csound.NewOpcodeList(<i>Csound</i>, <i>array</i>)</code>** sets the contents of the `array` to objects describing opcodes available in `Csound`, returning the new length of the `array` or a negative [error code](#status-codes). When you’re finished with the `array`, you should pass it to [`csound.DisposeOpcodeList`](#DisposeOpcodeList). The objects added to the `array` have these read-only properties:

<dl>
<dt><code>opname</code></dt><dd> is the opcode’s name.</dd>

<dt><code>outypes</code></dt><dd> is a string of characters that describe output arguments:
<table>
<tbody>
<tr><td><code>a</code></td><td>a‑rate vector</td></tr>
<tr><td><code>F</code></td><td>comma-separated list of frequency-domain variables, used by phase vocoder opcodes</td></tr>
<tr><td><code>m</code></td><td>comma-separated list of a‑rate vectors</td></tr>
<tr><td><code>N</code></td><td>comma-separated list of i‑time scalars, k‑rate scalars, a‑rate vectors, and strings</td></tr>
<tr><td><code>s</code></td><td>k‑rate scalar or a‑rate vector</td></tr>
<tr><td><code>X</code></td><td>comma-separated list of i‑time scalars, k‑rate scalars, and a‑rate vectors</td></tr>
<tr><td><code>z</code></td><td>comma-separated list of k‑rate scalars</td></tr>
<tr><td><code>*</code></td><td>comma-separated list of arguments of any type</td></tr>
</tbody>
</table></dd>

<dt><code>intypes</code></dt><dd> is a string of characters that describe input arguments:
<table>
<tbody>
<tr><td><code>a</code></td><td>a‑rate vector</td></tr>
<tr><td><code>B</code></td><td>Boolean</td></tr>
<tr><td><code>f</code></td><td>frequency-domain variable, used by phase vocoder opcodes</td></tr>
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
<tr><td><code>w</code></td><td>frequency-domain variable, used by <a href="https://csound.github.io/docs/manual/spectrum.html"><code>spectrum</code></a> and related opcodes</td></tr>
<tr><td><code>W</code></td><td>comma-separated list of strings</td></tr>
<tr><td><code>x</code></td><td>k‑rate scalar or a‑rate vector</td></tr>
<tr><td><code>y</code></td><td>comma-separated list of a‑rate vectors</td></tr>
<tr><td><code>z</code></td><td>comma-separated list of k‑rate scalars</td></tr>
<tr><td><code>Z</code></td><td>comma-separated list of alternating k‑rate scalars and a‑rate vectors</td></tr>
<tr><td><code>.</code></td><td>required argument of any type</td></tr>
<tr><td><code>?</code></td><td>optional argument of any type</td></tr>
<tr><td><code>*</code></td><td>comma-separated list of arguments of any type</td></tr>
</tbody>
</table></dd>
</dl>

<a name="DisposeOpcodeList"></a>**<code>csound.DisposeOpcodeList(<i>Csound</i>, <i>array</i>)</code>** frees resources associated with an `array` passed to [`csound.NewOpcodeList`](#NewOpcodeList).

---

### [Miscellaneous Functions](https://csound.github.io/docs/api/group__MISCELLANEOUS.html)

<a name="GetEnv"></a>**<code><i>environmentVariableValue</i> = csound.GetEnv(<i>Csound</i>, <i>environmentVariableName</i>)</code>** gets the string value of a [`Csound` environment variable](https://csound.github.io/docs/manual/CommandEnvironment.html) named `environmentVariableName`.

<a name="SetGlobalEnv"></a>**<code><i>status</i> = csound.SetGlobalEnv(<i>environmentVariableName</i>, <i>value</i>)</code>** sets the value of a [`Csound` environment variable](https://csound.github.io/docs/manual/CommandEnvironment.html) named `environmentVariableName` to string `value`.

---

### <a name="status-codes"></a>Status Codes

A number of csound-api functions return `csound.SUCCESS` upon successful completion, or one of these error codes:

* `csound.ERROR`
* `csound.INITIALIZATION`
* `csound.PERFORMANCE`
* `csound.MEMORY`
* `csound.SIGNAL`
