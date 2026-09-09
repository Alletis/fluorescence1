#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
template <int MaxBins, int MaxBases>
struct SpectrumSnapshot
{
    int numBins = 0;
    float binWidth = 0.0f;
    double sampleRate = 44100.0;
    int numBases = 0;
    bool pvBypassed = false;
    std::array<float, MaxBins> mag {};
    std::array<float, MaxBins> freq {};
    std::array<float, MaxBases> baseHz {};
    std::array<float, MaxBases> baseConf {};
};
template <int MaxBins, int MaxBases>
class SpectrumBridge
{
public:
    using Snapshot = SpectrumSnapshot<MaxBins, MaxBases>;
    Snapshot& startWrite() noexcept
    {
        const auto s = sequence.load(std::memory_order_relaxed);
        sequence.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        return buffer;
    }
    void publish() noexcept
    {
        std::atomic_thread_fence(std::memory_order_release);
        sequence.fetch_add(1, std::memory_order_relaxed);
    }
    bool read(Snapshot& out) noexcept
    {
        for(;;)
        {
            const auto s1 = sequence.load(std::memory_order_acquire);
            if(s1 & 1u)
                continue;
            std::atomic_thread_fence(std::memory_order_acquire);
            out = buffer;
            std::atomic_thread_fence(std::memory_order_acquire);
            const auto s2 = sequence.load(std::memory_order_relaxed);
            if(s1 == s2)
            {
                const bool isNew = (s1 != lastSeenSequence);
                lastSeenSequence = s1;
                return isNew;
            }
        }
    }
private:
    Snapshot buffer {};
    std::atomic<std::uint32_t> sequence { 0 };
    std::uint32_t lastSeenSequence = 0;
};