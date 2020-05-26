///
/// Created by Anonymous275 on 5/20/2020
///

#include "LuaSystem.hpp"
#include "../logger.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include "../Network 2.0/Client.hpp"
std::set<Lua*> PluginEngine;
namespace fs = std::experimental::filesystem;

void RegisterFiles(const std::string& Path){
    std::string Name = Path.substr(Path.find_last_of('\\')+1);
    info("Loading plugin : " + Name);
    for (const auto &entry : fs::directory_iterator(Path)) {
        int pos = entry.path().string().find(".lua");
        if (pos != std::string::npos && entry.path().string().length() - pos == 4) {
            Lua *Script = new Lua();
            PluginEngine.insert(Script);
            Script->SetFileName(entry.path().string());
            Script->SetPluginName(Name);
            Script->Init();
        }
    }
}

void FolderList(const std::string& Path){
    for (const auto &entry : fs::directory_iterator(Path)) {
        int pos = entry.path().filename().string().find('.');
        if (pos == std::string::npos) {
            RegisterFiles(entry.path().string());
        }
    }
}

void LuaMain(std::string Path){
    Path += "/Server";
    struct stat Info{};
    if(stat( Path.c_str(), &Info) != 0){
        fs::create_directory(Path);
    }
    FolderList(Path);
    info("Lua system online");
}
