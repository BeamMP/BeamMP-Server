///
/// Created by Anonymous275 on 7/15/2020
///
#include "Zlib/zlib.h"
#include <iostream>

#define Biggest 30000
std::string Comp(std::string Data){
    char*C = new char[Biggest];
    memset(C, 0, Biggest);
    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    defstream.avail_in = (uInt)Data.length();
    defstream.next_in = (Bytef *)&Data[0];
    defstream.avail_out = Biggest;
    defstream.next_out = reinterpret_cast<Bytef *>(C);
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_SYNC_FLUSH);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
    int TO = defstream.total_out;
    std::string Ret(TO,0);
    memcpy_s(&Ret[0],TO,C,TO);
    delete [] C;
    return Ret;
}
std::string DeComp(std::string Compressed){
    char*C = new char[Biggest];
    memset(C, 0, Biggest);
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = Biggest;
    infstream.next_in = (Bytef *)(&Compressed[0]);
    infstream.avail_out = Biggest;
    infstream.next_out = (Bytef *)(C);
    inflateInit(&infstream);
    inflate(&infstream, Z_SYNC_FLUSH);
    inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);
    int TO = infstream.total_out;
    std::string Ret(TO,0);
    memcpy_s(&Ret[0],TO,C,TO);
    delete [] C;
    return Ret;
}