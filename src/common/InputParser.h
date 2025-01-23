#ifndef PARSER_H
#define PARSER_H

#include "common/InputParams.h"

/**
 * @brief Parse the command line arguments
 *
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line arguments
 * @param[out] params Parsed parameters
 *
 * @return 0 iff parsing succceeded without errors
 *         1 if parsing failed
 */
int parseCommandLine(const int argc, char* argv[], InputParams& params);

#endif // PARSER_H
