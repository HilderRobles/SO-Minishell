#include "builtins.hpp"
#include "executor.hpp"
#include "parser.hpp"
#include <iostream>
#include <iomanip>
#include <cstdlib>      // Para exit(), getenv()
#include <unistd.h>     // Para getcwd(), chdir()
#include <pthread.h>    // Para hilos POSIX (parallel)
#include <vector>
#include <sstream>
#include <fstream>      // Para leer /proc/self/status (meminfo)
#include <map>          // Para std::map (aliases)
#include <mutex>        // Para std::mutex (sincronización)

std::vector<std::string> history_list; // Historial de comandos
std::map<std::string,std::string> aliases; // Alias definidos
std::mutex builtins_mutex; // Mutex para proteger history y aliases

// Comprueba si el comando es un built-in
bool is_builtin(const std::string &cmd) {
    return (cmd=="salir" || cmd=="cd" || cmd=="pwd" || cmd=="help" || cmd=="history" || cmd=="alias" || cmd=="parallel" || cmd=="meminfo");
}

// Muestra la ayuda de los comandos built-in
void print_help() {
    std::cout << "Mini-shell - comandos soportados (built-ins):\n";
    std::cout << "  salir            : salir de la shell\n";
    std::cout << "  cd <dir>         : cambiar directorio\n";
    std::cout << "  pwd              : mostrar directorio actual\n";
    std::cout << "  history          : lista comandos ejecutados en la sesión\n";
    std::cout << "  alias name='cmd' : crear alias simple (sin persistencia)\n";
    std::cout << "  parallel cmd1 ;; cmd2 ;; ... : ejecutar comandos en paralelo (separador ';;')\n";
    std::cout << "  meminfo          : muestra uso aproximado de memoria (VmSize, VmRSS, VmData)\n";
    std::cout << "  help             : esta ayuda\n";
}

// Ejecuta la lógica del comando built-in
void handle_builtin(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return;
    std::string cmd = tokens[0];
    if (cmd=="salir") {
        exit(0); // Termina la shell
    } else if (cmd=="pwd") {
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) std::cout << buf << "\n"; // Muestra directorio actual
        else perror("pwd");
    } else if (cmd=="cd") {
        std::string dir = (tokens.size()>=2 ? tokens[1] : getenv("HOME") ? getenv("HOME") : "/"); // Determina el directorio
        if (chdir(dir.c_str()) != 0) perror(("cd: "+dir).c_str()); // Cambia el directorio
    } else if (cmd=="help") {
        print_help();
    } else if (cmd=="history") {
        std::lock_guard<std::mutex> lk(builtins_mutex); // Bloquea para acceder a history
        for (size_t i=0;i<history_list.size();++i)
            std::cout << std::setw(4) << i+1 << "  " << history_list[i] << "\n"; // Imprime el historial
    } else if (cmd=="alias") {
        std::lock_guard<std::mutex> lk(builtins_mutex); // Bloquea para acceder a aliases
        if (tokens.size()==1) {
            for (auto &p: aliases)
                std::cout << p.first << "='" << p.second << "'\n"; // Lista aliases
        } else {
            // Lógica para crear un alias
            std::string rest;
            for (size_t i=1;i<tokens.size();++i) {
                if (i>1) rest += " ";
                rest += tokens[i];
            }
            auto eq = rest.find('=');
            if (eq==std::string::npos) { std::cerr << "alias: formato inválido\n"; return; }
            std::string name = trim(rest.substr(0, eq));
            std::string val = rest.substr(eq+1);
            if (!val.empty() && val.front()=='\'') val.erase(0,1);
            if (!val.empty() && val.back()=='\'') val.pop_back();
            if (name.empty() || val.empty()) { std::cerr << "alias: formato inválido\n"; return; }
            aliases[name] = val; // Almacena el alias
        }
    } else if (cmd=="meminfo") {
        // leer /proc/self/status y mostrar VmSize, VmRSS, VmData (aprox)
        std::ifstream f("/proc/self/status"); // Abre el archivo de estado del proceso
        if (!f) { perror("meminfo: /proc/self/status"); return; }
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("VmSize:",0)==0 || line.rfind("VmRSS:",0)==0 || line.rfind("VmData:",0)==0) {
                std::cout << line << "\n"; // Muestra información de memoria
            }
        }
    } else if (cmd=="parallel") {
        // no debería llegar aquí normalmente porque el servidor principal se enrutará; pero admite respaldo
        std::cerr << "Uso: parallel cmd1 ;; cmd2 ;; cmd3 ...\n";
    }
}

// Definición de resolve_alias: expande aliases correctamente
bool resolve_alias(std::vector<std::string> &tokens) {
    if (tokens.empty()) return false;
    auto it = aliases.find(tokens[0]);
    if (it == aliases.end()) return false; // No hay alias
    // tokenizar el valor del alias y reemplazar tokens[0] por los tokens del alias
    std::vector<std::string> ali_tok = tokenize(it->second); // Tokeniza el valor del alias
    std::vector<std::string> newt;
    newt.insert(newt.end(), ali_tok.begin(), ali_tok.end()); // Inserta el alias expandido
    if (tokens.size() > 1)
        newt.insert(newt.end(), tokens.begin()+1, tokens.end()); // Añade argumentos originales
    tokens.swap(newt); // Reemplaza la lista de tokens
    return true;
}

// Trabajador de subprocesos en paralelo: el argumento es un puntero a un std::string* asignado al montón
static void* parallel_thread_func(void* arg) {
    std::string* s = static_cast<std::string*>(arg);
    std::string cmdline = *s;
    delete s; // Libera el heap
    // tokenizar y ejecutar a través del ejecutor (en primer plano dentro del hilo)
    std::vector<std::string> tokens = tokenize(cmdline);
    if (!tokens.empty()) {
        // fuerza el fondo a falso para que el hilo principal pueda continuar
        execute_command_simple(tokens, false); // Ejecuta el comando en el hilo
    }
    return nullptr;
}

// Se espera el resto: texto completo después de "paralelo"; comandos separados por ";;"
void run_parallel_from_line(const std::string &rest) {
    // dividido por ";;"
    std::vector<std::string> cmds;
    size_t pos = 0;
    while (pos < rest.size()) {
        size_t p = rest.find(";;", pos); // Busca el separador
        if (p==std::string::npos) p = rest.size();
        std::string part = trim(rest.substr(pos, p-pos)); // Extrae el comando
        if (!part.empty()) cmds.push_back(part);
        pos = p + 2;
    }
    if (cmds.empty()) { std::cerr << "parallel: no hay comandos\n"; return; }
    std::vector<pthread_t> threads(cmds.size());
    for (size_t i=0;i<cmds.size();++i) {
        // asignar cadena en el montón para el hilo
        std::string *arg = new std::string(cmds[i]); // Asigna memoria para el argumento del hilo
        if (pthread_create(&threads[i], nullptr, &parallel_thread_func, arg) != 0) {
            perror("pthread_create");
            delete arg;
        }
    }
    // unir hilos
    for (size_t i=0;i<threads.size();++i) pthread_join(threads[i], nullptr); // Espera a que todos terminen
}
