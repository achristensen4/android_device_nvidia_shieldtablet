#include <unistd.h>
#include <log.h>
#include <trampoline/devices.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <pthread.h>
#include <fcntl.h>

#define PID_TOUCH    "/touch.pid"
#define PID_WATCHDOG "/watchdog.pid"

static void write_pid(const char* path, int pid) {
	FILE* f = fopen(path, "w");
	int num = fprintf(f, "%d\n", pid);
	fclose(f);
}

static void kill_pid(const char* path) {
	int pid;
	FILE *f	= fopen(path, "r");
	if (f == NULL)
		return;
	fscanf(f, "%d", &pid);
	fclose(f);
	kill(pid, SIGKILL);
	unlink(path);
}

static void ts_thread() {
	symlink("/realdata/media/0/multirom/touchscreen", "/sbin/touchscreen");
	wait_for_file("/dev/touch", 10);
	wait_for_file("/realdata/media/0/multirom", 10);

	ERROR("Starting rm-wrapper...\n");
	char* argv[] = {"/sbin/touchscreen/rm-wrapper", NULL};
	char* envp[] = {"LD_LIBRARY_PATH=/sbin/touchscreen", NULL};
	execve(argv[0], &argv[0], envp);
}

static void wd_thread() {
	int fd = -1;

	while ((fd = open("/dev/watchdog", O_RDWR)) == -1)
		sleep(1);

	ERROR("Opened /dev/watchdog, preventing auto-reboot.\n");

	while (1) {
		write(fd, "", 1);
		sleep(5);
	}
}

void mrom_hook_before_fb_close() {
	kill_pid(PID_TOUCH);
	kill_pid(PID_WATCHDOG);
}

int mrom_hook_after_android_mounts(const char *busybox_path, const char *base_path, int type) {
	mrom_hook_before_fb_close();
	return 0;
}

void tramp_hook_before_device_init() {
	signal(SIGCHLD, SIG_IGN);

	// Touch thread. Touchscreen driver wrapper needs run separately.
	if (fork() == 0) {
		write_pid(PID_TOUCH, getpid());
		ts_thread();
		_exit(0);
	}

	// Watchdog thread. If this isn't running, device will reset after a period of time.
	if (fork() == 0) {
		write_pid(PID_WATCHDOG, getpid());
		wd_thread();
		_exit(0);
	}

	// If host1x isn't initialized in mr_init_devices, reboot to secondary is very slow.
	// However, host1x only tries to load firmware once and causes problems on the internal
	// rom if it is missing. Copy the firmware from the internal rom, so host1x will pick it up.
	mkdir("/system", 0755);
	mkdir("/etc", 0755);
	mkdir("/etc/firmware", 0755);
	mount("/dev/block/platform/sdhci-tegra.3/by-name/APP", "/system", "ext4", MS_RDONLY, NULL);
	wait_for_file("/system/etc", 10);
	system("cp /system/etc/firmware/nvavp_vid_ucode*.bin /etc/firmware/");
	umount("/system");
	unlink("/system");
}
