#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <string>
#include <vector>
#include <map>
#include <mutex>

extern std::vector<std::string> history_list;
extern std::map<std::string,std::string> aliases;
extern std::mutex builtins_mutex;

bool is_builtin(const std::string &cmd);
void handle_builtin(const std::vector<std::string> &tokens);
void print_help();
bool resolve_alias(std::vector<std::string> &tokens);
// parallel: accepts the rest of the line (after "parallel ") and splits commands by ";;"
void run_parallel_from_line(const std::string &rest);

#endif
