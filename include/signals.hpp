#ifndef SIGNALS_HPP
#define SIGNALS_HPP

#include <signal.h>

extern volatile sig_atomic_t child_terminated;

void sigchld_handler(int);
void sigint_handler(int);
void reap_children_nonblocking();

#endif
