#pragma once

#include <string>

class TVehicleData final {
public:
    TVehicleData(int ID, const std::string& Data);

    bool IsInvalid() const { return _ID == -1; }
    int ID() const { return _ID; }

    std::string Data() const { return _Data; }
    void SetData(const std::string& Data) { _Data = Data; }

private:
    int _ID { -1 };
    std::string _Data;
};
