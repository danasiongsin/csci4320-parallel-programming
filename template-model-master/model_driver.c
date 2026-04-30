//MAIN LOGIC FILE _________________________________________

//The C driver file for a ROSS model
//This file includes:
// - an initialization function for each LP type
// - a forward event function for each LP type
// - a reverse event function for each LP type
// - a finalization function for each LP type

//Includes
#include <stdio.h>
#include <stdlib.h>
#include "ross.h"
#include "model.h"

stock_data *g_stocks = NULL;
int g_num_stocks = 0;

uint64_t g_computation_cycles = 0;

void model_init (state *s, tw_lp *lp) {
    int self = lp->gid;
    
    s->stock_id = self;
    s->current_day = 0;

    if (self == 0) {
        // --- EXCHANGE ---
        s->current_opening = 0.0;
        s->current_close = 0.0;
        s->current_volume = 0.0;
        s->accumulated_volume = 0.0;
        s->accumulated_orders = 0.0;
        s->cur_ticks = 0;

        tw_event *e = tw_event_new(self, 1, lp);
        message *msg = tw_event_data(e);
        msg->type = MARKET_OPEN;
        msg->day = 0; 
        msg->sender = self;
        tw_event_send(e);
    } else {
        // --- TRADER ---
        s->cash = g_starting_cash;         
        s->current_holdings = 0;    
        
        if (g_strategy_id == 0) {
            s->strategy_id = tw_rand_integer(lp->rng, 1, 3);
        } else {
            s->strategy_id = g_strategy_id;
        }
        
        tw_event *e = tw_event_new(self, 1, lp);
        message *msg = tw_event_data(e);
        msg->type = MARKET_UPDATE;
        msg->day = 0; 
        msg->sender = self;
        tw_event_send(e);
    }
}

void model_event (state *s, tw_bf *bf, message *in_msg, tw_lp *lp) {
    uint64_t start_comp = clock_now();
    
    int self = lp->gid;
    *(int *) bf = (int) 0; 
    in_msg->previous_close = s->current_close;
    int day = in_msg->day;
    
    csv_row *row = NULL;
    csv_row dummy_row = {day, 100.0, 105.0, 95.0, 102.0, 100000.0, 100.0, 98.0, 45.0, 1.2, 0.5, 110.0, 90.0};
    
    if (g_stocks != NULL && g_stocks[0].count > (size_t)day) {
        row = &g_stocks[0].rows[day];
    } else {
        row = &dummy_row;
    }

    if (self == 0) {
        // --- EXCHANGE LOGIC ---
        switch (in_msg->type) {
            case MARKET_OPEN: {
                s->current_day = day;
                s->current_opening = row->open;
                s->accumulated_orders = 0.0;
                s->accumulated_volume = 0.0;
                s->cur_ticks = 0;

                tw_event *e = tw_event_new(self, 1, lp);
                message *msg = tw_event_data(e);
                msg->type = HOURLY_UPDATE;
                msg->stock_id = self;
                msg->day = day;
                tw_event_send(e);
                break;      
            }
            case HOURLY_UPDATE: {
                s->cur_ticks++;
                s->accumulated_volume += row->volume / 6.0;

                if (s->cur_ticks < 6) {
                    tw_event *e = tw_event_new(self, 1, lp);
                    message *msg = tw_event_data(e);
                    msg->type = HOURLY_UPDATE;
                    msg->stock_id = self;
                    msg->day = day;
                    tw_event_send(e);
                } else {
                    tw_event *e = tw_event_new(self, 1, lp);
                    message *msg = tw_event_data(e);
                    msg->type = MARKET_CLOSE;
                    msg->stock_id = self;
                    msg->day = day;
                    tw_event_send(e);
                }
                break;
            }
            case MARKET_CLOSE: {
                s->current_close = row->close;
                int last_day = (g_stocks != NULL && g_stocks[0].count > 0) ? (int)g_stocks[0].count - 1 : 0;
                if (day < last_day) {
                    tw_event *e = tw_event_new(self, 12, lp); 
                    message *msg = tw_event_data(e);
                    msg->type = MARKET_OPEN;
                    msg->day = day + 1;
                    tw_event_send(e);
                }
                break;
            }
            case PLACE_ORDER: {
                s->accumulated_orders += 1;
                s->accumulated_volume += in_msg->quantity;
                break;
            }
            default:
                break;
        }
    } 
    else {
        // --- TRADER LOGIC ---
        switch (in_msg->type) {
            case MARKET_UPDATE: {
                s->current_day = day;
                int order_quantity = 0;
                int order_type = 0; 
                
                in_msg->order_type = 0;
                in_msg->quantity = 0;
                
                // Strategy 1: Mean Reversion
                if (s->strategy_id == 1) { 
                    if (row->rsi_14 < g_rsi_oversold && s->cash >= row->close * g_order_size) {
                        order_type = 1; 
                        order_quantity = g_order_size;
                    } else if (row->rsi_14 > g_rsi_overbought && s->current_holdings > 0) {
                        order_type = -1; 
                        order_quantity = s->current_holdings; 
                    }
                } 
                // Strategy 2: Trend Follower
                else if (s->strategy_id == 2) {
                    if (row->macd > row->macd_signal && s->cash >= row->close * g_order_size) {
                        order_type = 1; 
                        order_quantity = g_order_size;
                    } else if (row->macd < row->macd_signal && s->current_holdings > 0) {
                        order_type = -1; 
                        order_quantity = s->current_holdings;
                    }
                }
                // Strategy 3: Noise Trader
                else if (s->strategy_id == 3) {
                    if (tw_rand_unif(lp->rng) < g_noise_prob) { 
                        if (tw_rand_unif(lp->rng) > 0.5 && s->cash >= row->close * g_order_size) {
                            order_type = 1; 
                            order_quantity = g_order_size;
                        } else if (s->current_holdings > 0) {
                            order_type = -1; 
                            order_quantity = g_order_size; // Or s->current_holdings to dump
                        }
                    }
                }

                if (order_type != 0 && order_quantity > 0) {
                    in_msg->order_type = order_type;
                    in_msg->quantity = order_quantity;
                    
                    if (order_type == 1) { 
                        s->cash -= (row->close * order_quantity);
                        s->current_holdings += order_quantity;
                    } else if (order_type == -1) { 
                        s->cash += (row->close * order_quantity);
                        s->current_holdings -= order_quantity;
                    }

                    tw_event *order_event = tw_event_new(0, 1, lp); 
                    message *order_msg = tw_event_data(order_event);
                    order_msg->type = PLACE_ORDER;
                    order_msg->sender = lp->gid;
                    order_msg->order_type = order_type;
                    order_msg->quantity = order_quantity;
                    tw_event_send(order_event);
                }

                {
                    int last_day = (g_stocks != NULL && g_stocks[0].count > 0) ? (int)g_stocks[0].count - 1 : 0;
                    if (day < last_day) {
                        tw_event *e = tw_event_new(self, 24, lp);
                        message *msg = tw_event_data(e);
                        msg->type = MARKET_UPDATE;
                        msg->day = day + 1;
                        tw_event_send(e);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    
    uint64_t end_comp = clock_now();
    g_computation_cycles += (end_comp - start_comp);
}

void model_event_reverse (state *s, tw_bf *bf, message *in_msg, tw_lp *lp) {
    int self = lp->gid;
    int day = in_msg->day;
    
    csv_row *row = NULL;
    csv_row dummy_row = {day, 100.0, 105.0, 95.0, 102.0, 100000.0, 100.0, 98.0, 45.0, 1.2, 0.5, 110.0, 90.0};
    
    if (g_stocks != NULL && g_stocks[0].count > (size_t)day) {
        row = &g_stocks[0].rows[day];
    } else {
        row = &dummy_row;
    }

    if (self == 0) {
        switch (in_msg->type) {
            case MARKET_OPEN: {
                s->current_day = day - 1; 
                s->accumulated_orders = 0.0;
                s->accumulated_volume = 0.0;
                s->cur_ticks = 6; 
                break;      
            }
            case HOURLY_UPDATE: {
                s->cur_ticks--; 
                s->accumulated_volume -= row->volume / 6.0; 
                break;
            }
            case MARKET_CLOSE: {
                s->current_close = in_msg->previous_close; 
                break;
            }
            case PLACE_ORDER: {
                s->accumulated_orders -= 1;
                s->accumulated_volume -= in_msg->quantity;
                break;
            }
            default:
                break;
        }
    } 
    else {
        switch (in_msg->type) {
            case MARKET_UPDATE: {
                s->current_day = day - 1;
                if (in_msg->order_type == 1) { 
                    s->cash += (row->close * in_msg->quantity);
                    s->current_holdings -= in_msg->quantity;
                } 
                else if (in_msg->order_type == -1) { 
                    s->cash -= (row->close * in_msg->quantity);
                    s->current_holdings += in_msg->quantity;
                }
                break;
            }
            default:
                break;
        }
    }
}

void model_final (state *s, tw_lp *lp) {
    if (lp->gid == 0) {
        printf("EXCHANGE (GID 0) finished Day %d with %f total volume processed.\n", s->current_day, s->accumulated_volume);
    } else {
        double final_portfolio_value = s->cash + (s->current_holdings * s->current_close);
        double profit = final_portfolio_value - g_starting_cash; 
        
        char* strategy_name = "Unknown";
        if (s->strategy_id == 1) strategy_name = "Mean Reversion";
        if (s->strategy_id == 2) strategy_name = "Trend Follower";
        if (s->strategy_id == 3) strategy_name = "Noise Trader  ";

        printf("TRADER %4d | Strategy: %s | Final Value: $%9.2f | Profit: $%9.2f\n", 
               (int)lp->gid, strategy_name, final_portfolio_value, profit);
    }
}