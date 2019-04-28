#include <stdio.h>
#include "udm.h"

void callback(udm_device_t* device, const char* action, void* user_data) {
    printf("action: %s on device %s\n", action, device->devnode);
}

int main(int argv, char** argc) {
	udm_context_t udm_context;
	udm_init(&udm_context, "drm");
    udm_set_callback(&udm_context, callback, NULL);
    while (1) {
	    udm_process_events(&udm_context);
    }
	return 0;
}
