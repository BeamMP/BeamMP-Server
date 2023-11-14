print("test from lua")
print("Player count: " .. MP.GetPlayerCount())

function onPluginLoaded()
    print("HI!")
end

function onPlayerAuthenticated(name)
    print("hi welcome mista " .. name)

    print("current players:")
    for id, name in pairs(MP.GetPlayers()) do
        print("- [" .. id .. "]" .. name)
    end
end

MP.RegisterEventHandler("onPluginLoaded", "onPluginLoaded")
MP.RegisterEventHandler("onPlayerAuthenticated", "onPlayerAuthenticated")
