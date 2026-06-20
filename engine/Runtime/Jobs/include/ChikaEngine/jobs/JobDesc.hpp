#pragma once

#include "ChikaEngine/jobs/JobTypes.hpp"
#include "ChikaEngine/jobs/SmallJobFunction.hpp"

#include <cstdint>

namespace ChikaEngine::Jobs
{
    struct JobDesc
    {
        SmallJobFunction function;
        uint32_t nameId = 0;
        JobTarget target = JobTarget::AnyWorker;
        JobFailurePolicy failurePolicy = JobFailurePolicy::Cancel;
    };
} // namespace ChikaEngine::Jobs
