///
/// Created by Anonymous275 on 4/11/2020
///

#include <algorithm>
#include <filesystem>

namespace fs = std::experimental::filesystem;

std::string FileList;
std::string FileSizes;

void HandleResources(const std::string& path){
    struct stat info{};
    if(stat( "Resources", &info) != 0){
        _wmkdir(L"Resources");
    }
    for (const auto & entry : fs::directory_iterator(path)){
        int pos = entry.path().string().find(".zip");
        if(pos != std::string::npos){
            if(entry.path().string().length() - pos == 4){
                FileList += entry.path().string() + ";";
                FileSizes += std::to_string(fs::file_size(entry.path()))+";";
            }
        }
    }
    std::replace(FileList.begin(),FileList.end(),'\\','/');
}