# v3.0.2

- ADDED Periodic update message if a new server is released
- ADDED Config setting for the IP the http server listens on
- CHANGED Default MaxPlayers to 8
- CHANGED Default http server listen IP to localhost
- FIXED `MP.CreateEventTimer` filling up the queue (see <https://wiki.beammp.com/en/Scripting/new-lua-scripting#mpcreateeventtimerevent_name-string-interval_ms-number-strategy-number-since-v302>)
- FIXED `MP.TriggerClientEvent` not kicking the client if it failed
- FIXED Lua result queue handling not checking all results
- FIXED bug which caused ServerConfig.toml to generate incorrectly

# v3.0.1

- ADDED Backup URLs to UpdateCheck (will fail less often now)
- ADDED console cursor left and right movement (with arrow keys) and working HOME and END key (via github.com/lionkor/commandline)
- FIXED infinite snowmen / infinite unicycle spawning bug
- FIXED a bug where, when run with --working-directory, the Server.log would still be in the original directory
- FIXED a bug which could cause the plugin reload thread to spin at 100% if the reloaded plugin's didn't terminate
- FIXED an issue which would cause servers to crash on mod download via SIGPIPE on POSIX
- FIXED an issue which would cause servers to crash when checking if a vehicle is a unicycle

# v3.0.0

- CHANGED entire plugin Lua implementation (rewrite)
- CHANGED moved *almost all* Lua functions into MP.\*
- CHANGED console to use a custom language (type `help`, `list`, or `status`!)
- CHANGED all files of a Lua plugin to share a Lua state (no more state-per-file)
- ADDED many new Lua API functions, which can be found at <https://wiki.beammp.com/en/Scripting/functions>
- ADDED Commandline options. Run with `--help` to see all options.
- ADDED HTTP(S) Server (OpenAPI spec coming soon!)
- ADDED plugin directories to `package.path` and `package.cpath` before `onInit`
- ADDED ability to add `PluginConfig.toml` to your plugin folder to change some settings
- ADDED ability to share a lua state with other plugins via `StateId` setting in `PluginConfig.toml`
- ADDED ability to see name-to-thread-ID association in debug mode
- ADDED dumping tables with `print()` (try it with `print(MP)`)
- ADDED `MP.GetOSName()`, `MP.CreateTimer()`, `MP.GetLuaMemoryUsage()` and many more (see <https://wiki.beammp.com/en/Scripting/functions>)
- ADDED `MP.Settings` table to make usage of `MP.Set()` easier
- ADDED `FS.*` table with common filesystem operations (do `print(FS)` to see them!)
- FIXED i/o thread spin when stdout is /dev/null on linux
- FIXED removed extra whitespace infront of onChatMessage message

# v2.3.3

- CHANGED servers to be private by default

# v2.3.2

- ADDED Ctrl+C causes a graceful shutdown on windows (did already on linux)
- ADDED more meaningful shutdown messages
- ADDED even better backend connection error reporting
- ADDED `SendErrors` config in `ServerConfig.toml` to opt-out of error reporting
- ADDED hard-shutdown if Ctrl+C pressed 3 times
- FIXED issue with shells like bash being unusable after server exit

# v2.3.1

- CHANGED join/sync timeout to 20 minutes, players wont drop if loading takes >5 mins

# v2.3.0

- ADDED version check - the server will now let you know when a new release is out
- ADDED logging of various errors, crashes and exceptions to the backend
- ADDED chat messages are now logged to the server console as [CHAT]
- ADDED debug message telling you when the server heartbeats to the backend
- REMOVED various [DEBUG] messages which were confusing (such as "breaking client loop")
- FIXED various crashes and issues with handling unexpected backend responses
- FIXED minor bugs due to code correctness

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
