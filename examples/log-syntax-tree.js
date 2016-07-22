const csound = require('bindings')('csound-api.node');
const Csound = csound.Create();
const ASTRoot = csound.ParseOrc(Csound, `
  0dbfs = 1
  giFunctionTableID ftgen 0, 0, 16384, 10, 1
  instr A440
    outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)
  endin
`);
console.log(ASTRoot);
csound.DeleteTree(Csound, ASTRoot);
csound.Destroy(Csound);
