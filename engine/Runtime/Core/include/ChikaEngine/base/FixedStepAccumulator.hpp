#pragma once

#include <algorithm>
#include <cstdint>

namespace ChikaEngine::Core
{
    class FixedStepAccumulator
    {
      public:
        FixedStepAccumulator(float fixedStep = 1.0f / 60.0f, uint32_t maxStepsPerFrame = 4)
        {
            Configure(fixedStep, maxStepsPerFrame);
        }

        void Configure(float fixedStep, uint32_t maxStepsPerFrame)
        {
            m_fixedStep = fixedStep > 0.0f ? fixedStep : 1.0f / 60.0f;
            m_maxStepsPerFrame = std::max(maxStepsPerFrame, 1u);
            Reset();
        }

        void Reset()
        {
            m_accumulator = 0.0f;
            m_lastDroppedTime = 0.0f;
            m_totalDroppedTime = 0.0f;
            m_lastStepCount = 0;
        }

        template <typename Callback> uint32_t Consume(float deltaTime, Callback&& callback)
        {
            const float maxAcceptedDelta = m_fixedStep * static_cast<float>(m_maxStepsPerFrame);
            const float nonNegativeDelta = std::max(deltaTime, 0.0f);
            const float acceptedDelta = std::min(nonNegativeDelta, maxAcceptedDelta);
            m_lastDroppedTime = nonNegativeDelta - acceptedDelta;
            m_totalDroppedTime += m_lastDroppedTime;
            m_accumulator += acceptedDelta;

            uint32_t stepCount = 0;
            const float comparisonEpsilon = m_fixedStep * 1.0e-5f;
            while (m_accumulator + comparisonEpsilon >= m_fixedStep && stepCount < m_maxStepsPerFrame)
            {
                callback(m_fixedStep);
                m_accumulator = std::max(m_accumulator - m_fixedStep, 0.0f);
                ++stepCount;
            }
            m_lastStepCount = stepCount;
            return stepCount;
        }

        float GetFixedStep() const
        {
            return m_fixedStep;
        }

        float GetRemainder() const
        {
            return m_accumulator;
        }

        uint32_t GetMaxStepsPerFrame() const
        {
            return m_maxStepsPerFrame;
        }

        [[nodiscard]] float GetInterpolationAlpha() const
        {
            return std::clamp(m_accumulator / m_fixedStep, 0.0f, 1.0f);
        }

        [[nodiscard]] float GetLastDroppedTime() const
        {
            return m_lastDroppedTime;
        }

        [[nodiscard]] float GetTotalDroppedTime() const
        {
            return m_totalDroppedTime;
        }

        [[nodiscard]] uint32_t GetLastStepCount() const
        {
            return m_lastStepCount;
        }

      private:
        float m_fixedStep = 1.0f / 60.0f;
        float m_accumulator = 0.0f;
        float m_lastDroppedTime = 0.0f;
        float m_totalDroppedTime = 0.0f;
        uint32_t m_maxStepsPerFrame = 4;
        uint32_t m_lastStepCount = 0;
    };
} // namespace ChikaEngine::Core
