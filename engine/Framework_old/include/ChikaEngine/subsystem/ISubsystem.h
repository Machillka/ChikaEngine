/*!
 * @file ISubsystem.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief  子系统的一次封装,类似于 adapter 进行一次解耦
 * @version 0.1
 * @date 2026-02-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

namespace ChikaEngine::Framework
{

    class Scene;
    class ISubsystem
    {
      public:
        virtual ~ISubsystem() = default;
        virtual void Initialize(Scene* ownerWorld) = 0;
        virtual void Tick(float deltaTime) = 0;
        virtual void Cleanup() = 0;
    };
} // namespace ChikaEngine::Framework