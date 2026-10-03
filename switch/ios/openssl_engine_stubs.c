/*
 * MPVKit's OpenSSL is built with no-engine, but the prebuilt libcurl still references the
 * (deprecated) ENGINE API for --engine/client-key engines. AnikkuNX never selects an engine,
 * so these stubs only satisfy the linker and report "no engine available".
 */
#include <stddef.h>

void* ENGINE_by_id(const char* id) { (void)id; return NULL; }
int ENGINE_ctrl(void* e, int cmd, long i, void* p, void (*f)(void)) { (void)e; (void)cmd; (void)i; (void)p; (void)f; return 0; }
int ENGINE_ctrl_cmd(void* e, const char* name, long i, void* p, void (*f)(void), int optional) {
    (void)e; (void)name; (void)i; (void)p; (void)f; (void)optional;
    return 0;
}
int ENGINE_finish(void* e) { (void)e; return 0; }
int ENGINE_free(void* e) { (void)e; return 0; }
void* ENGINE_get_first(void) { return NULL; }
const char* ENGINE_get_id(const void* e) { (void)e; return ""; }
void* ENGINE_get_next(void* e) { (void)e; return NULL; }
int ENGINE_init(void* e) { (void)e; return 0; }
void* ENGINE_load_private_key(void* e, const char* id, void* ui, void* data) { (void)e; (void)id; (void)ui; (void)data; return NULL; }
int ENGINE_set_default(void* e, unsigned int flags) { (void)e; (void)flags; return 0; }
