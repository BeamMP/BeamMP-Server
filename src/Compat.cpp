#include "Compat.h"

#include <cstring>
#include <doctest/doctest.h>

#ifndef WIN32

static struct termios old, current;

static void initTermios(int echo) {
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

static void resetTermios(void) {
    tcsetattr(0, TCSANOW, &old);
}

TEST_CASE("init and reset termios") {
    if (isatty(STDIN_FILENO)) {
        struct termios original;
        tcgetattr(0, &original);
        SUBCASE("no echo") {
            initTermios(false);
        }
        SUBCASE("yes echo") {
            initTermios(true);
        }
        resetTermios();
        struct termios current;
        tcgetattr(0, &current);
        CHECK_EQ(std::memcmp(&current.c_cc, &original.c_cc, sizeof(current.c_cc)), 0);
        CHECK_EQ(current.c_cflag, original.c_cflag);
        CHECK_EQ(current.c_iflag, original.c_iflag);
        CHECK_EQ(current.c_ispeed, original.c_ispeed);
        CHECK_EQ(current.c_lflag, original.c_lflag);
        CHECK_EQ(current.c_line, original.c_line);
        CHECK_EQ(current.c_oflag, original.c_oflag);
        CHECK_EQ(current.c_ospeed, original.c_ospeed);
    }
}

static char getch_(int echo) {
    char ch;
    initTermios(echo);
    (void)read(STDIN_FILENO, &ch, 1);
    resetTermios();
    return ch;
}

char _getch(void) {
    return getch_(0);
}

#endif // !WIN32
