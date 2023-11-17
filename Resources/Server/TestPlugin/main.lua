print("test from lua")
print("Player count: " .. MP.GetPlayerCount())

function onPluginLoaded()
    print("HI!")
end

function onPlayerAuthenticated(joined_name, role, is_guest, identifiers)
    print("hi welcome mista " .. joined_name)
    print("u a guest? " .. tostring(is_guest))

    print("current players:")
    for id, name in pairs(MP.GetPlayers()) do
        print("- [" .. id .. "]" .. name)
    end
    print("yipee")

    print("Player count: " .. MP.GetPlayerCount())

    for key, value in pairs(identifiers) do
        print(key .. ": " .. value)
    end

    -- print("now trying with getting it ourself")
    -- for key, value in pairs(MP.GetPlayerIdentifiers(pid)) do
    --     print(key .. ": " .. value)
    -- end

    return 0 -- 0 = do not cancel
end

function onPlayerDisconnect(pid, name, identifiers)
    -- Player is already gone here, so for now, player data is no longer
    -- requestable for player with the id `pid`
    print("Player " .. name .. " (" .. pid .. ") has left!")
end

MP.RegisterEventHandler("onInit", "onPluginLoaded")
MP.RegisterEventHandler("onPlayerAuth", "onPlayerAuthenticated")
MP.RegisterEventHandler("onPlayerDisconnect", "onPlayerDisconnect")
