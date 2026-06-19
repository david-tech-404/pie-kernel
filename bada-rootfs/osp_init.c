/*
 * Pie Kernel - OSP Init Loader
 * Loads OspServer.so and calls OspServerMain()
 * This acts as the bridge between Linux init and the Bada OSP framework
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef int (*OspServerMain_t)(int argc, char **argv);

int main(int argc, char **argv)
{
    void *handle;
    OspServerMain_t OspServerMain;
    char *error;

    printf("Pie Kernel: Loading Bada OSP framework...\n");

    /* Load OspServer.so */
    handle = dlopen("/lib/OspServer.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "Failed to load OspServer.so: %s\n", dlerror());
        return 1;
    }

    dlerror();
    OspServerMain = (OspServerMain_t) dlsym(handle, "OspServerMain");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Failed to find OspServerMain: %s\n", error);
        dlclose(handle);
        return 1;
    }

    printf("Pie Kernel: Starting OspServerMain()...\n");
    int ret = OspServerMain(argc, argv);

    dlclose(handle);
    return ret;
}