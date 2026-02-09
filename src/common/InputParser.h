#pragma once

#include "common/InputParams.h"

/**
 * @brief Parse the command line arguments
 *
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line arguments
 * @param[out] params Parsed parameters
 *
 * @return True iff parsing succceeded without errors
 */
bool parseCommandLine(const int argc, char* argv[], InputParams& params);
