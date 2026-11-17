#ifndef BGZ_BASE_SECRETS_H
#define BGZ_BASE_SECRETS_H

////////////////////////////////
//~ bgz: Secrets
//
// Small values, like an API key, kept for this user of this machine by the
// OS, encrypted with the user's login - on Windows the Credential Manager
// ("Windows Credentials", generic credentials). Not in any file of the
// program's, so not in a folder that is shared, backed up or committed. Any
// program run by the same user can read them back.
//
// name is any text, the same name reading what writing wrote. Start it with
// the program's own name ("bgz_editor/..."), as every program shares one list.

#define SECRET_VALUE_MAX 2560 // bytes; the Credential Manager's limit

exported B32     SecretWrite(String8 name, String8 value); // replaces one with the same name; 0 when value is too long or on failure
exported String8 SecretRead(Arena *arena, String8 name);   // empty when there is none
exported B32     SecretDelete(String8 name);                // 1 when it is gone, or was never there

#endif // BGZ_BASE_SECRETS_H
