#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"


int main(int argc, char *argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }


    SmallShell &smash = SmallShell::getInstance();
    while (true) {
        smash.getJobList()->removeFinishedJobs();
        std::cout << smash.getPrompt() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        try {
            smash.executeCommand(cmd_line.c_str());
        } catch(void*){
           int i = 0;
            i++;//some compilers ignore empty catch blocks
        }
    }
    return 0;
}