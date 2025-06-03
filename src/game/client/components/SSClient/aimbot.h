#pragma once
#include <game/client/component.h>
#include <game/generated/protocol.h>

#define OFFSET 17.f

class CSSCAimbot : public CComponent
{
public:
    int BestAngle(vec2 currpos, vec2 targetpos);
    void AimTo(CNetObj_PlayerInput *pInput, int LocalId, int id, bool silent = false, const float offset = OFFSET);
    void AutoAimTo(CNetObj_PlayerInput *pInput, int LocalId, float fovDeg, bool silent = false);
    bool IsHookable(const vec2& from, const vec2& to);
    bool AdvancedIsHookable(const vec2& from, const vec2& to, const float offset = OFFSET);

    virtual int Sizeof() const override { return sizeof(*this); }
};