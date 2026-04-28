//The C main file for a ROSS model
//This file includes:
// - definition of the LP types
// - command line argument setup
// - a main function

//includes
#include "ross.h"
#include "model.h"

// Define the global command line variable
int g_strategy_id = 0; 

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

// Expose the argument to the command line
const tw_optdef model_opts[] = {
	TWOPT_GROUP("ROSS Model Options"),
	TWOPT_UINT("strategy", g_strategy_id, "Force a specific strategy (0=Random, 1=MeanRev, 2=Trend, 3=Noise)"),
	TWOPT_END(),
};

#define model_main main

int model_main (int argc, char* argv[]) {
	int num_lps_per_pe;

	tw_opt_add(model_opts);
	tw_init(&argc, &argv);

	if (g_tw_mynode == 0) { 
	    csv_load("SP500.csv");
	}

	// Set up LPs within ROSS (1 Exchange + 9 Traders per core for testing)
	num_lps_per_pe = 10; 
	tw_define_lps(num_lps_per_pe, sizeof(message));
	
	g_tw_lp_types = model_lps;
	tw_lp_setup_types();

	tw_run();
	tw_end();

	return 0;
}