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

// Touch thread. Touchscreen driver wrapper needs run separately.
static void ts_thread() {
	symlink("/realdata/media/0/multirom/touchscreen", "/sbin/touchscreen");
	wait_for_file("/dev/touch", 10);
	wait_for_file("/realdata/media/0/multirom", 10);

	ERROR("Starting rm-wrapper...\n");
	char* argv[] = {"/sbin/touchscreen/rm-wrapper", NULL};
	char* envp[] = {"LD_LIBRARY_PATH=/sbin/touchscreen", NULL};
	execve(argv[0], &argv[0], envp);
}

// Watchdog thread. If this isn't running, device will reset after a period of time.
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

// If gk20a doesn't get firmware, boot to secondary is slow and the internal fails to init graphics.
// The symlink is needed for an automatic followup firmware load.
// There has got to be a simpler way to do this.
static void load_firmware() {
	int fd_loading = -1, c = 0;
	FILE *fd_firm = NULL, *fd_data = NULL;
	
	while ((fd_loading = open("/sys/class/firmware/gk20a!gpmu_ucode.bin/loading", O_WRONLY)) == -1)
		sleep(1);
	write(fd_loading, "1", 1);

	wait_for_file("/dev/block/platform/sdhci-tegra.3/by-name/APP", 10);
	mount("/dev/block/platform/sdhci-tegra.3/by-name/APP", "/system", "ext4", MS_RDONLY, NULL);
	symlink("/system/etc", "/etc");

	wait_for_file("/system/etc/firmware/gk20a/gpmu_ucode.bin", 10);
	fd_firm = fopen("/system/etc/firmware/gk20a/gpmu_ucode.bin", "rb");
	fd_data = fopen("/sys/class/firmware/gk20a!gpmu_ucode.bin/data", "wb");
	while((c=getc(fd_firm))!=EOF)
		fprintf(fd_data,"%c",c);

	if (fclose(fd_firm)) ERROR("Error closing firmware file.\n");
	if (fclose(fd_data)) ERROR("Error closing firmware sysfs file.\n");
	
	write(fd_loading, "0", 1);
	close(fd_loading);
	sleep(1); // wait on other firmware to load.
	unlink("/etc");
	umount("/system");
	ERROR("Finished firmware load.\n");
}

void mrom_hook_before_fb_close() {
	// Kill the extra threads
	kill_pid(PID_TOUCH);
	kill_pid(PID_WATCHDOG);
}

int mrom_hook_after_android_mounts(const char *busybox_path, const char *base_path, int type) {
	mrom_hook_before_fb_close();
	return 0;
}

void tramp_hook_before_device_init() {
	signal(SIGCHLD, SIG_IGN);

	// Spawn touch thread
	if (fork() == 0) {
		write_pid(PID_TOUCH, getpid());
		ts_thread();
		_exit(0);
	}

	// Spawn thread to load firmware
	if (fork() == 0) {
		load_firmware();
		_exit(0);
	}

	// Spawn watchdog thread
	if (fork() == 0) {
		write_pid(PID_WATCHDOG, getpid());
		wd_thread();
		_exit(0);
	}
}
