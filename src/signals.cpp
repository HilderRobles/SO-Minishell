#include "signals.hpp"
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>

volatile sig_atomic_t child_terminated = 0;

void sigchld_handler(int) {
    child_terminated = 1;
}

void sigint_handler(int) {
    write(STDOUT_FILENO, "\n", 1);
}

void reap_children_nonblocking() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status))
            std::cout << "[reaped pid " << pid << " exit " << WEXITSTATUS(status) << "]\n";
        else if (WIFSIGNALED(status))
            std::cout << "[reaped pid " << pid << " signal " << WTERMSIG(status) << "]\n";
    }
    child_terminated = 0;
}
