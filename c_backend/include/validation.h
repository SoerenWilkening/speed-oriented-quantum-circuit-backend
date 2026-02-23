/**
 * @file validation.h
 * @brief Error codes and constants for Cython boundary validation (Phase 84).
 *
 * Entry-point C functions use these return codes to signal errors.
 * Cython translates error codes into Python exceptions.
 * Internal C-to-C calls do NOT use validation (they trust the pointer).
 *
 * Dependencies: types.h
 */

#ifndef QUANTUM_VALIDATION_H
#define QUANTUM_VALIDATION_H

#include "types.h"

/* Error codes for validated entry points */
#define QV_OK 0         /* Success */
#define QV_NULL_CIRC -1 /* Circuit pointer is NULL */
#define QV_NULL_SEQ -2  /* Sequence pointer is NULL */
#define QV_NULL_ARG -3  /* Other argument is NULL */

/* Qubit array scratch buffer limit (4 * 64 + 2 * 64 = 384) */
#define QV_MAX_QUBIT_SLOTS 384

#endif /* QUANTUM_VALIDATION_H */
