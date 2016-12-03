const csound = require('bindings')('csound-api.node');
const fs = require('fs');
const path = require('path');

describe('Csound API', () => {
  it('is defined', () => {
    expect(csound).toBeDefined();
  });

  it('gets initialization options', () => {
    // https://github.com/csound/csound/search?q=CSOUNDINIT_NO_SIGNAL_HANDLER+path%3Ainclude+filename%3Acsound.h
    expect(csound.INIT_NO_SIGNAL_HANDLER).toBe(1);
    expect(csound.INIT_NO_ATEXIT).toBe(2);
  });

  it('gets status codes', () => {
    // https://github.com/csound/csound/search?q=CSOUND_STATUS+path%3Ainclude+filename%3Acsound.h
    expect(csound.SUCCESS).toBe(0);
    expect(csound.ERROR).toBe(-1);
    expect(csound.INITIALIZATION).toBe(-2);
    expect(csound.PERFORMANCE).toBe(-3);
    expect(csound.MEMORY).toBe(-4);
    expect(csound.SIGNAL).toBe(-5);
  });

  it('gets sound file type codes', () => {
    // https://github.com/csound/csound/search?q=CSFTYPE_RAW_AUDIO+path%3Ainclude+filename%3Acsound.h
    expect(csound.FTYPE_RAW_AUDIO).toBe(10);
    expect(csound.FTYPE_IRCAM).toBe(11);
    expect(csound.FTYPE_AIFF).toBe(12);
    expect(csound.FTYPE_AIFC).toBe(13);
    expect(csound.FTYPE_WAVE).toBe(14);
    expect(csound.FTYPE_AU).toBe(15);
    expect(csound.FTYPE_SD2).toBe(16);
    expect(csound.FTYPE_W64).toBe(17);
    expect(csound.FTYPE_WAVEX).toBe(18);
    expect(csound.FTYPE_FLAC).toBe(19);
    expect(csound.FTYPE_CAF).toBe(20);
    expect(csound.FTYPE_WVE).toBe(21);
    expect(csound.FTYPE_OGG).toBe(22);
    expect(csound.FTYPE_MPC2K).toBe(23);
    expect(csound.FTYPE_RF64).toBe(24);
    expect(csound.FTYPE_AVR).toBe(25);
    expect(csound.FTYPE_HTK).toBe(26);
    expect(csound.FTYPE_MAT4).toBe(27);
    expect(csound.FTYPE_MAT5).toBe(28);
    expect(csound.FTYPE_NIST).toBe(29);
    expect(csound.FTYPE_PAF).toBe(30);
    expect(csound.FTYPE_PVF).toBe(31);
    expect(csound.FTYPE_SDS).toBe(32);
    expect(csound.FTYPE_SVX).toBe(33);
    expect(csound.FTYPE_VOC).toBe(34);
    expect(csound.FTYPE_XI).toBe(35);
    expect(csound.FTYPE_UNKNOWN_AUDIO).toBe(36);
  });

  it('gets message attribute codes', () => {
    // https://github.com/csound/csound/blob/develop/include/msg_attr.h
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
    // https://github.com/csound/csound/search?q=controlChannelType+path%3Ainclude+filename%3Acsound.h
    expect(csound.CONTROL_CHANNEL).toBe(1);
    expect(csound.AUDIO_CHANNEL).toBe(2);
    expect(csound.STRING_CHANNEL).toBe(3);
    expect(csound.PVS_CHANNEL).toBe(4);
    expect(csound.CHANNEL_TYPE_MASK).toBe(15);
    expect(csound.INPUT_CHANNEL).toBe(16);
    expect(csound.OUTPUT_CHANNEL).toBe(32);
  });

  it('gets control channel behavior', () => {
    // https://github.com/csound/csound/search?q=controlChannelBehavior+path%3Ainclude+filename%3Acsound.h
    expect(csound.CONTROL_CHANNEL_NO_HINTS).toBe(0);
    expect(csound.CONTROL_CHANNEL_INT).toBe(1);
    expect(csound.CONTROL_CHANNEL_LIN).toBe(2);
    expect(csound.CONTROL_CHANNEL_EXP).toBe(3);
  });

  it('defines Initialize', () => {
    expect(csound.Initialize).toBeDefined();
  });

  it('creates and destroys instance', () => {
    const Csound = csound.Create();
    expect(typeof Csound).toBe('object');
    expect(() => csound.Destroy(Csound)).not.toThrow();
  });

  describe('creates instance with host data that', () => {
    'use strict'; // Needed for Node.js 5 and earlier

    let Csound;
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
      const object = {};
      Csound = csound.Create(object);
      expect(csound.GetHostData(Csound)).toBe(object);
    });
  });

  it('returns version numbers', () => {
    expect(typeof csound.GetVersion()).toBe('number');
    expect(typeof csound.GetAPIVersion()).toBe('number');
  });
});

describe('Csound instance', () => {
  // Simplify console output.
  csound.SetDefaultMessageCallback(() => {});

  const outputChannelCount = 1;
  const sampleRate = 44100;
  const fullScalePeakAmplitude = 1;
  const samplesPerControlPeriod = 10;
  const orchestraHeader = `
    nchnls = ${outputChannelCount}
    sr = ${sampleRate}
    0dbfs = ${fullScalePeakAmplitude}
    ksmps = ${samplesPerControlPeriod}
  `;

  describe('synchronously', () => {
    'use strict'; // Needed for Node.js 5 and earlier

    let Csound;
    beforeEach(() => Csound = csound.Create());
    afterEach(() => csound.Destroy(Csound));

    it('parses and deletes abstract syntax tree', () => {
      const ASTRoot = csound.ParseOrc(Csound, orchestraHeader);
      expect(ASTRoot).not.toBeNull();
      expect(ASTRoot.next).not.toBeNull();
      expect(ASTRoot.next.value).not.toBeNull();
      expect(() => csound.DeleteTree(Csound, ASTRoot)).not.toThrow();
      expect(csound.ParseOrc(Csound, '')).toBeNull();
    });

    it('compiles abstract syntax tree', () => {
      const ASTRoot = csound.ParseOrc(Csound, orchestraHeader);
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

    // csoundCompile simply calls csoundCompileArgs and then csoundStart
    // (https://github.com/csound/csound/search?q=csoundCompile+path%3ATop+filename%3Amain.c)
    it('compiles command-line arguments', () => {
      const orchestraPath = path.join(__dirname, 'orchestra.orc');
      const scorePath = path.join(__dirname, 'score.sco');
      let file = fs.openSync(orchestraPath, 'w');
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
      const CSDPath = path.join(__dirname, 'document.csd');
      const file = fs.openSync(CSDPath, 'w');
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
      let isFinished = csound.PerformKsmps(Csound);
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
        const object = {};
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
      let level = csound.GetMessageLevel(Csound);
      // 1 + 2 + 4 + 96 + 128 = 231 is the maximum message level.
      if (level < 231)
        level++;
      else
        level--;
      csound.SetMessageLevel(Csound, level);
      expect(csound.GetMessageLevel(Csound)).toBe(level);
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
      const messageAttributes = [
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
      for (const attribute of messageAttributes) {
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
      const channelList = [];
      expect(channelList.length).toBe(0);
      const channelListLength = csound.ListChannels(Csound, channelList);
      expect(channelListLength).toBe(2);
      expect(channelList.length).toBe(channelListLength);
      let channelInfo = channelList[0];
      expect(channelInfo.name).toBe('Input');
      expect(channelInfo.type).toBe(csound.CONTROL_CHANNEL | csound.INPUT_CHANNEL);
      expect(typeof channelInfo.hints).toBe('object');
      channelInfo = channelList[1];
      expect(channelInfo.name).toBe('Output');
      expect(channelInfo.type).toBe(csound.CONTROL_CHANNEL | csound.OUTPUT_CHANNEL);
      expect(typeof channelInfo.hints).toBe('object');
      csound.DeleteChannelList(Csound, channelList);
      expect(channelList.length).toBe(0);
    });

    it('sets and gets control channel hints', () => {
      const name = 'test';
      expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
      expect(csound.CompileOrc(Csound, `chn_k "${name}", 1`)).toBe(csound.SUCCESS);
      expect(csound.Start(Csound)).toBe(csound.SUCCESS);
      let hints = {
        behav: csound.CONTROL_CHANNEL_INT,
        dflt: 5,
        min: 1,
        max: 10,
        attributes: 'attributes'
      };
      expect(csound.SetControlChannelHints(Csound, name, hints)).toBe(csound.SUCCESS);
      hints = {};
      expect(csound.GetControlChannelHints(Csound, name, hints)).toBe(csound.SUCCESS);
      expect(hints.dflt).toBe(5);
      expect(hints.min).toBe(1);
      expect(hints.max).toBe(10);
      expect(hints.attributes).toBe('attributes');
    });

    it('sets and gets control channel value', () => {
      const name = 'test';
      csound.SetControlChannel(Csound, name, 42);
      const info = {};
      expect(csound.GetControlChannel(Csound, name, info)).toBe(42);
      expect(info.status).toBe(csound.SUCCESS);
      expect(csound.GetControlChannel(Csound, name)).toBe(42);
    });

    it('populates and deletes opcode list', () => {
      const opcodeList = [];
      expect(opcodeList.length).toBe(0);
      const opcodeListLength = csound.NewOpcodeList(Csound, opcodeList);
      expect(opcodeListLength).toBeGreaterThan(0);
      expect(opcodeList.length).toBe(opcodeListLength);
      csound.DisposeOpcodeList(Csound, opcodeList);
      expect(opcodeList.length).toBe(0);
    });

    it('gets environment variable value', () => {
      expect(csound.SetGlobalEnv('SFDIR', __dirname)).toBe(csound.SUCCESS);
      const Csound = csound.Create();
      expect(csound.GetEnv(Csound, 'SFDIR')).toBe(__dirname);
      csound.Destroy(Csound);
    });

    it('gets utility names and descriptions', () => {
      const utilityNames = csound.ListUtilities(Csound);
      expect(utilityNames).not.toBeNull();
      for (const name of utilityNames) {
        expect(csound.GetUtilityDescription(Csound, name).length).toBeGreaterThan(0);
      }
      csound.DeleteUtilityList(Csound, utilityNames);
      expect(csound.GetUtilityDescription(Csound, '')).toBeNull();
    });
  });

  describe('asynchronously', () => {
    it('performs', done => {
      const Csound = csound.Create();
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
        csound.Destroy(Csound);
        done();
      });
      setTimeout(() => csound.Stop(Csound), 600);
    });

    it('sets message callback', done => {
      const Csound = csound.Create();
      csound.SetMessageCallback(Csound, (attributes, string) => {
        if (string === 'hello, world\n') {
          csound.Destroy(Csound);
          done();
        }
      });
      csound.Message(Csound, 'hello, world\n');
    });

    it('receives score events', done => {
      const Csound = csound.Create();
      expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
      expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
      expect(csound.Start(Csound)).toBe(csound.SUCCESS);
      csound.PerformAsync(Csound, result => {
        expect(result).toBeGreaterThan(0);
        csound.Destroy(Csound);
        done();
      });
      expect(csound.ScoreEvent(Csound, 'e')).toBe(csound.SUCCESS);
    });

    it('receives score statement messages', done => {
      const Csound = csound.Create();
      expect(csound.SetOption(Csound, '--nosound')).toBe(csound.SUCCESS);
      expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
      expect(csound.Start(Csound)).toBe(csound.SUCCESS);
      csound.PerformAsync(Csound, result => {
        expect(result).toBeGreaterThan(0);
        csound.Destroy(Csound);
        done();
      });
      csound.InputMessage(Csound, 'e');
    });

    it('gets and sets function table values', done => {
      const Csound = csound.Create();
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
        const index = 1024 / 4;
        expect(csound.TableGet(Csound, 1, index)).toBe(1);
        csound.TableSet(Csound, 1, index, 0);
        expect(csound.TableGet(Csound, 1, index)).toBe(0);
        csound.Destroy(Csound);
        done();
      });
    });

    it('sets graph callbacks', done => {
      const Csound = csound.Create();
      expect(csound.SetOption(Csound, '--output=dac')).toBe(csound.SUCCESS);
      expect(csound.SetIsGraphable(Csound, true)).toBe(false);
      expect(csound.SetIsGraphable(Csound, true)).toBe(true);
      csound.SetMakeGraphCallback(Csound, (data, name) => {});
      csound.SetDrawGraphCallback(Csound, data => {
        csound.Destroy(Csound);
        done();
      });
      expect(csound.CompileOrc(Csound, orchestraHeader)).toBe(csound.SUCCESS);
      expect(csound.ReadScore(Csound, `
        f 1 0 16384 10 1
        e
      `)).toBe(csound.SUCCESS);
      expect(csound.Start(Csound)).toBe(csound.SUCCESS);
      expect(csound.Perform(Csound)).toBeGreaterThan(0);
    });
  });

  // The test of setting audio output fails on Windows due to
  // https://github.com/csound/csound/issues/722.
  if (process.platform === 'win32')
    return;

  describe('asynchronously', () => {
    'use strict'; // Needed for Node.js 5 and earlier

    let timeoutInterval;
    beforeEach(() => {
      timeoutInterval = jasmine.DEFAULT_TIMEOUT_INTERVAL;
      jasmine.DEFAULT_TIMEOUT_INTERVAL = 20000;
    });

    it('sets audio output', done => {
      csound.SetGlobalEnv('SFDIR', __dirname);

      const Csound = csound.Create();
      csound.SetOutput(Csound, 'dac');
      expect(csound.GetOutputName(Csound)).toBe('dac');

      // Audio file types and formats from the documentation of csoundSetOutput
      // at https://csound.github.io/docs/api/group__FILEIO.html. The 'xi'
      // (FastTracker 2 <https://en.wikipedia.org/wiki/FastTracker_2> Extended
      // Instrument) type is commented out because, from
      // http://www.mega-nerd.com/libsndfile/, it requires a format of 8‑ or
      // 16‑bit differential PCM
      // <https://en.wikipedia.org/wiki/Differential_pulse-code_modulation>,
      // which Csound cannot write.
      const types = {
        [csound.FTYPE_WAVE]      : 'wav',
        [csound.FTYPE_AIFF]      : 'aiff',
        [csound.FTYPE_AU]        : 'au',
        [csound.FTYPE_RAW_AUDIO] : 'raw',
        [csound.FTYPE_PAF]       : 'paf',
        [csound.FTYPE_SVX]       : 'svx',
        [csound.FTYPE_NIST]      : 'nist',
        [csound.FTYPE_VOC]       : 'voc',
        [csound.FTYPE_IRCAM]     : 'ircam',
        [csound.FTYPE_W64]       : 'w64',
        [csound.FTYPE_MAT4]      : 'mat4',
        [csound.FTYPE_MAT5]      : 'mat5',
        [csound.FTYPE_PVF]       : 'pvf',
      //[csound.FTYPE_XI]        : 'xi',
        [csound.FTYPE_HTK]       : 'htk',
        [csound.FTYPE_SDS]       : 'sds',
        [csound.FTYPE_AVR]       : 'avr',
        [csound.FTYPE_WAVEX]     : 'wavex',
        [csound.FTYPE_SD2]       : 'sd2',
        [csound.FTYPE_FLAC]      : 'flac',
        [csound.FTYPE_CAF]       : 'caf',
        [csound.FTYPE_WVE]       : 'wve',
        [csound.FTYPE_OGG]       : 'ogg',
        [csound.FTYPE_MPC2K]     : 'mpc2k',
        [csound.FTYPE_RF64]      : 'rf64'
      };
      const formats = {
        schar   : 'signed chars',
        short   : 'shorts',
        '24bit' : '24bit ints',
        long    : 'longs',
        uchar   : 'unsigned bytes',
        float   : 'floats',
        double  : 'double floats',
        ulaw    : 'ulaw bytes',
        alaw    : 'alaw bytes',
        vorbis  : 'vorbis encoding'
      };

      const actualTypeCodes = [];
      csound.SetFileOpenCallback(Csound, (path, typeCode) => {
        // Ignore the distinction between AIFF and AIFC.
        actualTypeCodes.push(((typeCode === csound.FTYPE_AIFC) ? csound.FTYPE_AIFF : typeCode).toString());
        if (actualTypeCodes.length === expectedTypeCodes.length) {
          expect(actualTypeCodes).toEqual(expectedTypeCodes);
          csound.Destroy(Csound);
          done();
        }
      });

      const expectedTypeCodes = [];
      for (const typeCode of Object.keys(types)) {
        const type = types[typeCode];
        for (const format of Object.keys(formats)) {
          const fileName = `${format}.${type}`;
          csound.SetOutput(Csound, fileName, type, format);
          expect(csound.GetOutputName(Csound)).toBe(fileName);

          // In Csound 6.07 and earlier, there’s no direct way to get the output
          // format (https://github.com/csound/csound/issues/700). Instead,
          // write files using a minimal orchestra and examine Csound’s output.
          expect(csound.CompileOrc(Csound, '0dbfs = 1\n')).toBe(csound.SUCCESS);
          expect(csound.ReadScore(Csound, 'e\n')).toBe(csound.SUCCESS);
          csound.CreateMessageBuffer(Csound);
          if (csound.Start(Csound) === csound.SUCCESS) {
            expect(csound.Perform(Csound)).toBeGreaterThan(0);
            expectedTypeCodes.push(typeCode);
            while (csound.GetMessageCnt(Csound) > 0) {
              const message = csound.GetFirstMessage(Csound);
              if (/^writing \d+-byte blks of /.test(message))
                expect(new RegExp(`^writing \\d+-byte blks of ${formats[format]} to `).test(message)).toBe(true);
              csound.PopFirstMessage(Csound);
            }
          }

          csound.Reset(Csound);
          fs.unlinkSync(path.join(__dirname, fileName));

          // When writing SD2s, some versions of Csound create an extra file
          // with a name starting with “._”.
          if (type === 'sd2') {
            try {
              fs.unlinkSync(path.join(__dirname, `._${fileName}`));
            } catch (error) {
              // Do nothing
            }
          }
        }
      }
    });

    afterEach(() => {
      jasmine.DEFAULT_TIMEOUT_INTERVAL = timeoutInterval;
    });
  });
});
