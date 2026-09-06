#pragma once

/**
 * @defgroup LuaScripting Lua Scripting
 * @brief Lightweight Lua gameplay scripting module.
 *
 * This module contains runtime loading, per-actor script instances,
 * engine/Lua bindings, and the ScriptComponent bridge used by gameplay actors.
 */

/**
 * @defgroup LuaScriptingRuntime Runtime
 * @ingroup LuaScripting
 * @brief Lua VM ownership, script instance lifecycle, and script path cache.
 */

/**
 * @defgroup LuaScriptingBinding Binding
 * @ingroup LuaScripting
 * @brief Lua-exposed engine APIs (Actor/Component/utility globals).
 */

/**
 * @defgroup LuaScriptingComponent Component Bridge
 * @ingroup LuaScripting
 * @brief Actor component that forwards engine lifecycle/events to Lua callbacks.
 */

/**
 * @page LuaScriptingGuide Lua Scripting Module Guide
 * @tableofcontents
 *
 * @section lua_scripting_goal Goal
 * Provide a small, stable scripting layer for gameplay iteration.
 *
 * @section lua_scripting_flow Runtime Flow
 * 1. FLuaScriptSubsystem initializes one shared Lua VM.
 * 2. Each UScriptComponent creates one FLuaScriptInstance (isolated environment).
 * 3. Script chunk loads from ScriptPath and defines optional callbacks.
 * 4. ScriptComponent forwards BeginPlay/Tick/events to Lua when callbacks exist.
 * 5. Runtime errors are logged and fail-safe policy can disable only that component.
 *
 * @section lua_scripting_callbacks Common Callbacks
 * - OnStart(self)
 * - OnUpdate(self, deltaTime)
 * - OnEnable(self)
 * - OnDisable(self)
 * - OnDestroy(self)
 * - OnOverlapBegin(self, otherActor)
 * - OnOverlapEnd(self, otherActor)
 * - OnHit(self, otherActor, hitInfo)
 * @section lua_scripting_api Bound API
 * See:
 * - LuaBinder namespace (`@ref LuaScriptingBinding`)
 * - FLuaScriptSubsystem (`@ref LuaScriptingRuntime`)
 * - UScriptComponent (`@ref LuaScriptingComponent`)
 */
