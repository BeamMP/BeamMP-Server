// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "TVoiceChat.h"
#include <cmath>
#include <cstring>

namespace {
    // Explicit little-endian helpers — portable to any architecture.
    // Wire format: little-endian IEEE 754 floats, little-endian uint32.
    inline void pushLE32(std::vector<uint8_t>& buf, uint32_t v) {
        buf.push_back(static_cast<uint8_t>( v        & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }
    inline void pushLEFloat(std::vector<uint8_t>& buf, float f) {
        uint32_t bits; std::memcpy(&bits, &f, sizeof(float));
        pushLE32(buf, bits);
    }
} // namespace

void TVoiceChat::SetProximityDistance(float distance) {
    mProximityDistance.store(distance < 0.0f ? 0.0f : distance);
}

float TVoiceChat::GetProximityDistance() const {
    return mProximityDistance.load();
}

int TVoiceChat::CreateChannel(const std::string& name) {
    std::lock_guard lock(mChannelsMutex);
    int id = mNextChannelId++;
    Channel ch;
    ch.id = id;
    ch.name = name;
    ch.maxDistance = 0.0f;
    ch.position[0] = ch.position[1] = ch.position[2] = 0.0f;
    ch.spatial = false;
    mChannels[id] = std::move(ch);
    return id;
}

bool TVoiceChat::DeleteChannel(int channelId) {
    std::lock_guard lock(mChannelsMutex);
    return mChannels.erase(channelId) > 0;
}

bool TVoiceChat::AddPlayerToChannel(int playerId, int channelId) {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return false;
    it->second.members.insert(playerId);
    return true;
}

bool TVoiceChat::RemovePlayerFromChannel(int playerId, int channelId) {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return false;
    return it->second.members.erase(playerId) > 0;
}

bool TVoiceChat::RemovePlayerFromAllChannels(int playerId) {
    std::lock_guard lock(mChannelsMutex);
    bool removed = false;
    for (auto& [id, ch] : mChannels) {
        if (ch.members.erase(playerId) > 0) removed = true;
    }
    return removed;
}

std::unordered_set<int> TVoiceChat::GetChannelMembers(int channelId) const {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return {};
    return it->second.members;
}

std::unordered_set<int> TVoiceChat::GetPlayerChannels(int playerId) const {
    std::lock_guard lock(mChannelsMutex);
    std::unordered_set<int> result;
    for (const auto& [id, ch] : mChannels) {
        if (ch.members.count(playerId)) result.insert(id);
    }
    return result;
}

bool TVoiceChat::IsPlayerInChannel(int playerId, int channelId) const {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return false;
    return it->second.members.count(playerId) > 0;
}

// ── channel properties ──────────────────────────────────

bool TVoiceChat::SetChannelMaxDistance(int channelId, float distance) {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return false;
    it->second.maxDistance = distance < 0.0f ? 0.0f : distance;
    return true;
}

bool TVoiceChat::SetChannelPosition(int channelId, float x, float y, float z) {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return false;
    it->second.position[0] = x;
    it->second.position[1] = y;
    it->second.position[2] = z;
    return true;
}

bool TVoiceChat::SetChannelSpatial(int channelId, bool spatial) {
    std::lock_guard lock(mChannelsMutex);
    auto it = mChannels.find(channelId);
    if (it == mChannels.end()) return false;
    it->second.spatial = spatial;
    return true;
}

std::vector<TVoiceChat::ChannelInfo> TVoiceChat::ListChannels() const {
    std::lock_guard lock(mChannelsMutex);
    std::vector<ChannelInfo> result;
    result.reserve(mChannels.size());
    for (const auto& [id, ch] : mChannels) {
        result.push_back({ ch.id, ch.name });
    }
    return result;
}

// ── player mute ─────────────────────────────────────────

void TVoiceChat::MutePlayer(int playerId, bool muted) {
    std::lock_guard lock(mMuteMutex);
    if (muted) {
        mMutedPlayers.insert(playerId);
    } else {
        mMutedPlayers.erase(playerId);
    }
}

bool TVoiceChat::ShouldFireVoiceEvent(int playerId) {
    using namespace std::chrono;
    auto now = steady_clock::now();
    std::lock_guard lock(mVoiceEventMutex);
    auto& last = mLastVoiceEvent[playerId];
    if (duration_cast<milliseconds>(now - last).count() >= 300) {
        last = now;
        return true;
    }
    return false;
}

bool TVoiceChat::IsPlayerMuted(int playerId) const {
    std::lock_guard lock(mMuteMutex);
    return mMutedPlayers.count(playerId) > 0;
}

void TVoiceChat::CleanupPlayer(int playerId) {
    RemovePlayerFromAllChannels(playerId);
    {
        std::lock_guard lock(mMuteMutex);
        mMutedPlayers.erase(playerId);
    }
    {
        std::lock_guard lock(mVoiceEventMutex);
        mLastVoiceEvent.erase(playerId);
    }
}

// ── packet building ─────────────────────────────────────

std::vector<uint8_t> TVoiceChat::BuildPacket(uint8_t flags, uint16_t sourceId,
                                              const float pos[3],
                                              float maxDistance,
                                              float gain,
                                              const uint8_t* opusData, int opusLen) {
    std::vector<uint8_t> pkt;
    pkt.reserve(HEADER_SIZE + opusLen);
    pkt.push_back('F');
    pkt.push_back(PROTOCOL_VERSION);
    pkt.push_back(flags);
    pkt.push_back(static_cast<uint8_t>(sourceId & 0xFF));
    pkt.push_back(static_cast<uint8_t>((sourceId >> 8) & 0xFF));
    pushLEFloat(pkt, pos[0]);
    pushLEFloat(pkt, pos[1]);
    pushLEFloat(pkt, pos[2]);
    pushLEFloat(pkt, maxDistance);
    pushLEFloat(pkt, gain);
    pkt.insert(pkt.end(), opusData, opusData + opusLen);
    return pkt;
}

// ── audio injection ─────────────────────────────────────

void TVoiceChat::SendAudio(int channelId, const std::string& opusData, float x, float y, float z,
                            const UDPSendFunc& udpSend,
                            const std::function<std::shared_ptr<TClient>(int)>& getClient,
                            float gain) {
    std::unordered_set<int> members;
    float chPos[3] = { x, y, z };
    float maxDist = 0.0f;
    bool spatial = false;
    {
        std::lock_guard lock(mChannelsMutex);
        auto it = mChannels.find(channelId);
        if (it == mChannels.end()) return;
        members = it->second.members;
        // Use channel position when caller passes kUseChannelPos (NaN) sentinel
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
            chPos[0] = it->second.position[0];
            chPos[1] = it->second.position[1];
            chPos[2] = it->second.position[2];
        }
        maxDist = it->second.maxDistance;
        spatial = it->second.spatial;
    }

    uint8_t flags = FLAG_INJECTED;
    if (spatial) flags |= FLAG_PROXIMITY;

    auto pkt = BuildPacket(flags, static_cast<uint16_t>(channelId),
                           chPos,
                           maxDist,
                           gain,
                           reinterpret_cast<const uint8_t*>(opusData.data()),
                           static_cast<int>(opusData.size()));

    for (int pid : members) {
        auto client = getClient(pid);
        if (!client) continue;
        udpSend(*client, pkt);
    }
}
