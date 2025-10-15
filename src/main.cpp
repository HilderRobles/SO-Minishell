#include "parser.hpp"
#include "executor.hpp"
#include "builtins.hpp"
#include "signals.hpp"
#include <iostream>
#include <csignal>      // Para sigaction
#include <algorithm>    // Para std::find

int main() {
    // Configuración del manejador de la señal SIGCHLD (hijos terminados)
    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler; // Llama a sigchld_handler
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Flags para evitar zombies y reiniciar llamadas
    sigaction(SIGCHLD, &sa_chld, nullptr); // Establece el manejador

    // Configuración del manejador de la señal SIGINT (Ctrl+C)
    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler; // Llama a sigint_handler
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, nullptr); // Establece el manejador

    std::string line;
    std::string prompt = "mini-shell$ ";

    while (true) {
        if (child_terminated) reap_children_nonblocking(); // Recolecta procesos zombies si la bandera está activa
        
        std::cout << prompt;
        std::cout.flush(); // Asegura que el prompt se muestre inmediatamente
        if (!std::getline(std::cin, line)) break; // Lee la línea de entrada (o sale si EOF/Ctrl+D)
        
        line = trim(line); // Limpia espacios en blanco
        if (line.empty()) continue;

        // historial (protegido)
        {
            std::lock_guard<std::mutex> lk(builtins_mutex); // Bloquea el mutex para el historial
            history_list.push_back(line); // Añade el comando al historial
        }

        // Detecta casos especiales paralelos para preservar el resto de la línea
        if (line.rfind("parallel ", 0) == 0) { // Comprueba si comienza con "parallel "
            std::string rest = trim(line.substr(std::string("parallel ").size())); // Extrae el resto
            run_parallel_from_line(rest); // Ejecuta en paralelo
            continue;
        }

        // detección de fondo (signo & separado por espacio o al final)
        bool background = false;
        if (!line.empty() && line.back() == '&') {
            background = true; // Marca como ejecución en segundo plano
            line = trim(line.substr(0, line.size()-1)); // Elimina el '&'
        }

        std::vector<std::string> tokens = tokenize(line); // Tokeniza el comando
        if (tokens.empty()) continue;

        // comprobación rápida incorporada
        if (is_builtin(tokens[0])) {
            handle_builtin(tokens); // Ejecuta el built-in
            continue;
        }

        auto it_pipe = std::find(tokens.begin(), tokens.end(), "|"); // Busca el operador de pipe
        if (it_pipe != tokens.end()) {
            // Divide en comandos izquierdo y derecho
            std::vector<std::string> left(tokens.begin(), it_pipe);
            std::vector<std::string> right(it_pipe+1, tokens.end());
            execute_with_pipe(left, right, background); // Ejecuta con pipe
        } else {
            execute_command_simple(tokens, background); // Ejecuta comando simple
        }
    }

    std::cout << "\nSaliendo de mini-shell...\n";
    return 0;
}
