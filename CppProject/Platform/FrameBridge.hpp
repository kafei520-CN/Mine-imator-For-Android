#pragma once

// One application frame, shared by the Qt timer and the Android GLES host.
// The input pointer is null when the desktop timer already wrote the mouse.
struct MiFrameInput
{
	int width = 0;
	int height = 0;
	float x = 0.f;
	float y = 0.f;
	int pointer_down = 0;
};

extern "C" void mi_step_frame(const MiFrameInput* input);
