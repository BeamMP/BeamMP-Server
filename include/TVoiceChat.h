// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class TNetwork;
class TClient;

class TVoiceChat {
public:
    TVoiceChat() = default;
    TVoiceChat(const TVoiceChat&) = delete;
    TVoiceChat& operator=(const TVoiceChat&) = delete;

    // Packet v2 constants (must match Launcher VoiceChat.h exactly).
    // Wire layout (25 bytes, little-endian):
    //   [0]      'F'            uint8   — packet type discriminator
    //   [1]      version        uint8   — PROTOCOL_VERSION (currently 2)
    //   [2]      flags          uint8   — FLAG_PROXIMITY | FLAG_INJECTED
    //   [3-4]    sourceId       uint16  — sender client ID (or channel ID for injected)
    //   [5-16]   pos            floatx3 — source world position XYZ
    //   [17-20]  maxDistance    float   — spatialization cutoff (0 = unlimited)
    //   [21-24]  gain           float   — broadcast gain [0.0, 1.0]
    //   [25...]  opusData       bytes   — Opus-encoded audio payload
    // IMPORTANT: bump PROTOCOL_VERSION before changing this layout in a release.
    static constexpr uint8_t PROTOCOL_VERSION = 2;
    static constexpr uint8_t FLAG_PROXIMITY   = 0x01;
    static constexpr uint8_t FLAG_INJECTED    = 0x02;
    static constexpr size_t  HEADER_SIZE      = 1 + 1 + 1 + 2 + 12 + 4 + 4; // 25 bytes

    // Proximity distance (0 = unlimited)
    void SetProximityDistance(float distance);
    float GetProximityDistance() const;

    // Channel management
    int CreateChannel(const std::string& name);
    bool DeleteChannel(int channelId);
    bool AddPlayerToChannel(int playerId, int channelId);
    bool RemovePlayerFromChannel(int playerId, int channelId);
    bool RemovePlayerFromAllChannels(int playerId);
    std::unordered_set<int> GetChannelMembers(int channelId) const;
    std::unordered_set<int> GetPlayerChannels(int playerId) const;
    bool IsPlayerInChannel(int playerId, int channelId) const;

    // Channel properties
    bool SetChannelMaxDistance(int channelId, float distance);
    bool SetChannelPosition(int channelId, float x, float y, float z);
    bool SetChannelSpatial(int channelId, bool spatial);

    // Query API
    struct ChannelInfo {
        int id;
        std::string name;
    };
    std::vector<ChannelInfo> ListChannels() const;

    // Server-side player mute
    void MutePlayer(int playerId, bool muted);
    bool IsPlayerMuted(int playerId) const;

    // Removes all per-player state (channels, mute, throttle).
    // Must be called when a player disconnects.
    void CleanupPlayer(int playerId);

    // Throttled voice-activity query: returns true at most once per 300 ms
    // per player. Call this before firing the onPlayerVoice Lua event so the
    // server console is not flooded at 50 events/sec.
    bool ShouldFireVoiceEvent(int playerId);

    // Audio injection: builds and broadcasts an opus frame to channel members.
    // Pass kUseChannelPos (NaN) for x/y/z to use the channel's stored position
    // instead of an explicit caller-supplied position.
    static constexpr float kUseChannelPos = std::numeric_limits<float>::quiet_NaN();
    using UDPSendFunc = std::function<void(TClient&, const std::vector<uint8_t>&)>;
    void SendAudio(int channelId, const std::string& opusData, float x, float y, float z,
                   const UDPSendFunc& udpSend,
                   const std::function<std::shared_ptr<TClient>(int)>& getClient,
                   float gain = 1.0f);

    // Build a v2 broadcast packet
    static std::vector<uint8_t> BuildPacket(uint8_t flags, uint16_t sourceId,
                                            const float pos[3],
                                            float maxDistance,
                                            float gain,
                                            const uint8_t* opusData, int opusLen);

private:
    struct Channel {
        int id;
        std::string name;
        std::unordered_set<int> members;
        float maxDistance = 0.0f;      // 0 = unlimited (within channel)
        float position[3] = {0, 0, 0}; // source position for spatial channels
        bool spatial = false;           // if true, server sets PROXIMITY flag
    };

    std::atomic<float> mProximityDistance { 0.0f }; // 0 = unlimited
    // Throttle map for onPlayerVoice Lua events — protected by mVoiceEventMutex.
    // Intentionally separate from mChannelsMutex to avoid false lock contention
    // between channel management and the 50/sec UDP voice hot path.
    std::unordered_map<int, std::chrono::steady_clock::time_point> mLastVoiceEvent;
    mutable std::mutex mVoiceEventMutex;
    // mNextChannelId is always read and written inside mChannelsMutex so a plain
    // int is correct here.  Do NOT make it atomic: incrementing it must be atomic
    // with the mChannels insertion, and std::atomic alone would not guarantee that.
    int mNextChannelId = 1;
    std::unordered_map<int, Channel> mChannels;
    mutable std::mutex mChannelsMutex;

    std::unordered_set<int> mMutedPlayers;
    mutable std::mutex mMuteMutex;
};
