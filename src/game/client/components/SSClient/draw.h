#pragma once
#include <game/client/component.h>
#include <any>

class Draw : public CComponent
{
public:
    struct DrawEntry {
        std::string type;
        vec2 position;
        vec4 color; // e.g. RGBA
        std::unordered_map<std::string, std::any> metadata;
    };

    void CenterTo(vec2 m_Center, float m_Zoom);
    void CircleDraw(vec2 pos, vec4 color, float radius);
    void UCircleDraw(vec2 pos, vec4 color, float radius);
    void LineDraw(vec2 prevpos, vec2 pos, vec4 color);
    void ULineDraw(vec2 prevpos, vec2 pos, vec4 color);

    virtual int Sizeof() const override { return sizeof(*this); }
    virtual void OnRender() override;

private:
    std::vector<DrawEntry> ToDraw;
};