///
/// Created by Anonymous275 on 4/11/2020
///

#include <iostream>
#include <algorithm>
#include <filesystem>

namespace fs = std::experimental::filesystem;

std::string FileList;

void HandleResources(const std::string& path){
    struct stat info{};
    if(stat( "Resources", &info) != 0){
        _wmkdir(L"Resources");
    }
    for (const auto & entry : fs::directory_iterator(path)){
        FileList += entry.path().string() + ";";
    }
    std::replace(FileList.begin(),FileList.end(),'\\','/');
}