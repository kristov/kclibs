#ifndef UDM_H
#define UDM_H

#include <libudev.h>
#include <stdint.h>

typedef struct udm_device {
    struct udev_device* udev_dev;
    uint8_t fd;
    char* devnode;
} udm_device_t;

typedef void (*udm_callback_t)(udm_device_t* device, const char* action, void *user_data);

typedef struct udm_context {
    int hotplug_fd;
    struct udev* udev;
    struct udev_monitor* udev_mon;
    fd_set fds;
    int max_fd;
    uint8_t nr_devices;
    void* user_data;
    uint64_t select_sec;
    uint32_t select_usec;
    udm_callback_t callback;
	udm_device_t devices[256];
	udm_device_t* fd_lookup[256];
    char subsystem[256];
} udm_context_t;

void udm_init(udm_context_t* context, const char* subsystem);

void udm_process_events(udm_context_t* context);

void udm_set_select_interval(udm_context_t* context, uint64_t sec, uint32_t usec);

void udm_set_callback(udm_context_t* context, udm_callback_t callback, void* user_data);

#endif
