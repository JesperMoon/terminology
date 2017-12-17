#include "private.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#include <Eina.h>
#include "tycommon.h"

static void
print_usage(const char *argv0)
{
   printf("Usage: %s"HELP_ARGUMENT_SHORT" FILE1 [FILE2 ...]\n"
          "  Send file(s) to the terminal to save\n"
          HELP_ARGUMENT_DOC"\n"
          "\n",
          argv0);
}

static struct termios told, tnew;

static int
echo_off(void)
{
   if (tcgetattr(0, &told) != 0) return -1;
   tnew = told;
   tnew.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
   tnew.c_oflag &= ~(OPOST);
   tnew.c_lflag &= ~(ISIG | ICANON | ECHO | IEXTEN);
   tnew.c_cflag &= ~(CSIZE | PARENB);
   tnew.c_cflag |= CS8;
   tnew.c_cc[VMIN] = 1;
   tnew.c_cc[VTIME] = 0;
   if (tcsetattr(0, TCSAFLUSH, &tnew) != 0) return -1;
   return 0;
}

static int
echo_on(void)
{
   return tcsetattr(0, TCSAFLUSH, &told);
}

int
main(int argc, char **argv)
{
   int i;

   ON_NOT_RUNNING_IN_TERMINOLOGY_EXIT_1();
   ARGUMENT_ENTRY_CHECK(argc, argv, print_usage);

   if (argc <= 1)
     {
        print_usage(argv[0]);
        return 0;
     }

   echo_off();
   for (i = 1; i < argc; i++)
     {
        char *path, buf[8192], tbuf[PATH_MAX * 3];
        unsigned char rawbuf[8192 + 128], rawbuf2[8192 + 128];
        int file_fd, pksize, pksum, bin, bout;

        path = argv[i];
        snprintf(tbuf, sizeof(tbuf), "%c}fr%s", 0x1b, path);
        if (write(1, tbuf, strlen(tbuf) + 1) != (signed)(strlen(tbuf) + 1))
          goto err;
        file_fd = open(path, O_RDONLY);
        if (file_fd >= 0)
          {
             off_t off;

             off = lseek(file_fd, 0, SEEK_END);
             lseek(file_fd, 0, SEEK_SET);
             snprintf(tbuf, sizeof(tbuf), "%c}fs%llu", 0x1b, (unsigned long long)off);
             if (write(1, tbuf, strlen(tbuf) + 1) != (signed)(strlen(tbuf) + 1))
               goto err;
             for (;;)
               {
                  if (read(0, buf, 2) == 2)
                    {
                       if (buf[0] == 'k')
                         {
                            pksize = read(file_fd, rawbuf, 8192);

                            if (pksize > 0)
                              {
                                 bout = 0;
                                 for (bin = 0; bin < pksize; bin++)
                                   {
                                      if (rawbuf[bin] == 0x00)
                                        {
                                           rawbuf2[bout++] = 0xff;
                                           rawbuf2[bout++] = 0x01;
                                        }
                                      else if (rawbuf[bin] == 0xff)
                                        {
                                           rawbuf2[bout++] = 0xff;
                                           rawbuf2[bout++] = 0x02;
                                        }
                                      else
                                        {
                                           rawbuf2[bout++] = rawbuf[bin];
                                        }
                                   }
                                 rawbuf2[bout] = 0;
                                 pksum = 0;
                                 for (bin = 0; bin < bout; bin++)
                                   {
                                      pksum += rawbuf2[bin];
                                   }
                                 snprintf(tbuf, sizeof(tbuf), "%c}fd%i ", 0x1b, pksum);
                                 if (write(1, tbuf, strlen(tbuf)) != (signed)(strlen(tbuf)))
                                   goto err;
                                 if (write(1, rawbuf2, bout + 1) != bout + 1)
                                   goto err;
                              }
                            else break;
                         }
                       else
                         {
                            echo_on();
                            fprintf(stderr, "Send Fail\n");
                            goto err;
                         }
                    }
                  else goto err;
               }
             close(file_fd);
          }
        snprintf(tbuf, sizeof(tbuf), "%c}fx", 0x1b);
        if (write(1, tbuf, strlen(tbuf) + 1) != (signed)(strlen(tbuf) + 1))
          goto err;
        tbuf[0] = 0;
        if (write(1, tbuf, 1) != 1)
          goto err;
     }
   echo_on();
   return 0;
err:
   echo_on();
   return -1;
}
