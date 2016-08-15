#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;


typedef struct Vehicles
{
  Direction origin;
  Direction destination;
} Vehicle;


bool right_turn(Vehicle *v);
bool safe_entry(Vehicle *enter, Vehicle *present);


static struct cv* intersection_cv_N;
static struct cv* intersection_cv_S;
static struct cv* intersection_cv_W;
static struct cv* intersection_cv_E;

static struct lock* intersection_lock;
struct array* vehicle_array; 
int count_N=0;
int count_S=0;
int count_W=0;
int count_E=0;
bool can_go_N = true;
bool can_go_S = true;
bool can_go_W = true;
bool can_go_E = true;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */

void
intersection_sync_init(void)
{
  intersection_cv_N = cv_create("intersection_cv_N");
  intersection_cv_S = cv_create("intersection_cv_S");
  intersection_cv_W = cv_create("intersection_cv_W");
  intersection_cv_E = cv_create("intersection_cv_E");

  intersection_lock = lock_create("intersection_lock");
  vehicle_array = array_create();
  array_init(vehicle_array);

  KASSERT(intersection_cv_N != NULL);
  KASSERT(intersection_cv_S != NULL);
  KASSERT(intersection_cv_W != NULL);
  KASSERT(intersection_cv_E != NULL);
  KASSERT(intersection_lock != NULL); 
  KASSERT(vehicle_array != NULL);

  /* replace this default implementation with your own implementation */
  //intersectionSem = sem_create("intersectionSem",1);
  //if (intersectionSem == NULL) {
  //  panic("could not create intersection semaphore");
  //}
  //return;

}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */

void
intersection_sync_cleanup(void)
{
  KASSERT(intersection_lock != NULL);
  KASSERT(intersection_cv_N != NULL);
  KASSERT(intersection_cv_S != NULL);
  KASSERT(intersection_cv_W != NULL);
  KASSERT(intersection_cv_E != NULL);
  KASSERT(vehicle_array != NULL);

  lock_destroy(intersection_lock);

  cv_destroy(intersection_cv_N);
  cv_destroy(intersection_cv_S);
  cv_destroy(intersection_cv_W);
  cv_destroy(intersection_cv_E);

  array_destroy(vehicle_array);

  /* replace this default implementation with your own implementation */
  //KASSERT(intersectionSem != NULL);
  //sem_destroy(intersectionSem);
}


// return true if the vehicle take right turn.
bool
right_turn(Vehicle* v) {
  KASSERT(v != NULL);
  if (((v->origin == west) && (v->destination == south)) ||
      ((v->origin == south) && (v->destination == east)) ||
      ((v->origin == east) && (v->destination == north)) ||
      ((v->origin == north) && (v->destination == west))) {
    return true;
  } else {
    return false;
  }
}

// check the vehicle whether go into intersection safely.
bool safe_entry(Vehicle* va, Vehicle* vb) {
  if (va->origin == vb->origin){
    return true;
  }
  else if ((va->origin == vb->destination) && (va->destination == vb->origin)){
    return true;
  }
  else if ((va->destination != vb->destination) && (right_turn(va) || right_turn(vb))){
    return true;
  }
  else {
    return false;
  }
}



/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  int can_go = 0; 
  int num_v; //number of vehicles in the array
//  int count = 0; // for fairness

  KASSERT(intersection_lock != NULL);
  KASSERT(intersection_cv_N != NULL);
  KASSERT(intersection_cv_S != NULL);
  KASSERT(intersection_cv_W != NULL);
  KASSERT(intersection_cv_E != NULL);

  KASSERT(vehicle_array != NULL);

  lock_acquire(intersection_lock);


  Vehicle *vnew = kmalloc(sizeof(Vehicle));
  KASSERT(vnew != NULL);
  vnew->origin = origin;
  vnew->destination = destination;
  
  while(true) {

    can_go = 0;
    num_v=array_num(vehicle_array);
//    count = 0;

    for(int i=0; i<num_v; i++){
      Vehicle *array_v = array_get(vehicle_array,i);
      
//      if((vnew->origin == array_v->origin) &&
//         (vnew->destination == array_v->destination)){
//          count = count + 1;
//      }
      
      if(safe_entry(vnew, array_v)){
        can_go = can_go + 1;
      }
      else if(!safe_entry(vnew, array_v)){
        break;
      }
    }
 
    if((can_go == num_v) && (can_go_N == true) && (vnew->origin == north)){

      KASSERT(lock_do_i_hold(intersection_lock));
//      kprintf("entering origin:%d destination:%d\n", vnew->origin, vnew->destination);
      array_add(vehicle_array, vnew, NULL);
      count_N = count_N + 1;

      if(count_N >= 4){
        can_go_N = false;
      }
      break;
    }
    else if((can_go == num_v) && (can_go_S == true) && (vnew->origin == south)){

      KASSERT(lock_do_i_hold(intersection_lock));
//      kprintf("entering origin:%d destination:%d\n", vnew->origin, vnew->destination);
      array_add(vehicle_array, vnew, NULL);
      count_S = count_S + 1;

      if(count_S >= 4){
        can_go_S = false;
      }
      break;
    }
    else if((can_go == num_v) && (can_go_W == true) && (vnew->origin == west)){

      KASSERT(lock_do_i_hold(intersection_lock));
//      kprintf("entering origin:%d destination:%d\n", vnew->origin, vnew->destination);
      array_add(vehicle_array, vnew, NULL);
      count_W = count_W + 1;

      if(count_W >= 4){
        can_go_W = false;
      }
      break;
    }
    else if((can_go == num_v) && (can_go_E == true) && (vnew->origin == east)){

      KASSERT(lock_do_i_hold(intersection_lock));
//      kprintf("entering origin:%d destination:%d\n", vnew->origin, vnew->destination);
      array_add(vehicle_array, vnew, NULL);
      count_E = count_E + 1;

      if(count_E >= 4){
        can_go_E = false;
      }
      break;
    }
    else {
//      kprintf("waiting origin:%d destination:%d\n", vnew->origin, vnew->destination);
      if(vnew->origin == north){
        cv_wait(intersection_cv_N, intersection_lock);
      }
      else if(vnew->origin == south){
        cv_wait(intersection_cv_S, intersection_lock);
      }
      else if(vnew->origin == west){
        cv_wait(intersection_cv_W, intersection_lock);
      }
      else if(vnew->origin == east){
        cv_wait(intersection_cv_E, intersection_lock);
      }
    }
  }
  lock_release(intersection_lock);

  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //P(intersectionSem);

}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  int num_v;  // number of vehicle in the array
  
  KASSERT(intersection_lock != NULL);
  KASSERT(intersection_cv_N != NULL);
  KASSERT(intersection_cv_S != NULL);
  KASSERT(intersection_cv_W != NULL);
  KASSERT(intersection_cv_E != NULL);

  KASSERT(vehicle_array != NULL);

  lock_acquire(intersection_lock);
  
  num_v = array_num(vehicle_array);

  for (int i = 0; i < num_v; ++i){
    Vehicle* cur_vehicle = array_get(vehicle_array,i); 

    if ((cur_vehicle->origin == origin) && (cur_vehicle->destination == destination)){

      if(cur_vehicle->origin == north){
//        cv_broadcast(intersection_cv_N, intersection_lock);
        cv_broadcast(intersection_cv_S, intersection_lock);
        cv_broadcast(intersection_cv_W, intersection_lock);
        cv_broadcast(intersection_cv_E, intersection_lock);
        count_N = count_N - 1;
        if((count_N <= 0) || (can_go_N == true)){
          can_go_N = true;
          cv_broadcast(intersection_cv_N, intersection_lock);
        }
      }
      else if(cur_vehicle->origin == south){
        cv_broadcast(intersection_cv_N, intersection_lock);
//        cv_broadcast(intersection_cv_S, intersection_lock);
        cv_broadcast(intersection_cv_W, intersection_lock);
        cv_broadcast(intersection_cv_E, intersection_lock);
        count_S = count_S - 1;
        if((count_S <= 0) || (can_go_S == true)){
          can_go_S = true;
          cv_broadcast(intersection_cv_S, intersection_lock);
        }
      }
      else if(cur_vehicle->origin == west){
        cv_broadcast(intersection_cv_N, intersection_lock);
        cv_broadcast(intersection_cv_S, intersection_lock);
//        cv_broadcast(intersection_cv_W, intersection_lock);
        cv_broadcast(intersection_cv_E, intersection_lock);
        count_W = count_W - 1;
        if((count_W <= 0) || (can_go_W == true)){
          can_go_W = true;
          cv_broadcast(intersection_cv_W, intersection_lock);
        }
      }
      else if(cur_vehicle->origin == east){
        cv_broadcast(intersection_cv_N, intersection_lock);
        cv_broadcast(intersection_cv_S, intersection_lock);
        cv_broadcast(intersection_cv_W, intersection_lock);
//        cv_broadcast(intersection_cv_E, intersection_lock);
        count_E = count_E - 1;
        if((count_E <= 0) || (can_go_E == true)){
          can_go_E = true;
          cv_broadcast(intersection_cv_E, intersection_lock);
        }
      }

      array_remove(vehicle_array, i);
//      kprintf("leaving origin:%d destination:%d\n", origin, destination);
      break;
    }
  }
  lock_release(intersection_lock);

  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  //KASSERT(intersectionSem != NULL);
  //V(intersectionSem);
}
