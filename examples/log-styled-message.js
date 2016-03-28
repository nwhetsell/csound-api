var ansi = require('ansi-styles');
var path = require('path');
var csound = require(path.join('..', 'build', 'Release', 'csound-api.node'));

var textColors = {};
textColors[csound.CSOUNDMSG_FG_BLACK]   = ansi.black;
textColors[csound.CSOUNDMSG_FG_RED]     = ansi.red;
textColors[csound.CSOUNDMSG_FG_GREEN]   = ansi.green;
textColors[csound.CSOUNDMSG_FG_YELLOW]  = ansi.yellow;
textColors[csound.CSOUNDMSG_FG_BLUE]    = ansi.blue;
textColors[csound.CSOUNDMSG_FG_MAGENTA] = ansi.magenta;
textColors[csound.CSOUNDMSG_FG_CYAN]    = ansi.cyan;
textColors[csound.CSOUNDMSG_FG_WHITE]   = ansi.gray;

var backgroundColors = {};
backgroundColors[csound.CSOUNDMSG_BG_BLACK]   = ansi.bgBlack;
backgroundColors[csound.CSOUNDMSG_BG_RED]     = ansi.bgRed;
backgroundColors[csound.CSOUNDMSG_BG_GREEN]   = ansi.bgGreen;
backgroundColors[csound.CSOUNDMSG_BG_ORANGE]  = ansi.bgYellow;
backgroundColors[csound.CSOUNDMSG_BG_BLUE]    = ansi.bgBlue;
backgroundColors[csound.CSOUNDMSG_BG_MAGENTA] = ansi.bgMagenta;
backgroundColors[csound.CSOUNDMSG_BG_CYAN]    = ansi.bgCyan;
backgroundColors[csound.CSOUNDMSG_BG_GREY]    = ansi.bgWhite;

var Csound = csound.Create();
csound.SetMessageCallback(Csound, function(attributes, string) {
  var color = textColors[attributes & csound.CSOUNDMSG_FG_COLOR_MASK];
  if (color)
    string = color.open + string + color.close;
  if (attributes & csound.CSOUNDMSG_FG_BOLD)
    string = ansi.bold.open + string + ansi.bold.close;
  if (attributes & csound.CSOUNDMSG_FG_UNDERLINE)
    string = ansi.underline.open + string + ansi.underline.close;
  color = backgroundColors[attributes & csound.CSOUNDMSG_BG_COLOR_MASK];
  if (color)
    string = color.open + string + color.close;
  console.log(string);
});
csound.MessageS(
  Csound,
  csound.CSOUNDMSG_FG_YELLOW | csound.CSOUNDMSG_FG_UNDERLINE | csound.CSOUNDMSG_BG_BLUE,
  'Hail!'
);
csound.Destroy(Csound);
