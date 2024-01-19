// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
    if (read(STDIN_FILENO, &ch, 1) < 0) {
        // ignore, not much we can do
    }
    resetTermios();
    return ch;
}

char _getch(void) {
    return getch_(0);
}

#endif // !WIN32
