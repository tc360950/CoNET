#include "parameters.h"

size_t THREADS_NUM = 1;
bool USE_EVENT_LENGTHS_IN_ATTACHMENT = false;
double DATA_SIZE_PRIOR_CONSTANT = 0.01;
double COUNTS_SCORE_CONSTANT = 0.1;
double EVENTS_LENGTH_PENALTY = 1.0;
bool VERBOSE = false;
size_t PARAMETER_RESAMPLING_FREQUENCY = 10;
size_t NUMBER_OF_MOVES_BETWEEN_SWAPS = 10;
size_t THREADS_LIKELIHOOD = 10;
size_t MIXTURE_SIZE = 8;
long SEED = 12312414;
size_t BURNIN = 100000;
