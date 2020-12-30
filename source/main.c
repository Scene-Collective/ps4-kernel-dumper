#include "ps4.h"

#define KERNEL_CHUNK_SIZE PAGE_SIZE
#define KERNEL_CHUNK_NUMBER 0x1A42

int nthread_run;
char notify_buf[512];

void *nthread_func(void *arg) {
  UNUSED(arg);
  time_t t1, t2;
  t1 = 0;
  while (nthread_run) {
    if (notify_buf[0]) {
      t2 = time(NULL);
      if ((t2 - t1) >= 5) {
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

  char usb_name[64] = {0};
  char usb_path[64] = {0};
  char saveFile[64] = {0};

  initKernel();
  initLibc();
  initPthread();

  jailbreak();

  initSysUtil();

  nthread_run = 1;
  notify_buf[0] = '\0';
  ScePthread nthread;
  scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");

  uint64_t kernel_base = get_kernel_base();
  uint16_t fw_version = get_firmware();

  printf_notification("Running PS4 Kernel Dumper");

  if (!wait_for_usb(usb_name, usb_path)) {
    sprintf(notify_buf, "Waiting for USB device...");
    do {
      sceKernelSleep(1);
    } while (!wait_for_usb(usb_name, usb_path));
    notify_buf[0] = '\0';
  }

  sprintf(saveFile, "%s/kernel_dump_%i.bin", usb_path, fw_version);
  int fd = open(saveFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (fd < 0) {
    printf_notification("Unabled to create kernel dump! Quitting...");
    return 0;
  }

  printf_notification("USB device detected.\n\nStarting kernel dumping to %s.", usb_name);

  uint64_t *dump = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  uint64_t pos = 0;
  int percent = 0;
  for (int i = 0; i < KERNEL_CHUNK_NUMBER; i++) {
    get_memory_dump(kernel_base + pos, dump, KERNEL_CHUNK_SIZE);
    lseek(fd, pos, SEEK_SET);
    write(fd, (void *)dump, KERNEL_CHUNK_SIZE);
    percent = ((double)(KERNEL_CHUNK_SIZE * i) / ((double)KERNEL_CHUNK_SIZE * (double)KERNEL_CHUNK_NUMBER)) * 100;
    sprintf(notify_buf, "Kernel dumping to %s\nDone: %i%%", usb_name, percent);
    pos = pos + KERNEL_CHUNK_SIZE;
  }
  notify_buf[0] = '\0';

  close(fd);
  munmap(dump, 0x4000);

  printf_notification("Kernel dumped successfully!");

  nthread_run = 0;

  return 0;
}
