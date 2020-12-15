///
/// Created by Anonymous275 on 8/1/2020
///
#include "Security/Enc.h"
#include "UnixCompat.h"
#include "Settings.h"
#include "Client.hpp"
#include "Network.h"
#include "Logger.h"
#include <fstream>
#ifdef __linux
// we need this for `struct stat`
#include <sys/stat.h>
#endif // __linux

void SendFile(Client* c, const std::string& Name) {
    Assert(c);
    info(c->GetName() + " requesting : " + Name.substr(Name.find_last_of('/')));
    struct stat Info { };
    if (stat(Name.c_str(), &Info) != 0) {
        TCPSend(c, "Cannot Open");
        return;
    }
    std::ifstream f(Name.c_str(), std::ios::binary);
    f.seekg(0, std::ios_base::end);
    std::streampos fileSize = f.tellg();
    auto Size = size_t(fileSize);
    size_t Sent = 0;
    size_t Diff;
    int64_t Split = 64000;
    while (c->GetStatus() > -1 && Sent < Size) {
        Diff = Size - Sent;
        if (Diff > size_t(Split)) {
            std::string Data(size_t(Split), 0);
            f.seekg(int64_t(Sent), std::ios_base::beg);
            f.read(&Data[0], Split);
            TCPSend(c, Data);
            Sent += size_t(Split);
        } else {
            std::string Data(Diff, 0);
            f.seekg(int64_t(Sent), std::ios_base::beg);
            f.read(&Data[0], int64_t(Diff));
            TCPSend(c, Data);
            Sent += Diff;
        }
    }
    f.close();
}

void Parse(Client* c, const std::string& Packet) {
    Assert(c);
    if (c == nullptr || Packet.empty())
        return;
    char Code = Packet.at(0), SubCode = 0;
    if (Packet.length() > 1)
        SubCode = Packet.at(1);
    switch (Code) {
    case 'f':
        SendFile(c, Packet.substr(1));
        return;
    case 'S':
        if (SubCode == 'R') {
            debug(Sec("Sending Mod Info"));
            std::string ToSend = FileList + FileSizes;
            if (ToSend.empty())
                ToSend = "-";
            TCPSend(c, ToSend);
        }
        return;
    default:
        return;
    }
}

void SyncResources(Client* c) {
    Assert(c);
    if (c == nullptr)return;
    try {
        TCPSend(c, "WS");
        std::string Data;
        while (c->GetStatus() > -1){
            Data = TCPRcv(c);
            if(Data == "Done")break;
            Parse(c, Data);
        }
    } catch (std::exception& e) {
        except("Exception! : " + std::string(e.what()));
        c->SetStatus(-1);
    }
}
