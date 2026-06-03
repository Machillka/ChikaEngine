# AGENTS.md

## Project overview

ChikaEngine is a C++20 game engine project. The goal is to build a usable, learning-oriented engine with clear architecture, readable code, and incremental milestones.

## Repository structure

- `engine/`: core engine source code.
- `Assets/`: runtime assets and test resources.
- `docs/`: design notes and development documentation.
- `CMakeLists.txt`: root build entry.
- `pyproject.toml` and `uv.lock`: Python tooling environment used by build/codegen scripts.
- `.clang-format`: C++ formatting rules.

## Build expectations

- Prefer out-of-source builds:
  - `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build`
- Do not modify vendored third-party code under `engine/ThirdParty/` unless explicitly asked.
- Do not introduce new third-party dependencies without explaining why.
- Keep C++ standard at C++20 unless explicitly changed.
- Respect `.clang-format`.

## Development rules

- Before editing, inspect relevant files and summarize the plan.
- Prefer small commits and focused changes.
- For risky refactors, first create a design note in `docs/`.
- After modifying C++ or CMake files, run configure/build commands when possible.
- When build errors occur, fix the root cause instead of hiding warnings.
- Avoid broad rewrites unless the user explicitly asks.

## Architecture preferences

- Keep platform, rendering, physics, scripting, asset, and editor logic separated.
- Prefer explicit ownership and RAII.
- Avoid global mutable state unless there is a documented engine-level reason.
- Public APIs should be documented when behavior changes.

## Current priorities

1. Make the project easy to configure and build on a fresh machine.
2. Improve documentation for setup and architecture.
3. Stabilize core engine loop and module boundaries.
4. Add tests or sample scenes where practical.