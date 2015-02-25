
#include <unistd.h>

#define BCMD_GET_DATA         254
#define BCMD_ACQUIRE          255 /* usually followed by a break of a few milliseconds */

#define SAMPLE_DELAY_US       40000

typedef struct __attribute__((__packed__)) {

}

int read_adc(int serial) {
    cbm_send_broadcast(BCMD_ACQUIRE);
    usleep(SAMPLE_DELAY_US);
    cbm_send_broadcast(BCMD_GET_DATA);
    tcflush(serial, TCIFLUSH);

    ssize_t rd = read(serial, &c, 1);

    samples = [read_sample(s, i) for i in range(n)]
def read_sample(s, i):
    a,b,c = struct.unpack('<HHH', s.ser.rx_unescape(6))
	def rx_unescape(self, n):
		self.flushInput()
		self.flushInput()
		r = b'$'
		while r != b'!':
			r = self.read(1)
		return self.read_unescape(n)
}
