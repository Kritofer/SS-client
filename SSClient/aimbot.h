#pragma once
#include <game/client/component.h>
#include <game/generated/protocol.h>

class CSSCAimbot : public CComponent
{
public:
    int BestAngle(vec2 currpos, vec2 targetpos, vec2 targetvel, const float leadTime = 0.15f);
    void AimTo(CNetObj_PlayerInput *pInput, int LocalId, int id, bool silent = false, const float offset = 20.0f);
    void AutoAimTo(CNetObj_PlayerInput *pInput, int LocalId, float fovDeg, bool silent = false);
    bool IsHookable(const vec2& from, const vec2& to);
    bool AdvancedIsHookable(const vec2& from, const vec2& to, const float offset = 20.f);

    virtual int Sizeof() const override { return sizeof(*this); }
};