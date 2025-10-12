#include "parser.hpp"
#include "executor.hpp"
#include "builtins.hpp"
#include "signals.hpp"
#include <iostream>
#include <csignal>
#include <algorithm>

int main() {
    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);

    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, nullptr);

    std::string line;
    std::string prompt = "mini-shell$ ";

    while (true) {
        if (child_terminated) reap_children_nonblocking();
        std::cout << prompt;
        std::cout.flush();
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;

        // historial (protegido)
        {
            std::lock_guard<std::mutex> lk(builtins_mutex);
            history_list.push_back(line);
        }

        // detect parallel special-case to preserve rest of line
        if (line.rfind("parallel ", 0) == 0) {
            std::string rest = trim(line.substr(std::string("parallel ").size()));
            run_parallel_from_line(rest);
            continue;
        }

        // background detection (ampersand separated by space or at end)
        bool background = false;
        if (!line.empty() && line.back() == '&') {
            background = true;
            line = trim(line.substr(0, line.size()-1));
        }

        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) continue;

        // built-in quick check
        if (is_builtin(tokens[0])) {
            handle_builtin(tokens);
            continue;
        }

        auto it_pipe = std::find(tokens.begin(), tokens.end(), "|");
        if (it_pipe != tokens.end()) {
            std::vector<std::string> left(tokens.begin(), it_pipe);
            std::vector<std::string> right(it_pipe+1, tokens.end());
            execute_with_pipe(left, right, background);
        } else {
            execute_command_simple(tokens, background);
        }
    }

    std::cout << "\nSaliendo de mini-shell...\n";
    return 0;
}
