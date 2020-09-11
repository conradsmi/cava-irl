#include "cirlcommon.h"

int color(char *colorstr, char *rgb);

float amp(char *ampstr, float amplifier);

char setsigmoid(char *sigstr, char use_sig);

int processline(char *line, float amplifier, char use_sig);

void getcmd(char *line, char *rgb, float amplifier, char use_sig, char *cmd);