#include "signals.hpp"
#include <sys/wait.h>   // waitpid
#include <iostream>     // std::cout
#include <unistd.h>     // write

volatile sig_atomic_t child_terminated = 0; // Bandera para indicar que hay hijos terminados

// Manejador de la señal SIGCHLD
void sigchld_handler(int) {
    child_terminated = 1; // Activa la bandera
}

// Manejador de la señal SIGINT (Ctrl+C)
void sigint_handler(int) {
    // Usa write() que es seguro para manejar señales (async-safe)
    write(STDOUT_FILENO, "\n", 1); 
}

// Recolecta procesos hijos terminados (zombies) de forma no bloqueante
void reap_children_nonblocking() {
    int status;
    pid_t pid;
    // Llama a waitpid con WNOHANG para no bloquear; recolecta cualquier hijo (-1)
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status))
            std::cout << "[reaped pid " << pid << " exit " << WEXITSTATUS(status) << "]\n"; // Salida normal
        else if (WIFSIGNALED(status))
            std::cout << "[reaped pid " << pid << " signal " << WTERMSIG(status) << "]\n"; // Terminado por señal
        // Nota: std::cout no es async-safe, pero esta función se llama desde main.
    }
    child_terminated = 0; // Desactiva la bandera
}
