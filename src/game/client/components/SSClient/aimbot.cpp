#include "aimbot.h"
#include <game/client/gameclient.h>
#include <game/client/prediction/entity.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/gameclient.h>

int CSSCAimbot::BestAngle(vec2 currpos, vec2 targetpos)
{
    // Predict target's future position
    vec2 predicted;
    predicted.x = targetpos.x;
    predicted.y = targetpos.y;

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
    vec2 currpos = pLocalChar->m_Pos;

    CGameWorld pWorld;
    pWorld.CopyWorld(&m_pClient->m_GameWorld);
    pWorld.m_WorldConfig.m_PredictWeapons = true;
    pWorld.m_WorldConfig.m_PredictFreeze = true;
    pWorld.m_WorldConfig.m_PredictTiles = true;

    int HookLength = m_pClient->GetTuning(g_Config.m_ClDummy)->m_HookLength;
    int HookSpeed = m_pClient->GetTuning(g_Config.m_ClDummy)->m_HookFireSpeed;
    int Ping = m_pClient->m_Snap.m_pLocalInfo->m_Latency;

    int HookTicks = 1;
    if(m_pClient->m_PredictedChar.m_ActiveWeapon == WEAPON_LASER && m_pClient->m_PredictedChar.m_Input.m_Fire)
        HookTicks = 0;
    
    for (; pWorld.m_GameTick <= (m_pClient->m_PredictedWorld.m_GameTick + HookTicks ); pWorld.m_GameTick++) { pWorld.Tick(); }

    auto pEnt = pWorld.GetCharacterById(id);
    if (!pEnt)
        return;
    vec2 baseTargetPos = pEnt->GetCore().m_Pos;

    const float leadTime = 0.f;

    // First try the center
    const float HookReachScalar = 0.11f;
    vec2 hookVec = normalize(baseTargetPos - currpos) * m_pClient->m_aTuning->m_HookLength;
    vec2 off = currpos + hookVec * HookReachScalar;
    int angleDeg = BestAngle(currpos, baseTargetPos);

    if(((angleDeg < 0 || !IsHookable(off, baseTargetPos)) || silent) && AdvancedIsHookable(currpos, baseTargetPos))
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
            vec2 hookVec = normalize(altPos - currpos) * m_pClient->m_aTuning->m_HookLength;
            vec2 off = currpos + hookVec * HookReachScalar;
            if(!IsHookable(off, altPos))
                continue;

            angleDeg = BestAngle(currpos, altPos);
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
        float wobbleAmplitude = 1.1f;
        float wobbleFrequency = 500.0f;
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

    const CGameClient::CClientData &pEnt = m_pClient->m_aClients[LocalId];
    CGameWorld world;
    world.CopyWorld(&m_pClient->m_GameWorld);

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
            //|| ent.m_Afk
            || ent.m_Solo
            || pLocalChar->m_Solo
            || ((ent.m_Team != pEnt.m_Team ) && world.m_WorldConfig.m_IsDDRace)
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
    float maxDistance = m_pClient->GetTuning(g_Config.m_ClDummy)->m_HookLength;
    if(m_pClient->m_PredictedChar.m_ActiveWeapon == WEAPON_LASER && m_pClient->m_PredictedChar.m_Input.m_Fire)
        maxDistance = m_pClient->GetTuning(g_Config.m_ClDummy)->m_LaserReach;
    if(m_pClient->m_PredictedChar.m_ActiveWeapon == WEAPON_SHOTGUN && m_pClient->m_PredictedChar.m_Input.m_Fire)
        maxDistance = m_pClient->GetTuning(g_Config.m_ClDummy)->m_LaserReach;
    if(distance(from, to) > maxDistance)
        return false;

    // Use the Teeworlds collision system's hook check
    return !Collision()->IntersectLineTeleHook(from, to, nullptr, nullptr);
}

bool CSSCAimbot::AdvancedIsHookable(const vec2& from, const vec2& to, const float offset)
{
    const float HookReachScalar = 0.11f;
    vec2 hookVec = normalize(to - from) * m_pClient->m_aTuning->m_HookLength;
    vec2 off = from + hookVec * HookReachScalar;

    if(IsHookable(off, to))
    {
        m_pClient->m_Draw.UCircleDraw(off, {0.f, 1.f, 0.f, 1.f}, 2.5f); // Green if valid
        return true;
    }

    vec2 offsets[] = {
        vec2( offset,  offset),
        vec2(-offset,  offset),
        vec2( offset, -offset),
        vec2(-offset, -offset)
    };

    for(const vec2& o : offsets)
    {
        if(IsHookable(off, to + o))
        {
            m_pClient->m_Draw.UCircleDraw(off, {1.f, 1.f, 0.f, 1.f}, 2.5f); // Yellow for offset match
            return true;
        }
    }

    m_pClient->m_Draw.UCircleDraw(off, {1.f, 0.f, 0.f, 1.f}, 2.5f); // Red if not hookable
    return false;
}