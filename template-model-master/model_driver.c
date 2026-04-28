//MAIN LOGIC FILE _________________________________________

//The C driver file for a ROSS model
//This file includes:
// - an initialization function for each LP type
// - a forward event function for each LP type
// - a reverse event function for each LP type
// - a finalization function for each LP type

//Includes
#include <stdio.h>

#include "ross.h"
#include "model.h"


//Helper Functions
void SWAP (double *a, double *b) {
  double tmp = *a;
  *a = *b;
  *b = tmp;
}
/*
//parse the csv data ????
static void parse_data(FILE *stream) {
    
}


//calculate from closing price, news, and order imbalance
double opening_price() {

}

//calculate from opening price and volume pressure
double closing_price() {

}

double compute_obv() {

}
*/



//Init function
// - called once for each LP
// ! LP can only send messages to itself during init !
void model_init (state *s, tw_lp *lp) {
  int self = lp->gid;

  // init state data
  s->rcvd_count_H = 0;
  s->rcvd_count_G = 0;
  s->value = -1;

  /* //initalize state
  s->current_day = 0;
  s->current_opening = 100.0;   //opening price, default for now
  s->current_closing = 100.0;   //closing price
  s->current_volume = 1000000.0;
  s->current_obv = 0.0;
  s->accumulated_orders = 0.0;
  s->accumulated_volume = 0.0;
  s->cur_ticks = 0;


  //these depend on the stock, need input variables from dataset here
  s->sector_id = 0.0;       // determine the labels for sector & check dataset for possible labels
  s->volatility  = 0.00;  // 

  */

  // Init message to myself
  tw_event *e = tw_event_new(self, 1, lp);
  message *msg = tw_event_data(e);
  msg->type = HELLO;
  msg->contents = tw_rand_unif(lp->rng);
  msg->sender = self;
  tw_event_send(e);
}

//Forward event handler
void model_event (state *s, tw_bf *bf, message *in_msg, tw_lp *lp) {
  int self = lp->gid;

  // initialize the bit field
  *(int *) bf = (int) 0;

  //save old close in msg scratch so reverse can restore it
  in_msg->contents = s->current_close;

  //advance day when market is open
  int day = in_msg->day;

  //pull today's CSV row
  const csv_row *row = NULL;
  if (g_stocks && s->stock_id < g_num_stocks) {
    stock_data *sd = &g_stocks[s->stock_id];
    if (day >= 0 && (size_t)day < sd->count) {
      row = &sd->rows[day];
    }
  }

  // use a state for every event -------------------------------------------------------
  // handle the message
  switch (in_msg->type) {

    case MARKET_OPEN: {
      //init all day opening param: 
      s->current_day = day;
      s->accumulated_orders = 0.0;
      s->accumulated_volume = 0.0;
      s->cur_ticks = 0;

      //if we have csv data for the day, copy the values into the state

      //schedule the first hourly update
      tw_event *e = tw_event_new(self, 2, lp);
      message *msg = tw_event_data(e);
      msg->type = HOURLY_UPDATE;
      msg->stock_id = self;
      msg->day = day;
      //fill_msg_from_state(msg,s);
      tw_event_send(e);
      break;      
    }

    case HOURLY_UPDATE: {
      s->cur_ticks++;

      //if we have the csv data
      s->accumulated_volume += row->volume / 6.0;

      //if we are not at the last tick, schedule the next:
      if (s->cur_ticks < 6) {
        tw_event *e = tw_event_new(self, 2, lp);
        message *msg = tw_event_data(e);
        msg->type = HOURLY_UPDATE;
        msg->stock_id = self;
        msg->day = day;

        tw_event_send(e);
      } else {
        //close the market otherwise

        tw_event *e = tw_event_new(self, 2, lp);
        message *msg = tw_event_data(e);
        msg->type = MARKET_CLOSE;
        msg->stock_id = self;
        msg->day = day;

        tw_event_send(e);
      }

      break;
    }

    case ORDER_IMBALANCE_UPDATE: {
      break;
    }
    
     /* NOTE: if we want to include news, then we could use tw_random_unif for random prob of injecting 'news'
    case NEWS_BROADCAST: {
      break;
    }
    */

    default :
      printf("Unhandeled forward message type %d\n", in_msg->type); //CHANGE
  }

  //------------------from template: might need---------------------
  // tw_event *e = tw_event_new(self, 1, lp);
  // message *msg = tw_event_data(e);
  // //# randomly choose message type
  // double random = tw_rand_unif(lp->rng);
  // if (random < 0.5) {
  //   msg->type = HELLO;
  // } else {
  //   msg->type = GOODBYE;
  // }
  // msg->contents = tw_rand_unif(lp->rng);
  // msg->sender = self;
  // tw_event_send(e);
}

//Reverse Event Handler
void model_event_reverse (state *s, tw_bf *bf, message *in_msg, tw_lp *lp) {
  int self = lp->gid;

  // from template : might need
  // undo the state update using the value stored in the 'reverse' message
  // SWAP(&(s->value), &(in_msg->contents));

  // handle the message
  switch (in_msg->type) {
    case MARKET_CLOSE: {
      break;
    }

    case SECTOR_CORRELATION: {
      break;
    }

    /*
    case NEWS_BROADCAST: {
      break;
    }
    */

    default :
      printf("Unhandeled reverse message type %d\n", in_msg->type); //CHANGE
  }

  // don't forget to undo all rng calls
  tw_rand_reverse_unif(lp->rng);
  tw_rand_reverse_unif(lp->rng);
}

//report any final statistics for this LP
void model_final (state *s, tw_lp *lp){
  int self = lp->gid;
  printf("%d handled %d Hello and %d Goodbye messages\n", self, s->rcvd_count_H, s->rcvd_count_G);
}
