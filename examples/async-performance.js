var path = require('path');
var csound = require(path.join('..', 'build', 'Release', 'csound-api.node'));
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
