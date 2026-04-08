cmake_minimum_required(VERSION 3.20)

# Nome do projeto e linguagem (CXX = C++)
project(Cerberus VERSION 1.0 LANGUAGES CXX)

# Gera o compile_commands.json para o LSP (clangd/ccls) entender o projeto
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Cria o executável a partir do seu main.cpp
add_executable(cerberus src/main.cpp src/CerberusCore.cpp)
#find_package(OpenMP REQUIRED)
#target_link_libraries(rasterizer SDL2 SDL2main OpenMP::OpenMP_CXX)

# Diz ao compilador para incluir a pasta 'include' nas buscas de headers
target_include_directories(cerberus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# O Evangelho do C++ Moderno: Forçamos o C++20. 
# Se tentar usar feature legada depreciada, ele chia.
target_compile_features(cerberus PRIVATE cxx_std_23)

# A armadilha do seu Mentor (sua câmara de tortura e aprendizado):
target_compile_options(cerberus PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic 
    -Werror
    -O3
)
