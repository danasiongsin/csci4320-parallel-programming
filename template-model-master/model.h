//The header file template for a ROSS model
//This file includes:
// - the state and message structs
// - extern'ed command line arguments
// - custom mapping function prototypes (if needed)
// - any other needed structs, enums, unions, or #defines

#ifndef _model_h
#define _model_h

#include <ross.h>

// --- CSV Data Structures ---
typedef struct {
    int day;
    double open;
    double high;
    double low;
    double close;
    double volume;
    
    // Pre-computed dataset columns
    double sma_50;
    double sma_200;
    double rsi_14;
    double macd;
    double macd_signal;
    double bollinger_upper;
    double bollinger_lower;
} csv_row;

typedef struct {
    int stock_id;
    size_t count;
    csv_row *rows;
} stock_data;

// --- Message & Events ---
typedef enum { 
  MARKET_OPEN,
  HOURLY_UPDATE,
  MARKET_CLOSE,   
  SECTOR_CORRELATION,
  ORDER_IMBALANCE_UPDATE,
  MARKET_UPDATE,
  PLACE_ORDER
} message_type;

typedef struct {
  message_type type;   
  int stock_id;
  int day;
  double open;
  double high;
  double low;
  double close;
  double volume;
  double obv;
  double daily_return_pct;
  
  double previous_close; 
  tw_lpid sender;
  int order_type;      // 1 for BUY, -1 for SELL
  int quantity;        // Number of shares
} message;

// --- LP State ---
typedef struct {
  int stock_id;
  double current_opening;
  double current_close;
  double current_volume;
  double current_obv;
  int current_day;
  double accumulated_orders;
  double accumulated_volume;
  int cur_ticks;
  double sector_id;           
  double volatility;    
  double cash;             // Trader's bankroll
  int current_holdings;    // Shares owned
  int strategy_id;         // Which archetype they are
} state; 

// --- Globals ---
extern stock_data *g_stocks;
extern int g_num_stocks;
extern tw_lptype model_lps[];
extern int g_strategy_id;  // Command line argument global

// --- Function Declarations ---
extern void model_init(state *s, tw_lp *lp);
extern void model_event(state *s, tw_bf *bf, message *in_msg, tw_lp *lp);
extern void model_event_reverse(state *s, tw_bf *bf, message *in_msg, tw_lp *lp);
extern void model_final(state *s, tw_lp *lp);
extern tw_peid model_map(tw_lpid gid);

// Simplified CSV load signature
void csv_load(const char* path); 

#endif