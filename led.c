#include "cirlcommon.h"
#include "led.h"

// see github page for difference between these two functions
int cirlsigmoid(int x) {
    return floor(1000 / (1 + exp(-0.013 * (x - 500))));
}

int cirlexp(int peak, int amp) {
    return floorl((powl(peak, amp) / powl(1000, amp - 1)));
}

// TODO: maximize color value if option is set (ex. RGB{70, 70, 70} becomes RGB{255, 255, 255} automatically})
int color(char *colorstr, char *rgb) {
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

int setsigmoid(char *sigstr, char use_sig) {
    char *r = NULL, delim[] = " ", *temp = strtok_r(sigstr, " ", &r);
    int temp_use_sig;

    temp = strtok_r(NULL, delim, &r);
    if (temp != NULL) {
        temp_use_sig = atoi(temp[0]);
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
    int peak = 0, temp_int;

    if (line != NULL) {
        // get next line and feed into temp
        temp = strtok_r(line, delim, &r);

        // get max value of waveform
        while (temp != NULL) {
            temp_int = atoi(temp);
            peak = max(peak, temp_int);
            temp = strtok(NULL, delim);
        }

        // line holds the last token for some reason
        peak = max(peak, atoi(line));

        free(temp);
        return (use_sig) ? cirlsigmoid(peak) : cirlexp(peak, amplifier);
    }

    return 0;
}

void getcmd(char *line, char *rgb, float amplifier, char use_sig, char *cmd) {
    int peak, r, g, b;

    peak = floor(processline(line, amplifier, use_sig) * 0.255);
    r = floor(rgb[0] * ((double)peak / 255));
    g = floor(rgb[1] * ((double)peak / 255));
    b = floor(rgb[2] * ((double)peak / 255));
    sprintf(cmd, "/usr/local/bin/pigs p 17 %d p 22 %d p 24 %d", r, g, b);

    return;
}