const csound = require('bindings')('csound-api.node');
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
