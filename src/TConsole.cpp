#include "TConsole.h"
#include "Compat.h"

TConsole::TConsole() {
    _Commandline.enable_history();
    _Commandline.set_history_limit(20);
    _Commandline.on_command = [](Commandline& c) {
        auto cmd = c.get_command();
        if (cmd == "exit") {
            _Exit(0);
        } else if (cmd == "clear" || cmd == "cls") {
            // TODO: clear screen
        } else {
            // TODO: execute as lua
        }
    };
}
