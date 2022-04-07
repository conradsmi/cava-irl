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

unsigned char *getgradient(char *line, unsigned char **gradient_raw, float amplifier, char use_sig,
                           int gradient_count) {
    int peak_raw, step_adj_peak, peak;
    int g1, g2, step;
    unsigned char *rgb = calloc(3, sizeof(char));

    peak_raw = floor(processline(line, amplifier, use_sig));
    peak = peak_raw * 0.255;
    step = MAX_GRADIENT_COUNT / (gradient_count-1);

    g1 = peak_raw / step;
    g1 -= (g1 == gradient_count - 1);
    g2 = g1 + 1;
    step_adj_peak = peak_raw - (g1*step);

    rgb[0] = min(((step - step_adj_peak) / (double)step) * gradient_raw[g1][0] + (step_adj_peak / (double)step) * gradient_raw[g2][0], 255);
    rgb[1] = min(((step - step_adj_peak) / (double)step) * gradient_raw[g1][1] + (step_adj_peak / (double)step) * gradient_raw[g2][1], 255);
    rgb[2] = min(((step - step_adj_peak) / (double)step) * gradient_raw[g1][2] + (step_adj_peak / (double)step) * gradient_raw[g2][2], 255);

    return rgb;
}

unsigned char *getsolid(char *line, unsigned char *solid_raw, float amplifier, char use_sig) {
    double peak;
    unsigned char *rgb = calloc(3, sizeof(char));

    peak = floor(processline(line, amplifier, use_sig) * 0.255);
    rgb[0] = floor(solid_raw[0] * (peak / 255));
    rgb[1] = floor(solid_raw[1] * (peak / 255));
    rgb[2] = floor(solid_raw[2] * (peak / 255));
 
    return rgb;
}

void getcmd(char *line, unsigned char *solid_raw, unsigned char **gradient_raw, char GRADIENT,
            int gradient_count, float amplifier, char use_sig, char *cmd) {
    unsigned char *rgb;

    if (GRADIENT && gradient_count > 1) {
        rgb = getgradient(line, gradient_raw, amplifier, use_sig, gradient_count);
    }
    else {
        rgb = getsolid(line, solid_raw, amplifier, use_sig);
    }
    sprintf(cmd, "/usr/local/bin/pigs p 17 %d p 22 %d p 24 %d", rgb[0], rgb[1], rgb[2]);
    free(rgb);

    return;
}

/*void getcmd(char *line, unsigned char *rgb_raw, float amplifier, char use_sig, char *cmd) {
    unsigned char *rgb = getcolors(line, rgb_raw, amplifier, use_sig);
    sprintf(cmd, "/usr/local/bin/pigs p 17 %d p 22 %d p 24 %d", rgb[0], rgb[1], rgb[2]);
    free(rgb);
    return;
}*/