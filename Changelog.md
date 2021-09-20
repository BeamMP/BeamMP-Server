# v2.4.0

- CHANGED entire plugin Lua implementation (rewrite)
- CHANGED moved *all* functions into MP.\*
- CHANGED all files of a Lua plugin to share a Lua state (no more state-per-file)
- ADDED `MP.GetOSName() -> string`: Returns "Linux", "Windows" or "Other"
- ADDED `MP.GetServerVersion() -> string`: Returns major,minor,patch version
- ADDED `MP.IsPlayerGuest(id) -> boolean`: Whether player with id is a guest
- ADDED `MP.Settings` table providing aliases for 0,1,2,etc. in MP.Set(id,val)
- ADDED `MP.PrintRaw(...)`: prints messages without `[TIME DATE] [LUA]` etc.
- ADDED `FS` table to host some effective filesystem manipulation functions
- ADDED `FS.CreateDirectory(path) -> bool,string`: Creates the path's directory (all missing pieces) and returns whether it fails and why if it did
- ADDED `FS.Exists(path) -> bool`: Whether the file exists
- ADDED `FS.Rename(old,new) -> bool,string`: Renames a file or folder (same as "move")
- ADDED `FS.Remove(path) -> bool,string`: Removes a file or (empty) folder
- ADDED `FS.Copy(original,copy) -> bool,string`: Copies a file or directory
- ADDED plugin directories to `package.path` and `package.cpath` before `onInit`
- ADDED ability to add `PluginConfig.toml` to your plugin folder to change some settings
- ADDED ability to share a lua state with other plugins via `StateId` setting in `PluginConfig.toml`
- ADDED `MP.HttpsGET(host,port,target) -> status,body`: Does a synchronous HTTPS GET request
- ADDED `MP.HttpsPOST(host,port,target,body,content_type) -> status,body`: Does a synchronous HTTPS POST request
- ADDED `MP.GetStateMemoryUsage() -> number`: Current memory usage of the current state in bytes
- ADDED `MP.GetLuaMemoryUsage() -> number`: Current memory usage of all states combined, in bytes
- ADDED `MP.CreateEventTimer(event,interval_ms)`: Replacement for `CreateThread` - calls the event in the given interval
- ADDED `MP.CancelEventTimer(event)`: Cancels all event timers for that event

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