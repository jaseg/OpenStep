
import serial
import threading
import struct
import time

class TimeoutException(Exception):
	pass

escape = lambda s: s.replace(b'\\', b'\\\\')
unescape = lambda s: s.replace(b'\\\\', b'\\')

MAC_LEN = 16

def macrange(mac, mask):
	mac = list(mac) # clone array
	for i in range(16):
		mac[mask-1] = i
		yield mac

def pack_mac(mac):
	return [ mac[i]<<4 | mac[i+1] for i in range(0, len(mac), 2)]


class SerialMux(object):

	def __init__(self, device=None, baudrate=115200, ser=None, timeout=0.1):
		s = ser or LockableSerial(port=device, baudrate=baudrate, timeout=timeout)
		# Trust me, without the following two lines it *wont* *work*. Fuck serial ports.
		s.setXonXoff(True)
		s.setXonXoff(False)
		# Reset Arduinos
		s.setDTR(True)
		s.setDTR(False)
		self.ser = s
	
	def discover(self, mask=1, mac=None, found=None):
		""" Discover all node IDs connected to the bus
		
			Note: You do not need to set any of the arguments. These are used for the recursive discovery process.
		"""
		if mac is None:
			mac = [0]*MAC_LEN
		if found is None:
			found = []
		for a in macrange(mac, mask):
			next_address = len(found)
			if self._send_probe(a, mask, next_address):
				if(mask < MAC_LEN):
					print('-> descending.')
					self.discover(mask+1, a, found)
				else:
					print('-> found device.')
					found.append((pack_mac(a), next_address)) # clone array
		self.devices = found
		return found

	def _send_probe(self, mac, mask, next_address):
		with self.ser as s:
#			print('    probing', next_address, mask, mac)
			s.flushInput()
			s.flushInput()
			foo = b'\\?\xCC' + bytes([mask] + list(reversed(pack_mac(mac)[:(mask+1)//2])) + [next_address])
#			print('      tx', foo)
			s.write(foo)
			timeout_tmp = s.timeout
			s.timeout = 0.01
			try:
				_sync, token = s.read(2)
#				print(_sync, token)
				s.timeout = timeout_tmp
#				if token == 0xFF:
#					print(mask, mac)
				return token == 0xFF
			except TimeoutException:
				s.timeout = timeout_tmp
				return False

	def flash_led(self, device):
		with self.ser as s:
			s.write(b'\\?'+bytes([device])+b'\x03')

	def broadcast_acquire(self):
		with self.ser as s:
			s.write(b'\\?\xFF\x04') # turn off status leds for good measure
			s.write(b'\\?\xFF\x08')

	def broadcast_collect_data(self):
		with self.ser as s:
			s.flushInput()
			s.flushInput()
			s.write(b'\\?\xFF\x01')
			def try_rx(i):
				try:
					return s.rx_unescape(6)
				except:
					print('Error at', i)
			return [ struct.unpack('<HHH', try_rx(_i)) for _i in range(len(self.devices)) ]

	def collect_data(self, device):
		with self.ser as s:
			s.flushInput()
			s.flushInput()
			s.write(b'\\?'+bytes([device])+b'\x01')
			res = s.rx_unescape(6)
			foo = struct.unpack('<HHH', res)
			return foo
	
	def __del__(self):
		self.ser.close()

class LockableSerial(serial.Serial):
	def __init__(self, *args, **kwargs):
		super(serial.Serial, self).__init__(*args, **kwargs)
		self.lock = threading.RLock()

	def __enter__(self):
		self.lock.__enter__()
		return self
	
	def __exit__(self, *args):
		self.lock.__exit__(*args)
	
	def read(self, n):
		"""Read n bytes from the serial device and raise a TimeoutException in case of a timeout."""
		data = serial.Serial.read(self, n)
		if len(data) != n:
			raise TimeoutException('Read {} bytes trying to read {}'.format(len(data), n))
		return data

	def rx_unescape(self, n):
		r = b'$'
		while r != b'!':
			r = self.read(1)
		return self.read_unescape(n)

	def read_unescape(self, n):
		buf = b''
		escaped = False
		while len(buf)<n:
			r = self.read(1)
			if not escaped and r == b'\\':
				escaped = True
			else:
				buf += r
				escaped = False
		return buf

