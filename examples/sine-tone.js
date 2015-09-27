var path = require('path');
var csound = require(path.join('..', 'build/Release/csound-api.node'));
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
