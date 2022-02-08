#include "cirlcommon.h"


int color(char *colorstr, unsigned char *rgb);

float amp(char *ampstr, float amplifier);

char setsigmoid(char *sigstr, char use_sig);

int processline(char *line, float amplifier, char use_sig);

unsigned char *getcolors(char *line, unsigned char *rgb_raw, float amplifier, char use_sig);

void getcmd(char *line, unsigned char *rgb, float amplifier, char use_sig, char *cmd);