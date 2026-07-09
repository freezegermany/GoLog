#pragma once
#include "settings.h"

typedef enum {
  ACTIVITY_RUNNING = 0,
  ACTIVITY_CYCLING = 1,
} ActivityType;

void workout_window_push(ActivityType type, AppSettings *settings);
void workout_window_destroy(void);