#pragma once

#include "common/InputParams.h"

/**
 * @brief Parse the command line arguments
 *
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line arguments
 * @param[out] params Parsed parameters
 * @param[out] exitRequested Set when help or version was printed and the caller should exit successfully
 *
 * @return True iff parsing succeeded without errors
 */
bool parseCommandLine(const int argc, char* argv[], InputParams& params, bool* exitRequested = nullptr);
