#include "CrosshairPickData.h"

CrosshairPickData* CrosshairPickData::Get() noexcept
{
    // CommonLibSSE: RELOCATION_ID(515446, 401585), a CrosshairPickData**
    POINTER_SKYRIMSE(CrosshairPickData*, s_singleton, 401585);
    return *s_singleton.Get();
}
