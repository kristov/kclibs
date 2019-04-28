#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "udm.h"

void udm_fd_set_rebuild(udm_context_t* context) {
    FD_ZERO(&context->fds);

    printf("Getting %d devices\n", context->nr_devices);
    for (uint8_t idx = 0; idx < context->nr_devices; idx++) {
        udm_device_t* device = &context->devices[idx];
        printf("Device fd: %d\n", device->fd);
        FD_SET(device->fd, &context->fds);
        if (device->fd > context->max_fd) {
            context->max_fd = device->fd;
        }
    }

    if (context->hotplug_fd > 0) {
        printf("Hotplug fd: %d\n", context->hotplug_fd);
        FD_SET(context->hotplug_fd, &context->fds);
        if (context->hotplug_fd > context->max_fd) {
            context->max_fd = context->hotplug_fd;
        }
    }
}

uint8_t udm_device_create(udm_context_t* context, struct udev_device* dev) {
    const char* devnode = udev_device_get_devnode(dev);
    if (!devnode) {
        return 0;
    }
    const char* sysname = udev_device_get_sysname(dev);
    if (strncmp(sysname, "card", 4) != 0) {
        return 0;
    }

    int fd = open(devnode, O_RDWR);
    if (fd < 0) {
        printf("Unable to open device: %s\n", devnode);
        return 0;
    }

	udm_device_t* device = &context->devices[context->nr_devices];
    context->nr_devices++;

    device->fd = (uint8_t)fd;
	device->udev_dev = dev;
    device->devnode = malloc(strlen(devnode) + 1);
    strcpy(device->devnode, devnode);

    printf("Found %s [%s]\n", devnode, sysname);

    // configure callback
    return 1;
}

udm_device_t* udm_find_device(udm_context_t* context, struct udev_device *dev) {
    const char* devnode = udev_device_get_devnode(dev);
    if (!devnode) {
        return NULL;
    }
    for (uint8_t idx = 0; idx < context->nr_devices; idx++) {
        udm_device_t* device = &context->devices[idx];
        if (strcmp(devnode, device->devnode) == 0) {
            return device;
        }
    }
    return NULL;
}

uint8_t udm_device_remove(udm_context_t* context, struct udev_device *dev) {
    const char* devnode = udev_device_get_devnode(dev);

    uint8_t idx_to_remove = 0;
    uint8_t found_device = 0;
    for (uint8_t idx = 0; idx < context->nr_devices; idx++) {
        udm_device_t* device = &context->devices[idx];
        printf("removed devnode: %s, this devnode: %s\n", devnode, device->devnode);
        if (strcmp(devnode, device->devnode) == 0) {
            found_device = 1;
            idx_to_remove = idx;
        }
    }

    if (!found_device) {
        // Can happen when unmonitored devices are removed
        return context->nr_devices;
    }
    printf("removing device %d\n", idx_to_remove);

/*

    debug_print("Allocating space for new device list\n");
    devices_new = malloc(sizeof(hid_monitor_udm_device_t) * (monitor->nr_devices - 1));

    idx_new = 0;
    for (idx = 0; idx < monitor->nr_devices; idx++) {
        if (idx == idx_to_remove) {
            if (NULL != monitor->device_rem_callback) {
                monitor->device_rem_callback(&monitor->devices[idx]);
            }
            debug_print("Destroying device at %d\n", idx);
            hid_monitor_device_destroy(&monitor->devices[idx]);
        }
        else {
            debug_print("Copying device at %d into %d\n", idx, idx_new);
            memcpy(&devices_new[idx_new], &monitor->devices[idx], sizeof(hid_monitor_udm_device_t));
            idx_new++;
        }
    }
    debug_print("Freeing devices\n");
    free(monitor->devices);

    monitor->devices = devices_new;
    monitor->nr_devices--;
*/
    return context->nr_devices;
}

void udm_populate_devices(udm_context_t* context) {
    struct udev_enumerate *enumerate = udev_enumerate_new(context->udev);
    udev_enumerate_add_match_subsystem(enumerate, context->subsystem);
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);

    struct udev_list_entry* dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char* path;
        path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *dev = udev_device_new_from_syspath(context->udev, path);
        udm_device_create(context, dev);
    }

    udev_enumerate_unref(enumerate);
    udm_fd_set_rebuild(context);
}

void udm_populate_hotplug_fd(udm_context_t* context) {
    uint8_t res;

    context->udev_mon = udev_monitor_new_from_netlink(context->udev, "udev");

    res = udev_monitor_filter_add_match_subsystem_devtype(context->udev_mon, context->subsystem, NULL);
    if (res < 0) {
        printf("Unable to add %s filter to monitor\n", context->subsystem);
        return;
    }

    res = udev_monitor_enable_receiving(context->udev_mon);
    if (res < 0) {
        printf("Unable to enable receiving\n");
        return;
    }
    context->hotplug_fd = udev_monitor_get_fd(context->udev_mon);
}

void udm_process_hotplug(udm_context_t* context) {
    struct udev_device *dev;
    const char* action;

    printf("event from hotplug monitor\n");
    dev = udev_monitor_receive_device(context->udev_mon);

    action = udev_device_get_action(dev);

    if (strcmp(action, "add") == 0) {
        udm_device_create(context, dev);
        udm_fd_set_rebuild(context);
        if (context->callback != NULL) {
            udm_device_t* device = udm_find_device(context, dev);
            if (device) {
                context->callback(device, action, context->user_data);
            }
        }
        return;
    }
    else if (strcmp(action, "remove") == 0) {
        if (context->callback != NULL) {
            udm_device_t* device = udm_find_device(context, dev);
            if (device) {
                context->callback(device, action, context->user_data);
            }
        }
        udm_device_remove(context, dev);
        udm_fd_set_rebuild(context);
        return;
    }

    if (context->callback != NULL) {
        udm_device_t* device = udm_find_device(context, dev);
        if (device) {
            context->callback(device, action, context->user_data);
        }
    }
}

void udm_process_events(udm_context_t* context) {
    int fd;
    struct timeval tv;

    tv.tv_sec = (time_t)context->select_sec;
    tv.tv_usec = (long int)context->select_usec;

    fd_set dup = context->fds;
    int res = select(context->max_fd + 1, &dup, NULL, NULL, &tv);

    if (res > 0) {
        for (fd = 0; fd <= context->max_fd; fd++) {
            if (FD_ISSET(fd, &dup)) {
                if (context->hotplug_fd == fd) {
                    udm_process_hotplug(context);
                    continue;
                }
            }
        }
    }
    printf("poll ended\n");
}

void udm_set_callback(udm_context_t* context, udm_callback_t callback, void* user_data) {
    context->user_data = user_data;
    context->callback = callback;
}

void udm_set_select_interval(udm_context_t* context, uint64_t sec, uint32_t usec) {
    context->select_sec = sec;
    context->select_usec = usec;
}

void udm_init(udm_context_t* context, const char* subsystem) {
    memset(context, 0, sizeof(udm_context_t));
    context->udev = udev_new();
    if (NULL == context->udev) {
        fprintf(stderr, "Can not initialize udev\n");
        return;
    }

    context->select_sec = 1;
    context->select_usec = 0;

    strcpy(context->subsystem, subsystem);
    
    printf("Populating hotplug file descriptor\n");
    udm_populate_hotplug_fd(context);

    printf("Initial device population\n");
    udm_populate_devices(context);
}
