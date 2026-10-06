#pragma once

#include <string>

// Translate one GameMaker shader the same way Shader::LoadCode does for the
// non-batched OpenGL path, but emit GLSL ES 3.00 instead of desktop GLSL 150.
std::string TranslateMineimatorShader(const std::string& source, bool is_vertex);
