# Projeto: Cérbero (The Reaper)

## Visão Geral do Sistema
O Cérbero é um daemon concorrente para Linux. O trabalho dele é ler um arquivo de configuração contendo limites de memória para processos específicos, monitorar o sistema operacional lendo agressivamente o diretório `/proc` e assassinar sumariamente com `SIGKILL` qualquer processo que ultrapasse a cota estipulada. Tudo isso feito com eficiência máxima de CPU e alocação quase zero no Heap.

---

## Requisitos e Módulos

### 1. O Vigia de Configuração (`inotify` + `std::atomic`)
* **O que faz:** Monitora um arquivo de texto simples (ex: `cerbero.conf`). Formato sugerido em cada linha: `nome_do_processo limite_mb` (ex: `chrome 500`).
* **A Execução:** Use a API nativa do `inotify(7)` do Linux. Quando o usuário salvar uma edição nesse arquivo, o Cérbero deve interceptar o evento, ler o arquivo e atualizar as regras de limites internamente.
* **Restrição Arquitetural:** Para passar as regras novas para as threads de trabalho sem causar gargalo de concorrência, use estruturas baseadas em `std::atomic` (como ponteiros atômicos para a estrutura de regras) no lugar de `std::mutex` global, evitando que o programa trave cada vez que for checar uma regra.

### 2. O Motor de Varredura (Worker Threads e POSIX I/O)
* **O que faz:** Um pool de threads fixo (iniciado no boot do programa) que fica iterando continuamente sobre os diretórios numéricos dentro de `/proc` (que representam os PIDs dos processos rodando).
* **A Execução:** Use `opendir` e `readdir` (POSIX) para varrer as pastas. Para cada PID, leia o arquivo `/proc/[pid]/statm` e `/proc/[pid]/comm` (para pegar o nome do processo). Extraia a segunda coluna do `statm`, que é o RSS (*Resident Set Size*), calcule o tamanho da página de memória do sistema e converta isso para Megabytes.
* **Restrição Crítica (A regra de ouro):** Zero alocação dinâmica na varredura. Abra os arquivos no nível do sistema operativo (`open`), leia os dados diretamente para um buffer de tamanho fixo na Stack (ex: `char buffer[256];` usando `read()`), faça o parse da string em C puro e feche com `close`. Se eu ver um `new`, um `std::string` instanciado a cada loop, ou `std::ifstream` mastigando o I/O aqui, seu Code Review vai ser reprovado. Pense no Cache L1.

### 3. O Executor (Syscalls)
* **O que faz:** Compara o consumo de RAM atual do processo com a tabela de regras (acessada via ponteiro atômico). Se o limite for estourado, puxa o gatilho.
* **A Execução:** Dispare a syscall `kill(pid, SIGKILL)`.

---

## Entregáveis
Quero o projeto separado com decência:

* Arquivos `.hpp` definindo as interfaces e as estruturas de dados.
* Arquivos `.cpp` com as implementações.
* Um `CMakeLists.txt` configurado para compilar tudo linkando com a biblioteca de threads (`pthread`).
* **Flags obrigatórias de compilação:** `-Wall -Wextra -Werror -O3 -std=c++17` (ou `c++20`).

> **Nota:** Leia a man page do `proc(5)`, do `inotify(7)` e da syscall `kill(2)`.
