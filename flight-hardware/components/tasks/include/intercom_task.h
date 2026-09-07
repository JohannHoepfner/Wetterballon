#pragma once

#include "../../sensor/sensor.h"
#include "../../store/store.h"
#include "../../intercom/intercom.h"
#include "../../status_indicator/status_indicator.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stddef.h>

typedef enum {
	INTERCOM_TASK_MODE_TELEMETRY, // Uses a telemetry context which is updated and sent periodically
	INTERCOM_TASK_MODE_READ_FROM_STORE, // Reads lines from SD card and sends them via intercom
} IntercomTaskMode;

typedef struct {
	SemaphoreHandle_t mutex;
	char *value;
	size_t value_size;
} IntercomTaskStatus;

typedef struct {
	void (*set)(void *context, const char *value);
	void *context;
} IntercomStatusHandler;

typedef struct {
	IntercomTaskMode mode;
	Intercom *intercom;
	char *body;
	size_t body_size;
	union {
		IntercomTaskStatus *status;
		struct {
			Store *store;
			size_t lines_per_send;
		} source_store;
	} source;
    TickType_t send_interval;
	StatusIndicator *status_indicator;
} IntercomTaskContext;

IntercomStatusHandler INTERCOM_TASK_MODE_TELEMETRY_handler(IntercomTaskContext *context);
void intercom_task(void *arg);
