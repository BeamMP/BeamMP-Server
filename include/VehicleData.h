#pragma once

#include <string>

class TVehicleData final {
public:
    TVehicleData(int ID, std::string Data);
    ~TVehicleData();
    // We cannot delete this, since vector needs to be able to copy when it resizes.
    // Deleting this causes some wacky template errors which are hard to decipher,
    // and end up making no sense, so let's just leave the copy ctor.
    // TVehicleData(const TVehicleData&) = delete;

    [[nodiscard]] bool IsInvalid() const { return mID == -1; }
    [[nodiscard]] int ID() const { return mID; }

    [[nodiscard]] std::string Data() const { return mData; }
    void SetData(const std::string& Data) { mData = Data; }

    bool operator==(const TVehicleData& v) const { return mID == v.mID; }

private:
    int mID { -1 };
    std::string mData;
};

// TODO: unused now, remove?
namespace std {
template <>
struct hash<TVehicleData> {
    std::size_t operator()(const TVehicleData& s) const noexcept {
        return s.ID();
    }
};
}
