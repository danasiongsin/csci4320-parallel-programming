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

// Global dataset definitions
stock_data *g_stocks = NULL;
int g_num_stocks = 0;

void model_init (state *s, tw_lp *lp) {
    int self = lp->gid;
    
    // Initialize base state
    s->stock_id = self;
    s->current_day = 0;

    if (self == 0) {
        // ==========================================
        //         EXCHANGE LP INIT (GID 0)
        // ==========================================
        s->current_opening = 0.0;
        s->current_close = 0.0;
        s->current_volume = 0.0;
        s->accumulated_volume = 0.0;
        s->accumulated_orders = 0.0;
        s->cur_ticks = 0;

        // Kick off the market clock
        tw_event *e = tw_event_new(self, 1, lp);
        message *msg = tw_event_data(e);
        msg->type = MARKET_OPEN;
        msg->day = 0; 
        msg->sender = self;
        tw_event_send(e);
    } else {
        // ==========================================
        //         TRADER LP INIT (GID > 0)
        // ==========================================
        s->cash = 100000.0;         // Starting bankroll
        s->current_holdings = 0;    // Starting shares
        s->strategy_id = 1;         // Assign Strategy (1 = Mean Reversion)
        
        // Kick off the trader's daily check
        tw_event *e = tw_event_new(self, 1, lp);
        message *msg = tw_event_data(e);
        msg->type = MARKET_UPDATE;
        msg->day = 0; 
        msg->sender = self;
        tw_event_send(e);
    }
}

void model_event (state *s, tw_bf *bf, message *in_msg, tw_lp *lp) {
    int self = lp->gid;
    *(int *) bf = (int) 0; // Initialize bitfield
    
    // 1. Save data for reverse handler BEFORE changing state
    in_msg->previous_close = s->current_close;
    
    int day = in_msg->day;
    
    // 2. Fetch row dynamically from global dataset, with dummy fallback
    csv_row *row = NULL;
    csv_row dummy_row = {day, 100.0, 105.0, 95.0, 102.0, 100000.0, 100.0, 98.0, 45.0, 1.2, 0.5, 110.0, 90.0};
    
    if (g_stocks != NULL && g_stocks[0].count > (size_t)day) {
        row = &g_stocks[0].rows[day];
    } else {
        row = &dummy_row;
    }

    // 3. Process Event based on LP Type
    if (self == 0) {
        // ==========================================
        //         EXCHANGE LP LOGIC (GID 0)
        // ==========================================
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
                
                tw_event *e = tw_event_new(self, 12, lp); // Jump 12 hours forward for overnight
                message *msg = tw_event_data(e);
                msg->type = MARKET_OPEN;
                msg->day = day + 1;
                tw_event_send(e);
                break;
            }
            case PLACE_ORDER: {
                // Accounting ledger for the simulation
                s->accumulated_orders += 1;
                s->accumulated_volume += in_msg->quantity;
                break;
            }
            default:
                printf("Exchange unhandled forward msg type %d\n", in_msg->type);
        }
    } 
    else {
        // ==========================================
        //         TRADER LP LOGIC (GID > 0)
        // ==========================================
        switch (in_msg->type) {
            case MARKET_UPDATE: {
                s->current_day = day;
                int order_quantity = 0;
                int order_type = 0; // 1 for BUY, -1 for SELL
                
                // Initialize message scratchpad to 0 in case no trade happens
                in_msg->order_type = 0;
                in_msg->quantity = 0;
                
                // --- STRATEGY: MEAN REVERSION ---
                if (s->strategy_id == 1) { 
                    if (row->rsi_14 < 30.0 && s->cash >= row->close * 10) {
                        order_type = 1; // BUY
                        order_quantity = 10;
                    } else if (row->rsi_14 > 70.0 && s->current_holdings > 0) {
                        order_type = -1; // SELL
                        order_quantity = s->current_holdings; 
                    }
                } 

                // --- INSTANT EXECUTION ---
                if (order_type != 0 && order_quantity > 0) {
                    
                    // SAVE EXACT TRADE TO MESSAGE SO REVERSE HANDLER CAN UNDO IT
                    in_msg->order_type = order_type;
                    in_msg->quantity = order_quantity;
                    
                    // 1. Trader updates their own portfolio instantly
                    if (order_type == 1) { // Buy
                        s->cash -= (row->close * order_quantity);
                        s->current_holdings += order_quantity;
                    } else if (order_type == -1) { // Sell
                        s->cash += (row->close * order_quantity);
                        s->current_holdings -= order_quantity;
                    }

                    // 2. Send receipt to Exchange (GID 0)
                    tw_event *order_event = tw_event_new(0, 1, lp); 
                    message *order_msg = tw_event_data(order_event);
                    order_msg->type = PLACE_ORDER;
                    order_msg->sender = lp->gid;
                    order_msg->order_type = order_type;
                    order_msg->quantity = order_quantity;
                    tw_event_send(order_event);
                }

                // Schedule next day
                tw_event *e = tw_event_new(self, 24, lp);
                message *msg = tw_event_data(e);
                msg->type = MARKET_UPDATE;
                msg->day = day + 1;
                tw_event_send(e);
                break;
            }
            default:
                printf("Trader unhandled forward msg type %d\n", in_msg->type);
        }
    }
}

void model_event_reverse (state *s, tw_bf *bf, message *in_msg, tw_lp *lp) {
    int self = lp->gid;
    int day = in_msg->day;
    
    // Regenerate the row to reverse the math exactly
    csv_row *row = NULL;
    csv_row dummy_row = {day, 100.0, 105.0, 95.0, 102.0, 100000.0, 100.0, 98.0, 45.0, 1.2, 0.5, 110.0, 90.0};
    
    if (g_stocks != NULL && g_stocks[0].count > (size_t)day) {
        row = &g_stocks[0].rows[day];
    } else {
        row = &dummy_row;
    }

    if (self == 0) {
        // ==========================================
        //         EXCHANGE REVERSALS
        // ==========================================
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
                printf("Exchange unhandled reverse msg type %d\n", in_msg->type);
        }
    } 
    else {
        // ==========================================
        //         TRADER REVERSALS
        // ==========================================
        switch (in_msg->type) {
            case MARKET_UPDATE: {
                s->current_day = day - 1;
                
                // If a trade was executed and saved in the message scratchpad, undo it perfectly
                if (in_msg->order_type == 1) { // Undo Buy
                    s->cash += (row->close * in_msg->quantity);
                    s->current_holdings -= in_msg->quantity;
                } 
                else if (in_msg->order_type == -1) { // Undo Sell
                    s->cash -= (row->close * in_msg->quantity);
                    s->current_holdings += in_msg->quantity;
                }
                break;
            }
            default:
                printf("Trader unhandled reverse msg type %d\n", in_msg->type);
        }
    }
}

void model_final (state *s, tw_lp *lp) {
    if (lp->gid == 0) {
        printf("EXCHANGE (GID 0) finished Day %d with %f total volume processed.\n", s->current_day, s->accumulated_volume);
    } else {
        // Uncomment to print trader end states (can be noisy if you have thousands of traders)
        // printf("TRADER (GID %d) finished Day %d. Cash: $%.2f | Holdings: %d shares.\n", (int)lp->gid, s->current_day, s->cash, s->current_holdings);
    }
}