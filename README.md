# Csound API

This package is a [Node.js Addon](https://nodejs.org/api/addons.html) for using [Csound](https://en.wikipedia.org/wiki/Csound) through its C&nbsp;[API](https://csound.github.io/docs/api/index.html). The methods in this package try to match the functions in Csound’s API as closely as possible, and this package adds a `PerformAsync` method for running Csound in a background thread. If you `require` this package using
```javascript
var csound = require('csound-api');
```
then you can use Csound’s API as, for example,
```javascript
function messageCallback(attributes, string) {
  console.log(string);
};
var Csound = csound.Create();
csound.SetMessageCallback(Csound, messageCallback);
csound.Message(Csound, 'hello, world');
```
The equivalent in C would be something like
```c
static void messageCallback(CSOUND *Csound, int attributes, const char *format, va_list argumentList) {
  vprintf(format, argumentList);
}
CSOUND *Csound = csoundCreate(NULL);
csoundSetMessageCallback(Csound, messageCallback);
csoundMessage(Csound, "hello, world");
```

## Installing

Before you install this package, you’ll need [Boost](http://www.boost.org) and Csound.

### On OS&nbsp;X

The easiest way to install Boost is probably through [Homebrew](http://brew.sh). To install Homebrew, follow the instructions at [http://brew.sh](http://brew.sh). Then, run `brew install boost` in a Terminal.

If you aren’t able to build Csound from its [source code](https://github.com/csound/csound), the most reliable way to install Csound is probably to run the installer in the disk image you can download from [SourceForge](http://sourceforge.net/projects/csound/files/latest/download?source=files). (While Csound has a [tap](https://github.com/csound/homebrew-csound) on Homebrew, it does not install a necessary framework; this is a [known issue](https://github.com/csound/csound/blob/develop/BUILD.md#known-issues).) When you double-click the installer in the disk image, OS&nbsp;X may block the installer from running because it’s from an unidentified developer. To run the installer after this happens, open System Preferences, choose Security & Privacy, and click Open Anyway in the bottom half of the window.

After you install Boost and Csound, you can install this package using
```sh
npm install csound-api
```

## [Examples](https://github.com/nwhetsell/csound-api/tree/master/examples)

Play a 440&nbsp;Hz sine tone.

```javascript
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
var Csound = csound.Create();
csound.SetOption(Csound, '--output=dac');
csound.CompileOrc(Csound, [
  'nchnls = 1', 'sr = 44100', '0dbfs = 1', 'ksmps = 32',
  'instr SawtoothSweep',
    // This outputs a sawtooth wave with a fundamental frequency that starts at
    // 110 Hz, rises to 220 Hz over 1 second, and then falls back to 110 Hz over
    // 1 second. The score plays this instrument for 2 seconds, but the call to
    // setTimeout() stops Csound after 1 second, so only the rise is heard.
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
var Csound = csound.Create();
var opcodes = [];
csound.NewOpcodeList(Csound, opcodes);
console.log(opcodes);
csound.DisposeOpcodeList(Csound, opcodes);
csound.Destroy(Csound);
```

Log an abstract syntax tree parsed from an orchestra.

```javascript
var Csound = csound.Create();
var ASTRoot = csound.ParseOrc(Csound, [
  'nchnls = 1', 'sr = 44100', '0dbfs = 1', 'ksmps = 32',
  'giFunctionTableID ftgen 0, 0, 16384, 10, 1',
  'instr A440',
    'outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)',
  'endin'
].join('\n'));
console.log(ASTRoot);
csound.Destroy(Csound);
```

## Tests

Running the [tests](https://github.com/nwhetsell/csound-api/blob/master/spec/csound-api-spec.js) of this package requires [Jasmine](https://jasmine.github.io/edge/node.html). To install the Jasmine package on OS&nbsp;X globally, run `npm install -g jasmine` in a Terminal. To run the tests, `cd` to the `csound-api` folder (which should be in `node_modules` if you installed `csound-api` locally) and run `jasmine`.

## Contributing

[Open an issue](https://github.com/nwhetsell/csound-api/issues), or [fork this project and submit a pull request](https://guides.github.com/activities/forking/).

## API Coverage

Here are the methods you can use assuming you `require` this package as
```javascript
var csound = require('csound-api');
```

### Instantiation

* <code><i>Csound</i> = csound.Create([<i>value</i>])</code>
* <code>csound.Destroy(<i>Csound</i>)</code>
* <code><i>versionTimes1000</i> = csound.GetVersion()</code>
* <code><i>versionTimes100</i> = csound.GetAPIVersion()</code>

### Performance

* <code><i>ASTRoot</i> = csound.ParseOrc(<i>Csound</i>, <i>orchestraString</i>)</code>
* <code><i>status</i> = csound.CompileTree(<i>Csound</i>, <i>ASTRoot</i>)</code>
* <code>csound.DeleteTree(<i>Csound</i>, <i>ASTRoot</i>)</code>
* <code><i>status</i> = csound.CompileOrc(<i>Csound</i>, <i>orchestraString</i>)</code>
* <code><i>number</i> = csound.EvalCode(<i>Csound</i>, <i>string</i>)</code>
* <code><i>status</i> = csound.InitializeCscore(<i>Csound</i>, <i>inputFilePath</i>, <i>outputFilePath</i>)</code>
* <code><i>status</i> = csound.CompileArgs(<i>Csound</i>, <i>commandLineArguments</i>)</code>
* <code><i>status</i> = csound.Start(<i>Csound</i>)</code>
* <code><i>status</i> = csound.Compile(<i>Csound</i>, <i>commandLineArguments</i>)</code>
* <code><i>status</i> = csound.CompileCsd(<i>Csound</i>, <i>filePath</i>)</code>
* <code><i>result</i> = csound.Perform(<i>Csound</i>)</code>
* <code>csound.PerformAsync(<i>Csound</i>, function(<i>result</i>))</code><br>
* <code>csound.Stop(<i>Csound</i>)</code>
* <code><i>status</i> = csound.Cleanup(<i>Csound</i>)</code>
* <code>csound.Reset(<i>Csound</i>)</code>

### Attributes

* <code><i>sampleRate</i> = csound.GetSr(<i>Csound</i>)</code>
* <code><i>controlRate</i> = csound.GetKr(<i>Csound</i>)</code>
* <code><i>samplesPerControlPeriod</i> = csound.GetKsmps(<i>Csound</i>)</code>
* <code><i>outputChannelCount</i> = csound.GetNchnls(<i>Csound</i>)</code>
* <code><i>inputChannelCount</i> = csound.GetNchnlsInput(<i>Csound</i>)</code>
* <code><i>fullScalePeakAmplitude</i> = csound.Get0dBFS(<i>Csound</i>)</code>
* <code><i>performedSampleCount</i> = csound.GetCurrentTimeSamples(<i>Csound</i>)</code>
* <code><i>bytesPerFloat</i> = csound.GetSizeOfMYFLT()</code>
* <code><i>value</i> = csound.GetHostData(<i>Csound</i>)</code>
* <code>csound.SetHostData(<i>Csound</i>, <i>value</i>)</code>
* <code><i>status</i> = csound.SetOption(<i>Csound</i>, <i>commandLineOptionString</i>)</code>
* <code><i>printsDebugMessages</i> = csound.GetDebug(<i>Csound</i>)</code>
* <code>csound.SetDebug(<i>Csound</i>, <i>printsDebugMessages</i>)</code>

### General Input/Output

* <code><i>audioOutputName</i> = csound.GetOutputName(<i>Csound</i>)</code>

### Score Handling

* <code><i>status</i> = csound.ReadScore(<i>Csound</i>, <i>scoreString</i>)</code>
* <code><i>elapsedTime</i> = csound.GetScoreTime(<i>Csound</i>)</code>
* <code><i>performsScoreEvents</i> = csound.IsScorePending(<i>Csound</i>)</code>
* <code>csound.SetScorePending(<i>Csound</i>, <i>performsScoreEvents</i>)</code>
* <code><i>scoreEventStartTime</i> = csound.GetScoreOffsetSeconds(<i>Csound</i>)</code>
* <code>csound.SetScoreOffsetSeconds(<i>Csound</i>, <i>scoreEventStartTime</i>)</code>
* <code>csound.RewindScore(<i>Csound</i>)</code>

### Messages & Text

* <code>csound.Message(<i>Csound</i>, <i>string</i>)</code>
* <code>csound.SetMessageCallback(<i>Csound</i>, function(<i>attributes</i>, <i>string</i>))</code>
* <code><i>messageLevel</i> = csound.GetMessageLevel(<i>Csound</i>)</code>
* <code>csound.SetMessageLevel(<i>Csound</i>, <i>messageLevel</i>)</code>

### Channels, Control & Events

* <code><i>number</i> = csound.GetControlChannel(<i>Csound</i>, <i>name</i>[, <i>info</i>])</code>
* <code>csound.SetControlChannel(<i>Csound</i>, <i>name</i>, <i>number</i>)</code>
* <code><i>status</i> = csound.ScoreEvent(<i>Csound</i>, <i>eventType</i>[, <i>parameterFieldValues</i>])</code>

### Tables

* <code><i>functionTableLength</i> = csound.TableLength(<i>Csound</i>, <i>functionTableID</i>)</code>
* <code><i>number</i> = csound.TableGet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>)</code>
* <code>csound.TableSet(<i>Csound</i>, <i>functionTableID</i>, <i>index</i>, <i>number</i>)</code>

### Function Table Display

* <code><i>wasGraphable</i> = csound.SetIsGraphable(<i>Csound</i>, <i>isGraphable</i>)</code>
* <code>csound.SetMakeGraphCallback(<i>Csound</i>, function(<i>data</i>, <i>name</i>))</code>
* <code>csound.SetDrawGraphCallback(<i>Csound</i>, function(<i>data</i>))</code>

### Opcodes

* <code><i>opcodeCount</i> = csound.NewOpcodeList(<i>Csound</i>, <i>array</i>)</code>
* <code>csound.DisposeOpcodeList(<i>Csound</i>, <i>array</i>)</code>

### Miscellaneous Functions

* <code><i>environmentVariableValue</i> = csound.GetEnv(<i>Csound</i>, <i>environmentVariableName</i>)</code>
* <code><i>status</i> = csound.SetGlobalEnv(<i>environmentVariableName</i>, <i>value</i>)</code>
