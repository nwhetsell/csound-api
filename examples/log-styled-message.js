var ansi = require('ansi-styles');
var path = require('path');
var csound = require(path.join('..', 'build', 'Release', 'csound-api.node'));

var textColors = {};
textColors[csound.MSG_FG_BLACK]   = ansi.black;
textColors[csound.MSG_FG_RED]     = ansi.red;
textColors[csound.MSG_FG_GREEN]   = ansi.green;
textColors[csound.MSG_FG_YELLOW]  = ansi.yellow;
textColors[csound.MSG_FG_BLUE]    = ansi.blue;
textColors[csound.MSG_FG_MAGENTA] = ansi.magenta;
textColors[csound.MSG_FG_CYAN]    = ansi.cyan;
textColors[csound.MSG_FG_WHITE]   = ansi.gray;

var backgroundColors = {};
backgroundColors[csound.MSG_BG_BLACK]   = ansi.bgBlack;
backgroundColors[csound.MSG_BG_RED]     = ansi.bgRed;
backgroundColors[csound.MSG_BG_GREEN]   = ansi.bgGreen;
backgroundColors[csound.MSG_BG_ORANGE]  = ansi.bgYellow;
backgroundColors[csound.MSG_BG_BLUE]    = ansi.bgBlue;
backgroundColors[csound.MSG_BG_MAGENTA] = ansi.bgMagenta;
backgroundColors[csound.MSG_BG_CYAN]    = ansi.bgCyan;
backgroundColors[csound.MSG_BG_GREY]    = ansi.bgWhite;

var Csound = csound.Create();
csound.SetMessageCallback(Csound, function(attributes, string) {
  var color = textColors[attributes & csound.MSG_FG_COLOR_MASK];
  if (color)
    string = color.open + string + color.close;
  if (attributes & csound.MSG_FG_BOLD)
    string = ansi.bold.open + string + ansi.bold.close;
  if (attributes & csound.MSG_FG_UNDERLINE)
    string = ansi.underline.open + string + ansi.underline.close;
  color = backgroundColors[attributes & csound.MSG_BG_COLOR_MASK];
  if (color)
    string = color.open + string + color.close;
  console.log(string);
});
csound.MessageS(
  Csound,
  csound.MSG_FG_YELLOW | csound.MSG_FG_UNDERLINE | csound.MSG_BG_BLUE,
  'Hail!'
);
csound.Destroy(Csound);
