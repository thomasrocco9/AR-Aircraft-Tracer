#include "raylib.h"
#include "raymath.h"

#include <stdio.h>

#define MIN_ZOOM 0.06f
#define MAX_ZOOM 5.0f

#define MOVE_SPEED 200.0f // pixels / second

#define FLIGHT_PATH_LINE_THICKNESS 5.0f

#define FONT_SIZE 20
#define TEXT_LINE_SPACE 2

static int TextPosY;

static void DrawTextBreak(const char *Text)
{
	Font DefaultFont = GetFontDefault();
	int LineAdvance  = DefaultFont.baseSize + DefaultFont.glyphPadding;

	DrawText(Text, 0, TextPosY, FONT_SIZE, WHITE); 

	TextPosY += FONT_SIZE;
}

int main()
{
	int ScreenWidth  = 800;
	int ScreenHeight = 450;

	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(ScreenWidth, ScreenHeight, "Aircraft Map");

	char *WorldMapFile = "WorldMap.png";

	if (FileExists(WorldMapFile))
	{
		Texture2D WorldMapTexture = LoadTexture(WorldMapFile);

		Camera2D Camera = {};
		Camera.target   = { WorldMapTexture.width * 0.5f, WorldMapTexture.height * 0.5f };
		Camera.zoom     = 1.0f;

		SetTargetFPS(60);

		while (!WindowShouldClose())
		{
			float DeltaTime = GetFrameTime();

			if (IsWindowResized())
			{
				ScreenWidth  = GetScreenWidth();
				ScreenHeight = GetScreenHeight();
			}

			Camera.offset = { ScreenWidth * 0.5f, ScreenHeight * 0.5f };

			Camera.zoom += GetMouseWheelMove() * DeltaTime;
			Camera.zoom  = Clamp(Camera.zoom, MIN_ZOOM, MAX_ZOOM);

			if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
			{
				Vector2 MouseDelta = GetMouseDelta();
				Camera.target      = Vector2Add(Camera.target, Vector2Scale(Vector2Negate(MouseDelta), MOVE_SPEED * (1.0f / Camera.zoom) * DeltaTime));
			}

			BeginDrawing();
			ClearBackground(BLACK);

			BeginMode2D(Camera);
			DrawTexture(WorldMapTexture, 0, 0, WHITE);

			DrawSplineSegmentBezierQuadratic({ 0.0f, 0.0f }, { 0.0f, 100.0f }, { 100.0f, 100.0f }, FLIGHT_PATH_LINE_THICKNESS, RED);

			EndMode2D();

			int ICAO = 0;

			TextPosY = 0;

			DrawTextBreak(TextFormat("ICAO: %d\n", ICAO));
			DrawTextBreak(TextFormat("Start Location: [%f, %f]\n", 0.0f, 0.0f));
			DrawTextBreak(TextFormat("End Location: [%f, %f]\n", 0.0f, 0.0f));
			DrawTextBreak(TextFormat("Start Time: %d\n", 0));
			DrawTextBreak(TextFormat("End Time: %d\n", 0));

			EndDrawing();
		}
	}

	CloseWindow();

	return 0;
}
