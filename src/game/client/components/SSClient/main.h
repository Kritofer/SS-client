#pragma once
#include <base/vmath.h>
#include <game/client/component.h>
#include <game/generated/protocol.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>
#include <vector>
#include <deque>

struct Node {
    int x, y;
    float gCost, hCost;   // cost from start; heuristic to goal
    Node* parent;
};

class CSSClient : public CComponent
{
public:
	//temporary
	void CheckAndCrash();

	void Predict_Pos();
	void Avoid_Freeze(CNetObj_PlayerInput *pInput, int LocalId);
	void Fake_Aim(CNetObj_PlayerInput *pInput, int LocalId);
	void LeftRight_Avoid(CNetObj_PlayerInput *pInput, int LocalId);
    void Esp();
	void StartRecording(CNetObj_PlayerInput *pInput, int LocalId, CNetObj_PlayerInput *dInput, int DummyId);
	void PlayRecording(CNetObj_PlayerInput *pInput, int LocalId, CNetObj_PlayerInput *dInput, int DummyId);
	void ZeroInput(CNetObj_PlayerInput *pInput);
	void Update(CNetObj_PlayerInput *aInputdata, int LocalId, CNetObj_PlayerInput *aInputdummy, int DummyId);
	void ChangeMPos(vec2 NewMPos);
	void ChangeDir(int new_Direction);
	void debug(const char *owner, const char *fmt, ...);
	void LaserUnfreeze(CNetObj_PlayerInput *pInput, int LocalId);
	void SimulateMovement(CCharacterCore Predict, int direction, int &dirCounter);
	void DetermineBestDirection(CNetObj_PlayerInput *pInput, int dir01, int dir00, int dir11);
	void SimulateStep(CCharacterCore *Predict, bool deffered=true);
	void OnPlayerRender(vec2 Velocity, vec2 Position, float Angle, int ClientId);
	void ShowLaserPath(vec2 Pos, vec2 Target);
	void ShowGrenadePath(vec2 Pos, vec2 Target);
	void FixWeaponTP(CGameWorld &baseWorld);
	void SaveTas(const std::string &Filename);
	void LoadTas(const std::string &Filename);
	bool BounceCheck(const vec2 &src, const vec2 &dst, int maxBounces);
	int SimulateWithBacktracking(CCharacterCore Predict, CNetObj_PlayerInput input, int depth);
	int SimulateInput(CCharacterCore Predict, CNetObj_PlayerInput input_to_use, int ticks, bool *alive);
	int CountSafeTicks(CGameWorld *world, CCharacter *cc, CNetObj_PlayerInput &input, int horizon);

	int GetVersion() { return CURR_VER; };
	float GetTextSize(const char* pText, float FontSize = 10.0f);

	vec2 GetTasPos() { return mix(TeePos, OldTeePos, 0.5f); };

	CSSClient();

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	// virtual void OnUpdate() override;
private:
	std::vector<vec2> m_PredictedPos;
	std::vector<vec3> m_PredictedColor;

	std::vector<vec2> m_TasPos;
	std::deque<CNetObj_PlayerInput> m_RecordInputs;
	std::deque<CNetObj_PlayerInput> m_RecordInputsD;

	int m_RecordIndex = 0;
	int m_PredIndex = 0;
	int CURR_VER = 1;
	bool m_ResetPlayerInput = true;
	bool m_HookFlying = false;

	vec2 TeePos;
	vec2 HPos;
	vec2 OldTeePos;
	int Frozen;
	int Weapon;
	int Id;

	std::vector<CLaser> Lasers;
	std::vector<vec2> Projectiles_Pos;

	CCharacterCore PPredict;
};
