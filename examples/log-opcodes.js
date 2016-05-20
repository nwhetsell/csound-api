const csound = require('bindings')('csound-api.node');
const Csound = csound.Create();
const opcodes = [];
csound.NewOpcodeList(Csound, opcodes);
console.log(opcodes);
csound.DisposeOpcodeList(Csound, opcodes);
csound.Destroy(Csound);
