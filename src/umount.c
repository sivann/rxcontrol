/*sivann@cs.ntua.gr*/

/*warning, /etc/mtab & mount(1) doesn't report mounts correctly after this,
  you must use /proc/mounts*/

#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/cdrom.h>
#include <errno.h>

#include <sys/mount.h>

main()
{
int ret,fd;

  printf("Trying umount...\n");
  ret=umount("/mnt/cdrom");
  if (ret) perror("umount");
  else {
    printf("Success unmounting\n");
    update_mtab ("/mnt/cdrom", NULL);
  }


}
