#pragma once

// Shim de compatibilidad: los pines y capacidades viven ahora en board/
// (capa de seleccion de placa del plan, seccion 5). Placa por defecto:
// Waveshare AMOLED 1.75; define BOARD_AMOLED_175C=1 para la 1.75C (experimental,
// ver board.h) o BOARD_GUITION_4848S040 para el panel Guition.
#include "board.h"
#include "board/board.h"
