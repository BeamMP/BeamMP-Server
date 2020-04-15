///
/// Created by Anonymous275 on 4/11/2020
///

#include <thread>
#include <string>
#include <fstream>
#include <iostream>
#include <WinSock2.h>
#include "../Settings.hpp"

int ParseAndSend(SOCKET Client, std::string Data){
    std::string Response, Packet;
    char ID = Data.at(0);
    int Prev = 0,DataSent = 0, Size = 0;
    bool FileSent = true;
    switch (ID){
        case 'a' :
            Response = FileList+FileSizes;
            if(Response.empty())Response = " ";
            break;
        case 'b' :
            FileSent = false;
            break;
        case 'M' :
            Response = MapName;
            break;
    }
    std::string FLocation = Data.substr(1);
    if(FileList.find(FLocation) == std::string::npos)return -1;
    do {
        if(!FileSent){
            std::ifstream f;
            f.open(FLocation.c_str(), std::ios::binary);
            if(f.good()){
                if(!Size){
                    Size = f.seekg(0, std::ios_base::end).tellg();
                    Response.resize(Size);
                    f.seekg(0, std::ios_base::beg);
                    f.read(&Response[0], Size);
                    f.close();
                }else f.close();

                if(DataSent != Size){
                    Packet.clear();
                    if((Size-DataSent) < 65535){
                        Packet = Response.substr(Prev,(Size-DataSent));
                        DataSent += (Size-DataSent);
                        Response.clear();
                    }else{
                        DataSent += 65535;
                        Packet = Response.substr(Prev,65535);
                    }
                    Prev = DataSent;
                }else{
                    Size = 0;
                    DataSent = 0;
                    Prev = 0;
                    FileSent = true;
                    Packet.clear();
                }
            }else{
                FileSent = true;
                Response = "Cannot Open";
            }
        }

        int iSendResult;

        if(!Packet.empty())iSendResult = send(Client, Packet.c_str(), Packet.length(), 0);
        else iSendResult = send(Client, Response.c_str(), Response.length(), 0);

        if (iSendResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(Client);
            return -1;
        }
    }while(!FileSent);
    return 0;
}


void Client(void* ClientData){
    SOCKET Client = *(SOCKET*)ClientData;
    printf("Client connected\n");
    int iResult, iSendResult;
    char recvbuf[65535];
    int recvbuflen = 65535;

    do {
        iResult = recv(Client, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            //printf("Bytes received: %d\n", iResult);
            std::string Data = recvbuf,Response;
            Data.resize(iResult);

            if(ParseAndSend(Client,Data) == -1)break;

            // Echo the buffer back to the sender
            /*iSendResult = send(Client, Response.c_str(), Response.length(), 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(Client);
                return;
            }
            printf("Bytes sent: %d\n", iSendResult);*/
        }
        else if (iResult == 0)
            printf("Connection closing...\n");
        else  {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(Client);
            break;
        }
    } while (iResult > 0);
    std::cout << "Client Closed" << std::endl;
}


void CreateNewThread(void* ClientData){
    std::cout << "New Client" << std::endl;
    std::thread NewClient(Client,ClientData);
    NewClient.detach();
}