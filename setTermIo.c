
//================================================================================================
// https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>

#define   error_message   printf


#if 1
char *InitCOM( int hSerial, int speed, int parity )
{
#else
const char *InitCOM(const char *TTY)
{
    hSerial = open(TTY, O_RDWR | O_NOCTTY | O_NDELAY);
	
    if(hSerial == -1)
		return "Opening of the port failed";
#endif
	
    struct termios tty;

    fcntl(hSerial, F_SETFL, 0);
    if(tcgetattr(hSerial, &tty) != 0)
		return "Getting the parameters failed.";
	
    if(cfsetispeed(&tty, speed) != 0)
		return "Setting the baud rate failed.";

    //CFlags
    //Note: I am full aware, that there's an '=', and that it makes the '&=' obsolete, but they're in there for the sake of completeness.
    tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;    // 8-bit characters
    tty.c_cflag |= (CLOCAL | CREAD);                // und erlaubt 'Lesen'.
    tty.c_cflag &= ~(PARENB | PARODD);          
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;                    

    //Input Flags
    tty.c_iflag     &= ~IGNBRK;             
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         

    //Local Flags
    tty.c_lflag  = 0;                   

    //Output Flags
    tty.c_oflag  = 0;

    //Control-Characters
    tty.c_cc[VMIN]   = 0;
    tty.c_cc[VTIME]  = 5;
	
    if(tcsetattr(hSerial, TCSAFLUSH, &tty) != 0)
		return "Setting the new parameters failed";

	return NULL;
}


int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        if (tcgetattr (fd, &tty) != 0)
        {
                error_message ("- error %d from tcgetattr: %s\n", errno, strerror (errno));
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);        // ignore modem controls,
                                                // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                error_message ("error %d from tcsetattr: %s\n", errno, strerror (errno));
                return -1;
        }
        return 0;
}


int set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                error_message ("error %d from tggetattr: %s\n", errno, strerror (errno));
                return -1;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;                      // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
		{
                error_message ("error %d setting term attributes: %s\n", errno, strerror (errno));
				return -1;
		}
		return 0;
}
