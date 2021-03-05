#pragma once

#include <string>

class TVehicleData final {
public:
    TVehicleData(int ID, std::string Data);
    ~TVehicleData();

    [[nodiscard]] bool IsInvalid() const { return mID == -1; }
    [[nodiscard]] int ID() const { return mID; }

    [[nodiscard]] std::string Data() const { return mData; }
    void SetData(const std::string& Data) { mData = Data; }

    bool operator==(const TVehicleData& v) const { return mID == v.mID; }

private:
    int mID { -1 };
    std::string mData;
};

namespace std {
template <>
struct hash<TVehicleData> {
    std::size_t operator()(const TVehicleData& s) const noexcept {
        return s.ID();
    }
};
}
