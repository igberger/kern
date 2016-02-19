#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>

/* 
 * This simple default synchronization mechanism allows only creature at a time to
 * eat.   The globalCatMouseSem is used as a a lock.   We use a semaphore
 * rather than a lock so that this code will work even before locks are implemented.
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
static struct lock *globalCatMouseLock;//Protect critial region of an animal entering or leaving a bowl
static struct cv **globalCatCv;
static struct cv **globalMouseCv;
static int catsTurn; //1 - turn for cats to eat, 0 - turn for mice to eat
static int catsThatHaveEaten;
static int miceThatHaveEaten;
static int numBowls;
static char *bowlsArray;


/* 
 * The CatMouse simulation will call this function once before any cat or
 * mouse tries to each.
 *
 * You can use it to initialize synchronization and other variables.
 * 
 * parameters: the number of bowls
 */
void
catmouse_sync_init(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_init */

  (void)bowls; /* keep the compiler from complaining about unused parameters */
  bowlsArray = kmalloc(sizeof(char)*bowls);
  globalCatMouseLock = lock_create("globalCatMouseLock");
  if (globalCatMouseLock == NULL) {
    panic("could not create global CatMouse synchronization lock");
  }
  globalCatCv = kmalloc(sizeof(struct cv *)*bowls);
  globalMouseCv = kmalloc(sizeof(struct cv *)*bowls);
  for(int i=0; i<bowls; i++){
    bowlsArray[i] = '-';
    globalCatCv[i] = cv_create("globalCatCv");
    if (&(globalCatMouseLock[i]) == NULL) {
      panic("could not create global Cat synchronization condition variable");
    }
    globalMouseCv[i] = cv_create("globalMouseCv");
    if (&(globalCatMouseLock[i]) == NULL) {
      panic("could not create global Mouse synchronization condition variable");
    }
  }
  catsTurn = 1;
  catsThatHaveEaten = 0;
  miceThatHaveEaten = 0;
  numBowls = bowls;
  return;
}

/* 
 * The CatMouse simulation will call this function once after all cat
 * and mouse simulations are finished.
 *
 * You can use it to clean up any synchronization and other variables.
 *
 * parameters: the number of bowls
 */
void
catmouse_sync_cleanup(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_cleanup */
  (void)bowls; /* keep the compiler from complaining about unused parameters */
  KASSERT(globalCatMouseLock != NULL);
  lock_destroy(globalCatMouseLock);
  KASSERT(globalCatCv != NULL);
  KASSERT(globalMouseCv != NULL);
  for(int i=0; i<numBowls; i++){
    KASSERT(globalCatCv[i] != NULL);
    cv_destroy(globalCatCv[i]);
    KASSERT(globalMouseCv[i] != NULL);
    cv_destroy(globalMouseCv[i]);
  }
  kfree(globalCatCv);
  kfree(globalMouseCv);
}


/*
 * The CatMouse simulation will call this function each time a cat wants
 * to eat, before it eats.
 * This function should cause the calling thread (a cat simulation thread)
 * to block until it is OK for a cat to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the cat is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_before_eating(unsigned int bowl) 
{
  kprintf("Cat trying to eat at bowl: %d\n", bowl);
  /* replace this default implementation with your own implementation of cat_before_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseLock != NULL);
  lock_acquire(globalCatMouseLock);
  while(catsTurn != 1 || bowlsArray[bowl-1] != '-' || catsThatHaveEaten >= numBowls){
    cv_wait(globalCatCv[bowl-1], globalCatMouseLock);
  }
  bowlsArray[bowl-1] = 'c';
  kprintf("Cat going to eat at bowl: %d\n", bowl);
  lock_release(globalCatMouseLock);
}

/*
 * The CatMouse simulation will call this function each time a cat finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this cat finished.
 *
 * parameter: the number of the bowl at which the cat is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_after_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseLock != NULL);
  lock_acquire(globalCatMouseLock);
  bowlsArray[bowl-1] = '-';
  catsThatHaveEaten++;
  if(catsThatHaveEaten >= numBowls){//threshold for turn crossed
    int stillEating = 0;
    for(int i=0; i<numBowls; i++){//check if any cats are still eating
      if(bowlsArray[i] != '-'){
        stillEating = 1;
        break;
      }
    }
    if(stillEating == 0){//if no cats are eating change turn to mice, reset eating counter, and signal all mice
      catsTurn = 0;
      kprintf("Mice Turn\n");
      catsThatHaveEaten = 0;
      for(int i=0; i<numBowls; i++){
        cv_signal(globalMouseCv[i]);
      }
    }
  } else{//threshold not crossed, signal one cat that might be waiting on this bowl
    cv_signal(globalCatCv[bowl-1]);
  }
  lock_release(globalCatMouseLock);
}

/*
 * The CatMouse simulation will call this function each time a mouse wants
 * to eat, before it eats.
 * This function should cause the calling thread (a mouse simulation thread)
 * to block until it is OK for a mouse to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the mouse is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_before_eating(unsigned int bowl) 
{
  kprintf("Mouse trying to eat at bowl: %d\n", bowl);
  /* replace this default implementation with your own implementation of mouse_before_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseLock != NULL);
  lock_acquire(globalCatMouseLock);
  while(catsTurn != 0 || bowlsArray[bowl-1] != '-' || miceThatHaveEaten >= numBowls){
    cv_wait(globalMouseCv[bowl-1], globalCatMouseLock);
  }
  bowlsArray[bowl-1] = 'm';
  kprintf("Mouse going to eat at bowl: %d\n", bowl);
  lock_release(globalCatMouseLock);
}

/*
 * The CatMouse simulation will call this function each time a mouse finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this mouse finished.
 *
 * parameter: the number of the bowl at which the mouse is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_after_eating */
  (void)bowl;  /* keep the compiler from complaining about an unused parameter */
  KASSERT(globalCatMouseLock != NULL);
  lock_acquire(globalCatMouseLock);
  bowlsArray[bowl-1] = '-';
  miceThatHaveEaten++;
  if(miceThatHaveEaten >= numBowls){//threshold for turn crossed
    int stillEating = 0;
    for(int i=0; i<numBowls; i++){//check if any mice are still eating
      if(bowlsArray[i] != '-'){
        stillEating = 1;
        break;
      }
    }
    if(stillEating == 0){//if no mice are eating change turn to cats, reset eating counter, and signal all cats
      catsTurn = 1;
      kprintf("Cats Turn\n");
      miceThatHaveEaten = 0;
      for(int i=0; i<numBowls; i++){
        cv_signal(globalCatCv[i]);
      }
    }
  } else{//threshold not crossed, signal one mouse that might be waiting on this bowl
      cv_signal(globalMouseCv[bowl-1]);
  }
  lock_release(globalCatMouseLock);
}
