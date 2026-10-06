#include "gles_shader.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace {

void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
	if (from.empty()) {
		return;
	}
	size_t pos = 0;
	while ((pos = text.find(from, pos)) != std::string::npos) {
		text.replace(pos, from.size(), to);
		pos += to.size();
	}
}

std::string Trim(const std::string& text) {
	size_t begin = 0;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
		++begin;
	}
	size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
		--end;
	}
	return text.substr(begin, end - begin);
}

const char* kMatrixFrom[] = {
	"gm_Matrices[MATRIX_WORLD_VIEW_PROJECTION]",
	"gm_Matrices[MATRIX_VIEW_PROJECTION]",
	"gm_Matrices[MATRIX_WORLD_VIEW]",
	"gm_Matrices[MATRIX_WORLD]",
	"gm_Matrices[MATRIX_VIEW]",
	"gm_Matrices[MATRIX_PROJECTION]",
};

const char* kMatrixTo[] = {
	"_uMatrixMVP",
	"_uMatrixVP",
	"_uMatrixMV",
	"_uMatrixM",
	"_uMatrixV",
	"_uMatrixP",
};

}  // namespace

std::string TranslateMineimatorShader(const std::string& source, bool is_vertex) {
	std::string code = source;
	ReplaceAll(code, "\r\n", "\n");
	ReplaceAll(code, "\r", "\n");
	if (code.empty() || code.back() != '\n') {
		code.push_back('\n');
	}

	std::string header;
	std::string body;
	size_t line_start = 0;
	while (line_start < code.size()) {
		size_t line_end = code.find('\n', line_start);
		if (line_end == std::string::npos) {
			line_end = code.size();
		}
		std::string line = code.substr(line_start, line_end - line_start);
		if (!line.empty() && line[0] == '#') {
			header += line + "\n";
		} else {
			body += line + "\n";
		}
		line_start = line_end + 1;
	}
	code.swap(body);

	if (!is_vertex && code.find("gl_FragColor") != std::string::npos) {
		header += "layout(location = 0) out vec4 out_FragColor;\n";
		ReplaceAll(code, "gl_FragColor", "out_FragColor");
	}
	for (int output = 0; !is_vertex; ++output) {
		const std::string from = "gl_FragData[" + std::to_string(output) + "]";
		if (code.find(from) == std::string::npos) {
			break;
		}
		header += "layout(location = " + std::to_string(output) + ") out vec4 out_FragData" +
			std::to_string(output) + ";\n";
		ReplaceAll(code, from, "out_FragData" + std::to_string(output));
	}

	const bool samples = code.find("texture2D(") != std::string::npos;
	if (samples) {
		header +=
			"vec4 _sampleUvRect(sampler2D s, vec4 uvRect, bool repeat, vec2 uv)\n"
			"{\n"
			"\tfloat lod = 0.0;\n"
			"\tif (repeat) uv = mod(uv, vec2(1.0, 1.0));\n"
			"\tuv = uvRect.xy + uv * uvRect.zw;\n"
			"\tuv.y = 1.0 - uv.y;\n"
			"\treturn textureLod(s, uv, lod);\n"
			"}\n";
	}

	ReplaceAll(code, "attribute ", "in ");
	ReplaceAll(code, "varying ", is_vertex ? "out " : "in ");

	if (code.find("gm_BaseTexture") != std::string::npos) {
		ReplaceAll(code, "gm_BaseTexture", "_uBaseTexture");
		code = "uniform sampler2D _uBaseTexture;\n" + code;
	}

	for (int i = 0; i < 6; ++i) {
		ReplaceAll(code, kMatrixFrom[i], kMatrixTo[i]);
	}
	for (int i = 0; i < 6; ++i) {
		if (code.find(kMatrixTo[i]) != std::string::npos) {
			code = std::string("uniform mat4 ") + kMatrixTo[i] + ";\n" + code;
		}
	}

	if (!header.empty()) {
		code = header + code;
	}

	size_t define_at = 0;
	while ((define_at = code.find("#define ", define_at)) != std::string::npos) {
		size_t name_at = define_at + 8;
		size_t name_end = name_at;
		while (name_end < code.size() && !std::isspace(static_cast<unsigned char>(code[name_end]))) {
			++name_end;
		}
		size_t value_at = name_end;
		while (value_at < code.size() && std::isspace(static_cast<unsigned char>(code[value_at])) &&
			code[value_at] != '\n') {
			++value_at;
		}
		size_t value_end = code.find('\n', value_at);
		if (value_end == std::string::npos) {
			break;
		}
		const std::string name = code.substr(name_at, name_end - name_at);
		const std::string value = Trim(code.substr(value_at, value_end - value_at));
		if (!name.empty() && !value.empty()) {
			ReplaceAll(code, "[" + name + "]", "[" + value + "]");
		}
		define_at = value_end + 1;
	}

	std::vector<std::string> samplers;
	std::vector<std::string> calls;
	size_t call_at = 0;
	while ((call_at = code.find("texture2D(", call_at)) != std::string::npos) {
		const size_t name_at = call_at + 10;
		const size_t comma = code.find(',', name_at);
		if (comma == std::string::npos) {
			break;
		}
		calls.push_back(Trim(code.substr(name_at, comma - name_at)));
		call_at = comma;
	}
	for (const std::string& name : calls) {
		size_t index = samplers.size();
		for (size_t i = 0; i < samplers.size(); ++i) {
			if (samplers[i] == name) {
				index = i;
				break;
			}
		}
		if (index == samplers.size()) {
			samplers.push_back(name);
		}
		ReplaceAll(code, "texture2D(" + name + ",",
			"_sampleUvRect(" + name + ", _uUvRect[" + std::to_string(index) +
			"], _uTexRepeat[" + std::to_string(index) + "] > 0,");
	}

	std::string prelude = "#version 300 es\nprecision mediump float;\n";
	if (!samplers.empty()) {
		prelude += "uniform vec4 _uUvRect[" + std::to_string(samplers.size()) + "];\n";
		prelude += "uniform int _uTexRepeat[" + std::to_string(samplers.size()) + "];\n";
	}
	return prelude + code;
}
