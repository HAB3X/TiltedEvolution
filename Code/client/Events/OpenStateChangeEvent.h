#pragma once

/**
 * @brief An activation opened or closed a door or an activator with open and close animations, in its settled
 * state (ObjectData::SettledDoorState).
 */
struct OpenStateChangeEvent
{
    OpenStateChangeEvent(uint32_t aFormId, uint8_t aState)
        : FormId(aFormId)
        , State(aState)
    {
    }

    uint32_t FormId;
    uint8_t State;
};
