#include <iostream>
#include <signal.h>
#include "signals.h"

#include <sys/wait.h>

#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    if (SmallShell::getInstance().pid_of_foreGround == -10)
        return;
    pid_t is_foreGround = waitpid(SmallShell::getInstance().pid_of_foreGround,
        nullptr, WNOHANG);
    if (!is_foreGround) {
        if (!kill(SmallShell::getInstance().pid_of_foreGround, SIGINT)) {
            cout << "smash: process " << SmallShell::getInstance().pid_of_foreGround <<" was killed" << endl;
        } else {
            cerr << "smash error: kill failed"<<endl;
        }
    }
    else {
        SmallShell::getInstance().pid_of_foreGround = -10;
    }
}
