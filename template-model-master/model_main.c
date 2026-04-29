//The C main file for a ROSS model
//This file includes:
// - definition of the LP types
// - command line argument setup
// - a main function

//includes
#include "ross.h"
#include "model.h"

// Define the global command line variables with default fallbacks
int g_strategy_id = 0; 
int g_num_lps_per_pe = 10;
double g_starting_cash = 100000.0;
double g_rsi_oversold = 30.0;
double g_rsi_overbought = 70.0;
int g_order_size = 10;
double g_noise_prob = 0.05;

tw_lptype model_lps[] = {
  {
    (init_f) model_init,
    (pre_run_f) NULL,
    (event_f) model_event,
    (revent_f) model_event_reverse,
    (commit_f) NULL,
    (final_f) model_final,
    (map_f) model_map,
    sizeof(state)
  },
  { 0 },
};

// Expose the arguments to the command line
const tw_optdef model_opts[] = {
	TWOPT_GROUP("ROSS Agent Options"),
	TWOPT_UINT("strategy", g_strategy_id, "Force a specific strategy (0=MeanRev, 1=Trend, 2=Noise)"),
	TWOPT_UINT("lps", g_num_lps_per_pe, "Number of LPs per PE (1 Exchange + N Traders)"),
	TWOPT_DOUBLE("cash", g_starting_cash, "Starting cash for each trader"),
	TWOPT_DOUBLE("rsi-low", g_rsi_oversold, "RSI Oversold threshold (Mean Reversion)"),
	TWOPT_DOUBLE("rsi-high", g_rsi_overbought, "RSI Overbought threshold (Mean Reversion)"),
	TWOPT_UINT("order-size", g_order_size, "Base number of shares to buy/sell"),
	TWOPT_DOUBLE("noise-prob", g_noise_prob, "Daily probability of a noise trader acting (0.0 to 1.0)"),
	TWOPT_END(),
};

#define model_main main

int model_main (int argc, char* argv[]) {
	tw_opt_add(model_opts);
	tw_init(&argc, &argv);

	if (g_tw_mynode == 0) { 
	    csv_load("SP500.csv");
	}

	// Set up LPs within ROSS using the command line argument
	tw_define_lps(g_num_lps_per_pe, sizeof(message));
	
	g_tw_lp_types = model_lps;
	tw_lp_setup_types();

	uint64_t start_total = clock_now();
	tw_run();
	uint64_t end_total = clock_now();
	uint64_t total_cycles = end_total - start_total;

	uint64_t comm_overhead_cycles = total_cycles - g_computation_cycles;

	printf("PE %d: Total cycles: %llu, Computation cycles: %llu, Communication overhead cycles: %llu\n",
	       g_tw_mynode, total_cycles, g_computation_cycles, comm_overhead_cycles);

	if (g_tw_mynode == 0) {
	    printf("Total simulation time: %.6f seconds\n", (double)total_cycles / 512000000.0);
	}

	tw_end();

	return 0;
}