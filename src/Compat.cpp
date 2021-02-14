#include "Compat.h"

#ifndef WIN32

static struct termios old, current;

void initTermios(int echo) {
    tcgetattr(0, &old); /* grab old terminal i/o settings */
    current = old; /* make new settings same as old settings */
    current.c_lflag &= ~ICANON; /* disable buffered i/o */
    if (echo) {
        current.c_lflag |= ECHO; /* set echo mode */
    } else {
        current.c_lflag &= ~ECHO; /* set no echo mode */
    }
    tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
}

void resetTermios(void) {
    tcsetattr(0, TCSANOW, &old);
}

char getch_(int echo) {
    char ch;
    initTermios(echo);
    read(STDIN_FILENO, &ch, 1);
    resetTermios();
    return ch;
}

char _getch(void) {
    return getch_(0);
}

#endif // !WIN32
