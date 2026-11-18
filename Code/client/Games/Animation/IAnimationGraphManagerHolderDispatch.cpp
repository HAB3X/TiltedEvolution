#include <TiltedOnlinePCH.h>

#include <Games/Animation/IAnimationGraphManagerHolder.h>

// These are the game's virtual functions: Skyrim Together only declares them and calls them through the game's
// vtables. Clang (unlike MSVC) calls them directly when it believes it knows the object's type, e.g. on a holder
// embedded in a game object (actor.animationGraphHolder.GetVariableFloat(...)), which then needs a definition.
// Each definition here forwards through the object's own vtable, which for every holder that exists at run time is
// the game's, so the call lands in the game's function exactly as an MSVC build's would.

bool IAnimationGraphManagerHolder::SendAnimationEvent(BSFixedString* apAnimEvent)
{
    using TFunc = bool (IAnimationGraphManagerHolder::*)(BSFixedString*);
    constexpr TFunc cFunc = &IAnimationGraphManagerHolder::SendAnimationEvent;
    return (std::launder(this)->*cFunc)(apAnimEvent);
}

bool IAnimationGraphManagerHolder::GetVariableFloat(BSFixedString* apVariable, float* apReturn)
{
    using TFunc = bool (IAnimationGraphManagerHolder::*)(BSFixedString*, float*);
    constexpr TFunc cFunc = &IAnimationGraphManagerHolder::GetVariableFloat;
    return (std::launder(this)->*cFunc)(apVariable, apReturn);
}

bool IAnimationGraphManagerHolder::GetVariableInt(BSFixedString* apVariable, uint32_t* apReturn)
{
    using TFunc = bool (IAnimationGraphManagerHolder::*)(BSFixedString*, uint32_t*);
    constexpr TFunc cFunc = &IAnimationGraphManagerHolder::GetVariableInt;
    return (std::launder(this)->*cFunc)(apVariable, apReturn);
}

bool IAnimationGraphManagerHolder::GetVariableBool(BSFixedString* apVariable, bool* apReturn)
{
    using TFunc = bool (IAnimationGraphManagerHolder::*)(BSFixedString*, bool*);
    constexpr TFunc cFunc = &IAnimationGraphManagerHolder::GetVariableBool;
    return (std::launder(this)->*cFunc)(apVariable, apReturn);
}
