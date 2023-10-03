#pragma once

#include <stdbool.h>
#include <stdio.h>

void error(int column, int line, const char *message, ...);
void error_args(int column, int line, const char *message, va_list args);
void check(bool check, int column, int line, const char *message, ...);
void check_args(bool check, int column, int line, const char *message, va_list args);
