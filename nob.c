#define NOB_IMPLEMENTATION
#include "nob.h"
#define OPTI 0
int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    // Création des dossiers nécessaires
    nob_mkdir_if_not_exists("build");
    nob_mkdir_if_not_exists("src");

    printf("----------\n");
    // Compilation du client
    {
        // Check if the client is running and kill it before rebuilding
        Nob_Cmd kill_cmd = {0};
        nob_cmd_append(&kill_cmd, "taskkill", "/F", "/IM", "client.exe", "/T");
        // Ignore failure since the process might not be running
        nob_cmd_run_sync(kill_cmd);

        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "gcc");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g");
        #if OPTI
            nob_cmd_append(&cmd, "-O2", "-march=native", "-ffast-math");
        #endif
        nob_cmd_append(&cmd, "-I./include", "-L./lib");
        nob_cmd_append(&cmd, "./src/main.c", "./src/capture.c", "./src/network.c");
        nob_cmd_append(&cmd, "-o", "./build/client");
        nob_cmd_append(&cmd, "-lraylib", "-lenet", "-lopengl32", "-lgdi32", "-lwinmm", "-lws2_32");
        if (!nob_cmd_run_sync(cmd)) return 1;
    }
    printf("----------\n");

    
    return 0;
}