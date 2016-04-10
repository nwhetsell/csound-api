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

// Convert the AST to an object that is not backed by a Csound structure.
var ASTObject = JSON.parse(JSON.stringify(ASTRoot));
csound.DeleteTree(Csound, ASTRoot);
csound.Destroy(Csound);

// The next property of an AST node seems to be for the second argument of an
// opcode, its next property for the third argument, and so on. Add nextNodes
// properties to AST objects to reflect this relationship.
function addNextNodesToASTObject(ASTObject) {
  var ASTObjectsForDepths = [ASTObject];
  function addNextNodes(ASTObject, depth, key) {
    var depthPlus1 = depth + 1;
    ASTObjectsForDepths[depthPlus1] = ASTObject;
    if (ASTObject.left)
      addNextNodes(ASTObject.left, depthPlus1, 'left');
    if (ASTObject.right)
      addNextNodes(ASTObject.right, depthPlus1, 'right');
    if (ASTObject.next) {
      var ASTObjectForDepth = ASTObjectsForDepths[depth];
      if (ASTObjectForDepth.nextNodes)
        ASTObjectForDepth.nextNodes.push(ASTObject.next);
      else
        ASTObjectForDepth.nextNodes = [ASTObject.next];
      addNextNodes(ASTObject.next, depth, 'next');
    }
  }
  addNextNodes(ASTObject, 0, 'none');
}
addNextNodesToASTObject(ASTObject);

// Log the AST as JSON.
console.log(require('json-stable-stringify')(ASTObject, {
  cmp: function(a, b) {
    // When converting AST objects to JSON, put the left, right, and nextNodes
    // properties last.
    switch (a.key) {
      // Note fallthrough.
      case 'left': if (b.key === 'right') return -1;
      case 'right': if (b.key === 'nextNodes') return -1;
      case 'nextNodes': return (a.key === b.key) ? 0 : 1;
    }
    return a.key.localeCompare(b.key);
  },
  replacer: function(key, value) {
    return (key === 'next') ? undefined : value;
  },
  space: '  '
}));
