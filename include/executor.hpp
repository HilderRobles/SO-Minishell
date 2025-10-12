#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <string>
#include <vector>

void execute_command_simple(std::vector<std::string> tokens, bool background);
void execute_with_pipe(std::vector<std::string> left_tokens, std::vector<std::string> right_tokens, bool background);
std::string resolve_command_path(const std::string &cmd);
bool file_exists_and_executable(const std::string &path);

#endif
