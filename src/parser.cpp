#include "parser.hpp"
#include <sstream>      // Para std::istringstream
#include <vector>       // Para std::vector
#include <string>       // Para std::string

// Elimina espacios en blanco (espacios, tabs, newlines, retornos de carro) al principio y al final de una cadena
std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\n\r"); // Encuentra el primer no-espacio
    if (a == std::string::npos) return ""; // Si solo hay espacios o está vacía
    size_t b = s.find_last_not_of(" \t\n\r"); // Encuentra el último no-espacio
    return s.substr(a, b - a + 1); // Retorna la subcadena limpia
}

// Divide una cadena de entrada en tokens (palabras) usando espacios en blanco como delimitador
std::vector<std::string> tokenize(const std::string &line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line); // Stream a partir de la línea
    std::string t;
    while (iss >> t) tokens.push_back(t); // Extrae y almacena cada token
    return tokens;
}
