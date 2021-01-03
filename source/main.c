#include "ps4.h"

#define KERNEL_CHUNK_SIZE PAGE_SIZE
#define KERNEL_CHUNK_NUMBER 0x1A42

int nthread_run = 1;
int notify_time = 20;
char notify_buf[512] = {0};

void *nthread_func(void *arg) {
  UNUSED(arg);
  time_t t1 = 0;
  while (nthread_run) {
    if (notify_buf[0]) {
      time_t t2 = time(NULL);
      if ((t2 - t1) >= notify_time) {
        t1 = t2;
        systemMessage(notify_buf);
      }
    } else {
      t1 = 0;
    }
    sceKernelSleep(1);
  }
  return NULL;
}

int _main(struct thread *td) {
  UNUSED(td);

  char fw_version[6] = {0};
  char usb_name[7] = {0};
  char usb_path[13] = {0};
  char output_root[PATH_MAX] = {0};
  char saveFile[PATH_MAX] = {0};
  char completion_check[PATH_MAX] = {0};

  initKernel();
  initLibc();
  initPthread();

  jailbreak();

  initSysUtil();

  get_firmware_string(fw_version);
  uint64_t kernel_base = get_kernel_base();

  ScePthread nthread;
  scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");

  printf_notification("Running Kernel Dumper");

  if (!wait_for_usb(usb_name, usb_path)) {
    snprintf_s(notify_buf, sizeof(notify_buf), "Waiting for USB device...");
    do {
      sceKernelSleep(1);
    } while (!wait_for_usb(usb_name, usb_path));
    notify_buf[0] = '\0';
  }

  snprintf_s(output_root, sizeof(output_root), "%s/PS4", usb_path);
  mkdir(output_root, 0777);
  snprintf_s(output_root, sizeof(output_root), "%s/%s", output_root, fw_version);
  mkdir(output_root, 0777);

  snprintf_s(saveFile, sizeof(saveFile), "%s/PS4/%s/kernel.bin", usb_path, fw_version);

  snprintf_s(completion_check, sizeof(completion_check), "%s/kernel.complete", output_root);
  if (file_exists(completion_check)) {
    printf_notification("Kernel already dumped for %s, skipping dumping", fw_version);
    return 0;
  } else {
    unlink(saveFile);
  }

  int fd = open(saveFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (fd < 0) {
    printf_notification("Unabled to create kernel dump! Quitting...");
    return 0;
  }

  printf_notification("USB device detected.\n\nStarting kernel dumping to %s.", usb_name);

  notify_time = 5;
  uint64_t *dump = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  uint64_t pos = 0;
  for (int i = 0; i < KERNEL_CHUNK_NUMBER; i++) {
    get_memory_dump(kernel_base + pos, dump, KERNEL_CHUNK_SIZE);
    lseek(fd, pos, SEEK_SET);
    write(fd, (void *)dump, KERNEL_CHUNK_SIZE);
    int percent = ((double)(KERNEL_CHUNK_SIZE * i) / ((double)KERNEL_CHUNK_SIZE * (double)KERNEL_CHUNK_NUMBER)) * 100;
    snprintf_s(notify_buf, sizeof(notify_buf), "Kernel dumping to %s\nDone: %i%%", usb_name, percent);
    pos = pos + KERNEL_CHUNK_SIZE;
  }
  notify_buf[0] = '\0';
  nthread_run = 0;

  close(fd);
  munmap(dump, 0x4000);

  touch_file(completion_check);

  printf_notification("Kernel dumped successfully!");

  return 0;
}
