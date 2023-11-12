print("test from lua")
print("Player count: " .. MP.GetPlayerCount())

function onPluginLoaded()
    print("HI!")
end

MP.RegisterEventHandler("onPluginLoaded", "onPluginLoaded")
