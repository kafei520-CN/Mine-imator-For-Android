#include "Platform/Storage.hpp"

#include <string>

#if defined(_WIN32)
#include <direct.h>
#define mi_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define mi_mkdir(path) mkdir((path), 0755)
#endif

namespace {

std::string g_cache;
std::string g_user;
std::string g_projects;
std::string g_skins;
std::string g_saves;
std::string g_imports;

void MakeDirs(const std::string& path) {
	std::string current;
	current.reserve(path.size());
	for (size_t i = 0; i < path.size(); ++i) {
		const char ch = path[i];
		current.push_back(ch);
		if (ch == '/' || i + 1 == path.size()) {
			if (current.size() > 1) {
				mi_mkdir(current.c_str());
			}
		}
	}
}

}  // namespace

void mi_storage_set_cache(const char* cache_dir) {
	g_cache = cache_dir == nullptr ? "" : cache_dir;
	if (!g_cache.empty() && g_cache.back() != '/')
		g_cache.push_back('/');
}

void mi_storage_set_root(const char* files_dir) {
	const std::string root = files_dir == nullptr ? "" : files_dir;
	// GML joins these with a filename and does not add a slash.
	g_user = root + "/Mine-imator/";
	g_projects = g_user + "Projects/";
	g_skins = g_user + "Skins/";
	g_saves = root + "/minecraft/saves/";
	g_imports = root + "/imports/";
}

void mi_storage_ensure_dirs() {
	MakeDirs(g_user);
	MakeDirs(g_projects);
	MakeDirs(g_skins);
	MakeDirs(g_saves);
	MakeDirs(g_imports);
}

const char* mi_storage_cache_dir() { return g_cache.c_str(); }
const char* mi_storage_user_dir() { return g_user.c_str(); }
const char* mi_storage_projects_dir() { return g_projects.c_str(); }
const char* mi_storage_skins_dir() { return g_skins.c_str(); }
const char* mi_storage_saves_dir() { return g_saves.c_str(); }
const char* mi_storage_imports_dir() { return g_imports.c_str(); }
