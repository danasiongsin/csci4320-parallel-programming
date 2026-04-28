//The C main file for a ROSS model
//This file includes:
// - definition of the LP types
// - command line argument setup
// - a main function

//includes
#include "ross.h"
#include "model.h"

// Define LP types
// these are the functions called by ROSS for each LP
// multiple sets can be defined (for multiple LP types)
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

//Define command line arguments default values
// Add your command line opts
const tw_optdef model_opts[] = {
	TWOPT_GROUP("ROSS Model"),
	TWOPT_END(),
};

#define model_main main

int model_main (int argc, char* argv[]) {
	int num_lps_per_pe;

	tw_opt_add(model_opts);
	tw_init(&argc, &argv);

	// Load historical data prior to starting the simulation
	if (g_tw_mynode == 0) { // Ensures we don't try to open the file 100 times if running 100 PEs
	    csv_load("SP500.csv");
	}

	// Set up LPs within ROSS
	num_lps_per_pe = 1; // Increase this based on how many entities you want on each processor
	tw_define_lps(num_lps_per_pe, sizeof(message));
	
	// Register the LP types
	g_tw_lp_types = model_lps;
	tw_lp_setup_types();

	tw_run();
	tw_end();

	return 0;
}