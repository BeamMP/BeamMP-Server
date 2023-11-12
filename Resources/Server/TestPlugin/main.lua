print("test from lua")

function onPluginLoaded()
    print("HI!")
end

MP:RegisterEventHandler("onPluginLoaded", "onPluginLoaded")
