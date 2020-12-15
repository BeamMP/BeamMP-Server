///
/// Created by Anonymous275 on 8/1/2020
///
#include "Compressor.h"
#include "Logger.h"
#include "Network.h"
#include "Security/Enc.h"
#include "UnixCompat.h"
#include <thread>

bool TCPSend(Client* c, const std::string& Data) {
    Assert(c);
    if (c == nullptr)
        return false;
    // Size is BIG ENDIAN now, use only for header!
    //auto Size = htonl(int32_t(Data.size()));
    ///TODO : BIG ENDIAN for other OS
    int32_t Size, Sent, Temp;
    std::string Send(4, 0);
    Size = int32_t(Data.size());
    memcpy(&Send[0], &Size, sizeof(Size));
    Send += Data;
    Sent = 0;
    Size += 4;
    do {
        Temp = send(c->GetTCPSock(), &Send[Sent], Size - Sent, 0);
        if (Temp == 0) {
            if (c->GetStatus() > -1)
                c->SetStatus(-1);
            return false;
        } else if (Temp < 0) {
            if (c->GetStatus() > -1)
                c->SetStatus(-1);
            // info(Sec("Closing socket, Temp < 0"));
            CloseSocketProper(c->GetTCPSock());
            return false;
        }
        Sent += Temp;
    } while (Sent < Size);
    return true;
}

bool CheckBytes(Client* c, int32_t BytesRcv) {
    Assert(c);
    if (BytesRcv == 0) {
        debug(Sec("(TCP) Connection closing..."));
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
        return false;
    } else if (BytesRcv < 0) {
#ifdef WIN32
        debug(Sec("(TCP) recv failed with error: ") + std::to_string(WSAGetLastError()));
#else // unix
        debug(Sec("(TCP) recv failed with error: ") + std::string(strerror(errno)));
#endif // WIN32
        if (c->GetStatus() > -1)
            c->SetStatus(-1);
        info(Sec("Closing socket in CheckBytes, BytesRcv < 0"));
        CloseSocketProper(c->GetTCPSock());
        return false;
    }
    return true;
}

std::string TCPRcv(Client* c) {
    Assert(c);
    int32_t Header, BytesRcv = 0, Temp;
    if (c == nullptr || c->GetStatus() < 0)
        return "";

    std::vector<char> Data(sizeof(Header));
    do {
        Temp = recv(c->GetTCPSock(), &Data[BytesRcv], 4 - BytesRcv, 0);
        if (!CheckBytes(c, Temp)) {
#ifdef DEBUG
            error(std::string(__func__) + Sec(": failed on CheckBytes in while(BytesRcv < 4)"));
#endif // DEBUG
            return "";
        }
        BytesRcv += Temp;
    } while (size_t(BytesRcv) < sizeof(Header));
    memcpy(&Header, &Data[0], sizeof(Header));

#ifdef DEBUG
    //debug(std::string(__func__) + Sec(": expecting ") + std::to_string(Header) + Sec(" bytes."));
#endif // DEBUG
    if (!CheckBytes(c, BytesRcv)) {
#ifdef DEBUG
        error(std::string(__func__) + Sec(": failed on CheckBytes"));
#endif // DEBUG
        return "";
    }
    Data.resize(Header);
    BytesRcv = 0;
    do {
        Temp = recv(c->GetTCPSock(), &Data[BytesRcv], Header - BytesRcv, 0);
        if (!CheckBytes(c, Temp)) {
#ifdef DEBUG
            error(std::string(__func__) + Sec(": failed on CheckBytes in while(BytesRcv < Header)"));
#endif // DEBUG

            return "";
        }
#ifdef DEBUG
        //debug(std::string(__func__) + Sec(": Temp: ") + std::to_string(Temp) + Sec(", BytesRcv: ") + std::to_string(BytesRcv));
#endif // DEBUG
        BytesRcv += Temp;
    } while (BytesRcv < Header);
#ifdef DEBUG
    //debug(std::string(__func__) + Sec(": finished recv with Temp: ") + std::to_string(Temp) + Sec(", BytesRcv: ") + std::to_string(BytesRcv));
#endif // DEBUG
    std::string Ret(Data.data(), Header);

    if (Ret.substr(0, 4) == "ABG:") {
        Ret = DeComp(Ret.substr(4));
    }
#ifdef DEBUG
    //debug("Parsing from " + c->GetName() + " -> " +std::to_string(Ret.size()));
#endif

    return Ret;
}

void TCPClient(Client* c) {
    DebugPrintTIDInternal(Sec("Client(") + c->GetName() + Sec(")"), true);
    Assert(c);
    if (c->GetTCPSock() == -1) {
        CI->RemoveClient(c);
        return;
    }
    OnConnect(c);
    while (c->GetStatus() > -1){
        GParser(c, TCPRcv(c));
    }
    OnDisconnect(c, c->GetStatus() == -2);
}
void InitClient(Client* c) {
    std::thread NewClient(TCPClient, c);
    NewClient.detach();
}
