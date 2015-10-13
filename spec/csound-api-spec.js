var path = require('path');
var csound = require(path.join('..', 'build', 'Release', 'csound-api.node'));
var fs = require('fs');

describe('Csound API', function() {
  it('is defined', function() {
    expect(csound).toBeDefined();
  });

  it('gets status codes', function() {
    // From https://github.com/csound/csound/blob/develop/include/csound.h#L223
    expect(csound.CSOUND_SUCCESS).toBe(0);
    expect(csound.CSOUND_ERROR).toBe(-1);
    expect(csound.CSOUND_INITIALIZATION).toBe(-2);
    expect(csound.CSOUND_PERFORMANCE).toBe(-3);
    expect(csound.CSOUND_MEMORY).toBe(-4);
    expect(csound.CSOUND_SIGNAL).toBe(-5);
  });

  it('creates and destroys instance', function() {
    var Csound = csound.Create();
    expect(typeof Csound).toBe('object');
    expect(function() { csound.Destroy(Csound); }).not.toThrow();
  });

  describe('creates instance with host data that', function() {
    var Csound;
    afterEach(function() {
      csound.Destroy(Csound);
    });

    it('is undefined', function() {
      Csound = csound.Create();
      expect(csound.GetHostData(Csound)).toBeUndefined();
    });

    it('is null', function() {
      Csound = csound.Create(null);
      expect(csound.GetHostData(Csound)).toBeNull();
    });

    it('is a Boolean', function() {
      Csound = csound.Create(true);
      expect(csound.GetHostData(Csound)).toBe(true);
    });

    it('is a number', function() {
      Csound = csound.Create(42);
      expect(csound.GetHostData(Csound)).toBe(42);
    });

    it('is a string', function() {
      Csound = csound.Create('hello, world');
      expect(csound.GetHostData(Csound)).toBe('hello, world');
    });

    it('is a function', function() {
      function hostDataFunction() {}
      Csound = csound.Create(hostDataFunction);
      expect(csound.GetHostData(Csound)).toBe(hostDataFunction);
    });

    it('is an object', function() {
      var object = {};
      Csound = csound.Create(object);
      expect(csound.GetHostData(Csound)).toBe(object);
    });
  });

  it('returns version numbers', function() {
    expect(typeof csound.GetVersion()).toBe('number');
    expect(typeof csound.GetAPIVersion()).toBe('number');
  });
});

describe('Csound instance', function() {
  var outputChannelCount = 1;
  var sampleRate = 44100;
  var fullScalePeakAmplitude = 1;
  var samplesPerControlPeriod = 10;
  var orchestraHeader = [
    'nchnls = ' + outputChannelCount,
    'sr = ' + sampleRate,
    '0dbfs = ' + fullScalePeakAmplitude,
    'ksmps = ' + samplesPerControlPeriod
  ].join('\n');
  var Csound;
  beforeEach(function() {
    Csound = csound.Create();
  });
  afterEach(function() {
    csound.Destroy(Csound);
  });

  it('parses and deletes abstract syntax tree', function() {
    var ASTRoot = csound.ParseOrc(Csound, orchestraHeader);
    expect(ASTRoot).not.toBeNull();
    expect(ASTRoot.next).not.toBeNull();
    expect(ASTRoot.next.value).not.toBeNull();
    expect(function() { csound.DeleteTree(Csound, ASTRoot); }).not.toThrow();
    expect(csound.ParseOrc(Csound, '')).toBeNull();
  });

  it('compiles abstract syntax tree', function() {
    var ASTRoot = csound.ParseOrc(Csound, orchestraHeader);
    expect(csound.CompileTree(Csound, ASTRoot)).toBe(csound.CSOUND_SUCCESS);
    expect(function() { csound.DeleteTree(Csound, ASTRoot); }).not.toThrow();
  });

  it('compiles orchestra string', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, '')).toBe(csound.CSOUND_ERROR);
  });

  it('evaluates code with return opcode', function() {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.EvalCode(Csound, [
      'i1 = 40 + 2',
      'return i1'
    ].join('\n'))).toBe(42);
  });

  it('initializes Cscore', function() {
    var inputFilePath = path.join(__dirname, 'input.sco');
    var outputFilePath = path.join(__dirname, 'output.sco');
    var inputFile = fs.openSync(inputFilePath, 'w');
    fs.writeSync(inputFile, [
      'i 1 0 1',
      'e'
    ].join('\n'));
    fs.closeSync(inputFile);
    expect(csound.InitializeCscore(Csound, inputFilePath, outputFilePath)).toBe(csound.CSOUND_SUCCESS);
    expect(fs.statSync(outputFilePath).isFile()).toBe(true);
    fs.unlinkSync(inputFilePath);
    fs.unlinkSync(outputFilePath);
  });

  // csoundCompile() simply calls csoundCompileArgs() and then csoundStart()
  // <https://github.com/csound/csound/blob/develop/Top/main.c#L486>.
  it('compiles command-line arguments', function() {
    var orchestraPath = path.join(__dirname, 'orchestra.orc');
    var scorePath = path.join(__dirname, 'score.sco');
    var file = fs.openSync(orchestraPath, 'w');
    fs.writeSync(file, [
      orchestraHeader,
      'instr 1',
        'prints "hello, world"',
      'endin'
    ].join('\n'));
    fs.closeSync(file);
    file = fs.openSync(scorePath, 'w');
    fs.writeSync(file, [
      'i 1 0 1',
      'e'
    ].join('\n'));
    fs.closeSync(file);
    expect(csound.CompileArgs(Csound, ['csound', '--nosound', orchestraPath, scorePath])).toBe(csound.CSOUND_SUCCESS);
    fs.unlinkSync(orchestraPath);
    fs.unlinkSync(scorePath);
  });

  it('compiles Csound document (CSD)', function() {
    var CSDPath = path.join(__dirname, 'document.csd');
    var file = fs.openSync(CSDPath, 'w');
    fs.writeSync(file, [
      '<CsoundSynthesizer>',
      '<CsOptions>',
      '--nosound',
      '</CsOptions>',
      '<CsInstruments>',
      orchestraHeader,
      'instr 1',
        'prints "hello, world"',
      'endin',
      '</CsInstruments>',
      '<CsScore>',
      'i 1 0 1',
      'e',
      '</CsScore>',
      '</CsoundSynthesizer>'
    ].join('\n'));
    fs.closeSync(file);
    expect(csound.CompileCsd(Csound, CSDPath)).toBe(csound.CSOUND_SUCCESS);
    fs.unlinkSync(CSDPath);
  });

  it('performs', function() {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, [
      orchestraHeader,
      'instr 1',
        'out oscil(0.1 * 0dbfs, 440)',
      'endin'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.ReadScore(Csound, [
      'i 1 0 1',
      'e'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
  });

  it('performs live', function() {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, [
      orchestraHeader,
      'instr 1',
        'out vco2(linseg(0.1 * 0dbfs, 0.5, 0), cpspch(p4), 2, 0.5)',
      'endin'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.ReadScore(Csound, [
      'i 1 0 0.07  9.11',
      'i . + 0.5  10.04',
      'e'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
  });

  it('performs asynchronously', function(done) {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, [
      orchestraHeader,
      'instr 1',
        'out vco2(linseg(0.1 * 0dbfs, 0.5, 0), cpspch(p4), 2, 0.5)',
      'endin'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.ReadScore(Csound, [
      'i 1 0  0.07  9.11',
      'i . + 10    10.04',
      'e'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    csound.PerformAsync(Csound, function(result) {
      expect(result).toBe(0);
      done();
    });
    setTimeout(function() {
      csound.Stop(Csound);
    }, 600);
  });

  it('gets sample rate (sr)', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetSr(Csound)).toBe(sampleRate);
  });

  it('gets control rate (kr)', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetKr(Csound)).toBe(sampleRate / samplesPerControlPeriod);
  });

  it('gets samples per control period (ksmps)', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetKsmps(Csound)).toBe(samplesPerControlPeriod);
  });

  it('gets number of output channels (nchnls)', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetNchnls(Csound)).toBe(outputChannelCount);
  });

  it('gets number of input channels (nchnls_i)', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetNchnlsInput(Csound)).toBe(1);
  });

  it('gets full-scale peak amplitude (0dbfs)', function() {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Get0dBFS(Csound)).toBe(fullScalePeakAmplitude);
  });

  describe('sets host data that', function() {
    it('is undefined', function() {
      csound.SetHostData(Csound, undefined);
      expect(csound.GetHostData(Csound)).toBeUndefined();
    });

    it('is null', function() {
      csound.SetHostData(Csound, null);
      expect(csound.GetHostData(Csound)).toBeNull();
    });

    it('is a Boolean', function() {
      csound.SetHostData(Csound, true);
      expect(csound.GetHostData(Csound)).toBe(true);
    });

    it('is a number', function() {
      csound.SetHostData(Csound, 42);
      expect(csound.GetHostData(Csound)).toBe(42);
    });

    it('is a string', function() {
      csound.SetHostData(Csound, 'hello, world');
      expect(csound.GetHostData(Csound)).toBe('hello, world');
    });

    it('is a function', function() {
      function hostDataFunction() {}
      csound.SetHostData(Csound, hostDataFunction);
      expect(csound.GetHostData(Csound)).toBe(hostDataFunction);
    });

    it('is an object', function() {
      var object = {};
      csound.SetHostData(Csound, object);
      expect(csound.GetHostData(Csound)).toBe(object);
    });
  });

  it('sets and gets whether debug messages print', function() {
    expect(csound.GetDebug(Csound)).toBeFalsy();
    csound.SetDebug(Csound, true);
    expect(csound.GetDebug(Csound)).toBeTruthy();
  });

  it('gets audio output name', function() {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetOutputName(Csound)).toBe('dac');
  });

  it('sets and gets message level', function() {
    var level = csound.GetMessageLevel(Csound);
    // 231 is the maximum message level according to
    // https://github.com/csound/csound/blob/develop/include/csound.h#L1307
    if (level < 231) {
      level++;
    } else {
      level--;
    }
    csound.SetMessageLevel(Csound, level);
    expect(csound.GetMessageLevel(Csound)).toBe(level);
  });

  it('sets message callback', function(done) {
    csound.SetMessageCallback(Csound, function(attributes, string) {
      done();
    });
    csound.Message(Csound, 'hello, world\n');
  });

  it('sets and gets control channel value', function() {
    var controlChannelName = 'test';
    csound.SetControlChannel(Csound, controlChannelName, 42);
    var info = {};
    expect(csound.GetControlChannel(Csound, controlChannelName, info)).toBe(42);
    expect(info.status).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetControlChannel(Csound, controlChannelName)).toBe(42);
  });

  it('receives score events', function(done) {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    csound.PerformAsync(Csound, function(result) {
      expect(result).toBeGreaterThan(0);
      done();
    });
    expect(csound.ScoreEvent(Csound, 'e')).toBe(csound.CSOUND_SUCCESS);
  });

  it('receives score statement messages', function(done) {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    csound.PerformAsync(Csound, function(result) {
      expect(result).toBeGreaterThan(0);
      done();
    });
    csound.InputMessage(Csound, 'e');
  });

  it('gets and sets function table values', function(done) {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.ReadScore(Csound, [
      'f 1 0 1024 10 1',
      'e'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    csound.PerformAsync(Csound, function(result) {
      expect(result).toBeGreaterThan(0);
      expect(csound.TableLength(Csound, 1)).toBe(1024);
      var index = 1024 / 4;
      expect(csound.TableGet(Csound, 1, index)).toBe(1);
      csound.TableSet(Csound, 1, index, 0);
      expect(csound.TableGet(Csound, 1, index)).toBe(0);
      done();
    });
  });

  it('sets graph callbacks', function(done) {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.SetIsGraphable(Csound, true)).toBeFalsy();
    expect(csound.SetIsGraphable(Csound, true)).toBeTruthy();
    csound.SetMakeGraphCallback(Csound, function(data, name) {});
    csound.SetDrawGraphCallback(Csound, function(data) {
      done();
    });
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.ReadScore(Csound, [
      'f 1 0 16384 10 1',
      'e'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
  });

  it('populates and deletes opcode list', function() {
    var opcodeList = [];
    expect(opcodeList.length).toBe(0);
    var opcodeListLength = csound.NewOpcodeList(Csound, opcodeList);
    expect(opcodeListLength).toBeGreaterThan(0);
    expect(opcodeList.length).toBe(opcodeListLength);
    csound.DisposeOpcodeList(Csound, opcodeList);
    expect(opcodeList.length).toBe(0);
  });

  it('gets environment variable value', function() {
    expect(csound.SetGlobalEnv('SFDIR', __dirname)).toBe(csound.CSOUND_SUCCESS);
    expect(csound.GetEnv(csound.Create(), 'SFDIR')).toBe(__dirname);
  });

  it('gets utility names and descriptions', function() {
    var utilityNames = csound.ListUtilities(Csound);
    expect(utilityNames.length).toBeGreaterThan(0);
    for (var i = 0; i < utilityNames.length; i++) {
      expect(csound.GetUtilityDescription(Csound, utilityNames[i]).length).toBeGreaterThan(0);
    }
    csound.DeleteUtilityList(Csound, utilityNames);
    expect(utilityNames.length).toBe(0);
    expect(csound.GetUtilityDescription(Csound, '')).toBeNull();
  });

  // The debugger tests are based on the tests in
  // <https://github.com/csound/csound/blob/develop/tests/c/csound_debugger_test.c>

  it('stops at breakpoint', function(done) {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, [
      orchestraHeader,
      'instr 1',
        'aSignal oscil 1, p4',
      'endin'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    csound.InputMessage(Csound, 'i 1.1 0 1 440');
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    csound.DebuggerInit(Csound);
    csound.SetBreakpointCallback(Csound, function(breakpointInfo) {
      expect(breakpointInfo).not.toBeNull();
      done();
    });
    csound.SetInstrumentBreakpoint(Csound, 1.1);
    csound.PerformKsmps(Csound);
    csound.DebuggerClean(Csound);
  });

  it('gets variables when stopped at breakpoint', function(done) {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.CSOUND_SUCCESS);
    expect(csound.CompileOrc(Csound, [
      orchestraHeader,
      'instr 1',
        'iVariable init 40.2',
        'kVariable init 0.7',
        'aSignal init 1.1',
        'Sstring init "hello, world"',
      'endin'
    ].join('\n'))).toBe(csound.CSOUND_SUCCESS);
    csound.InputMessage(Csound, 'i 1 0 1');
    expect(csound.Start(Csound)).toBe(csound.CSOUND_SUCCESS);
    csound.DebuggerInit(Csound);
    csound.SetBreakpointCallback(Csound, function(breakpointInfo) {
      var instrumentVariable = breakpointInfo.instrVarList;
      expect(instrumentVariable).not.toBeNull();
      expect(instrumentVariable.name).toBe('iVariable');
      expect(instrumentVariable.typeName).toBe('i');
      expect(instrumentVariable.data).toBe(40.2);
      instrumentVariable = instrumentVariable.next;
      expect(instrumentVariable.name).toBe('kVariable');
      expect(instrumentVariable.typeName).toBe('k');
      expect(instrumentVariable.data).toBe(0.7);
      instrumentVariable = instrumentVariable.next;
      expect(instrumentVariable.name).toBe('aSignal');
      expect(instrumentVariable.typeName).toBe('a');
      expect(instrumentVariable.data).toBe(1.1);
      instrumentVariable = instrumentVariable.next;
      expect(instrumentVariable.name).toBe('Sstring');
      expect(instrumentVariable.typeName).toBe('S');
      expect(instrumentVariable.data).toBe('hello, world');
      done();
    });
    csound.SetInstrumentBreakpoint(Csound, 1);
    csound.PerformKsmps(Csound);
    csound.DebuggerClean(Csound);
  });
});
