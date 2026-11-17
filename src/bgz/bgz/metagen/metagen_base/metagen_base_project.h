// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BASE_PROJECT_H
#define BASE_PROJECT_H

//- rjf: main thread entry point
root_function void BaseMainThreadEntry(void (*entry)(struct CmdLine *cmdln), U64 argument_count, char **arguments);

//- rjf: non-main-thread entry point
root_function void BaseThreadEntry(void (*entry)(void *p), void *params);

#endif // BASE_PROJECT_H
