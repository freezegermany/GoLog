#include "vibration.h"
static const uint32_t s_manual_toggle_segments[] = {80};
static const VibePattern s_manual_toggle_pattern = { .durations = s_manual_toggle_segments, .num_segments = ARRAY_LENGTH(s_manual_toggle_segments) };
static const uint32_t s_autopause_start_segments[] = {60, 80, 60};
static const VibePattern s_autopause_start_pattern = { .durations = s_autopause_start_segments, .num_segments = ARRAY_LENGTH(s_autopause_start_segments) };
static const uint32_t s_autopause_end_segments[] = {60, 80, 140};
static const VibePattern s_autopause_end_pattern = { .durations = s_autopause_end_segments, .num_segments = ARRAY_LENGTH(s_autopause_end_segments) };
static const uint32_t s_split_segments[] = {250};
static const VibePattern s_split_pattern = { .durations = s_split_segments, .num_segments = ARRAY_LENGTH(s_split_segments) };
void vibration_feedback_manual_toggle(void) { vibes_enqueue_custom_pattern(s_manual_toggle_pattern); }
void vibration_feedback_autopause_start(void) { vibes_enqueue_custom_pattern(s_autopause_start_pattern); }
void vibration_feedback_autopause_end(void) { vibes_enqueue_custom_pattern(s_autopause_end_pattern); }
void vibration_feedback_split(void) { vibes_enqueue_custom_pattern(s_split_pattern); }
