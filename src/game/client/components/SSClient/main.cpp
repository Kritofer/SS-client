#include "main.h"
#include <engine/keys.h>
#include <engine/external/json-parser/json.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/client/components/controls.h>
#include <game/client/components/SSClient/aimbot.h>
#include <game/client/animstate.h>
#include <game/gamecore.h>
#include <game/generated/protocol.h>
#include <game/collision.h>
#include <base/system.h>
#include <curl/curl.h>
#include <utility> 
#include <stack>
#include <fstream>
#include <sstream>

#define VEC2_TRANSFORM(v, min, max) \
    vec2( \
        (((v).x) - (min)) / ((max) - (min)), \
        (((v).y) - (min)) / ((max) - (min)) \
    )

#define SORT std::sort

#define CNW CNetObj_PlayerInput::m_NextWeapon
#define CPW CNetObj_PlayerInput::m_PrevWeapon
#define CWW CNetObj_PlayerInput::m_WantedWeapon
#define CTX CNetObj_PlayerInput::m_TargetX
#define CTY CNetObj_PlayerInput::m_TargetY
#define CDR CNetObj_PlayerInput::m_Direction

#define Bor(true, false, reason) \
    (reason) ? true : false

// Or, more generally (using pointer-to-member):
template<typename U, typename V, typename M>
inline void CHANGE(U &one, const V &two, M U::*member) {
    one.*member = two.*member;  // IntelliSense shows all three parameters
}

CSSClient::CSSClient()
{

}

float CSSClient::GetTextSize(const char* pText, float FontSize)
{
    return TextRender()->TextWidth(FontSize, pText, -1, -1.0f);
}

void CSSClient::OnPlayerRender(vec2 Velocity, vec2 Position, float Angle, int ClientId)
{
    if ((ClientId == m_pClient->m_Snap.m_LocalClientId && g_Config.m_ClSSClientShowColl == 1) || g_Config.m_ClSSClientShowColl == 0)
        return;

    CCamera &Camera = m_pClient->m_Camera;
    m_pClient->m_Draw.CenterTo(Camera.m_Center, Camera.m_Zoom);
    vec2 moved = Position + vec2(cosf(Angle), sinf(Angle)) * 400.0f;

    m_pClient->m_Draw.LineDraw(Position, moved, vec4(.0f, 1.f, .0f, .3f));
}

void CSSClient::ChatDraw(float x, float y, std::string Name, std::string Text, bool IsFriend, bool ShouldHighlight)
{
    static const float offset = 1.86f;
    static const float MaxWidth = 145.f;
    static const float FontSize = 5.f;

    if (GetTextSize(Text.c_str(), FontSize) == 0) return;

    ColorRGBA ori = TextRender()->GetTextColor();

    std::string T;
    if (Name == "*** ")
    {
        TextRender()->TextColor({1.f, 1.f, 0.5f, 1.f});
        T = Name + Text;
    }
    else if (Name == "— ")
    {
        TextRender()->TextColor({0.6f, 1.f, 0.6f, 1.f});
        T = Name + Text;
    }
    else {
        if(IsFriend)
        {
            Name = "♡"+Name;
        }

        if (ShouldHighlight)
        {
            TextRender()->TextColor({1.f, 0.6f, 0.6f, 1.f});
            T = Name + ": " + Text;
        }
        else
        {
            TextRender()->TextColor({1.f, 1.f, 1.f, 1.f});
            T = Name + ": " + Text;
        }
    }

    // Wrap text by inserting \n when width exceeds MaxWidth
    std::string Wrapped;
    std::string CurrentLine;
    std::istringstream iss(T);
    std::string word;
    float h = 0.f;
    
    while (iss >> word)
    {
        std::string test = CurrentLine.empty() ? word : CurrentLine + " " + word;
        if (GetTextSize(test.c_str(), FontSize) > MaxWidth)
        {
            Wrapped += CurrentLine + "\n";
            h += FontSize;
            CurrentLine = word;
        }
        else
        {
            if (!CurrentLine.empty()) CurrentLine += " ";
            CurrentLine += word;
        }
    }

    if (!CurrentLine.empty())
    {
        Wrapped += CurrentLine;
        h += FontSize + offset;
    }

    // Draw background
    CUIRect r, l;
    r.x = x;
    l.x = x - 1.5f;
    r.y = y - offset;
    l.y = y - offset;
    r.h = h;
    l.h = h;
    r.w = MaxWidth;
    l.w = 1.5f;
    r.Draw({0.0f, 0.0f, 0.0f, 0.3f}, IGraphics::CORNER_NONE, 0.0f);

    l.Draw({0.8f, 0.0f, 0.0f, 0.7f}, IGraphics::CORNER_NONE, 0.0f);

    // Draw wrapped text
    TextRender()->Text(x, y, FontSize, Wrapped.c_str());

    // Restore text color
    TextRender()->TextColor(ori);
}

void CSSClient::TempSaveC(IConfigManager *pConfig, void *pUserData)
{
    // Convert back to CSSClient instance
    CSSClient *pThis = static_cast<CSSClient *>(pUserData);
    pThis->TempSave();
}

void CSSClient::TempSave()
{
    SaveTas("data/temp.ssc");
}

void CSSClient::OnConsoleInit()
{
    LoadTas("data/temp.ssc");
    ConfigManager()->RegisterCallback(&CSSClient::TempSaveC, this);
}

void CSSClient::Update(CNetObj_PlayerInput *aInputdata, int LocalId, CNetObj_PlayerInput *aInputdummy, int DummyId)
{
    bool is_dummy = g_Config.m_ClDummy;

    if (g_Config.m_ClSSClientTasState == 0)
        m_RecordIndex = 0;
    else if (g_Config.m_ClSSClientTasState == 1)
    {
        if (is_dummy)
            StartRecording(aInputdummy, DummyId, aInputdata, LocalId);
        else
            StartRecording(aInputdata, LocalId, aInputdummy, DummyId);
    }
    else if (g_Config.m_ClSSClientTasState == 2)
    {
        if (g_Config.m_ClDummy)
            PlayRecording(aInputdummy, DummyId, aInputdata, LocalId);
        else
            PlayRecording(aInputdata, LocalId, aInputdummy, DummyId);

        // and still update the engine’s dummy slot:
        // m_pClient->m_DummyInput = *aInputdummy;
        // m_pClient->m_DummyFire = aInputdummy->m_Fire;
    }
    else if (g_Config.m_ClSSClientTasState == 3)
    {
        m_RecordInputs.clear();
        m_RecordInputsD.clear();
        m_TasPos.clear();
        g_Config.m_ClSSClientTasState = 0;
        m_RecordIndex = 0;
    }

    if (g_Config.m_ClSSClientBotHookEnabled && g_Config.m_ClSSClientBotEnabled)
    {
        Avoid_Freeze(aInputdata,  LocalId);
        Avoid_Freeze(aInputdummy, DummyId);
    }
        
    if (g_Config.m_ClSSClientBotMoveEnabled && g_Config.m_ClSSClientBotEnabled)
    {
        LeftRight_Avoid(aInputdata,  LocalId);
        LeftRight_Avoid(aInputdummy, DummyId);
    }

    if (g_Config.m_ClSSClientFakeAimEnabled)
    {
        Fake_Aim(aInputdata,  LocalId);
        Fake_Aim(aInputdummy, DummyId);
    }

    if (g_Config.m_ClSSClientAimbotEnabled)
        if (aInputdata->m_Hook || (aInputdata->m_Fire & 1))
            m_pClient->m_SSCAimbot.AutoAimTo(aInputdata, LocalId, g_Config.m_ClSSClientAimbotFov, m_pClient->m_GameWorld.m_WorldConfig.m_IsFNG);

    // TODO: make this actually work
    // LaserUnfreeze(aInputdata, LocalId);
}

void CSSClient::OnRender()
{
    // CheckAndCrash();

    static const CSkin *ninja_skin = m_pClient->m_Skins.Find("x_ninja");

    if (g_Config.m_ClSSClientHidden)
        return;

    Id = m_pClient->m_Snap.m_LocalClientId;

    if (Id < 0 || Id >= MAX_CLIENTS)
        return;

    PPredict = m_pClient->m_aClients[Id].m_Predicted;
    std::vector<std::string> enabledOptions;
    if(g_Config.m_ClSSClientEspEnabled) enabledOptions.push_back("ESP");
    if(g_Config.m_ClSSClientPredEnabled) enabledOptions.push_back("Predict");
    if(g_Config.m_ClSSClientAimbotEnabled) enabledOptions.push_back("Aimbot");
    if(g_Config.m_ClSSClientBotMoveEnabled && g_Config.m_ClSSClientBotEnabled) enabledOptions.push_back("BotMove");
    if(g_Config.m_ClSSClientBotHookEnabled && g_Config.m_ClSSClientBotEnabled) enabledOptions.push_back("BotHook");
    if(g_Config.m_ClSSClientTasState != 0 && g_Config.m_ClSSClientTasState != 3) enabledOptions.push_back("TAS");
    if(g_Config.m_ClSSClientFakeAimEnabled) enabledOptions.push_back("FakeAim");

    SORT(enabledOptions.begin(), enabledOptions.end(), [this](const std::string& a, const std::string& b) {
        return GetTextSize(a.c_str()) > GetTextSize(b.c_str());
    });      

    CCamera &Camera = m_pClient->m_Camera;
    m_pClient->m_Draw.CenterTo(vec2(0, 0), 1);

    vec2 m_Target = Camera.m_Center + vec2(m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetX, m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetY);

    ColorRGBA ori = TextRender()->GetTextColor();
    TextRender()->TextColor(0, 0.75f, 0.35f, 1);

    // Draw activated cheats
    float xRight = 700; // padding from right edge
    float y = -200; // starting y

    for(const auto& label : enabledOptions)
    {
        float w = GetTextSize(label.c_str(), 20.0f);
        TextRender()->Text(xRight - w, y, 20.0f, label.c_str(), -1.0f);
        y += 22.0f; // line spacing
    }

    TextRender()->TextColor(ori);
    ori = NULL;

    if (g_Config.m_ClSSClientEspEnabled)
        Esp();

    if (g_Config.m_ClSSClientPredEnabled && g_Config.m_ClSSClientTasState != 1)
        Predict_Pos();

    else if ((!m_PredictedPos.empty()) && g_Config.m_ClSSClientTasState != 1)
    {
       m_PredictedPos.clear();
       m_PredictedColor.clear();
    }

    m_pClient->m_Draw.CenterTo(Camera.m_Center, Camera.m_Zoom);

    vec2 m_Pos = PPredict.m_Pos;
    vec2 m_PTarget = vec2(PPredict.m_Input.m_TargetX, PPredict.m_Input.m_TargetY);
    vec2 m_DTarget = vec2(m_pClient->m_DummyInput.m_TargetX, m_pClient->m_DummyInput.m_TargetY);

    if (Weapon == WEAPON_LASER || Weapon == WEAPON_SHOTGUN)
    {
        if (g_Config.m_ClSSClientTasState == 1)
            ShowLaserPath(TeePos, m_PTarget);
        else
            ShowLaserPath(m_Pos, m_PTarget);
    }
    else if (Weapon == WEAPON_GRENADE)
    {
        if (g_Config.m_ClSSClientTasState == 1)
            ShowGrenadePath(TeePos, m_PTarget);
        else
            ShowGrenadePath(m_Pos, m_PTarget);
    }

    {
        vec2 POSS = Bor(TeePos, m_Pos, g_Config.m_ClSSClientTasState == 1);
        vec2 LP1 = POSS;
        vec2 LP2 = normalize(m_PTarget)*m_pClient->m_aTuning->m_HookLength;
        vec2 T = LP1+LP2;
        vec4 C;
        Collision()->IntersectLine(LP1, LP1+LP2, nullptr, &LP2);
        if (T == LP2)
            C = vec4(.7f, .1f, .0f, 1.f);
        else
            C = vec4(.1f, .7f, .0f, 1.f);

        m_pClient->m_Draw.LineDraw(LP1, LP2, C);
    }

    m_pClient->m_Draw.CircleDraw(m_Target,                    vec4(.0f, .5f, 1.f, .2f), 20.0f);
    m_pClient->m_Draw.CircleDraw(m_DTarget + Camera.m_Center, vec4(1.f, .5f, .0f, .15f), 20.0f);

    for (size_t i = 1; i < m_PredictedPos.size(); i++)
    {
        vec2 Prev = m_PredictedPos[i - 1];
        vec2 Curr = m_PredictedPos[i];
        vec3 Color = m_PredictedColor[i];
        m_pClient->m_Draw.CircleDraw(Curr, vec4(Color.r, Color.g, Color.b, 0.8f), 3.f);// m_pClient->m_Draw.LineDraw(Prev, Curr, vec4(Color.r, Color.g, Color.b, 0.8f));
    }

    int state = g_Config.m_ClSSClientTasState;
    if ((state != 3) && !m_TasPos.empty())
    {
        vec2 pos = m_TasPos[0];
        vec2 ppos = (state == 1) ? TeePos : m_Pos;

        for (vec2 prevpos : m_TasPos)
        {
            if (distance(ppos, prevpos) < 500.f && ppos != prevpos) 
                m_pClient->m_Draw.CircleDraw(pos, vec4(0.f, 0.6f, 0.3f, .7f), 3.f); // m_pClient->m_Draw.LineDraw(prevpos, pos, vec4(0.f, 0.6f, 0.3f, 1.f));
            pos = prevpos;
        }
    }

    if (g_Config.m_ClSSClientTasState == 1 && TeePos != vec2(0,0))
    {
        vec2 Pos = GetTasPos();

        // Use ghost tee rendering (e.g., semi-transparent)
        CGameClient::CClientData pData = m_pClient->m_aClients[m_pClient->m_aLocalIds[g_Config.m_ClDummy]];
        CTeeRenderInfo pInfo = pData.m_RenderInfo;
        // dbg_msg("SSC_SKIN", "valid: %d", ninja_skin.IsValidName(ninja_skin.GetName()));

        if (Frozen)
        {
            pInfo.Apply(ninja_skin);
        }
        
        for(auto pLaser : Lasers)
        {
            // Lasers usually have From() and Pos() methods for endpoints
            vec2 From = pLaser.GetFrom();
            vec2 To = pLaser.GetPos();
            vec4 Color = vec4(1.0f, 0.6f, 0.0f, 1.0f);
            m_pClient->m_Draw.LineDraw(From, To, Color); // red laser line
        }
        
        for(auto pPos : Projectiles_Pos)
        {
            m_pClient->m_Draw.CircleDraw(pPos, vec4(.7f, .7f, .3f, 1.f), 10.f); // yellow projectile trail
        }

        Graphics()->TextureClear();    
        RenderTools()->RenderTee(
            CAnimState::GetIdle(), 
            &pInfo, 
            pData.m_Emoticon, 
            normalize(
                vec2(
                    m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetX, 
                    m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetY
                )
            ), 
            Pos
        );
        
        if (m_HookFlying)
            m_pClient->m_Draw.LineDraw(Pos, HPos, vec4(.7f, .7f, .1f, 1.f));
        
    }
}

void CSSClient::Fake_Aim(CNetObj_PlayerInput *pInput, int LocalId)
{
    // Check if LocalId is within valid bounds
    if (LocalId < 0 || LocalId >= MAX_CLIENTS)
        return;
        
    // Check if the player is not hooking and not firing (left mouse button)
    if (!(pInput->m_Hook) && !(pInput->m_Fire & 1))
    {
        // Makes the player look up in the server
        pInput->m_TargetX = 0;
        pInput->m_TargetY = -1;
    }
}

int CSSClient::SimulateWithBacktracking(CCharacterCore startPredict, CNetObj_PlayerInput input, int depth)
{
    struct SimState
    {
        CCharacterCore core;
        CNetObj_PlayerInput input;
        int step;
    };

    std::stack<SimState> simStack;
    simStack.push({startPredict, input, 0});

    int totalSteps = 0;

    while (!simStack.empty())
    {
        SimState state = simStack.top();
        simStack.pop();

        if (state.step >= depth)
            continue;

        CCharacterCore predict1 = state.core;
        CCharacterCore predict0 = state.core;

        CNetObj_PlayerInput input1 = state.input;
        CNetObj_PlayerInput input0 = state.input;
        input0.m_Hook = !state.input.m_Hook;

        bool h1 = true;
        bool h0 = true;

        int ticks1 = SimulateInput(predict1, input1, (depth-totalSteps)+1, &h1);
        int ticks0 = SimulateInput(predict0, input0, (depth-totalSteps)+1, &h0);

        if (ticks1 > ticks0)
        {
            predict1.m_Input = input1;
            SimulateStep(&predict1);
            simStack.push({predict1, input1, state.step + 1});
            totalSteps++;
        }
        if (ticks0 > ticks1)
        {
            predict0.m_Input = input0;
            SimulateStep(&predict0);
            simStack.push({predict0, input0, state.step + 1});
            totalSteps++;
        }
        if (ticks0 == 0 && ticks1 == 0)
        {
            simStack.push({predict0, input0, state.step + 1});
        }
    }

    return totalSteps;
}

// Count how many ticks (up to 'horizon') remain unfrozen
int CSSClient::CountSafeTicks(CGameWorld *world, CCharacter *cc, CNetObj_PlayerInput &input, int horizon)
{
    int safe = 0;
    int i = 0;
    while(i < horizon)
    {
        i++;
        cc->OnDirectInput(&input);
        world->m_GameTick++;
        cc->OnPredictedInput(&input);
        world->Tick();

        if (cc->m_FreezeTime > 0)
            continue;
        safe++;
    }
    return safe;
}

void CSSClient::Avoid_Freeze(CNetObj_PlayerInput *pInput, int LocalId)
{
    // Validate prereqs
    if(!m_pClient || !m_pClient->m_Snap.m_pLocalCharacter) return;
    if(LocalId < 0 || LocalId >= MAX_CLIENTS) return;

    // Base predicted core and input

    if (m_pClient->m_aClients[LocalId].m_Predicted.m_Pos == vec2(0,0)) return;

    CGameWorld baseWorld, changeworld;
    baseWorld. CopyWorld(&m_pClient->m_PredictedWorld);
    changeworld.CopyWorld(&m_pClient->m_PredictedWorld);

    CCharacter *pLocalChar  = baseWorld.  GetCharacterById(LocalId);
    CCharacter *pChangeChar = changeworld.GetCharacterById(LocalId);

    if(!pLocalChar) return;

    CNetObj_PlayerInput original = *pInput;

    // Determine horizon from config
    int horizon = g_Config.m_ClSSClientBotTick;

    // Simulate safe-tick counts for both hook states
    original.m_Hook = pInput->m_Hook; 
    int safeOriginal = CountSafeTicks(&baseWorld, pLocalChar, original, horizon);

    CNetObj_PlayerInput toggled = original;
    toggled.m_Hook = !original.m_Hook;
    int safeToggled = CountSafeTicks(&changeworld, pChangeChar, toggled, horizon);

    // dbg_msg("SSC", "change: %d, not: %d", safeToggled, safeOriginal);

    // Only toggle if it clearly improves safety
    if(safeOriginal < safeToggled)
    {
        pInput->m_Hook = toggled.m_Hook;
    }
    // Otherwise leave hook unchanged
}

void CSSClient::LeftRight_Avoid(CNetObj_PlayerInput *pInput, int LocalId)
{
    // Validate local client ID
    if (LocalId < 0 || LocalId >= MAX_CLIENTS)
        return;

    // Get the predicted position of the local player
    CCharacterCore Predict = m_pClient->m_aClients[LocalId].m_Predicted;
    if (Predict.m_Pos == vec2(0, 0))
        return;

    CGameWorld stopworld, leftworld, rightworld;

    stopworld .CopyWorld(&m_pClient->m_PredictedWorld);
    leftworld .CopyWorld(&m_pClient->m_PredictedWorld);
    rightworld.CopyWorld(&m_pClient->m_PredictedWorld);

    CCharacter *pLocalChar = stopworld .GetCharacterById(LocalId);
    CCharacter *pLeftChar  = leftworld .GetCharacterById(LocalId);
    CCharacter *pRightChar = rightworld.GetCharacterById(LocalId);

    // crash check
    if(!pLocalChar) return;

    // Init Dir
    CNetObj_PlayerInput stop  = *pInput;
    CNetObj_PlayerInput right = *pInput;
    CNetObj_PlayerInput left  = *pInput;

    // setup
    stop. m_Direction =  0;
    right.m_Direction =  1;
    left. m_Direction = -1;

    // horizon
    int hor = g_Config.m_ClSSClientBotTick2;

    // Initialize direction counters
    int dir01 = CountSafeTicks(&rightworld, pRightChar, right, hor); // Right (1)
    int dir00 = CountSafeTicks(&stopworld,  pLocalChar, stop,  hor); // No movement (0)
    int dir11 = CountSafeTicks(&leftworld,  pLeftChar,  left,  hor); // Left (-1)

    // Determine the best direction based on simulation results
    DetermineBestDirection(pInput, dir01, dir00, dir11);
}

void CSSClient::Predict_Pos() 
{
    if (!m_pClient)
        return;

    int LocalId = m_pClient->m_Snap.m_LocalClientId;
    if (LocalId < 0 || LocalId >= MAX_CLIENTS)
        return;

    m_PredictedPos.clear();
    m_PredictedColor.clear();
    
    CCharacterCore Predict = m_pClient->m_aClients[LocalId].m_Predicted;

    if (Predict.m_Pos == vec2(0,0))
        return;

    CGameWorld pred;
    pred.CopyWorld(&m_pClient->m_PredictedWorld);

    CNetObj_PlayerInput inp = Predict.m_Input;

    CCharacter *pLocalChar = pred.GetCharacterById(m_pClient->m_aLocalIds[g_Config.m_ClDummy]);

    if (!pLocalChar) return;

    for (int i = 0; i < g_Config.m_ClSSClientPredTicks ; i++)
    {
        vec3 Color = vec3(0.f, .7f, 0.f);
        
        pLocalChar->OnDirectInput(&inp);
        pred.m_GameTick++;
        pLocalChar->OnPredictedInput(&inp);

        pred.Tick();

        if (pLocalChar->m_FreezeTime > 0)
        {
            Color = vec3(.7f, 0.f, 0.f);
        }
        m_PredictedPos.push_back(pLocalChar->GetCore().m_Pos);
        m_PredictedColor.push_back(Color);
    }
}

void CSSClient::Esp()
{
    if (!m_pClient || !m_pClient->m_Snap.m_pLocalCharacter)
    {
        return;
    }

    int LocalId = m_pClient->m_Snap.m_LocalClientId;
    if (LocalId < 0 || LocalId >= MAX_CLIENTS)
    {
        return;
    }

    vec2 LocalPos = m_pClient->m_aClients[LocalId].m_RenderPos;

    CCamera &Camera = m_pClient->m_Camera;
    m_pClient->m_Draw.CenterTo(Camera.m_Center, Camera.m_Zoom);

    Graphics()->TextureClear();

    for (int i = 0;i < MAX_CLIENTS;i++)
    {
        if (i == LocalId 
            || !m_pClient->m_aClients[i].m_Active 
            || m_pClient->m_aClients[i].m_Predicted.m_Pos == vec2(0, 0)
            || distance(m_pClient->m_aClients[i].m_Predicted.m_Pos, LocalPos) >= g_Config.m_ClSSClientEspDistance
        )
            continue;
        CGameClient::CClientData *Enemy = &m_pClient->m_aClients[i];

        vec2 EnemyPos = Enemy->m_Predicted.m_Pos;

        m_pClient->m_Draw.LineDraw(LocalPos, EnemyPos, vec4(.7f, .7f, .0f, 1.f));

        vec2 NamePos = (LocalPos*3 + EnemyPos) / 4;
        NamePos.y -= 20;

        float NameWidth = TextRender()->TextWidth(15.f, Enemy->m_aName, -1, 200.f);
        TextRender()->Text(NamePos.x - (NameWidth / 2), NamePos.y, 15.f, Enemy->m_aName, 200.f);
    }
}

vec2 Quantize(vec2 v)
{
    return vec2(
        static_cast<int>(v.x * 1000) / 1000.0f,
        static_cast<int>(v.y * 1000) / 1000.0f
    );
}

void CSSClient::StartRecording(CNetObj_PlayerInput *pInput, int LocalId, CNetObj_PlayerInput *dInput, int DummyId)
{
    // —————————————————————————————————————————————
    // 1) Validate LocalId early and reset positions on error
    // —————————————————————————————————————————————
    if (LocalId < 0 || LocalId >= MAX_CLIENTS)
    {
        TeePos    = vec2(0,0);
        OldTeePos = vec2(0,0);
        return;
    }

    Lasers.clear();
    Projectiles_Pos.clear();
    m_TasPos.clear();
    if (g_Config.m_ClSSClientPredEnabled)
    {
        m_PredictedPos.clear();
        m_PredictedColor.clear();
    }

    // —————————————————————————————————————————————
    // 2) Copy & clear real input so the engine doesn’t act on it
    // —————————————————————————————————————————————
    static CNetObj_PlayerInput nInput = *pInput;
    static CNetObj_PlayerInput kInput = *dInput;

    // —————————————————————————————————————————————
    // 3) Capture *all* relevant keys: left/right, jump, hook & fire
    //    (DDRacing leans heavily on small directional tweaks and firing
    //     to reset your hook cooldown!)
    // —————————————————————————————————————————————

    if (g_Config.m_ClDummy)
    {
        CHANGE(kInput, *dInput, &CDR); // dir

        CHANGE(kInput, *dInput, &CTX); // target x
        CHANGE(kInput, *dInput, &CTY); // target y

        CHANGE(kInput, *dInput, &CWW); // wanted weapon
        CHANGE(kInput, *dInput, &CNW); // next weapon
        CHANGE(kInput, *dInput, &CPW); // previous weapon

        kInput.m_Jump  = Input()->KeyIsPressed(KEY_SPACE)   ? 1 : 0;
        kInput.m_Hook  = Input()->KeyIsPressed(KEY_MOUSE_2) ? 1 : 0;
        kInput.m_Fire  = Input()->KeyIsPressed(KEY_MOUSE_1) ? 1 : 0;
    }
    else
    {
        CHANGE(nInput, *pInput, &CDR); // dir

        CHANGE(nInput, *pInput, &CTX); // target x
        CHANGE(nInput, *pInput, &CTY); // target y

        CHANGE(nInput, *pInput, &CWW); // wanted weapon
        CHANGE(nInput, *pInput, &CNW); // next weapon
        CHANGE(nInput, *pInput, &CPW); // previous weapon

        nInput.m_Jump  = Input()->KeyIsPressed(KEY_SPACE)   ? 1 : 0;
        nInput.m_Hook  = Input()->KeyIsPressed(KEY_MOUSE_2) ? 1 : 0;
        nInput.m_Fire  = Input()->KeyIsPressed(KEY_MOUSE_1) ? 1 : 0;
    }

    ZeroInput(pInput);
    ZeroInput(dInput);

    // —————————————————————————————————————————————                        
    // 4) Handle rewind safely (only pop if we actually recorded something)
    // —————————————————————————————————————————————
    if (g_Config.m_ClSSClientTasRewind)
    {
        if (!m_RecordInputs.empty()) m_RecordInputs.pop_back();
        if (!m_RecordInputsD.empty()) m_RecordInputsD.pop_back();
        g_Config.m_ClSSClientTasRewind = 0;
        // return;
    }

    // —————————————————————————————————————————————
    // 5) Record this tick’s input when “step” is hit
    //    (you can just tap your bind to step frame‑by‑frame)
    // —————————————————————————————————————————————
    if (g_Config.m_ClSSClientTasStep) //  || (m_pClient->m_GameWorld.m_GameTick % 20) == 0
    {
        m_RecordInputs.push_back(nInput);
        m_RecordInputsD.push_back(kInput);
        g_Config.m_ClSSClientTasStep = 0;
    }

    // —————————————————————————————————————————————
    // 6) Re‑simulate *all* recorded inputs from the current predicted state
    //    (so you always see exactly where your tee would land)
    // —————————————————————————————————————————————
    m_PredIndex = m_RecordInputs.size();
    CGameClient::CClientData &LocalPlayer = m_pClient->m_aClients[LocalId];
    CGameWorld baseWorld, before;
    baseWorld.CopyWorld(&m_pClient->m_PredictedWorld);

    CCharacter *pLocalChar = baseWorld.GetCharacterById(LocalId);
    if(!pLocalChar)
      return;

    bool hasdummy = true;
    CCharacter *pLocalDummy = baseWorld.GetCharacterById(DummyId);
    if(!pLocalDummy)
      hasdummy = false;

    OldTeePos = mix(OldTeePos, TeePos, 0.5);
    vec2 o = OldTeePos;

    for (int i = 0; i < m_RecordInputs.size(); i++)
    {
        CNetObj_PlayerInput pinp = m_RecordInputs[i];
        CNetObj_PlayerInput dinp = m_RecordInputsD[i];

        pLocalChar->OnDirectInput(&pinp);
        if (hasdummy)
        {pLocalDummy->OnDirectInput(&dinp);}
        baseWorld.m_GameTick++;
        pLocalChar->OnPredictedInput(&pinp);
        if (hasdummy)
        {pLocalDummy->OnPredictedInput(&dinp);}

        // FixWeaponTP(baseWorld);

        baseWorld.Tick();
        if (g_Config.m_ClDummy)
        TeePos = pLocalDummy->GetPos();
        else
        TeePos = pLocalChar->GetPos();

        m_TasPos.push_back(TeePos);
    }
    if (g_Config.m_ClSSClientPredEnabled)
    {
        before.CopyWorld(&baseWorld);
        CCharacter *bef = before.GetCharacterById(LocalId);
        CCharacter *befd = before.GetCharacterById(DummyId);
        vec2 t = TeePos;
        if (TeePos != vec2(0, 0))
        {
            m_PredictedPos.push_back(t);
            m_PredictedColor.push_back(bef->m_FreezeTime > 0 ? vec3(.7f, 0.f, 0.f) : vec3(0.f, .7f, 0.f));
        }
        bool f;
        for (int i = 0; i < g_Config.m_ClSSClientPredTicks; i++)
        {
            o = t;
            bef->OnDirectInput(&nInput);
            if (hasdummy)
            {befd->OnDirectInput(&kInput);}

            before.m_GameTick++;
            bef->OnPredictedInput(&nInput);

            if (hasdummy)
            {befd->OnPredictedInput(&kInput);}

            before.Tick();

            if (g_Config.m_ClDummy)
            {
            t = befd->GetPos();
            f = befd->m_FreezeTime;
            }
            else
            {
            t = bef->GetPos();
            f = bef->m_FreezeTime;
            }

            m_PredictedPos.push_back(t);
            m_PredictedColor.push_back(f > 0 ? vec3(.7f, 0.f, 0.f) : vec3(0.f, .7f, 0.f));
        }
    }
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if ((i == LocalId && !g_Config.m_ClDummy) || (i == DummyId && g_Config.m_ClDummy))
            continue;

        CCharacter *client = baseWorld.GetCharacterById(i);
        if (!client)
            continue;

        vec2 pos = client->GetPos();
        m_pClient->m_Draw.UCircleDraw(pos, vec4(.7f, 0.f, 0.f, 1.f), 20.f);
    }
    CCharacter *pChar;
    if (g_Config.m_ClDummy)
        pChar = pLocalDummy;
    else
        pChar = pLocalChar;

    Frozen = pChar->m_FreezeTime > 0;
    Weapon = pChar->GetActiveWeapon();
    for(int Type : { CGameWorld::ENTTYPE_LASER, CGameWorld::ENTTYPE_PROJECTILE })
    {
        for(CEntity *pEnt = baseWorld.FindFirst(Type); pEnt; pEnt = pEnt->TypeNext())
        {
            if(Type == CGameWorld::ENTTYPE_LASER)
            {
                CLaser *pLaser = static_cast<CLaser *>(pEnt);
                if(!pLaser) continue;

                Lasers.emplace_back(*pLaser);
            }
            else if(Type == CGameWorld::ENTTYPE_PROJECTILE)
            {
                CProjectile *pProj = static_cast<CProjectile *>(pEnt);
                if(!pProj) continue;

                float Time = (baseWorld.GameTick() - pProj->GetStartTick()) / (float)Client()->GameTickSpeed();

                Projectiles_Pos.emplace_back(pProj->GetPos(Time));
            }
        }
    }
    m_HookFlying = pChar->GetCore().m_HookState != HOOK_IDLE;
    HPos = pChar->GetCore().m_HookPos;
}

void CSSClient::PlayRecording(CNetObj_PlayerInput *pInput, int LocalId, CNetObj_PlayerInput *dInput, int DummyId)
{
    // 1) Invalid client or no recording → clear input
    if (LocalId < 0 || LocalId >= MAX_CLIENTS || m_RecordInputs.empty())
    {
        ZeroInput(pInput);
        ZeroInput(dInput);
        return;
    }

    // ZeroInput(pInput);
    // ZeroInput(dInput);

    bool HasDummy = true;

    if (DummyId < 0 || DummyId >= MAX_CLIENTS || m_RecordInputsD.empty())
    {
        HasDummy = false;
    }

    // 2) If TAS playback is turned off, just clear input
    // if (!g_Config.m_ClSSClientTasState)
    // {
    //     ZeroInput(pInput);
    //     ZeroInput(dInput);
    //     return;
    // }

    // 3) If we've reached the end, stop playback and reset index
    if (m_RecordIndex >= static_cast<int>(m_RecordInputs.size()))
    {
        // Reset ONLY if explicitly stopped
        if (g_Config.m_ClSSClientTasState == 2)
        {
            m_RecordIndex = 0;
            g_Config.m_ClSSClientTasState = 0;
        }
        // ZeroInput(pInput);
        // ZeroInput(dInput);
        return;
    }

    // 4) Otherwise feed the next recorded frame into the engine
    
    *pInput = m_RecordInputs[m_RecordIndex];
    if (HasDummy) 
    {*dInput = m_RecordInputsD[m_RecordIndex];}
    m_RecordIndex++;
}

void CSSClient::ChangeDir(int new_Direction)
{
    m_pClient->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] = (new_Direction == -1);
    m_pClient->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy] = (new_Direction == 1);
}

void CSSClient::ChangeMPos(vec2 new_MousePos)
{
    m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetX = static_cast<int>(new_MousePos.x);
    m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_TargetY = static_cast<int>(new_MousePos.y);
}

void CSSClient::ZeroInput(CNetObj_PlayerInput *Input)
{
    Input->m_Direction = 0;
    Input->m_Jump = 0;
    Input->m_Fire = 0;
    Input->m_Hook = 0;
}

// Reflects vector v around normal n
static vec2 Reflect(const vec2 &v, const vec2 &n)
{
    return v - n * (2.0f * dot(v, n));;
}

// Samples the tile‐collision normal at a hit point.
// Rough but works for axis‐aligned tiles.
static vec2 GetCollisionNormalAt(const vec2 &hitPos, CCollision *col)
{
    int tx = int(hitPos.x) / 32;
    int ty = int(hitPos.y) / 32;
    vec2 normal{0,0};

    // check free space on each side; if neighbor is solid, normal points outwards
    if (col->GetIndex(tx-1, ty) == TILE_SOLID) normal.x +=  1;  // solid on left → normal points right
    if (col->GetIndex(tx+1, ty) == TILE_SOLID) normal.x += -1;
    if (col->GetIndex(tx, ty-1) == TILE_SOLID) normal.y +=  1;
    if (col->GetIndex(tx, ty+1) == TILE_SOLID) normal.y += -1;

    if (length(normal) < 0.1f)   // fallback if corner or isolated
        return vec2(0,1);        // treat as floor
    return normalize(normal);
}


// Returns true if a ray from `src` aimed at `dst` will reach `dst`
// after reflecting off walls up to `maxBounces` times.
bool CSSClient::BounceCheck(const vec2 &src, const vec2 &dst, int maxBounces)
{
    CCollision *col = Collision();
    vec2   dir   = dst - src;
    float  dist0 = length(dir);
    if (dist0 < 0.001f) return true;
    dir = normalize(dir);

    vec2  start = src;
    float remaining = dist0;

    for(int bounce = 0; bounce <= maxBounces; ++bounce)
    {
        // 1) try a straight shot for the remaining distance
        vec2  end = start + dir * remaining;
        vec2  hitPos;
        if (!col->IntersectLine(start, end, &hitPos, nullptr))
        {
            // nothing in the way → we hit dst!
            return true;
        }

        // 2) we hit a wall at hitPos before getting to dst
        float traveled = length(hitPos - start);
        remaining -= traveled;
        if (remaining < 0.001f)
            return false;   // we didn’t quite reach dst

        // 3) compute normal and reflect
        vec2 normal = GetCollisionNormalAt(hitPos, col);
        dir = Reflect(dir, normal);

        // 4) step off the wall a bit to avoid re‑hitting the same tile
        start = hitPos + dir * 0.1f;
    }

    return false;
}

void CSSClient::LaserUnfreeze(CNetObj_PlayerInput *pInput, int LocalId)
{
    // 1) Sanity checks
    if (LocalId < 0 || LocalId >= MAX_CLIENTS) return;
    auto &P = m_pClient->m_aClients[LocalId].m_Predicted;
    if (P.m_Pos == vec2(0,0) || P.m_IsInFreeze) return;

    // 2) Predict “unfreeze” landing spot
    CCharacterCore mP = P, fP = P;
    mP.Tick(true);  mP.Move();
    int ti = Collision()->GetIndex(int(mP.m_Pos.x/32), int(mP.m_Pos.y/32));
    if (ti != TILE_FREEZE) return;

    ZeroInput(&fP.m_Input);
    fP.m_Input.m_Hook = 0;
    fP.m_Pos = mP.m_Pos; 
    fP.m_Vel = mP.m_Vel;
    for (int i = 0; i < 10; ++i)
    {
        fP.Tick(true);  fP.Move();
        ti = Collision()->GetIndex(int(fP.m_Pos.x/32), int(fP.m_Pos.y/32));
        if (ti != TILE_FREEZE) break;
    }
    if (ti == TILE_FREEZE) return;
    fP.Tick(true,false); fP.Move();
    fP.Tick(true,false); fP.Move();

    // Draw landing spot
    m_pClient->m_Draw.UCircleDraw(fP.m_Pos, {0.7f,0.f,0.f,0.3f}, 20.f);

    // 3) “Wall‑bang” bounce planning
    const vec2 src = P.m_Pos;
    const vec2 dst = fP.m_Pos;
    float totalDist = length(dst - src);
    if (totalDist < 0.001f) return;

    CCollision *col = Collision();
    vec2 dir       = normalize(dst - src);
    vec2 start     = src;
    float remaining = totalDist;

    vec2 firstHitPos{0,0};
    bool gotFirstHit = false;
    bool canHit      = false;
    const int maxBounces = 3;

    for (int b = 0; b <= maxBounces; ++b)
    {
        vec2 end;
        vec2 hitPos;
        end = start + dir * remaining;

        // If no wall in the way, we’d hit dst now
        if (!col->IntersectLine(start, end, &hitPos, nullptr))
        {
            // only accept if we already recorded a wall hit
            if (gotFirstHit)
                canHit = true;
            break;
        }

        // record first wall‐hit position
        if (!gotFirstHit)
        {
            firstHitPos = hitPos;
            gotFirstHit = true;
        }

        // subtract traveled distance
        float traveled = length(hitPos - start);
        remaining -= traveled;
        if (remaining < 0.001f)
            break;

        // compute approximate tile normal
        int tx = int(hitPos.x) / 32;
        int ty = int(hitPos.y) / 32;
        vec2 normal{0,0};
        if (col->GetIndex(tx-1, ty) == TILE_SOLID) normal.x +=  1;
        if (col->GetIndex(tx+1, ty) == TILE_SOLID) normal.x += -1;
        if (col->GetIndex(tx, ty-1) == TILE_SOLID) normal.y +=  1;
        if (col->GetIndex(tx, ty+1) == TILE_SOLID) normal.y += -1;
        if (length(normal) < 0.1f)
            normal = {0,1};
        else
            normal = normalize(normal);

        // reflect direction
        dir = dir - normal * (2.0f * dot(dir, normal));

        // nudge off the wall
        start = hitPos + dir * 0.1f;
    }

    // 4) Fire at the wall hit position if we found a valid bounce path
    if (canHit)
    {
        vec2 aim = normalize(firstHitPos - src);
        float md = g_Config.m_ClMouseMaxDistance;
        pInput->m_TargetX = aim.x * md;
        pInput->m_TargetY = aim.y * md;
        pInput->m_Fire    = true;
    }
}

void CSSClient::ShowLaserPath(vec2 Pos, vec2 Target)
{
    vec2 Vel = normalize(Target); // laser speed per frame
    int ray_length = 800;
    int curr_bounces = 0;
    int MaxBounces = ray_length*2;

    vec2 PrevPos = Pos;

    for(int i = 0; i < ray_length; ++i)
    {
        Pos.x += Vel.x;
        if(Collision()->CheckPoint(Pos.x, Pos.y))
        {
            Vel.x = -Vel.x;
            Pos.x += Vel.x;
            curr_bounces++;
        }

        Pos.y += Vel.y;
        if(Collision()->CheckPoint(Pos.x, Pos.y))
        {
            Vel.y = -Vel.y;
            Pos.y += Vel.y;
            curr_bounces++;
        }

        // Draw each step segment
        m_pClient->m_Draw.LineDraw(PrevPos, Pos, vec4(1, 0.2f, 0.2f, 1.0f));
        PrevPos = Pos;

        if(curr_bounces > MaxBounces)
            break;
    }
}

void CSSClient::ShowGrenadePath(vec2 StartPos, vec2 Target)
{
    // 1) compute normalized firing direction
    vec2 Dir = normalize(Target);

    // 2) real Teeworlds parameters (tweak if you have custom values)
    const float Speed     = 775.0f;  // units/sec
    const float Curvature = 400.0f;   // gravity factor (units/sec²)
    const float TickRate  = (float)Client()->GameTickSpeed();    // usually 50
    const float StepTime  = 1.0f / TickRate;                 // seconds per tick

    // 3) simulate forward, drawing segment by segment
    vec2 PrevPos = StartPos;
    for(int Tick = 1; Tick < 100; ++Tick)  // simulate 100 ticks (2s)
    {
        float t = Tick * StepTime;

        // position = start + dir*speed*t + [0, curvature*t²]
        vec2 Pos;
        Pos.x = StartPos.x + Dir.x * Speed * t;
        Pos.y = StartPos.y + Dir.y * Speed * t + Curvature * t * t;

        // if we hit a wall between PrevPos→Pos, draw only up to hit‑point and stop
        vec2 Hit;
        if(Collision()->IntersectLine(PrevPos, Pos, &Hit, nullptr))
        {
            m_pClient->m_Draw.LineDraw(PrevPos, Hit, vec4(1.f, 0.4f, 0.4f, 1.f));
            return;
        }

        // otherwise draw this segment
        m_pClient->m_Draw.LineDraw(PrevPos, Pos, vec4(1.f, 0.4f, 0.4f, 1.f));
        PrevPos = Pos;
    }
}

void CSSClient::debug(const char *owner, const char *fmt, ...)
{
    char aBuf[256];
    va_list args;
    va_start(args, fmt);
    str_format(aBuf, sizeof(aBuf), fmt, args);
    va_end(args);

    if (owner != nullptr)
        dbg_msg("SSC", "%s: %s", owner, aBuf);
    else
        dbg_msg("SSC", "%s", aBuf);
}

void CSSClient::SimulateStep(CCharacterCore *Predict, bool deffered)
{   
    Predict->m_Pos = Quantize(Predict->m_Pos);
    Predict->m_Vel = Quantize(Predict->m_Vel);
    Predict->Tick(true, deffered);
    Predict->Move();
    int TileX = static_cast<int>(Predict->m_Pos.x) / 32; // Truncate, don't round
    int TileY = static_cast<int>(Predict->m_Pos.y) / 32;
    int TileIndex = Collision()->GetIndex(TileX, TileY);
    if (TileIndex == TILE_FREEZE || TileIndex == TILE_DEATH || TileIndex == TILE_TELEIN || Predict->m_IsInFreeze)
    {
        Predict->m_IsInFreeze = true;
    }    
}

int CSSClient::SimulateInput(CCharacterCore Predict, CNetObj_PlayerInput input_to_use, int ticks, bool *alive)
{
    int survived_ticks = 0;
    *alive = true;

    Predict.m_Input = input_to_use; // Apply the given input

    for (int i = 0; i < ticks; i++)
    {
        SimulateStep(&Predict, true);

        if (Predict.m_IsInFreeze)
        {
            *alive = false;
            break;
        }

        survived_ticks++;
    }

    return survived_ticks;
}

void CSSClient::SimulateMovement(CCharacterCore Predict, int direction, int &dirCounter)
{
    CNetObj_PlayerInput input_to_use = Predict.m_Input; // Initialize input
    input_to_use.m_Direction = direction;   // Set the direction

    bool alive = true;
    int ticks = g_Config.m_ClSSClientBotTick2;

    // Simulate the input and get the number of survived ticks
    dirCounter = SimulateInput(Predict, input_to_use, ticks, &alive);
}

void CSSClient::DetermineBestDirection(CNetObj_PlayerInput *pInput, int dir01, int dir00, int dir11)
{
    // If all directions are equally safe or unsafe, don't change the direction
    if (dir00 == dir01 && dir01 == dir11)
    {
        return;
    }

    // Determine the best direction
    if (dir00 > dir01 && dir00 > dir11)
    {
        pInput->m_Direction = 0; // No movement
    }
    else if (dir01 > dir00 && dir01 > dir11)
    {
        pInput->m_Direction = 1; // Right
    }
    else if (dir11 > dir01 && dir11 > dir00)
    {
        pInput->m_Direction = -1; // Left
    }
    // else if (dir00 == dir11 && dir00 > dir01 && (pInput->m_Direction == 1 || pInput->m_Direction == 0))
    // {
    //     pInput->m_Direction = 0; // No movement (tie between left and no movement)
    // }
    // else if (dir00 == dir01 && dir00 > dir11 && (pInput->m_Direction == -1 || pInput->m_Direction == 0))
    // {
    //     pInput->m_Direction = 0; // No movement (tie between right and no movement)
    // }
}

void CSSClient::SaveTas(const std::string &Filename)
{
    std::ofstream out(Filename);
    if(!out.is_open()) return;
    // Header
    out << "# SSC TAS Replay\n";
    out << "Version: 3\n";
    out << "Ticks: " << m_RecordInputs.size() << "\n\n";
    // Frames
    for(size_t i = 0; i < m_RecordInputs.size(); ++i)
    {
        const auto &f = m_RecordInputs[i];
        const auto &d = m_RecordInputsD[i];
        out << "<ID:" << i << ">\n";
        out << "[Fire="    << f.m_Fire     << "]\n";
        out << "[Hook="    << f.m_Hook     << "]\n";
        out << "[Jump="    << f.m_Jump     << "]\n";
        out << "[Dir="     << f.m_Direction<< "]\n";
        out << "[TargetX=" << f.m_TargetX  << "]\n";
        out << "[TargetY=" << f.m_TargetY  << "]\n";

        out << "[TargetW=" << f.m_WantedWeapon  << "]\n";

        out << '\n';

        out << "[DFire="   << d.m_Fire     << "]\n";
        out << "[DHook="   << d.m_Hook     << "]\n";
        out << "[DJump="   << d.m_Jump     << "]\n";
        out << "[DDir="    << d.m_Direction<< "]\n";
        out << "[DTargetX="<< d.m_TargetX  << "]\n";
        out << "[DTargetY="<< d.m_TargetY  << "]\n";

        out << "[DTargetW="<< d.m_WantedWeapon  << "]\n";

        out << "</>\n\n";
    }
    out.close();
}

void CSSClient::LoadTas(const std::string &Filename)
{
    std::ifstream in(Filename);
    if (!in.is_open())
        return;

    m_RecordInputs.clear();

    std::string line;
    CNetObj_PlayerInput currentInput{};
    CNetObj_PlayerInput currentDInput{};
    bool inFrame = false;

    while (std::getline(in, line))
    {
        if (line.rfind("<ID:", 0) == 0)
        {
            inFrame = true;
            currentInput = {}; // Reset for a new frame
        }
        else if (line == "</>")
        {
            inFrame = false;
            m_RecordInputs.push_back(currentInput);
            m_RecordInputsD.push_back(currentDInput);
        }
        else if (inFrame)
        {
            if (line.rfind("[Fire=", 0) == 0)
                currentInput.m_Fire = std::stoi(line.substr(6, line.find("]") - 6));
            else if (line.rfind("[Hook=", 0) == 0)
                currentInput.m_Hook = std::stoi(line.substr(6, line.find("]") - 6));
            else if (line.rfind("[Jump=", 0) == 0)
                currentInput.m_Jump = std::stoi(line.substr(6, line.find("]") - 6));
            else if (line.rfind("[Dir=", 0) == 0)
                currentInput.m_Direction = std::stoi(line.substr(5, line.find("]") - 5));
            else if (line.rfind("[TargetX=", 0) == 0)
                currentInput.m_TargetX = std::stoi(line.substr(9, line.find("]") - 9));
            else if (line.rfind("[TargetY=", 0) == 0)
                currentInput.m_TargetY = std::stoi(line.substr(9, line.find("]") - 9));
            else if (line.rfind("[TargetW=", 0) == 0)
                currentInput.m_WantedWeapon = std::stoi(line.substr(9, line.find("]") - 9));

            if (line.rfind("[DFire=", 0) == 0)
                currentDInput.m_Fire = std::stoi(line.substr(7, line.find("]") - 7));
            else if (line.rfind("[DHook=", 0) == 0)
                currentDInput.m_Hook = std::stoi(line.substr(7, line.find("]") - 7));
            else if (line.rfind("[DJump=", 0) == 0)
                currentDInput.m_Jump = std::stoi(line.substr(7, line.find("]") - 7));
            else if (line.rfind("[DDir=", 0) == 0)
                currentDInput.m_Direction = std::stoi(line.substr(6, line.find("]") - 6));
            else if (line.rfind("[DTargetX=", 0) == 0)
                currentDInput.m_TargetX = std::stoi(line.substr(10, line.find("]") - 10));
            else if (line.rfind("[DTargetY=", 0) == 0)
                currentDInput.m_TargetY = std::stoi(line.substr(10, line.find("]") - 10));
            else if (line.rfind("[DTargetW=", 0) == 0)
                currentDInput.m_WantedWeapon = std::stoi(line.substr(10, line.find("]") - 10));
        }
    }

    in.close();
}

void OnTimeResponse(char *pResponse, size_t Length, void *pUser)
{
    std::string TimeStr(pResponse, Length); // "2025-04-23T17:08:12+00:00"

    // Truncate to "YYYY-MM-DDTHH:MM:SS"
    TimeStr = TimeStr.substr(0, 19);
    std::replace(TimeStr.begin(), TimeStr.end(), 'T', ' ');

    struct tm Tm {};
    strptime(TimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &Tm);

    time_t Timestamp = timegm(&Tm); // Convert to Unix timestamp (UTC)
    dbg_msg("timer", "Parsed UNIX timestamp: %lld", (long long)Timestamp);

    // You can now compare it to the initial launch time to enforce a 24h limit
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void CSSClient::CheckAndCrash()
{
    // 1) Fire off the HTTP GET
    //    NOTE: HttpGet returns a CHttpRequest which you must then run & wait on
    std::shared_ptr<CHttpRequest> pReq = HttpGet("http://worldtimeapi.org/api/timezone/Etc/UTC");
    if (!pReq)
    {
        dbg_msg("SSC", "HttpGet failed → crash");
        Client()->Quit();
        return;
    }
    pReq->Timeout({ /* connect */ 5, /* total */ 10, /* low-speed */ 0, /* low-speed time */ 0 });
    // IClient::Engine()->Http()->Run(pReq);
    pReq->Wait();  // block until done :contentReference[oaicite:2]{index=2}

    // 2) Parse the JSON body
    json_value *pJson = pReq->ResultJson();
    if (!pJson)
    {
        dbg_msg("SSC", "ResultJson() failed → crash");
        Client()->Quit();
        return;
    }

    // 3) Extract the utc_datetime string
    //    operator[] and operator const char* are provided by json-parser/json.h
    const char *pUtc = (*pJson)["utc_datetime"];            // returns "" if missing :contentReference[oaicite:3]{index=3}
    std::string datetime(pUtc);
    json_value_free(pJson);                                 // MUST free the parse tree

    if (datetime.size() < 19)
    {
        dbg_msg("SSC", "Bad datetime format → crash");
        Client()->Quit();
        return;
    }

    // 4) Truncate to "YYYY-MM-DDTHH:MM:SS", replace 'T' with space
    datetime.resize(19);
    std::replace(datetime.begin(), datetime.end(), 'T', ' ');

    // 5) Convert to time_t
    struct tm tm_time = {};
    if (!strptime(datetime.c_str(), "%Y-%m-%d %H:%M:%S", &tm_time))
    {
        dbg_msg("SSC", "strptime failed → crash");
        Client()->Quit();
        return;
    }
    time_t ts = timegm(&tm_time);

    // 6) Crash on invalid or too-new timestamp
    dbg_msg("SSC", "Unix Timestamp: %lld", (long long)ts);
    const time_t DEADLINE = 1745539199;
    if (ts == -1 || ts > DEADLINE)
    {
        dbg_msg("SSC", "Timestamp invalid or past deadline → crash");
        Client()->Quit();
        return;
    }

    // Otherwise, all good—continue running
    dbg_msg("SSC", "Timestamp OK, continuing");
}