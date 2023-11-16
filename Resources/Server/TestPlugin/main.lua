print("test from lua")
print("Player count: " .. MP.GetPlayerCount())

function onPluginLoaded()
    print("HI!")
end

function onPlayerAuthenticated(joined_name, role, is_guest, identifiers)
    print("hi welcome mista " .. joined_name)

    print("current players:")
    for id, name in pairs(MP.GetPlayers()) do
        print("- [" .. id .. "]" .. name)
    end
    print("yipee")

    print("Player count: " .. MP.GetPlayerCount())

    return 0 -- 0 = do not block
end

function onPlayerDisconnect(pid)
    print("Player with PID " .. pid .. " has left!")
end

MP.RegisterEventHandler("onInit", "onPluginLoaded")
MP.RegisterEventHandler("onPlayerAuth", "onPlayerAuthenticated")
MP.RegisterEventHandler("onPlayerDisconnect", "onPlayerDisconnect")
