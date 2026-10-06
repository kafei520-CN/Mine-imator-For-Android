#pragma once

// App-private paths. Android has no home-directory .minecraft folder.
void mi_storage_set_root(const char* files_dir);
void mi_storage_set_cache(const char* cache_dir);
void mi_storage_ensure_dirs();
const char* mi_storage_cache_dir();
const char* mi_storage_user_dir();
const char* mi_storage_projects_dir();
const char* mi_storage_skins_dir();
const char* mi_storage_saves_dir();
const char* mi_storage_imports_dir();

// Modal open-file. Empty string if the user cancels. Android implements it.
const char* mi_storage_pick_open();
