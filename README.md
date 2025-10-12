# SO-Minishell

Estructura:
  include/   -- encabezados
  src/       -- fuentes

Cómo compilar (manual, sin Makefile):

  g++ -std=c++17 src/*.cpp -Iinclude -o mini_shell -pthread

Ejecutar:
  ./mini_shell

Notas:
- Built-ins: salir, cd, pwd, help, history, alias, parallel, meminfo
  - parallel: use separator ';;' to separate commands, for example:
      parallel sleep 2 ;; echo done ;; ls -l
  - meminfo: muestra valores aproximados leídos de /proc/self/status (VmSize, VmRSS, VmData)
- Pipes, redirecciones (<, >, >>) y background (&) soportados.
- Tokens deben separarse por espacios, tal como pediste.