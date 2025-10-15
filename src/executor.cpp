#include "executor.hpp"
#include "builtins.hpp"
#include "parser.hpp"
#include "signals.hpp"
#include <iostream>
#include <unistd.h>     // fork, exec, access, dup2, pipe
#include <fcntl.h>      // open flags
#include <sys/wait.h>   // waitpid
#include <errno.h>
#include <vector>
#include <cstring>

// Comprueba si la ruta es accesible y ejecutable (X_OK)
bool file_exists_and_executable(const std::string &path) {
    return (access(path.c_str(), X_OK) == 0);
}

// Resuelve la ruta del comando buscando en /bin/ y /usr/bin/ si no contiene '/'
std::string resolve_command_path(const std::string &cmd) {
    if (cmd.find('/') != std::string::npos) return cmd;
    std::string candidate = std::string("/bin/") + cmd;
    if (file_exists_and_executable(candidate)) return candidate;
    candidate = std::string("/usr/bin/") + cmd;
    if (file_exists_and_executable(candidate)) return candidate;
    return cmd; // Dejar que execvp lo resuelva con PATH
}

// Ejecuta un comando simple (sin pipe), manejando alias, built-ins y redirecciones
void execute_command_simple(std::vector<std::string> tokens, bool background) {
    if (tokens.empty()) return;
    resolve_alias(tokens); // Expande alias
    if (tokens.empty()) return;

    if (is_builtin(tokens[0])) {
        handle_builtin(tokens); // Ejecuta built-in en la shell
        return;
    }

    std::string infile = "", outfile = "";
    bool append = false;
    std::vector<std::string> argv_tokens;

    // Procesa tokens y extrae las redirecciones (<, >, >>)
    for (size_t i=0; i<tokens.size(); ++i) {
        if (tokens[i] == "<") {
            if (i+1 < tokens.size()) { infile = tokens[i+1]; ++i; }
            else { std::cerr << "Error: '<' sin archivo\n"; return; }
        }
        else if (tokens[i] == ">") {
            if (i+1 < tokens.size()) { outfile = tokens[i+1]; append = false; ++i; }
            else { std::cerr << "Error: '>' sin archivo\n"; return; }
        }
        else if (tokens[i] == ">>") {
            if (i+1 < tokens.size()) { outfile = tokens[i+1]; append = true; ++i; }
            else { std::cerr << "Error: '>>' sin archivo\n"; return; }
        }
        else argv_tokens.push_back(tokens[i]); // Argumento de comando
    }

    // Prepara el array de argumentos en formato C (char* array terminado en nullptr)
    std::vector<char*> argv;
    for (auto &s: argv_tokens) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    pid_t pid = fork(); // Crea proceso hijo
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) { // Código del hijo
        // Restaura el manejador de SIGINT a por defecto (SIG_DFL)
        struct sigaction sa_default; sa_default.sa_handler = SIG_DFL;
        sigemptyset(&sa_default.sa_mask); sa_default.sa_flags = 0;
        sigaction(SIGINT, &sa_default, nullptr);

        if (!infile.empty()) {
            // Maneja redirección de entrada
            int fd = open(infile.c_str(), O_RDONLY);
            if (fd < 0) { perror((std::string("open ")+infile).c_str()); _exit(1); }
            dup2(fd, STDIN_FILENO); // Redirige STDIN
            close(fd);
        }
        if (!outfile.empty()) {
            // Maneja redirección de salida (TRUNC/APPEND)
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd = open(outfile.c_str(), flags, 0644);
            if (fd < 0) { perror((std::string("open ")+outfile).c_str()); _exit(1); }
            dup2(fd, STDOUT_FILENO); // Redirige STDOUT
            close(fd);
        }

        std::string cmd_path = resolve_command_path(argv_tokens[0]);
        if (cmd_path.find('/') != std::string::npos) {
            execv(cmd_path.c_str(), argv.data()); // Ejecuta con ruta explícita
            perror((std::string("execv ")+cmd_path).c_str());
            _exit(1);
        } else {
            execvp(argv_tokens[0].c_str(), argv.data()); // Ejecuta buscando en PATH
            perror((std::string("execvp ")+argv_tokens[0]).c_str());
            _exit(1);
        }
    } else { // Código del padre
        if (background) {
            std::cout << "[background pid " << pid << "]\n"; // Proceso en segundo plano
        } else {
            int status;
            if (waitpid(pid, &status, 0) < 0) perror("waitpid"); // Espera por el hijo
        }
    }
}

// Ejecuta dos comandos conectados por una tubería
void execute_with_pipe(std::vector<std::string> left_tokens, std::vector<std::string> right_tokens, bool background) {
    resolve_alias(left_tokens); // Expande alias comando izquierdo
    resolve_alias(right_tokens); // Expande alias comando derecho
    
    int fd[2];
    if (pipe(fd) < 0) { perror("pipe"); return; } // Crea la tubería

    pid_t p1 = fork(); // Primer hijo (escritor del pipe)
    if (p1 < 0) { perror("fork"); return; }
    if (p1 == 0) {
        // Restaura SIGINT
        struct sigaction sa_default; sa_default.sa_handler = SIG_DFL;
        sigemptyset(&sa_default.sa_mask); sa_default.sa_flags = 0;
        sigaction(SIGINT, &sa_default, nullptr);
        
        dup2(fd[1], STDOUT_FILENO); // Redirige STDOUT al extremo de escritura
        close(fd[0]); close(fd[1]); // Cierra descriptores no usados
        
        std::vector<char*> argv;
        for (auto &s:left_tokens) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);
        
        std::string cmd_path = resolve_command_path(left_tokens[0]);
        if (cmd_path.find('/') != std::string::npos) execv(cmd_path.c_str(), argv.data());
        else execvp(left_tokens[0].c_str(), argv.data());
        perror((std::string("exec left ")+left_tokens[0]).c_str());
        _exit(1);
    }

    pid_t p2 = fork(); // Segundo hijo (lector del pipe)
    if (p2 < 0) { perror("fork"); return; }
    if (p2 == 0) {
        // Restaura SIGINT
        struct sigaction sa_default; sa_default.sa_handler = SIG_DFL;
        sigemptyset(&sa_default.sa_mask); sa_default.sa_flags = 0;
        sigaction(SIGINT, &sa_default, nullptr);
        
        dup2(fd[0], STDIN_FILENO); // Redirige STDIN al extremo de lectura
        close(fd[0]); close(fd[1]); // Cierra descriptores no usados
        
        std::vector<char*> argv;
        for (auto &s:right_tokens) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);
        
        std::string cmd_path = resolve_command_path(right_tokens[0]);
        if (cmd_path.find('/') != std::string::npos) execv(cmd_path.c_str(), argv.data());
        else execvp(right_tokens[0].c_str(), argv.data());
        perror((std::string("exec right ")+right_tokens[0]).c_str());
        _exit(1);
    }

    close(fd[0]); close(fd[1]); // Cierra ambos extremos del pipe en el padre. ¡Crucial!
    if (background) {
        std::cout << "[background pids " << p1 << " " << p2 << "]\n"; // Ejecución en segundo plano
    } else {
        int status;
        waitpid(p1, &status, 0); // Espera por el escritor
        waitpid(p2, &status, 0); // Espera por el lector
    }
}
