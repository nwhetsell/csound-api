const csound = require('bindings')('csound-api.node');
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
