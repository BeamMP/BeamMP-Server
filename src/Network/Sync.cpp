// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 8/1/2020
///

#include "Client.hpp"
#include "Logger.h"
#include "Network.h"
#include "Security/Enc.h"
#include "Settings.h"
#include "UnixCompat.h"
#include <filesystem>
#include <fstream>

bool TCPSendRaw(SOCKET C, char* Data, int32_t Size) {
    int64_t Sent = 0;
    do {
        int64_t Temp = send(C, &Data[Sent], int(Size - Sent), 0);
        if (Temp < 1) {
            info("Socket Closed! " + std::to_string(C));
            CloseSocketProper(C);
            return false;
        }
        Sent += Temp;
    } while (Sent < Size);
    return true;
}

void SplitLoad(Client* c, int64_t Sent, int64_t Size, bool D, const std::string& Name) {
    std::ifstream f(Name.c_str(), std::ios::binary);
    int32_t Split = 0x7735940; //125MB
    char* Data;
    if (Size > Split)
        Data = new char[Split];
    else
        Data = new char[Size];
    SOCKET TCPSock;
    if (D)
        TCPSock = c->GetDownSock();
    else
        TCPSock = c->GetTCPSock();
    info("Split load Socket " + std::to_string(TCPSock));
    while (c->GetStatus() > -1 && Sent < Size) {
        int64_t Diff = Size - Sent;
        if (Diff > Split) {
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Split);
            if (!TCPSendRaw(TCPSock, Data, Split)) {
                if (c->GetStatus() > -1)
                    c->SetStatus(-1);
                break;
            }
            Sent += Split;
        } else {
            f.seekg(Sent, std::ios_base::beg);
            f.read(Data, Diff);
            if (!TCPSendRaw(TCPSock, Data, int32_t(Diff))) {
                if (c->GetStatus() > -1)
                    c->SetStatus(-1);
                break;
            }
            Sent += Diff;
        }
    }
    delete[] Data;
    f.close();
}

void SendFile(Client* c, const std::string& Name) {
    Assert(c);
    info(c->GetName() + " requesting : " + Name.substr(Name.find_last_of('/')));

    if (!std::filesystem::exists(Name)) {
        TCPSend(c, "CO");
        warn("File " + Name + " could not be accessed!");
        return;
    } else
        TCPSend(c, "AG");

    ///Wait for connections
    int T = 0;
    while (c->GetDownSock() < 1 && T < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        T++;
    }

    if (c->GetDownSock() < 1) {
        error("Client doesn't have a download socket!");
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
        return;
    }

    int64_t Size = std::filesystem::file_size(Name), MSize = Size / 2;

    std::thread Dt(SplitLoad, c, 0, MSize, false, Name);
    Dt.detach();

    SplitLoad(c, MSize, Size, true, Name);

    if (Dt.joinable())
        Dt.join();
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
            debug("Sending Mod Info");
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
    if (c == nullptr)
        return;
#ifndef DEBUG
    try {
#endif
        TCPSend(c, "P" + std::to_string(c->GetID()));
        std::string Data;
        while (c->GetStatus() > -1) {
            Data = TCPRcv(c);
            if (Data == "Done")
                break;
            Parse(c, Data);
        }
#ifndef DEBUG
    } catch (std::exception& e) {
        except("Exception! : " + std::string(e.what()));
        c->SetStatus(-1);
    }
#endif
}
