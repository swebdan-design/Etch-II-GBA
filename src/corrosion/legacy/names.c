/*
 * ETCH -- A Generative Acid Machine -- by Synth Centric
 * Name tables (bank 0, readable from any banked UI code).
 */
#include "corrosion.h"

const char * const corr_style_names[STYLE_COUNT] = {
    "ACID HOUSE", "ELECTRO   ", "MINIMAL   ", "BROKEN    ", "INDUSTRIAL", "SPARSE    "
};
const char * const corr_scale_names[SCALE_COUNT] = {
    "MINOR   ", "MAJOR   ", "DORIAN  ", "PHRYGIAN", "MIN PENT", "CHROMATC"
};
const char * const corr_mode_names[EVO_COUNT] = { "STABLE ", "DRIFT  ", "CORRODE" };
