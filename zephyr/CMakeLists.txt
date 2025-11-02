# SPDX-License-Identifier: Apache-2.0

# Versão mínima do CMake
cmake_minimum_required(VERSION 3.13.1)

# Carrega o boilerplate do Zephyr (método antigo)
# O PlatformIO define $ENV{ZEPHYR_BASE} para apontar para o framework 2.7.1
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake)

# Define o nome do projeto
project(zephyr-blink)

# Informa ao CMake onde está seu código-fonte
# (O PlatformIO coloca por padrão em src/main.c)



target_sources(app PRIVATE ../src/main.c)
