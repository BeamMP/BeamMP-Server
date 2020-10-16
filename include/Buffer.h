///
/// Created by Anonymous275 on 8/25/2020
///
#pragma once
#include <mutex>
class Client;
void GParser(Client*c, const std::string&Packet);
class Buffer{
public:
    void Handle(Client*c,const std::string& Data){
        if(c == nullptr)return;
        Buf += Data;
        Manage(c);
    }
    void clear(){
        Buf.clear();
    }
private:
    std::string Buf;
    void Manage(Client*c){
        if(!Buf.empty()){
            std::string::size_type p;
            if (Buf.at(0) == '\n'){
                p = Buf.find('\n',1);
                if(p != -1){
                    std::string R = Buf.substr(1,p-1);
                    std::string_view B(R.c_str(),R.find(char(0)));
                    GParser(c, B.data());
                    Buf = Buf.substr(p+1);
                    Manage(c);
                }
            }else{
                p = Buf.find('\n');
                if(p == -1)Buf.clear();
                else{
                    Buf = Buf.substr(p);
                    Manage(c);
                }
            }
        }
    }
};
