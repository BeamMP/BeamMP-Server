#pragma once

#include "Common.h"
#include "Compat.h"
#include "IThreaded.h"
#include "TServer.h"

class TTCPServer : public IThreaded {
public:
    explicit TTCPServer(TServer& Server);

private:
    TServer& mServer;
};