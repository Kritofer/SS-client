#include "draw.h"
#include <engine/graphics.h>
#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <cmath>

void Draw::CenterTo(vec2 m_Center, float m_Zoom)
{
    float aPoints[4];
    RenderTools()->MapScreenToWorld(m_Center.x, m_Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), m_Zoom, aPoints);

    Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}

void Draw::CircleDraw(vec2 pos, vec4 color, float radius)
{
    const int numSegments = 32;

    Graphics()->TextureClear();
    Graphics()->QuadsBegin();
    Graphics()->SetColor(color.r, color.g, color.b, color.a);

    // Draw the filled circle
    Graphics()->DrawCircle(pos.x, pos.y, radius, numSegments);

    Graphics()->QuadsEnd();
}

void Draw::UCircleDraw(vec2 pos, vec4 color, float radius)
{
    DrawEntry Circle = DrawEntry();
    Circle.type = "circle";
    Circle.color = color;
    Circle.position = pos;
    Circle.metadata.insert({"radius", radius});

    ToDraw.emplace_back(Circle);
}

void Draw::LineDraw(vec2 prevpos, vec2 pos, vec4 color)
{
    Graphics()->TextureClear();
    Graphics()->LinesBegin();
    Graphics()->SetColor(color.r, color.g, color.b, color.a);

    IGraphics::CLineItem Line(prevpos.x, prevpos.y, pos.x, pos.y);
    Graphics()->LinesDraw(&Line, 1);

    Graphics()->LinesEnd();
}

void Draw::ULineDraw(vec2 prevpos, vec2 pos, vec4 color)
{
    DrawEntry Line = DrawEntry();
    Line.type = "line";
    Line.color = color;
    Line.position = pos;
    Line.metadata.insert({"prevpos", prevpos});

    ToDraw.emplace_back(Line);
}

void Draw::OnRender()
{
    if (ToDraw.empty())
        return;

    CCamera &m_Camera = m_pClient->m_Camera;
    CenterTo(m_Camera.m_Center, m_Camera.m_Zoom);

    while (!ToDraw.empty()) {
        DrawEntry& entry = ToDraw.back();
    
        if (entry.type == "circle")
        {
            CircleDraw(entry.position, entry.color, std::any_cast<float>(entry.metadata.extract("radius").mapped()));
        }
        if (entry.type == "line")
        {
            LineDraw(std::any_cast<vec2>(entry.metadata.extract("prevpos").mapped()), entry.position, entry.color);
        }
    
        // Remove it after use
        ToDraw.pop_back();
    }
}