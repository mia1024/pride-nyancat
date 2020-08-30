/*
 * Copyright (c) 2020 Jerry Wang.
 *
 * Pride flags added by:    Jerry Wang
 *                          https://github.com/pkqxdd/pride-flag-nyancat
 *
 * Animation developed by:  K. Lange
 *                          http://github.com/klange/nyancat
 *                          http://nyancat.dakko.us
 *
 * 40-column support by:    Peter Hazenberg
 *                          http://github.com/Peetz0r/nyancat
 *                          http://peter.haas-en-berg.nl
 *
 * Build tools unified by:  Aaron Peschel
 *                          https://github.com/apeschel
 *
 * For a complete listing of contributors, please see the git commit history.
 *
 * This is a standalone application which renders the
 * classic Nyan Cat (or "poptart cat") with pride flags.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimers.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimers in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the Association for Computing Machinery, K.
 *      Lange, nor the names of its contributors may be used to endorse
 *      or promote products derived from this Software without specific prior
 *      written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * WITH THE SOFTWARE.
 */

#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE 1
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define __BSD_VISIBLE 1

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <setjmp.h>
#include <getopt.h>

#include <sys/ioctl.h>

#ifndef TIOCGWINSZ
#include <termios.h>
#endif

#ifdef ECHO
#undef ECHO
#endif


/*
 * The animation frames are stored separately in
 * this header so they don't clutter the core source
 */
#include "animation_3.c"
#include "animation_4.c"
#include "animation_5.c"
#include "animation_6.c"

/*
 * Color palette to use for final output
 * Specifically, this should be either control sequences
 * or raw characters (ie, for vt220 mode)
 */
const char *colors[256] = {NULL};

/*
 * For most modes, we output spaces, but for some
 * we will use block characters (or even nothing)
 */
const char *output = "  ";

/*
 * Whether or not to show the counter
 */
int show_counter = 1;

/*
 * Number of frames to show before quitting
 * or 0 to repeat forever (default)
 */
unsigned int frame_count = 0;

/*
 * Clear the screen between frames (as opposed to resetting
 * the cursor position)
 */
int clear_screen = 1;

/*
 * Force-set the terminal title.
 */
int set_title = 1;

/*
 * Environment to use for setjmp/longjmp
 * when breaking out of options handler
 */
jmp_buf environment;


/*
 * I refuse to include libm to keep this low
 * on external dependencies.
 *
 * Count the number of digits in a number for
 * use with string output.
 */
int digits(int val) {
    int d = 1, c;
    if (val >= 0) for (c = 10; c <= val; c *= 10) d++;
    else for (c = -10; c >= val; c *= 10) d++;
    return (c < 0) ? ++d : d;
}

/*
 * These values crop the animation, as we have a full 64x64 stored,
 * but we only want to display 40x24 (double width).
 */
int min_row = -1;
int max_row = -1;
int min_col = -1;
int max_col = -1;

/*
 * Actual width/height of terminal.
 */
int terminal_width = 80;
int terminal_height = 24;

/*
 * Flags to keep track of whether width/height were automatically set.
 */
char using_automatic_width = 0;
char using_automatic_height = 0;

/*
 * Print escape sequences to return cursor to visible mode
 * and exit the application.
 */
void finish() {
    if (clear_screen) {
        printf("\033[?25h\033[0m\033[H\033[2J");
    } else {
        printf("\033[0m\n");
    }
    exit(0);
}

/*
 * In the standalone mode, we want to handle an interrupt signal
 * (^C) so that we can restore the cursor and clear the terminal.
 */
void SIGINT_handler(int sig) {
    (void) sig;
    finish();
}


void SIGWINCH_handler(int sig) {
    (void) sig;
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    terminal_width = w.ws_col;
    terminal_height = w.ws_row;

    if (using_automatic_width) {
        min_col = (FRAME_WIDTH - terminal_width / 2) / 2;
        max_col = (FRAME_WIDTH + terminal_width / 2) / 2;
    }

    if (using_automatic_height) {
        min_row = (FRAME_HEIGHT - (terminal_height - 1)) / 2;
        max_row = (FRAME_HEIGHT + (terminal_height - 1)) / 2;
    }

    signal(SIGWINCH, SIGWINCH_handler);
}

void newline(int n) {
    for (int i = 0; i < n; ++i) {
        putc('\n', stdout);
    }
}


/*
 * Print the usage / help text describing options
 */
void usage(char *argv[]) {
    printf(
            "Terminal Nyancat with Pride Flags\n"
            "\n"
            "usage: %s [-htnLGBTQPNA] [-f \033[3mframes\033[0m] [-p l|g|b|t|q|a|nb|p]\n"
            "\n"
            " -L --lesbian    \033[3mShow the nyancat with lesbian flag\033[0m\n"
            " -G --gay    \033[3mShow the nyancat with the gay flag. \033[0m\n"
            " -B --bisexual    \033[3mShow the nyancat with the bisexual flag\033[0m\n"
            " -T --transgender    \033[3mShow the nyancat with the transgender flag\033[0m\n"
            " -Q --queer    \033[3mShow the nyancat with the queer flag\033[0m\n"
            " -P --pansexual    \033[3mShow the nyancat with the pansexual flag\033[0m\n"
            " -N --non-binary    \033[3mShow the nyancat with the non-binary flag\033[0m\n"
            " -A --asexual   \033[3mShow the nyancat with the asexual flag\033[0m\n"
            " -n --no-counter \033[3mDo not display the timer\033[0m\n"
            " -s --no-title   \033[3mDo not set the titlebar text\033[0m\n"
            " -e --no-clear   \033[3mDo not clear the display between frames\033[0m\n"
            " -d --delay      \033[3mDelay image rendering by anywhere between 10ms and 1000ms\n"
            " -f --frames     \033[3mDisplay the requested number of frames, then quit\033[0m\n"
            " -W --width      \033[3mCrop the animation to the given width\033[0m\n"
            " -H --height     \033[3mCrop the animation to the given height\033[0m\n"
            " -h --help       \033[3mShow this help message.\033[0m\n"
            " -p --pride      \033[3mSupports alternative spellings for pride flags.\033[0m\n\n"
            "Supported pride types are: \n"
            "                 lesbian (l)\n"
            "                 gay (g)\n"
            "                 bisexual (bi, b)\n"
            "                 transgender (trans, t)\n"
            "                 queer (q)\n"
            "                 asexual (ace, a)\n"
            "                 non-binary (nonbinary, nb)\n"
            "                 pan-sexual (pansexual, pan, p)\n",
            argv[0]);
}

int main(int argc, char **argv) {

    char *term = NULL;
    unsigned int k;
    int ttype;


    enum flag_type {
        L=0, G=1, B=2, T=3, Q=4, NB=5, A=6, P=7
    };

    srand(time(NULL));
    enum flag_type flag = rand()%8;

    /* Long option names */
    static struct option long_opts[] = {
            {"lesbian",     no_argument,       0, 'L'},
            {"gay",         no_argument,       0, 'G'},
            {"bisexual",    no_argument,       0, 'B'},
            {"transgender", no_argument,       0, 'T'},
            {"queer",       no_argument,       0, 'Q'},
            {"asexual",     no_argument,       0, 'A'},
            {"nonbinary",   no_argument,       0, 'N'},
            {"pansexual",   no_argument,       0, 'P'},
            {"help",        no_argument,       0, 'h'},
            {"no-counter",  no_argument,       0, 'n'},
            {"no-title",    no_argument,       0, 's'},
            {"no-clear",    no_argument,       0, 'e'},
            {"delay",       required_argument, 0, 'd'},
            {"frames",      required_argument, 0, 'f'},
            {"width",       required_argument, 0, 'W'},
            {"height",      required_argument, 0, 'H'},
            {"pride",       required_argument, 0, 'p'},
            {0, 0,                             0, 0}
    };

    /* Time delay in milliseconds */
    int delay_ms = 90; // Default to original value

    /* Process arguments */
    int index, c;
    while ((c = getopt_long(argc, argv, "LGBTQAPNeshnd:f:W:H:p:", long_opts, &index)) != -1) {
        if (!c) {
            if (long_opts[index].flag == 0) {
                c = long_opts[index].val;
            }
        }
        switch (c) {
            case 'e':
                clear_screen = 0;
                break;
            case 's':
                set_title = 0;
                break;
            case 'h': /* Show help and exit */
                usage(argv);
                exit(0);
            case 'n':
                show_counter = 0;
                break;
            case 'd':
                if (10 <= atoi(optarg) && atoi(optarg) <= 1000)
                    delay_ms = atoi(optarg);
                break;
            case 'f':
                frame_count = atoi(optarg);
                break;
            case 'W':
                min_col = (FRAME_WIDTH - atoi(optarg)) / 2;
                max_col = (FRAME_WIDTH + atoi(optarg)) / 2;
                break;
            case 'H':
                min_row = (FRAME_HEIGHT - atoi(optarg)) / 2;
                max_row = (FRAME_HEIGHT + atoi(optarg)) / 2;
                break;
            case 'L':
                flag=L;
                break;
            case 'G':
                flag=G;
                break;
            case 'B':
                flag=B;
                break;
            case 'T':
                flag=T;
                break;
            case 'Q':
                flag=Q;
                break;
            case 'A':
                flag=A;
                break;
            case 'P':
                flag=P;
                break;
            case 'N':
                flag=NB;
                break;
            case 'p':
                if (strcmp(optarg, "lesbian") == 0 || strcmp(optarg, "l") == 0)
                    flag = L;
                else if (strcmp(optarg, "gay") == 0 || strcmp(optarg, "g") == 0)
                    flag = G;
                else if (strcmp(optarg, "bisexual") == 0 || strcmp(optarg, "bi") == 0 || strcmp(optarg, "b") == 0)
                    flag = B;
                else if (strcmp(optarg, "trans") == 0 || strcmp(optarg, "transgender") == 0 || strcmp(optarg, "t") == 0)
                    flag = T;
                else if (strcmp(optarg, "queer") == 0 || strcmp(optarg, "q") == 0)
                    flag = Q;
                else if (strcmp(optarg, "nonbinary") == 0 || strcmp(optarg, "non-binary") == 0 ||
                         strcmp(optarg, "nb") == 0)
                    flag = NB;
                else if (strcmp(optarg, "asexual") == 0 || strcmp(optarg, "a") == 0 || strcmp(optarg, "ace") == 0)
                    flag = A;
                else if (strcmp(optarg, "pansexual") == 0 || strcmp(optarg, "pan-sexual") == 0 ||
                         strcmp(optarg, "pan") == 0 || strcmp(optarg, "p") == 0)
                    flag = P;
                else {
                    printf("Unrecognized pride type %s\n", optarg);
                    exit(1);
                }
                break;
            default:
                exit(1);
        }
    }


    term = getenv("TERM");

    /* Also get the number of columns */
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    terminal_width = w.ws_col;
    terminal_height = w.ws_row;


    /* Default ttype */
    ttype = 2;

    if (term) {
        /* Convert the entire terminal string to lower case */
        for (k = 0; k < strlen(term); ++k) {
            term[k] = tolower(term[k]);
        }

        /* Do our terminal detection */
        if (strstr(term, "xterm")) {
            ttype = 1; /* 256-color, spaces */
        } else if (strstr(term, "toaru")) {
            ttype = 1; /* emulates xterm */
        } else if (strstr(term, "linux")) {
            ttype = 3; /* Spaces and blink attribute */
        } else if (strstr(term, "vtnt")) {
            ttype = 5; /* Extended ASCII fallback == Windows */
        } else if (strstr(term, "cygwin")) {
            ttype = 5; /* Extended ASCII fallback == Windows */
        } else if (strstr(term, "vt220")) {
            ttype = 6; /* No color support */
        } else if (strstr(term, "fallback")) {
            ttype = 4; /* Unicode fallback */
        } else if (strstr(term, "rxvt-256color")) {
            ttype = 1; /* xterm 256-color compatible */
        } else if (strstr(term, "rxvt")) {
            ttype = 3; /* Accepts LINUX mode */
        } else if (strstr(term, "vt100") && terminal_width == 40) {
            ttype = 7; /* No color support, only 40 columns */
        } else if (!strncmp(term, "st", 2)) {
            ttype = 1; /* suckless simple terminal is xterm-256color-compatible */
        }
        char *truecolor = getenv("COLORTERM");
        if (truecolor)
            if (strstr(truecolor, "truecolor")) {
                ttype = 0;
            }
    }

    int always_escape = 0; /* Used for text mode */

    signal(SIGINT, SIGINT_handler);
    signal(SIGWINCH,SIGWINCH_handler);

    const char ***frames;
    switch (flag) {
        case G:
            frames = frames_6; // 6 strips
            break;

        case L:
        case T:
            frames = frames_5; // 5 strips
            break;

        case P:
        case B:
        case Q:
            frames = frames_3; // 3 strips
            break;

        case A:
        case NB:
            frames = frames_4;
            break;
    }


    switch (ttype) {
        case 0:
            colors[','] = "\033[48;2;9;22;128m";  /* Blue background */
            colors['.'] = "\033[48;2;255;255;255m"; /* White stars */
            colors['\''] = "\033[48;2;0;0;0m";  /* Black border */
            colors['@'] = "\033[48;2;248;206;160m"; /* Tan poptart */
            colors['$'] = "\033[48;2;242;160;250m"; /* Pink poptart */
            colors['-'] = "\033[48;2;236;74;151m"; /* Red poptart */
            colors['*'] = "\033[48;2;154;154;154m"; /* Gray cat face */
            colors['%'] = "\033[48;2;242;158;156m"; /* Pink cheeks */
            break;
        case 1:
            colors[','] = "\033[48;5;18m";  /* Blue background */
            colors['.'] = "\033[48;5;231m"; /* White stars */
            colors['\''] = "\033[48;5;16m";  /* Black border */
            colors['@'] = "\033[48;5;223m"; /* Tan poptart */
            colors['$'] = "\033[48;5;219m"; /* Pink poptart */
            colors['-'] = "\033[48;5;204m"; /* Red poptart */
            colors['*'] = "\033[48;5;102m"; /* Gray cat face */
            colors['%'] = "\033[48;5;217m"; /* Pink cheeks */
            break;
        case 2:
            colors[','] = "\033[104m";      /* Blue background */
            colors['.'] = "\033[107m";      /* White stars */
            colors['\''] = "\033[40m";       /* Black border */
            colors['@'] = "\033[47m";       /* Tan poptart */
            colors['$'] = "\033[105m";      /* Pink poptart */
            colors['-'] = "\033[101m";      /* Red poptart */
            colors['*'] = "\033[100m";      /* Gray cat face */
            colors['%'] = "\033[105m";      /* Pink cheeks */
            break;
        default:
            printf("Unsupported terminal. Please use an xterm compatible terminal.\n");
            return 1;
    }

    switch (flag) {
        case L:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;198;59;30m";
                    colors['&'] = "\033[48;2;243;160;99m";
                    colors['+'] = "\033[48;2;255;255;255m";
                    colors['#'] = "\033[48;2;199;106;163m";
                    colors['='] = "\033[48;2;152;31;96m";
                    break;
                case 1:
                    colors['>'] = "\033[48;5;166m";
                    colors['&'] = "\033[48;5;215m";
                    colors['+'] = "\033[48;5;231m";
                    colors['#'] = "\033[48;5;169m";
                    colors['='] = "\033[48;5;89m";
                    break;
                case 2:
                    // 16 color approximation doesn't work on this
                    printf("Unsupported terminal. Please use an xterm compatible terminal.\n");
                    return 1;
                    break;
            }
            break;
        case G:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;236;51;44m"; /* Red  */
                    colors['&'] = "\033[48;2;244;168;74m"; /* Orange  */
                    colors['+'] = "\033[48;2;255;254;104m"; /* Yellow  */
                    colors['#'] = "\033[48;2;53;126;43m"; /* Green  */
                    colors['='] = "\033[48;2;0;28;239m";  /* Light blue  */
                    colors[';'] = "\033[48;2;123;26;121m";  /* Purple  */
                    break;
                case 1:
                    colors['>'] = "\033[48;5;202m"; /* Red  */
                    colors['&'] = "\033[48;5;215m"; /* Orange  */
                    colors['+'] = "\033[48;5;227m"; /* Yellow  */
                    colors['#'] = "\033[48;5;64m"; /* Green  */
                    colors['='] = "\033[48;5;21m";  /* Light blue  */
                    colors[';'] = "\033[48;5;90m";  /* Purple  */
                    break;
                case 2:
                    colors['>'] = "\033[101m";      /* Red  */
                    colors['&'] = "\033[43m";       /* Orange  */
                    colors['+'] = "\033[103m";      /* Yellow  */
                    colors['#'] = "\033[102m";      /* Green  */
                    colors['='] = "\033[104m";      /* Light blue  */
                    colors[';'] = "\033[45m";       /* Dark blue  */
                    break;
            }
            break;
        case T: /*5 strips, > & + # =, */
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;120;205;246m"; /* blue */
                    colors['&'] = "\033[48;2;235;174;186m"; /* pink */
                    colors['+'] = "\033[48;2;255;255;255m"; /* white */
                    colors['#'] = "\033[48;2;235;174;186m"; /* pink */
                    colors['='] = "\033[48;2;120;205;246m";  /* blue */
                    break;
                case 1:
                    colors['>'] = "\033[48;5;117m"; /* blue */
                    colors['&'] = "\033[48;5;217m"; /* pink */
                    colors['+'] = "\033[48;5;231m"; /* white */
                    colors['#'] = "\033[48;5;217m"; /* pink */
                    colors['='] = "\033[48;5;117m";  /* blue */
                    break;
                case 2:
                    colors['>'] = "\033[106m";      /* blue */
                    colors['&'] = "\033[105m";       /* pink */
                    colors['+'] = "\033[107m";      /* white */
                    colors['#'] = "\033[105m";      /* pink */
                    colors['='] = "\033[106m";      /* blue */
                    break;
            }
            break;
        case B:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;199;43;112m";
                    colors['+'] = "\033[48;2;147;84;148m";
                    colors['='] = "\033[48;2;14;56;163m";
                    break;
                case 1:
                    colors['>'] = "\033[48;5;161m";
                    colors['+'] = "\033[48;5;96m";
                    colors['='] = "\033[48;5;25m";
                    break;
                case 2:
                    colors['>'] = "\033[41m";
                    colors['+'] = "\033[45m";
                    colors['='] = "\033[104m";
                    break;
            }
            break;
        case Q:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;175;131;215m";
                    colors['+'] = "\033[48;2;255;255;255m";
                    colors['='] = "\033[48;2;86;128;48m";
                    break;
                case 1:
                    colors['>'] = "\033[48;5;140m";
                    colors['+'] = "\033[48;5;231m";
                    colors['='] = "\033[48;5;65m";
                    break;
                case 2:
                    colors['>'] = "\033[105m";
                    colors['+'] = "\033[107m";
                    colors['='] = "\033[102m";
                    break;
            }
            break;
        case NB:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;254;243;93m";
                    colors['+'] = "\033[48;2;255;255;255m";
                    colors['#'] = "\033[48;2;147;95;203m";
                    colors[';'] = "\033[48;2;0;0;0m";
                    break;
                case 1:
                    colors['>'] = "\033[48;5;227m";
                    colors['+'] = "\033[48;5;231m";
                    colors['#'] = "\033[48;5;98m";
                    colors[';'] = "\033[48;5;16m";
                    break;
                case 2:
                    colors['>'] = "\033[103m";
                    colors['+'] = "\033[107m";
                    colors['#'] = "\033[45m";
                    colors[';'] = "\033[40m";
                    break;
            }
            break;
        case A:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;0;0;0m";
                    colors['+'] = "\033[48;2;164;164;164m";
                    colors['#'] = "\033[48;2;255;255;255m";
                    colors[';'] = "\033[48;2;119;25;125m";
                    break;
                case 1:
                    colors['>'] = "\033[48;5;16m";
                    colors['+'] = "\033[48;5;145m";
                    colors['#'] = "\033[48;5;231m";
                    colors[';'] = "\033[48;5;90m";
                    break;
                case 2:
                    colors['>'] = "\033[40m";
                    colors['+'] = "\033[47m";
                    colors['#'] = "\033[107m";
                    colors[';'] = "\033[45m";
                    break;
            }
            break;
        case P:
            switch (ttype) {
                case 0:
                    colors['>'] = "\033[48;2;236;61;140m";
                    colors['+'] = "\033[48;2;250;217;74m";
                    colors['='] = "\033[48;2;80;177;249m";
                    break;
                case 1:
                    colors['>'] = "\033[48;5;204m";
                    colors['+'] = "\033[48;5;221m";
                    colors['='] = "\033[48;5;75m";
                    break;
                case 2:
                    colors['>'] = "\033[105m";
                    colors['+'] = "\033[103m";
                    colors['='] = "\033[107m";
                    break;
            }
            break;
    }

    if (min_col == max_col) {
        min_col = (FRAME_WIDTH - terminal_width / 2) / 2;
        max_col = (FRAME_WIDTH + terminal_width / 2) / 2;
        using_automatic_width = 1;
    }

    if (min_row == max_row) {
        min_row = (FRAME_HEIGHT - (terminal_height - 1)) / 2;
        max_row = (FRAME_HEIGHT + (terminal_height - 1)) / 2;
        using_automatic_height = 1;
    }

    /* Attempt to set terminal title */
    if (set_title) {
        printf("\033kNyanyanyanyanyanyanya...\033\134");
        printf("\033]1;Nyanyanyanyanyanyanya...\007");
        printf("\033]2;Nyanyanyanyanyanyanya...\007");
    }

    if (clear_screen) {
        /* Clear the screen */
        printf("\033[H\033[2J\033[?25l");
    } else {
        printf("\033[s");
    }

    /* Store the start time */
    time_t start, current;
    time(&start);

    size_t i = 0;       /* Current frame # */
    unsigned int f = 0; /* Total frames passed */
    char last = 0;      /* Last color index rendered */
    int y, x;        /* x/y coordinates of what we're drawing */
    for (;;) {
        /* Reset cursor */
        if (clear_screen) {
            printf("\033[H");
        } else {
            printf("\033[u");
        }
        /* Render the frame */
        for (y = min_row; y < max_row; ++y) {
            for (x = min_col; x < max_col; ++x) {
                char color;
                if (y > 23 && y < 43 && x < 0) {
                    /*
                     * Generate the rainbow tail.
                     *
                     * This is done with a pretty simplistic square wave.
                     */
                    int mod_x = ((-x + 2) % 16) / 8;
                    if ((i / 2) % 2) {
                        mod_x = 1 - mod_x;
                    }
                    const char *rainbow;
                    switch (flag) {
                        case G:
                            rainbow = ",,>>&&&+++###==;;;,,"; // 6 strips
                            break;

                        case L:
                        case T:
                            rainbow = ",,>>&&&+++###==,,,,,"; // 5 strips
                            break;

                        case P:
                        case B:
                        case Q:
                            rainbow = ",,>>>>>++++++=====,,"; // 3 strips
                            break;

                        case A:
                        case NB:
                            rainbow = ",,>>>>++++####;;;;,,"; // 4 strips
                            break;
                    }

                    color = rainbow[mod_x + y - 23];
                    if (color == 0) color = ',';
                } else if (x < 0 || y < 0 || y >= FRAME_HEIGHT || x >= FRAME_WIDTH) {
                    /* Fill all other areas with background */
                    color = ',';
                } else {
                    /* Otherwise, get the color from the animation frame. */
                    color = frames[i][y][x];
                }
                if (always_escape) {
                    /* Text mode (or "Always Send Color Escapes") */
                    printf("%s", colors[(int) color]);
                } else {
                    if (color != last && colors[(int) color]) {
                        /* Normal Mode, send escape (because the color changed) */
                        last = color;
                        printf("%s%s", colors[(int) color], output);
                    } else {
                        /* Same color, just send the output characters */
                        printf("%s", output);
                    }
                }
            }
            /* End of row, send newline */
            newline(1);
        }
        if (show_counter) {
            /* Get the current time for the "You have nyaned..." string */
            time(&current);
            double diff = difftime(current, start);
            /* Now count the length of the time difference so we can center */
            int nLen = digits((int) diff);
            /*
             * 29 = the length of the rest of the string;
             * XXX: Replace this was actually checking the written bytes from a
             * call to sprintf or something
             */
            int width = (terminal_width - 29 - nLen) / 2;
            /* Spit out some spaces so that we're actually centered */
            while (width > 0) {
                printf(" ");
                width--;
            }
            /* You have nyaned for [n] seconds!
             * The \033[J ensures that the rest of the line has the dark blue
             * background, and the \033[1;37m ensures that our text is bright white.
             * The \033[0m prevents the Apple ][ from flipping everything, but
             * makes the whole nyancat less bright on the vt220
             */
            printf("\033[1;37mYou have prided for %0.0f seconds!\033[J\033[0m", diff);
        }
        /* Reset the last color so that the escape sequences rewrite */
        last = 0;
        /* Update frame count */
        ++f;
        if (frame_count != 0 && f == frame_count) {
            finish();
            return 0;
        }
        ++i;
        if (!frames[i]) {
            /* Loop animation */
            i = 0;
        }
        /* Wait */
        usleep(1000 * delay_ms);
    }
}
