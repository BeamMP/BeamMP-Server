///
/// Created by Anonymous275 on 7/28/2020
///
#include "Security/Enc.h"
#include <filesystem>
#include "Settings.h"
#include <algorithm>
#include "Logger.h"
namespace fs = std::experimental::filesystem;
uint64_t MaxModSize = 0;
std::string FileSizes;
std::string FileList;
int ModsLoaded = 0;

void InitRes(){
    std::string Path = Resource + Sec("/Client");
    if(!fs::exists(Path))fs::create_directory(Path);
    for (const auto & entry : fs::directory_iterator(Path)){
        auto pos = entry.path().string().find(Sec(".zip"));
        if(pos != std::string::npos){
            if(entry.path().string().length() - pos == 4){
                FileList += entry.path().string() + ";";
                FileSizes += std::to_string(fs::file_size(entry.path()))+";";
                MaxModSize += fs::file_size(entry.path());
                ModsLoaded++;
            }
        }
    }
    std::replace(FileList.begin(),FileList.end(),'\\','/');
    if(ModsLoaded){
        info(Sec("Loaded ")+std::to_string(ModsLoaded)+Sec(" Mods"));
    }
}