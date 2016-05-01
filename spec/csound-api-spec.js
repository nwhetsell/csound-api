var path = require('path');
var csound = require(path.join('..', 'build', 'Release', 'csound-api.node'));
var fs = require('fs');

describe('Csound API', () => {
  it('is defined', () => {
    expect(csound).toBeDefined();
  });

  it('gets status codes', () => {
    // From https://github.com/csound/csound/blob/develop/include/csound.h#L223
    expect(csound.SUCCESS).toBe(0);
    expect(csound.ERROR).toBe(-1);
    expect(csound.INITIALIZATION).toBe(-2);
    expect(csound.PERFORMANCE).toBe(-3);
    expect(csound.MEMORY).toBe(-4);
    expect(csound.SIGNAL).toBe(-5);
  });

  it('gets message attribute codes', () => {
    // From https://github.com/csound/csound/blob/develop/include/msg_attr.h
    expect(csound.MSG_DEFAULT).toBe(0x0000);
    expect(csound.MSG_ERROR).toBe(0x1000);
    expect(csound.MSG_ORCH).toBe(0x2000);
    expect(csound.MSG_REALTIME).toBe(0x3000);
    expect(csound.MSG_WARNING).toBe(0x4000);

    expect(csound.MSG_FG_BLACK).toBe(0x0100);
    expect(csound.MSG_FG_RED).toBe(0x0101);
    expect(csound.MSG_FG_GREEN).toBe(0x0102);
    expect(csound.MSG_FG_YELLOW).toBe(0x0103);
    expect(csound.MSG_FG_BLUE).toBe(0x0104);
    expect(csound.MSG_FG_MAGENTA).toBe(0x0105);
    expect(csound.MSG_FG_CYAN).toBe(0x0106);
    expect(csound.MSG_FG_WHITE).toBe(0x0107);

    expect(csound.MSG_FG_BOLD).toBe(0x0008);
    expect(csound.MSG_FG_UNDERLINE).toBe(0x0080);

    expect(csound.MSG_BG_BLACK).toBe(0x0200);
    expect(csound.MSG_BG_RED).toBe(0x0210);
    expect(csound.MSG_BG_GREEN).toBe(0x0220);
    expect(csound.MSG_BG_ORANGE).toBe(0x0230);
    expect(csound.MSG_BG_BLUE).toBe(0x0240);
    expect(csound.MSG_BG_MAGENTA).toBe(0x0250);
    expect(csound.MSG_BG_CYAN).toBe(0x0260);
    expect(csound.MSG_BG_GREY).toBe(0x0270);

    expect(csound.MSG_TYPE_MASK).toBe(0x7000);
    expect(csound.MSG_FG_COLOR_MASK).toBe(0x0107);
    expect(csound.MSG_FG_ATTR_MASK).toBe(0x0088);
    expect(csound.MSG_BG_COLOR_MASK).toBe(0x0270);
  });

  it('gets channel type and mode', () => {
    // From https://github.com/csound/csound/blob/develop/include/csound.h#L510
    expect(csound.CONTROL_CHANNEL).toBe(1);
    expect(csound.AUDIO_CHANNEL).toBe(2);
    expect(csound.STRING_CHANNEL).toBe(3);
    expect(csound.PVS_CHANNEL).toBe(4);
    expect(csound.CHANNEL_TYPE_MASK).toBe(15);
    expect(csound.INPUT_CHANNEL).toBe(16);
    expect(csound.OUTPUT_CHANNEL).toBe(32);
  });

  it('gets channel behavior', () => {
    // From https://github.com/csound/csound/blob/develop/include/csound.h#L523
    expect(csound.CONTROL_CHANNEL_NO_HINTS).toBe(0);
    expect(csound.CONTROL_CHANNEL_INT).toBe(1);
    expect(csound.CONTROL_CHANNEL_LIN).toBe(2);
    expect(csound.CONTROL_CHANNEL_EXP).toBe(3);
  });

  it('creates and destroys instance', () => {
    var Csound = csound.Create();
    expect(typeof Csound).toBe('object');
    expect(() => csound.Destroy(Csound)).not.toThrow();
  });

  describe('creates instance with host data that', () => {
    var Csound;
    afterEach(() => csound.Destroy(Csound));

    it('is undefined', () => {
      Csound = csound.Create();
      expect(csound.GetHostData(Csound)).toBeUndefined();
    });

    it('is null', () => {
      Csound = csound.Create(null);
      expect(csound.GetHostData(Csound)).toBeNull();
    });

    it('is a Boolean', () => {
      Csound = csound.Create(true);
      expect(csound.GetHostData(Csound)).toBe(true);
    });

    it('is a number', () => {
      Csound = csound.Create(42);
      expect(csound.GetHostData(Csound)).toBe(42);
    });

    it('is a string', () => {
      Csound = csound.Create('hello, world');
      expect(csound.GetHostData(Csound)).toBe('hello, world');
    });

    it('is a function', () => {
      function hostDataFunction() {}
      Csound = csound.Create(hostDataFunction);
      expect(csound.GetHostData(Csound)).toBe(hostDataFunction);
    });

    it('is an object', () => {
      var object = {};
      Csound = csound.Create(object);
      expect(csound.GetHostData(Csound)).toBe(object);
    });
  });

  it('returns version numbers', () => {
    expect(typeof csound.GetVersion()).toBe('number');
    expect(typeof csound.GetAPIVersion()).toBe('number');
  });

  it('creates control channel hints', () => {
    var hints = new csound.ChannelHints();
    expect(typeof hints).toBe('object');

    expect(hints.behav).toBe(csound.CONTROL_CHANNEL_NO_HINTS);
    expect(hints.dflt).toBe(0);
    expect(hints.min).toBe(0);
    expect(hints.max).toBe(0);
    expect(hints.x).toBe(0);
    expect(hints.y).toBe(0);
    expect(hints.width).toBe(0);
    expect(hints.height).toBe(0);
    expect(hints.attributes).toBeNull();

    hints.behav = csound.CONTROL_CHANNEL_INT;
    hints.dflt = 5;
    hints.min = 1;
    hints.max = 10;
    hints.x = 2;
    hints.y = 3;
    hints.width = 4;
    hints.height = 6;
    hints.attributes = "attributes";

    expect(hints.behav).toBe(csound.CONTROL_CHANNEL_INT);
    expect(hints.dflt).toBe(5);
    expect(hints.min).toBe(1);
    expect(hints.max).toBe(10);
    expect(hints.x).toBe(2);
    expect(hints.y).toBe(3);
    expect(hints.width).toBe(4);
    expect(hints.height).toBe(6);
    expect(hints.attributes).toBe("attributes");
  });
});

describe('Csound instance', () => {
  var outputChannelCount = 1;
  var sampleRate = 44100;
  var fullScalePeakAmplitude = 1;
  var samplesPerControlPeriod = 10;
  var orchestraHeader = `
    nchnls = ${outputChannelCount}
    sr = ${sampleRate}
    0dbfs = ${fullScalePeakAmplitude}
    ksmps = ${samplesPerControlPeriod}
  `;
  var Csound;
  beforeEach(() => Csound = csound.Create());
  afterEach(() => csound.Destroy(Csound));

  it('parses and deletes abstract syntax tree', () => {
    var ASTRoot = csound.ParseOrc(Csound, orchestraHeader);
    expect(ASTRoot).not.toBeNull();
    expect(ASTRoot.next).not.toBeNull();
    expect(ASTRoot.next.value).not.toBeNull();
    expect(() => csound.DeleteTree(Csound, ASTRoot)).not.toThrow();
    expect(csound.ParseOrc(Csound, '')).toBeNull();
  });

  it('compiles abstract syntax tree', () => {
    var ASTRoot = csound.ParseOrc(Csound, orchestraHeader);
    expect(csound.CompileTree(Csound, ASTRoot)).toBe(csound.SUCCESS);
    expect(() => csound.DeleteTree(Csound, ASTRoot)).not.toThrow();
  });

  it('compiles orchestra string', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, '')).toBe(csound.ERROR);
  });

  it('evaluates code with return opcode', () => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    expect(csound.EvalCode(Csound, `
      iResult = 19 + 23
      return iResult
    `)).toBe(42);
  });

  if (process.platform !== 'win32') {
    it('initializes Cscore', () => {
      var inputFilePath = path.join(__dirname, 'input.sco');
      var outputFilePath = path.join(__dirname, 'output.sco');
      var inputFile = fs.openSync(inputFilePath, 'w');
      fs.writeSync(inputFile, `
        i 1 0 1
        e
      `);
      fs.closeSync(inputFile);
      expect(csound.InitializeCscore(Csound, inputFilePath, outputFilePath)).toBe(csound.SUCCESS);
      expect(fs.statSync(outputFilePath).isFile()).toBe(true);
      fs.unlinkSync(inputFilePath);
      fs.unlinkSync(outputFilePath);
    });
  } else {
    it('does not initialize Cscore on Windows', () => {
      expect(csound.InitializeCscore).toBeUndefined();
    });
  }

  // csoundCompile simply calls csoundCompileArgs and then csoundStart; see
  // <https://github.com/csound/csound/blob/develop/Top/main.c#L486>.
  it('compiles command-line arguments', () => {
    var orchestraPath = path.join(__dirname, 'orchestra.orc');
    var scorePath = path.join(__dirname, 'score.sco');
    var file = fs.openSync(orchestraPath, 'w');
    fs.writeSync(file, `
      ${orchestraHeader}
      instr 1
        prints "hello, world"
      endin
    `);
    fs.closeSync(file);
    file = fs.openSync(scorePath, 'w');
    fs.writeSync(file, `
      i 1 0 1
      e
    `);
    fs.closeSync(file);
    expect(csound.CompileArgs(Csound, ['csound', '--nosound', orchestraPath, scorePath])).toBe(csound.SUCCESS);
    fs.unlinkSync(orchestraPath);
    fs.unlinkSync(scorePath);
  });

  it('compiles Csound document (CSD)', () => {
    var CSDPath = path.join(__dirname, 'document.csd');
    var file = fs.openSync(CSDPath, 'w');
    fs.writeSync(file, `
      <CsoundSynthesizer>
      <CsOptions>
      --nosound
      </CsOptions>
      <CsInstruments>
      ${orchestraHeader}
      instr 1
        prints "hello, world"
      endin
      </CsInstruments>
      <CsScore>
      i 1 0 1
      e
      </CsScore>
      </CsoundSynthesizer>
    `);
    fs.closeSync(file);
    expect(csound.CompileCsd(Csound, CSDPath)).toBe(csound.SUCCESS);
    fs.unlinkSync(CSDPath);
  });

  it('performs', () => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, `
      ${orchestraHeader}
      instr 1
        out oscil(0.1 * 0dbfs, 440)
      endin
    `)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      i 1 0 1
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
  });

  it('performs a control period', () => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, `
      ${orchestraHeader}
      instr 1
        out oscil(0.1 * 0dbfs, 440)
      endin
    `)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      i 1 0 1
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    var isFinished = csound.PerformKsmps(Csound);
    while (isFinished === false) {
      isFinished = csound.PerformKsmps(Csound);
    }
    expect(isFinished).toBe(true);
  });

  it('performs live', () => {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, `
      ${orchestraHeader}
      instr 1
        out vco2(linseg(0.1 * 0dbfs, 0.5, 0), cpspch(p4), 2, 0.5)
      endin
    `)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      i 1 0 0.07  9.11
      i . + 0.5  10.04
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
  });

  it('performs asynchronously', done => {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, `
      ${orchestraHeader}
      instr 1
        out vco2(linseg(0.1 * 0dbfs, 0.5, 0), cpspch(p4), 2, 0.5)
      endin
    `)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      i 1 0  0.07  9.11
      i . + 10    10.04
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    csound.PerformAsync(Csound, result => {
      expect(result).toBe(0);
      done();
    });
    setTimeout(() => csound.Stop(Csound), 600);
  });

  it('gets sample rate (sr)', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.GetSr(Csound)).toBe(sampleRate);
  });

  it('gets control rate (kr)', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.GetKr(Csound)).toBe(sampleRate / samplesPerControlPeriod);
  });

  it('gets samples per control period (ksmps)', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.GetKsmps(Csound)).toBe(samplesPerControlPeriod);
  });

  it('gets number of output channels (nchnls)', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.GetNchnls(Csound)).toBe(outputChannelCount);
  });

  it('gets number of input channels (nchnls_i)', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.GetNchnlsInput(Csound)).toBe(1);
  });

  it('gets full-scale peak amplitude (0dbfs)', () => {
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.Get0dBFS(Csound)).toBe(fullScalePeakAmplitude);
  });

  it('gets performed sample count', () => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, `
      ${orchestraHeader}
      instr 1
        out oscil(0.1 * 0dbfs, 440)
      endin
    `)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      i 1 0 1
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
    expect(csound.GetCurrentTimeSamples(Csound)).toBe(sampleRate);
  });

  it('gets default host data', () => {
    expect(csound.GetHostData(Csound)).toBeUndefined();
  });

  describe('sets host data that', () => {
    it('is undefined', () => {
      csound.SetHostData(Csound, undefined);
      expect(csound.GetHostData(Csound)).toBeUndefined();
    });

    it('is null', () => {
      csound.SetHostData(Csound, null);
      expect(csound.GetHostData(Csound)).toBeNull();
    });

    it('is a Boolean', () => {
      csound.SetHostData(Csound, true);
      expect(csound.GetHostData(Csound)).toBe(true);
    });

    it('is a number', () => {
      csound.SetHostData(Csound, 42);
      expect(csound.GetHostData(Csound)).toBe(42);
    });

    it('is a string', () => {
      csound.SetHostData(Csound, 'hello, world');
      expect(csound.GetHostData(Csound)).toBe('hello, world');
    });

    it('is a function', () => {
      function hostDataFunction() {}
      csound.SetHostData(Csound, hostDataFunction);
      expect(csound.GetHostData(Csound)).toBe(hostDataFunction);
    });

    it('is an object', () => {
      var object = {};
      csound.SetHostData(Csound, object);
      expect(csound.GetHostData(Csound)).toBe(object);
    });
  });

  it('sets and gets whether debug messages print', () => {
    expect(csound.GetDebug(Csound)).toBe(false);
    csound.SetDebug(Csound, true);
    expect(csound.GetDebug(Csound)).toBe(true);
  });

  it('gets audio output name', () => {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.SUCCESS);
    expect(csound.GetOutputName(Csound)).toBe('dac');
  });

  it('sets and gets message level', () => {
    var level = csound.GetMessageLevel(Csound);
    // 1 + 2 + 4 + 96 + 128 = 231 is the maximum message level.
    if (level < 231)
      level++;
    else
      level--;
    csound.SetMessageLevel(Csound, level);
    expect(csound.GetMessageLevel(Csound)).toBe(level);
  });

  it('sets message callback', done => {
    csound.SetMessageCallback(Csound, (attributes, string) => done());
    csound.Message(Csound, 'hello, world\n');
  });

  it('gets messages from message buffer', () => {
    expect(() => csound.CreateMessageBuffer(Csound)).not.toThrow();
    csound.Message(Csound, 'hello, world\n');
    csound.Cleanup(Csound);
    expect(csound.GetMessageCnt(Csound)).toBeGreaterThan(0);
    expect(csound.GetFirstMessage(Csound)).toBe('hello, world\n');
    expect(csound.GetFirstMessageAttr(Csound)).toBe(0);
    expect(() => csound.PopFirstMessage(Csound)).not.toThrow();
    expect(() => csound.DestroyMessageBuffer(Csound)).not.toThrow();
  });

  it('sends messages with attributes', () => {
    var messageAttributes = [
      csound.MSG_ERROR,
      csound.MSG_ORCH,
      csound.MSG_REALTIME,
      csound.MSG_WARNING,

      csound.MSG_FG_BLACK,
      csound.MSG_FG_RED,
      csound.MSG_FG_GREEN,
      csound.MSG_FG_YELLOW,
      csound.MSG_FG_BLUE,
      csound.MSG_FG_MAGENTA,
      csound.MSG_FG_CYAN,
      csound.MSG_FG_WHITE,

      csound.MSG_FG_BOLD,
      csound.MSG_FG_UNDERLINE,

      csound.MSG_BG_BLACK,
      csound.MSG_BG_RED,
      csound.MSG_BG_GREEN,
      csound.MSG_BG_ORANGE,
      csound.MSG_BG_BLUE,
      csound.MSG_BG_MAGENTA,
      csound.MSG_BG_CYAN,
      csound.MSG_BG_GREY
    ];
    expect(() => csound.CreateMessageBuffer(Csound)).not.toThrow();
    for (var attribute of messageAttributes) {
      csound.MessageS(Csound, attribute, 'hello, world\n');
      expect(csound.GetFirstMessageAttr(Csound)).toBe(attribute);
      csound.PopFirstMessage(Csound);
    }
    csound.Cleanup(Csound);
  });

  it('populates and deletes channel list', () => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, `
      ${orchestraHeader}
      chn_k "Input", 1
      chn_k "Output", 2
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    var channelList = [];
    expect(channelList.length).toBe(0);
    var channelListLength = csound.ListChannels(Csound, channelList);
    expect(channelListLength).toBe(2);
    expect(channelList.length).toBe(channelListLength);
    var channelInfo = channelList[0];
    expect(channelInfo.name).toBe('Input');
    expect(channelInfo.type).toBe(csound.CONTROL_CHANNEL | csound.INPUT_CHANNEL);
    channelInfo = channelList[1];
    expect(channelInfo.name).toBe('Output');
    expect(channelInfo.type).toBe(csound.CONTROL_CHANNEL | csound.OUTPUT_CHANNEL);
    csound.DeleteChannelList(Csound, channelList);
    expect(channelList.length).toBe(0);
  });

  it('sets and gets control channel value', () => {
    var name = 'test';
    csound.SetControlChannel(Csound, name, 42);
    var info = {};
    expect(csound.GetControlChannel(Csound, name, info)).toBe(42);
    expect(info.status).toBe(csound.SUCCESS);
    expect(csound.GetControlChannel(Csound, name)).toBe(42);
  });

  it('receives score events', done => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    csound.PerformAsync(Csound, result => {
      expect(result).toBeGreaterThan(0);
      done();
    });
    expect(csound.ScoreEvent(Csound, 'e')).toBe(csound.SUCCESS);
  });

  it('receives score statement messages', done => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    csound.PerformAsync(Csound, result => {
      expect(result).toBeGreaterThan(0);
      done();
    });
    csound.InputMessage(Csound, 'e');
  });

  it('gets and sets function table values', done => {
    expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      f 1 0 1024 10 1
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    csound.PerformAsync(Csound, result => {
      expect(result).toBeGreaterThan(0);
      expect(csound.TableLength(Csound, 1)).toBe(1024);
      var index = 1024 / 4;
      expect(csound.TableGet(Csound, 1, index)).toBe(1);
      csound.TableSet(Csound, 1, index, 0);
      expect(csound.TableGet(Csound, 1, index)).toBe(0);
      done();
    });
  });

  it('sets graph callbacks', done => {
    expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.SUCCESS);
    expect(csound.SetIsGraphable(Csound, true)).toBeFalsy();
    expect(csound.SetIsGraphable(Csound, true)).toBeTruthy();
    csound.SetMakeGraphCallback(Csound, (data, name) => {});
    csound.SetDrawGraphCallback(Csound, data => done());
    expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
    expect(csound.ReadScore(Csound, `
      f 1 0 16384 10 1
      e
    `)).toBe(csound.SUCCESS);
    expect(csound.Start(Csound)).toBe(csound.SUCCESS);
    expect(csound.Perform(Csound)).toBeGreaterThan(0);
  });

  it('populates and deletes opcode list', () => {
    var opcodeList = [];
    expect(opcodeList.length).toBe(0);
    var opcodeListLength = csound.NewOpcodeList(Csound, opcodeList);
    expect(opcodeListLength).toBeGreaterThan(0);
    expect(opcodeList.length).toBe(opcodeListLength);
    csound.DisposeOpcodeList(Csound, opcodeList);
    expect(opcodeList.length).toBe(0);
  });

  it('gets environment variable value', () => {
    expect(csound.SetGlobalEnv('SFDIR', __dirname)).toBe(csound.SUCCESS);
    expect(csound.GetEnv(csound.Create(), 'SFDIR')).toBe(__dirname);
  });

  it('gets utility names and descriptions', () => {
    var utilityNames = csound.ListUtilities(Csound);
    expect(utilityNames.length).toBeGreaterThan(0);
    for (var name of utilityNames) {
      expect(csound.GetUtilityDescription(Csound, name).length).toBeGreaterThan(0);
    }
    csound.DeleteUtilityList(Csound, utilityNames);
    expect(utilityNames.length).toBe(0);
    expect(csound.GetUtilityDescription(Csound, '')).toBeNull();
  });
});
