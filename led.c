#include "cirlcommon.h"
#include "led.h"

#define max(a,b) (a > b) ? a : b

// see github page for difference between these two functions
int cirlsigmoid(int x) {
    return floor(1000 / (1 + exp(-0.013 * (x - 500))));
}

int cirlexp(int peak, int amp) {
    return floorl((powl(peak, amp) / powl(1000, amp - 1)));
}

// TODO: maximize color value if option is set (ex. RGB{70, 70, 70} becomes RGB{255, 255, 255} automatically})
int color(char *colorstr, unsigned char *rgb) {
    char *r = NULL, delim[] = " ", *temp = strtok_r(colorstr, delim, &r);
    int temp_rgb[3], i;

    for (i = 0; i < 3; i++) {
        temp = strtok_r(NULL, delim, &r);
        if (temp != NULL) {
            temp_rgb[i] = atoi(temp);
            if (temp_rgb[i] < 0 || temp_rgb[i] > 255) {
                printf("Each color value should be between 0 and 255\n");
                return -1;
            }
        }
        else {
            printf("Each color value should be between 0 and 255\n");
            return -1;
        }
    }

    for (i = 0; i < 3; i++) {
        rgb[i] = temp_rgb[i];
    }
    return 0;
}

float amp(char *ampstr, float amplifier) {
    float temp_amplifier;
    char *r = NULL, delim[] = " ", *temp = strtok_r(ampstr, delim, &r);

    temp = strtok_r(NULL, delim, &r);
    if (temp != NULL) {
        temp_amplifier = atof(temp);
        if (temp_amplifier < 1.0 || temp_amplifier > 5.0) {
            printf("Amplifier should be between 1.0 and 5.0\n");
            return amplifier;
        }
    }
    else {
        printf("Amplifier should be between 1.0 and 5.0\n");
        return amplifier;
    }

    return temp_amplifier;
}

char setsigmoid(char *sigstr, char use_sig) {
    char *r = NULL, delim[] = " ", *temp = strtok_r(sigstr, " ", &r);
    int temp_use_sig;

    temp = strtok_r(NULL, delim, &r);
    if (temp != NULL) {
        temp_use_sig = atoi(temp);
        if (temp_use_sig != 0 && temp_use_sig != 1) {
            printf("Value should be 0 or 1\n");
            return use_sig;
        }
    }
    else {
        printf("Value should be 0 or 1\n");
        return use_sig;
    }

    return temp_use_sig;
}

int processline(char *line, float amplifier, char use_sig) {
    char *r = NULL, delim[] = " ", *temp = calloc(8, sizeof(char));
    int peak = 0, temp_int, brightness;

    if (line != NULL) {
        // get next line and feed into temp
        temp = strtok_r(line, delim, &r);

        // get max value of waveform
        while (temp != NULL) {
            temp_int = atoi(temp);
            peak = max(peak, temp_int);
            temp = strtok_r(NULL, delim, &r);
        }

        // line holds the last token for some reason

        free(temp);
        return (use_sig) ? cirlsigmoid(peak) : cirlexp(peak, amplifier);
    }

    return 0;
}

unsigned char *getcolors(char *line, unsigned char *rgb_raw, float amplifier, char use_sig) {
    double peak;
    unsigned char *rgb = calloc(3, sizeof(char));

    peak = floor(processline(line, amplifier, use_sig) * 0.255);
    rgb[0] = floor(rgb_raw[0] * (peak / 255));
    rgb[1] = floor(rgb_raw[1] * (peak / 255));
    rgb[2] = floor(rgb_raw[2] * (peak / 255));

    return rgb;
}

void getcmd(char *line, unsigned char *rgb_raw, float amplifier, char use_sig, char *cmd) {
    unsigned char *rgb = getcolors(line, rgb_raw, amplifier, use_sig);
    sprintf(cmd, "/usr/local/bin/pigs p 17 %d p 22 %d p 24 %d", rgb[0], rgb[1], rgb[2]);
    free(rgb);
    return;
}