var path = require('path');
var csound = require(path.join('..', 'build', 'Release', 'csound-api.node'));
var Csound = csound.Create();
var ASTRoot = csound.ParseOrc(Csound, `
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
