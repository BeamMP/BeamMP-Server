print("test from lua")
print("Player count: " .. MP.GetPlayerCount())

function onPluginLoaded()
    print("HI!")
end

function onPlayerAuthenticated(joined_name)
    print("hi welcome mista " .. joined_name)

    print("current players:")
    for id, name in pairs(MP.GetPlayers()) do
        print("- [" .. id .. "]" .. name)
    end
    print("yipee")

    return 0 -- 0 = do not block
end

MP.RegisterEventHandler("onPluginLoaded", "onPluginLoaded")
MP.RegisterEventHandler("onPlayerAuthenticated", "onPlayerAuthenticated")
