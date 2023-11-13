print("test from lua")
print("Player count: " .. MP.GetPlayerCount())

function onPluginLoaded2()
    print("HI!")
end

function onPlayerAuthenticated2(name)
    print("hi welcome mista " .. name)
end

MP.RegisterEventHandler("onPluginLoaded", "onPluginLoaded2")
MP.RegisterEventHandler("onPlayerAuthenticated", "onPlayerAuthenticated2")
