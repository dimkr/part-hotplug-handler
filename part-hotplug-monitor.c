#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <libudev.h>

/* the subsystem to filter events by */
#define PARTITION_SUBSYSTEM "block"

/* the device type to filter events by */
#define PARTITION_DEVTYPE "partition"

/* the event source - can be either "udev" or "kernel" */
#define EVENT_SOURCE "udev"

/* the program to run */
#define CHILD_PROCESS_FILE "part-hotplug-handler"

/* a boolean data type */
typedef enum {
	FALSE,
	TRUE
} bool_t;

#define GET_DEVICE_PROPERTY(entry, entries, entry_name, value, failure_label) \
	entry = udev_list_entry_get_by_name(entries, entry_name); \
	if (NULL == entry) \
		goto failure_label; \
	value = udev_list_entry_get_value(entry)

bool_t run_child_process(const char *device_name,
                         const char *device_path,
                         const char *event_type) {
	/* the child process ID */
	pid_t child_pid;

	/* the child process command-line arguments */
	char *child_arguments[2];

	/* create a child process */
	child_pid = fork();

	switch (child_pid) {
		/* if an error occurred, report failure */
		case (-1):
			return FALSE;

		case 0:
			/* put the information received from udev in the environment */
			if (-1 == setenv("DEVNAME", device_name, 1))
				return FALSE;
			if (-1 == setenv("DEVPATH", device_path, 1))
				return FALSE;
			if (-1 == setenv("ACTION", event_type, 1))
				return FALSE;

			/* initialize the child process command-line arguments array */
			child_arguments[0] = CHILD_PROCESS_FILE;
			child_arguments[1] = NULL;

			/* execute the child process */
			(void) execvp(CHILD_PROCESS_FILE, child_arguments);

			/* execvp() returns upon failure - terminate the child process */
			_exit(EXIT_FAILURE);

		default:
			return TRUE;
	}
}

int main(int argc, char *argv[]) {
	int exit_code = EXIT_FAILURE;

	/* the udev library context */
    struct udev *udev;

    /* the device event source */
    struct udev_monitor *udev_monitor;

	/* the udev socket file descriptor */
	int monitor_fd;

	/* the set of polled file descriptors */
	fd_set fds;

	/* the device a received event is associated with */
	struct udev_device *device;

	/* the device properties */
	struct udev_list_entry *device_properties;

	/* a single device property */
	struct udev_list_entry *device_property;

	/* the event type */
	const char *event_type;

	/* the device name */
	const char *device_name;

	/* the device path under sysfs */
	const char *device_path;

	/* initialize a udev context */
    if (NULL == (udev = udev_new()))
		goto end;

	/* start monitoring events */
    if (NULL == (udev_monitor = udev_monitor_new_from_netlink(udev,
                                                              EVENT_SOURCE)))
        goto free_udev;

	/* filter only partition-related events */
    if (0 != udev_monitor_filter_add_match_subsystem_devtype(
                                                            udev_monitor,
                                                            PARTITION_SUBSYSTEM,
                                                            PARTITION_DEVTYPE))
		goto free_udev;

	/* start receiving events */
	if (0 != udev_monitor_enable_receiving(udev_monitor))
		goto free_udev;

	/* empty the file descriptor set */
	FD_ZERO(&fds);

	/* add the udev socket file descriptor to the set */
	monitor_fd = udev_monitor_get_fd(udev_monitor);
	FD_SET(monitor_fd, &fds);

	do {
		/* block until an event is received */
		if (-1 == select(1 + monitor_fd, &fds, NULL, NULL, NULL))
			goto free_udev;

		/* receive the associated device information */
		if (NULL == (device = udev_monitor_receive_device(udev_monitor)))
			goto free_udev;

		/* get the device properties list */
		if (NULL == (device_properties = udev_device_get_properties_list_entry(
		                                                               device)))
			goto free_device;

		/* check the event type */
		event_type = udev_device_get_action(device);

		if ((0 == strcmp(event_type, "add")) ||
			(0 == strcmp(event_type, "remove")) ||
			(0 == strcmp(event_type, "change"))) {

			/* get the device name */
			GET_DEVICE_PROPERTY(device_property,
			                    device_properties,
			                    "DEVNAME",
			                    device_name,
			                    free_device);

			/* get the device path */
			GET_DEVICE_PROPERTY(device_property,
			                    device_properties,
			                    "DEVPATH",
			                    device_path,
			                    free_device);

			/* run the child process */
			if (FALSE == run_child_process(device_name,
			                               device_path,
			                               event_type))
				goto free_device;
		}

		/* free the device */
		udev_device_unref(device);
	}  while (TRUE);

	/* report success */
	exit_code = EXIT_SUCCESS;
	goto free_udev;

free_device:
	/* free the device */
	udev_device_unref(device);

free_udev:
	/* free the udev context */
	udev_unref(udev);

end:
    return exit_code;
}