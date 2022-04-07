#include "cirlcommon.h"

#define MAX_GRADIENT_COUNT 1000
#define MAX_GRADIENT_COUNT_SLEN 5 // how many bytes a string needs to hold the string rep of the count
#define MAX_GRADIENT_COUNT_IDLEN 13 // how many bytes a string needs to hold the gradient id

int color(char *colorstr, unsigned char *rgb);

float amp(char *ampstr, float amplifier);

char setsigmoid(char *sigstr, char use_sig);

int processline(char *line, float amplifier, char use_sig);

unsigned char *getcolors(char *line, unsigned char *rgb_raw, float amplifier, char use_sig);

void getcmd(char *line, unsigned char *rgb, float amplifier, char use_sig, char *cmd);
void getcmd(char *line, unsigned char *rgb, unsigned char **gradient, char GRADIENT,
            int gradient_count, float amplifier, char use_sig, char *cmd);
