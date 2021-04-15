#include "cirlcommon.h"
#include "led.h"

#define max(a,b) (a > b) ? a : b
#define min(a,b) (a < b) ? a : b

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

void getcmd(char *line, unsigned char *rgb, unsigned char **gradient, char GRADIENT,
            int gradient_count, float amplifier, char use_sig, char *cmd) {
    int peak_raw, peak;
    int g1, g2, step;
    unsigned char r, g, b;

    peak_raw = floor(processline(line, amplifier, use_sig));
    peak = peak_raw * 0.255;
    if (GRADIENT && gradient_count > 1) {
        step = MAX_GRADIENT_COUNT / (gradient_count-1);
        g1 = peak_raw / step;
        g1 = (g1 == gradient_count - 1) ? g1 - 1 : g1;
        g2 = g1 + 1;
        printf("%d %d ", g1, g2);
        /*r = min(floor((((MAX_GRADIENT_COUNT - peak_raw) - (step * g1)) / (MAX_GRADIENT_COUNT - step * g2 * 1.0)) * gradient[g1][0] +q
                  ((peak_raw - (step * g1)) / (step * g2 * 1.0)) * gradient[g2][0]), 255);*/
        r = ((step - (peak_raw/g2)) / (step * 1.0)) * gradient[g1][0] + ((peak_raw/g2) / (step * 1.0)) * gradient[g2][0];
        printf("%d ", r);
        /*g = min(floor((((MAX_GRADIENT_COUNT - peak_raw) - (step * g1)) / (step * g2 * 1.0)) * gradient[g1][1] +
                  ((peak_raw - (step * g1)) / (step * g2 * 1.0)) * gradient[g2][1]), 255);*/
        g = ((step - (peak_raw/g2)) / (step * 1.0)) * gradient[g1][1] + ((peak_raw/g2) / (step * 1.0)) * gradient[g2][1];
        printf("%d ", g);
        /*b = min(floor((((MAX_GRADIENT_COUNT - peak_raw) - (step * g1)) / (step * g2 * 1.0)) * gradient[g1][2] +
                  ((peak_raw - (step * g1)) / (step * g2 * 1.0)) * gradient[g2][2]), 255);*/
        b = ((step - (peak_raw/g2)) / (step * 1.0)) * gradient[g1][2] + ((peak_raw/g2) / (step * 1.0)) * gradient[g2][2];
        printf("%d\n", b);
    }
    else {
        r = floor(rgb[0] * ((double)peak / 255));
        g = floor(rgb[1] * ((double)peak / 255));
        b = floor(rgb[2] * ((double)peak / 255));
        printf("%d %d %d\n", r, g, b);
    }
    sprintf(cmd, "/usr/local/bin/pigs p 17 %d p 22 %d p 24 %d", r, g, b);

    return;
}