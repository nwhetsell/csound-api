const csound = require('bindings')('csound-api.node');
const stringify = require('json-stable-stringify');

const Csound = csound.Create();
const ASTRoot = csound.ParseOrc(Csound, `
  0dbfs = 1
  giFunctionTableID ftgen 0, 0, 16384, 10, 1
  instr A440
    outc oscili(0.5 * 0dbfs, 440, giFunctionTableID)
  endin
`);

// Convert the AST to an object that is not backed by a Csound structure.
const ASTObject = JSON.parse(JSON.stringify(ASTRoot));
csound.DeleteTree(Csound, ASTRoot);
csound.Destroy(Csound);

// The next property of an AST node seems to be for the second argument of an
// opcode, its next property for the third argument, and so on. Add nextNodes
// properties to AST objects to reflect this relationship.
function addNextNodesToASTObject(ASTObject) {
  const ASTObjectsForDepths = [ASTObject];
  function addNextNodes(ASTObject, depth) {
    const depthPlus1 = depth + 1;
    ASTObjectsForDepths[depthPlus1] = ASTObject;
    if (ASTObject.left)
      addNextNodes(ASTObject.left, depthPlus1);
    if (ASTObject.right)
      addNextNodes(ASTObject.right, depthPlus1);
    if (ASTObject.next) {
      const ASTObjectForDepth = ASTObjectsForDepths[depth];
      if (ASTObjectForDepth.nextNodes)
        ASTObjectForDepth.nextNodes.push(ASTObject.next);
      else
        ASTObjectForDepth.nextNodes = [ASTObject.next];
      addNextNodes(ASTObject.next, depth);
    }
  }
  addNextNodes(ASTObject, 0);
}
addNextNodesToASTObject(ASTObject);

// Log the AST as JSON.
console.log(stringify(ASTObject, {
  cmp: (a, b) => {
    // When converting AST objects to JSON, put the left, right, and nextNodes
    // properties last.
    function compareKeys(key1, key2) {
      switch (key1) {
        case 'left': if (key2 === 'right') return -1; /* falls through */
        case 'right': if (key2 === 'nextNodes') return -1; /* falls through */
        case 'nextNodes': return (key1 === key2) ? 0 : 1;
      }
      return null;
    }
    let result = compareKeys(a.key, b.key);
    if (result !== null) return result;
    result = compareKeys(b.key, a.key);
    return (result !== null) ? -result : a.key.localeCompare(b.key);
  },
  replacer: (key, value) => (key === 'next') ? undefined : value,
  space: '  '
}));
