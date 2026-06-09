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
        }

        template <typename Callback> uint32_t Consume(float deltaTime, Callback&& callback)
        {
            const float maxAcceptedDelta = m_fixedStep * static_cast<float>(m_maxStepsPerFrame);
            m_accumulator += std::clamp(deltaTime, 0.0f, maxAcceptedDelta);

            uint32_t stepCount = 0;
            while (m_accumulator >= m_fixedStep && stepCount < m_maxStepsPerFrame)
            {
                callback(m_fixedStep);
                m_accumulator -= m_fixedStep;
                ++stepCount;
            }
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

      private:
        float m_fixedStep = 1.0f / 60.0f;
        float m_accumulator = 0.0f;
        uint32_t m_maxStepsPerFrame = 4;
    };
} // namespace ChikaEngine::Core
