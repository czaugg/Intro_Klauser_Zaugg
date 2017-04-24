/**
 * \file
 * \brief Semaphore usage
 * \author Erich Styger, erich.styger@hslu.ch
 *
 * Module using semaphores.
 */

/**
 * \file
 * \brief Semaphore usage
 * \author Erich Styger, erich.styger@hslu.ch
 *
 * Module using semaphores.
 */

#include "Platform.h" /* interface to the platform */
#if PL_CONFIG_HAS_SEMAPHORE
#include "FRTOS1.h"
#include "Sem.h"
#include "LED.h"

static xSemaphoreHandle sem = NULL;

static void vSlaveTask(void *pvParameters) {
  /*! \todo Implement functionality */
   while (sem==NULL) {
     vTaskDelay(100/portTICK_PERIOD_MS);
   }
   for(;;) {
     if (xSemaphoreTake(sem, portMAX_DELAY)==pdPASS) { /* block on semaphore */
       LED1_Neg();
     }
   }
}

static void vMasterTask(void *pvParameters) {
  /*! \todo send semaphore from master task to slave task */

  (void)pvParameters; /* parameter not used */
  while (sem==NULL) {
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  for(;;) {
    (void)xSemaphoreGive(sem); /* give control to other task */
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void SEM_Deinit(void) {
}

/*! \brief Initializes module */
void SEM_Init(void) {
  sem = xSemaphoreCreateBinary();
  if (sem==NULL) {
    for(;;) {} /* ups? */
  }
  vQueueAddToRegistry(sem, "IPC_Sem");
  if (xTaskCreate(vMasterTask, "Master", 300/sizeof(StackType_t), NULL, tskIDLE_PRIORITY+1, NULL) != pdPASS) {
    for(;;){} /* error */
  }
  /* create slave task */
  if (xTaskCreate(vSlaveTask, "Slave", 300/sizeof(StackType_t), NULL, tskIDLE_PRIORITY+1, NULL) != pdPASS) {
    for(;;){} /* error */
  }
}
#endif /* PL_CONFIG_HAS_SEMAPHORE */
