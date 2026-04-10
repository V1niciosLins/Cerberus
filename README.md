# Cerberus

Daemon de monitoramento de memória para sistemas Linux, escrito em C++20. O sistema opera em baixo nível, varrendo o `procfs` nativamente e reagindo a alterações de configuração em tempo real via `inotify`, sem a necessidade de reiniciar o serviço.

## Arquitetura

* **Monitoramento O(1) de Arquivos:** Utiliza `inotify` em uma thread isolada para recarregar a configuração (`cerberus.conf`) atomicamente no momento da escrita.
* **Leitura Direta de Kernel:** Varre `/proc/[pid]/comm` e `/proc/[pid]/statm` para extrair o RSS (Resident Set Size) em Megabytes. Evita alocações dinâmicas através do uso de `std::string_view` e `std::from_chars`.
* **Escalonamento POSIX:** Quando um processo excede o limite definido:
  1. O daemon envia `SIGTERM` e registra o PID em memória.
  2. No próximo ciclo, se o processo persistir, o daemon escala para `SIGKILL`.

## Dependências

* Linux (dependência estrutural do `procfs` e `inotify`)
* Compilador com suporte a C++20 (GCC ou Clang)
* CMake 3.10+

## Compilação

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Configuração

O arquivo de configuração padrão é lido de /etc/cerberus/cerberus.conf (ou o caminho definido em CERBERUS_PATH).
A sintaxe não suporta aspas e exige correspondência exata com o nome do binário mapeado pelo Kernel (máximo de 15 caracteres).

Formato: <process_name> <limite_em_mb>

Exemplo:

```txt
# Comentários são ignorados
chrome 2048
android-studio 1024
```

## Execução

O binário exige privilégios de administrador (Root) para enviar sinais POSIX para processos do sistema e de outros usuários.
Bash
```bash
sudo ./cerberus
```
