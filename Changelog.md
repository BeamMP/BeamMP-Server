# v2.3.0

- ADDED logging of various errors, crashes and exceptions to the backend
- ADDED `[CHAT]` messages to server console

# v2.2.0

- FIXED major security flaw
- FIXED minor bugs

# v2.1.4

- ADDED debug heartbeat print
- ADDED kicking every player before shutdown
- FIXED rare bug which led to violent crash
- FIXED minor bugs

# v2.1.3

- FIXED Lua events not cancelling properly on Linux

# v2.1.2

- CHANGED default map to gridmap v2
- FIXED version number display

# v2.1.1
# v2.1.0 (pre-v2.1.1)
# v2.0.4 (pre-v2.1.0)

- REMOVED boost as a runtime dependency
- FIXED Lua plugins on Linux
- FIXED console history on Windows
- CHANGED to new config format TOML

# v2.0.3

- WORKAROUND for timeout bug / ghost player bug
- FIXED 100% CPU spin when stdin is /dev/null.

# v2.0.2

- ADDED fully new commandline
- ADDED new backend
- ADDED automated build system
- ADDED lua GetPlayerIdentifiers
- ADDED lots of debug info
- ADDED better POSTing and GETing
- ADDED a license
- FIXED ghost players in player list issue
- FIXED ghost vehicle after joining issue
- FIXED missing vehicle after joining issue
- FIXED a lot of desync issues
- FIXED some memory leaks
- FIXED various crashes
- FIXED various data-races
- FIXED some linux-specific crashes
- FIXED some linux-specific issues
- FIXED bug which caused kicking to be logged as leaving
- FIXED various internal developer quality-of-life things
