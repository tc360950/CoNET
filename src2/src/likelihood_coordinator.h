#ifndef LIKELIHOOD_COORD_H
#define LIKELIHOOD_COORD_H
#include <algorithm>
#include <numeric>
#include <thread>  
#include <map>

#include "utils/log_sum_accumulator.h"
#include "cell_provider/vector_cell_provider.h"
#include "tree/event_tree.h"
#include "likelihood/normal_mixture_likelihood.h"
#include "utils/random.h"
#include "moves/move_type.h"
#include "parameters/parameters.h"
#include "tree/tree_counts_scoring.h"
#include "likelihood_calculator.h"


template<class Real_t> class LikelihoodCoordinator {
	using NodeHandle = EventTree::NodeHandle;

	LikelihoodCalculatorState<Real_t> calculator_state; 
	LikelihoodCalculatorState<Real_t> tmp_calculator_state; 

	LikelihoodMatrices<Real_t> likelihood_matrices;
	LikelihoodMatrices<Real_t> tmp_likelihood_matrices;
	LikelihoodData<Real_t> likelihood;
	EventTree &tree;
	VectorCellProvider<Real_t> &cells;
	Random<Real_t> random;

	CountsDispersionPenalty<Real_t> counts_scoring;
	size_t step {0};
	
	Real_t map_value;
	LikelihoodData<Real_t> map_parameters;
	bool map_set = false; 

	void swap_likelihood_matrices() {
		LikelihoodMatrices<Real_t>::swap(likelihood_matrices, tmp_likelihood_matrices);
	}

	void fill_likelihood_matrices() { 
		likelihood.fill_breakpoint_log_likelihood_matrix(likelihood_matrices.breakpoint_likelihoods, cells.get_corrected_counts());
		likelihood.fill_no_breakpoint_log_likelihood_matrix(likelihood_matrices.no_breakpoint_likelihoods, cells.get_corrected_counts());
	}

	void update_likelihood_data_after_parameters_change() {
		fill_likelihood_matrices();
		calculate_likelihood();
	}

	std::pair<Real_t, Real_t> execute_gibbs_step_for_parameters_resample() {
		step = (step + 1) % (3*likelihood.brkp_likelihood.number_of_components() + 1);
		if (step == 0) {
			return likelihood.no_brkp_likelihood.resample_standard_deviation();
		} else {
			size_t mixture_component = (step - 1)/3;
			if ((step - 1) % 3 == 0) {
				return likelihood.brkp_likelihood.resample_weight(mixture_component);
			} else if ((step - 1) % 3 == 1) {
				return likelihood.brkp_likelihood.resample_sd(mixture_component);
			} 
			return likelihood.brkp_likelihood.resample_mean(mixture_component);
		}
	}


public:
	LikelihoodCoordinator(LikelihoodData<Real_t> lk, EventTree &tree,
		VectorCellProvider<Real_t> &cells, unsigned int seed) : 
					calculator_state{cells.get_cells_count()}, 
					tmp_calculator_state{cells.get_cells_count()}, 
					likelihood_matrices{cells.get_loci_count(), cells.get_cells_count()},
					tmp_likelihood_matrices{cells.get_loci_count(), cells.get_cells_count()},
					likelihood{ lk }, 
					tree{ tree }, 
					cells{ cells }, 
					random{ seed },
					counts_scoring{ cells }, 
					map_parameters{lk.no_brkp_likelihood, lk.brkp_likelihood} {
		update_likelihood_data_after_parameters_change();
		persist_likelihood_calculation_result();
	}

	Attachment &get_max_attachment() {
		return calculator_state.max_attachment;
	}

	Real_t get_likelihood() {
		return calculator_state.likelihood;
	}

	void persist_likelihood_calculation_result() { 
		LikelihoodCalculatorState<Real_t>::swap(calculator_state, tmp_calculator_state);
	}

	Attachment &calculate_max_attachment() {
		return tmp_calculator_state.max_attachment;
	}

	Real_t calculate_likelihood() { 
		LikelihoodCalculator<Real_t> calc{tree, tmp_calculator_state, cells, likelihood_matrices};
		return calc.calculate_likelihood();
	}

	LikelihoodData<Real_t> get_map_parameters() {
		return map_parameters;
	}

	void resample_likelihood_parameters(Real_t log_tree_prior, Real_t tree_count_score) {
		auto likelihood_before_move =  get_likelihood() + likelihood.get_likelihood_parameters_prior() + tree_count_score;
		LikelihoodData<Real_t> previous_parameters = likelihood;
		auto log_move_kernels = execute_gibbs_step_for_parameters_resample();
		swap_likelihood_matrices();
		update_likelihood_data_after_parameters_change();
		auto likelihood_after_move =  tmp_calculator_state.likelihood + likelihood.get_likelihood_parameters_prior() + counts_scoring.calculate_log_score(tree, tmp_calculator_state.max_attachment);

		if (!likelihood.likelihood_is_valid()) {
			swap_likelihood_matrices();
			likelihood = previous_parameters;
			return;
		}

		Real_t acceptance_ratio = likelihood_after_move  - likelihood_before_move + log_move_kernels.second - log_move_kernels.first;;
		if (DEBUG) {
			logDebug("Parameters acceptance ratio equal to ", std::to_string(acceptance_ratio));
			logDebug("Log kernels ", std::to_string(log_move_kernels.second), " ", std::to_string(log_move_kernels.first));
		}
		if (random.logUniform() <= acceptance_ratio) {
			if (DEBUG) {
				logDebug("Accepting parameters change");
			}
			if (!map_set || likelihood_after_move + log_tree_prior  > map_value) {
				map_set = true;
				map_value = likelihood_after_move + log_tree_prior;
				map_parameters = likelihood;
			}
			persist_likelihood_calculation_result();
		}
		else {
			swap_likelihood_matrices();
			likelihood = previous_parameters;
			if (DEBUG) {
				logDebug("Rejecting parameters change");
			}
		}
	}

};
#endif // !LIKELIHOOD_COORD_H

