#include "aimbot.h"
#include <game/client/gameclient.h>

int CSSCAimbot::BestAngle(vec2 currpos, vec2 targetpos, vec2 targetvel, const float leadTime)
{
    // Predict target's future position
    vec2 predicted;
    predicted.x = targetpos.x + targetvel.x * leadTime;
    predicted.y = targetpos.y + targetvel.y * leadTime;

    // Calculate delta
    float dx = predicted.x - currpos.x;
    float dy = predicted.y - currpos.y;

    // Convert to angle (in degrees)
    float angleRad = atan2f(dy, dx);
    float angleDeg = angleRad * (180.0f / M_PI);

    // Normalize to 0-359
    if (angleDeg < 0)
        angleDeg += 360.0f;

    return static_cast<int>(angleDeg);
}

void CSSCAimbot::AimTo(CNetObj_PlayerInput *pInput, int LocalId, int id, bool silent, const float offset)
{
    if(!pInput)
        return;

    CCharacterCore *pLocalChar = &m_pClient->m_PredictedChar;
    if(!pLocalChar)
        return;
    vec2 currpos = pLocalChar->m_Pos;

    auto pEnt = m_pClient->m_aClients[id];
    vec2 baseTargetPos = pEnt.m_Predicted.m_Pos;
    vec2 targetvel = vec2(pEnt.m_RenderCur.m_VelX, pEnt.m_RenderCur.m_VelY);

    const float leadTime = 0.0f;

    // First try the center
    int angleDeg = BestAngle(currpos, baseTargetPos, targetvel, leadTime);

    if((angleDeg < 0 || !IsHookable(currpos, baseTargetPos)) && AdvancedIsHookable(currpos, baseTargetPos))
    {
        // Try around the hitbox
        vec2 offsets[] = {
            vec2( offset,  offset),
            vec2(-offset,  offset),
            vec2( offset, -offset),
            vec2(-offset, -offset)
        };

        bool found = false;
        for(const vec2& o : offsets)
        {
            vec2 altPos = baseTargetPos + o;
            if(!IsHookable(currpos, altPos))
                continue;

            angleDeg = BestAngle(currpos, altPos, targetvel, leadTime);
            if(angleDeg >= 0)
            {
                found = true;
                break;
            }
        }

        if(!found)
            return; // no visible hook point found
    }

    if(silent)
    {
        static float time = 0.0f;
        time += Client()->RenderFrameTime();
        float wobbleAmplitude = 2.f;
        float wobbleFrequency = 80.0f;
        float wobbleOffset = sinf(time * 2.0f * (float)M_PI * wobbleFrequency) * wobbleAmplitude;
        angleDeg += (int)wobbleOffset;
    }

    float rad = angleDeg * (float)M_PI / 180.0f;
    vec2 dir = vec2(cosf(rad), sinf(rad));
    vec2 NewTarget = vec2(int(dir.x * g_Config.m_ClMouseMaxDistance), int(dir.y * g_Config.m_ClMouseMaxDistance));

    pInput->m_TargetX = NewTarget.x;
    pInput->m_TargetY = NewTarget.y;
}

void CSSCAimbot::AutoAimTo(CNetObj_PlayerInput *pInput, int LocalId, float fovDeg, bool silent)
{
    if(!pInput)
        return;

    // Convert FOV to radians
    const float fovRad = fovDeg * (float)M_PI / 180.0f;

    // Get local character info
    CCharacterCore *pLocalChar = &m_pClient->m_PredictedChar;
    if(!pLocalChar)
        return;

    vec2 currpos = pLocalChar->m_Pos;
    vec2 viewDir = normalize(vec2(pInput->m_TargetX, pInput->m_TargetY));

    float closestAngle = fovRad;
    int bestTarget = -1;

    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(i == LocalId)
            continue;
       
        const CGameClient::CClientData &ent = m_pClient->m_aClients[i];
           
        // Skip inactive client
        if(!ent.m_Active 
            || ent.m_Solo
            || pLocalChar->m_Solo
            //|| (ent.m_LiveFrozen && g_Config.m_ClSSClientAimbotAlive)
        )
            continue;

        vec2 targetpos = vec2(ent.m_RenderCur.m_X, ent.m_RenderCur.m_Y);

        // if(targetpos == vec2(0, 0))
        //     return;

        // Check line of sight
        if(!AdvancedIsHookable(currpos, targetpos))
            continue;

        vec2 toTarget = normalize(targetpos - currpos);
        float angleBetween = acosf(clamp(dot(viewDir, toTarget), -1.0f, 1.0f));

        if(angleBetween < closestAngle)
        {
            closestAngle = angleBetween;
            bestTarget = i;
        }
    }

    if(bestTarget >= 0)
    {
        AimTo(pInput, LocalId, bestTarget, silent);
    }
}

bool CSSCAimbot::IsHookable(const vec2& from, const vec2& to)
{
    float maxDistance = 800.0f;
    if(distance(from, to) > maxDistance)
        return false;

    // Use the Teeworlds collision system's hook check
    return !Collision()->IntersectLineTeleHook(from, to, nullptr, nullptr);
}

bool CSSCAimbot::AdvancedIsHookable(const vec2& from, const vec2& to, const float offset)
{
    if(IsHookable(from, to))
        return true;

    vec2 offsets[] = {
        vec2( offset,  offset),
        vec2(-offset,  offset),
        vec2( offset, -offset),
        vec2(-offset, -offset)
    };

    for(const vec2& o : offsets)
    {
        if(IsHookable(from, to + o))
            return true;
    }

    return false;
}