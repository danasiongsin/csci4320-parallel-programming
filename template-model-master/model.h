//The header file template for a ROSS model
//This file includes:
// - the state and message structs
// - extern'ed command line arguments
// - custom mapping function prototypes (if needed)
// - any other needed structs, enums, unions, or #defines

#ifndef _model_h
#define _model_h

#include "ross.h"

//Example enumeration of message type... could also use #defines
//message types for stock market
typedef enum { 
  MARKET_OPEN,            // start of trading day
  HOURLY_UPDATE,          // hourly count of stock volume, there are 6 ticks in a trading day
  MARKET_CLOSE,   
  SECTOR_CORRELATION,     // for closely related companies or sectors the performance will spread
  // NEWS_BROADCAST,      // market news 
  ORDER_IMBALANCE_UPDATE  // update before market opens to adjust opening price based on previous buy/sell pressure 
} message_type;

//Message struct
//   this contains all data sent in an event
typedef struct {
  int stock_id;
  int day;
  double price;
  double volume;
  double obv;                 //on-balance volume
  double sector_impact;
  // double news_sentiment;   // from -1 to 1 for negative or positive imapct
  tw_lpid sender;
} message;


//State struct
//   this defines the state of each LP
// for stock market one LP = one stock
typedef struct {
  double historical_close;
  double historical_volume;
  double historical_obv;

  double current_opening;
  double current_close;
  double current_volume;
  double current_obv;
  int current_day;

  double accumulated_orders;  // this is for order imbalance update
  double accumulated_volume;
  int cur_ticks;

  double sector_id;           
  double volatility;          
} state;


//Command Line Argument declarations
extern unsigned int training_years;
extern unsigned int simulation_years;
extern unsigned int num_stocks;

//Global variables used by both main and driver
// - this defines the LP types
extern tw_lptype model_lps[];

//Function Declarations
// defined in model_driver.c:
extern void model_init(state *s, tw_lp *lp);
extern void model_event(state *s, tw_bf *bf, message *in_msg, tw_lp *lp);
extern void model_event_reverse(state *s, tw_bf *bf, message *in_msg, tw_lp *lp);
extern void model_final(state *s, tw_lp *lp);
// defined in model_map.c:
extern tw_peid model_map(tw_lpid gid);

/*
//Custom mapping prototypes
void model_cutom_mapping(void);
tw_lp * model_mapping_to_lp(tw_lpid lpid);
tw_peid model_map(tw_lpid gid);
*/

#endif
