const ansi = require('ansi-styles');
const csound = require('bindings')('csound-api.node');
const path = require('path');

const textColors = {
  [csound.MSG_FG_BLACK]   : ansi.black,
  [csound.MSG_FG_RED]     : ansi.red,
  [csound.MSG_FG_GREEN]   : ansi.green,
  [csound.MSG_FG_YELLOW]  : ansi.yellow,
  [csound.MSG_FG_BLUE]    : ansi.blue,
  [csound.MSG_FG_MAGENTA] : ansi.magenta,
  [csound.MSG_FG_CYAN]    : ansi.cyan,
  [csound.MSG_FG_WHITE]   : ansi.gray
};

const backgroundColors = {
  [csound.MSG_BG_BLACK]   : ansi.bgBlack,
  [csound.MSG_BG_RED]     : ansi.bgRed,
  [csound.MSG_BG_GREEN]   : ansi.bgGreen,
  [csound.MSG_BG_ORANGE]  : ansi.bgYellow,
  [csound.MSG_BG_BLUE]    : ansi.bgBlue,
  [csound.MSG_BG_MAGENTA] : ansi.bgMagenta,
  [csound.MSG_BG_CYAN]    : ansi.bgCyan,
  [csound.MSG_BG_GREY]    : ansi.bgWhite
};

const Csound = csound.Create();
csound.SetMessageCallback(Csound, (attributes, string) => {
  let color = textColors[attributes & csound.MSG_FG_COLOR_MASK];
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
