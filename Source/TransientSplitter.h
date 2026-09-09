/**
 * CREDITS TO ZL-AUDIO AND DERRY FITZGERALD FOR THE ENHANCED TRANSIENT DETECTION ALGORITHM!!!
 * PLEASE ALSO ADHERE TO THEIR LICENSING CONDITIONS FOR THIS PART!
 */
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
namespace transientsplit
{
    template <typename T, int N>
    class HeapFilter
    {
    public:
        HeapFilter() { clear(); }
        void clear()
        {
            data_.fill(T(0)); pos_.fill(0); heap_.fill(0);
            idx_ = 0; minCt_ = 0; maxCt_ = 0; dataSize_ = 0;
            int n = N;
            while(n--) { pos_[(size_t) n] = ((n + 1) / 2) * ((n & 1) ? -1 : 1);
                          heap_[(size_t) (centerPos_ + pos_[(size_t) n])] = n; }
        }
        void insert(T v)
        {
            dataSize_ = (dataSize_ + 1) % N;
            const int p = pos_[(size_t) idx_];
            T old = data_[(size_t) idx_];
            data_[(size_t) idx_] = v;
            idx_ = (idx_ + 1) % N;
            if(p > 0)
            {
                if(minCt_ < (N - 1) / 2) ++minCt_;
                else if(v > old) { minSortDown(p); return; }
                if(minSortUp(p) && mmCmpExchange(0, -1)) maxSortDown(-1);
            }
            else if(p < 0)
            {
                if(maxCt_ < N / 2) ++maxCt_;
                else if(v < old) { maxSortDown(p); return; }
                if(maxSortUp(p) && minCt_ && mmCmpExchange(1, 0)) minSortDown(1);
            }
            else
            {
                if(maxCt_ && maxSortUp(-1)) maxSortDown(-1);
                if(minCt_ && minSortUp(1)) minSortDown(1);
            }
        }
        T getMedian()
        {
            if(minCt_ < maxCt_)
                return(data_[(size_t) heap_[(size_t) centerPos_]]
                      + data_[(size_t) heap_[(size_t) (centerPos_ - 1)]]) / T(2);
            return data_[(size_t) heap_[(size_t) centerPos_]];
        }
    private:
        std::array<T, N> data_;
        std::array<int, N> pos_;
        std::array<int, N> heap_;
        static constexpr int centerPos_ = N / 2;
        int idx_ = 0, minCt_ = 0, maxCt_ = 0, dataSize_ = 0;
        int mmExchange(int i, int j)
        {
            const auto ii = (size_t) (centerPos_ + i), jj = (size_t) (centerPos_ + j);
            std::swap(heap_[ii], heap_[jj]);
            pos_[(size_t) heap_[ii]] = i; pos_[(size_t) heap_[jj]] = j; return 1;
        }
        void minSortDown(int i) { for(i *= 2; i <= minCt_; i *= 2) { if(i < minCt_ && mmLess(i + 1, i)) ++i; if(! mmCmpExchange(i, i / 2)) break; } }
        void maxSortDown(int i) { for(i *= 2; i >= -maxCt_; i *= 2) { if(i > -maxCt_ && mmLess(i, i - 1)) --i; if(! mmCmpExchange(i / 2, i)) break; } }
        int mmLess(int i, int j) { return data_[(size_t) heap_[(size_t) (centerPos_ + i)]] < data_[(size_t) heap_[(size_t) (centerPos_ + j)]]; }
        int mmCmpExchange(int i, int j) { return(mmLess(i, j) && mmExchange(i, j)); }
        int minSortUp(int i) { while(i > 0 && mmCmpExchange(i, i / 2)) i /= 2; return(i == 0); }
        int maxSortUp(int i) { while(i < 0 && mmCmpExchange(i / 2, i)) i /= 2; return(i == 0); }
    };
    class TransientMask
    {
    public:
        void prepare(int numBins)
        {
            numBins_ = numBins;
            timeMed_.assign((size_t) numBins_, {});
            mask_.assign((size_t) numBins_, 0.0f);
            reset();
        }
        void reset()
        {
            for(auto& m : timeMed_) m.clear();
            freqMed_.clear();
            std::fill(mask_.begin(), mask_.end(), 0.0f);
            timeCtr_ = 0;
        }
        void setThreshold(float x) { const float xc = juce::jlimit(0.0f, 1.0f, x); threshold_ = juce::jlimit(0.0f, 1.0f, 1.0f - xc * xc * std::sqrt(xc)); }
        void setBalance(float x) { cBalance_ = std::pow(16.0f, x - 0.75f); }
        void setSeparation(float x) { cSeparation_ = std::exp(x * 4.0f) - 1.0f; }
        void setHold(float x) { cHoldBase_ = (32.0f - std::pow(32.0f, 1.0f - x)) / 31.0f * 0.75f + 0.24f; updateHold(); }
        void setOverlap(int overlap)
        {
            overlap_ = juce::jmax(1, overlap);
            updateHold();
            const int base = juce::jmax(1, overlap_ / kRefOverlap);
            const int extra = juce::jmax(0, (overlap_ - kRefOverlap) / kRefOverlap) * (kSpanMult - 1);
            timeStride_ = base + extra;
        }
        void setSmooth(float x) { cSmooth_ = x; }
        void compute(const float* mag, float* maskOut)
        {
            const bool feedTime = (++timeCtr_ >= timeStride_);
            if(feedTime) timeCtr_ = 0;
            freqMed_.clear();
            for(int i = 0; i < kFreqHalf; ++i) freqMed_.insert(mag[0]);
            for(int i = 0; i < kFreqHalf; ++i) freqMed_.insert(mag[(size_t) i]);
            for(int i = 0; i < numBins_ - kFreqHalf; ++i)
            {
                freqMed_.insert(mag[(size_t) (i + kFreqHalf)]);
                if(feedTime) timeMed_[(size_t) i].insert(mag[(size_t) i]);
                mask_[(size_t) i] = std::max(mask_[(size_t) i] * cHold_,
                                              portion(freqMed_.getMedian(), timeMed_[(size_t) i].getMedian()));
            }
            for(int i = numBins_ - kFreqHalf; i < numBins_; ++i)
            {
                freqMed_.insert(mag[(size_t) (numBins_ - 1)]);
                if(feedTime) timeMed_[(size_t) i].insert(mag[(size_t) i]);
                mask_[(size_t) i] = std::max(mask_[(size_t) i] * cHold_,
                                              portion(freqMed_.getMedian(), timeMed_[(size_t) i].getMedian()));
            }
            double sum = 0.0;
            for(int i = 0; i < numBins_; ++i) sum += mask_[(size_t) i];
            const float mean = (float) (sum / (double) numBins_);
            for(int i = 0; i < numBins_; ++i)
                maskOut[i] = (mean - mask_[(size_t) i]) * cSmooth_ + mask_[(size_t) i];
        }
    private:
        float portion(float transientWeight, float steadyWeight) const
        {
            const float t = transientWeight * cBalance_, s = steadyWeight;
            const float p = (t * t) / std::max(t * t + s * s, 1.0e-8f);
            const float lo = threshold_;
            const float hi = std::min(1.0f, threshold_ + ramp());
            float g = std::clamp((p - lo) / std::max(1.0e-6f, hi - lo), 0.0f, 1.0f);
            return g * g * (3.0f - 2.0f * g);
        }
        float ramp() const { return std::clamp(0.40f - 0.10f * cSeparation_, 0.06f, 0.40f); }
        void updateHold() { cHold_ = std::pow(cHoldBase_, (float) kRefOverlap / (float) overlap_); }
        static constexpr int kRefOverlap = 4;
        static constexpr int kSpanMult = 3;
        static constexpr int kFreqSize = 5, kFreqHalf = kFreqSize / 2, kTimeSize = 5;
        int numBins_ = 0;
        std::vector<HeapFilter<float, kTimeSize>> timeMed_;
        HeapFilter<float, kFreqSize> freqMed_;
        std::vector<float> mask_;
        float cBalance_ = 1.0f, cSeparation_ = 1.0f, cHold_ = 0.9f, cHoldBase_ = 0.9f, cSmooth_ = 0.5f, threshold_ = 1.0f;
        int overlap_ = kRefOverlap;
        int timeStride_ = 1, timeCtr_ = 0;
    };
}